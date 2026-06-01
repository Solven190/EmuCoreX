// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/Vulkan/VKLoader.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/DynamicLibrary.h"
#include "common/Error.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {

#define VULKAN_MODULE_ENTRY_POINT(name, required) PFN_##name name;
#define VULKAN_INSTANCE_ENTRY_POINT(name, required) PFN_##name name;
#define VULKAN_DEVICE_ENTRY_POINT(name, required) PFN_##name name;
#include "VKEntryPoints.inl"
#undef VULKAN_DEVICE_ENTRY_POINT
#undef VULKAN_INSTANCE_ENTRY_POINT
#undef VULKAN_MODULE_ENTRY_POINT
}

void Vulkan::ResetVulkanLibraryFunctionPointers()
{
#define VULKAN_MODULE_ENTRY_POINT(name, required) name = nullptr;
#define VULKAN_INSTANCE_ENTRY_POINT(name, required) name = nullptr;
#define VULKAN_DEVICE_ENTRY_POINT(name, required) name = nullptr;
#include "VKEntryPoints.inl"
#undef VULKAN_DEVICE_ENTRY_POINT
#undef VULKAN_INSTANCE_ENTRY_POINT
#undef VULKAN_MODULE_ENTRY_POINT
}

static DynamicLibrary s_vulkan_library;
static bool s_using_direct_wrapper_icd = false;

static VKAPI_ATTR VkResult VKAPI_CALL EmuCoreXWrapperEnumerateInstanceLayerProperties(
	uint32_t* pPropertyCount, VkLayerProperties* pProperties)
{
	if (!pPropertyCount)
		return VK_ERROR_INITIALIZATION_FAILED;

	if (!pProperties)
	{
		*pPropertyCount = 0;
		return VK_SUCCESS;
	}

	*pPropertyCount = 0;
	return VK_SUCCESS;
}

template <typename T>
static T EmuCoreXGetWrapperModuleEntryPointFallback(const char* name, T)
{
	return nullptr;
}

static PFN_vkEnumerateInstanceLayerProperties EmuCoreXGetWrapperModuleEntryPointFallback(
	const char* name, PFN_vkEnumerateInstanceLayerProperties)
{
	return (std::strcmp(name, "vkEnumerateInstanceLayerProperties") == 0) ?
		&EmuCoreXWrapperEnumerateInstanceLayerProperties :
		nullptr;
}

bool Vulkan::IsVulkanLibraryLoaded()
{
	return s_vulkan_library.IsOpen();
}

bool Vulkan::LoadVulkanLibrary(Error* error)
{
	pxAssertRel(!s_vulkan_library.IsOpen(), "Vulkan module is not loaded.");

	const auto load_and_validate = [](const char* filename, Error* error) {
		if (!s_vulkan_library.Open(filename, error))
			return false;

		bool required_functions_missing = false;
#define VULKAN_MODULE_ENTRY_POINT(name, required) \
	if (!s_vulkan_library.GetSymbol(#name, &name)) \
	{ \
		ERROR_LOG("Vulkan: Failed to load required module function {}", #name); \
		required_functions_missing = true; \
	}

#include "VKEntryPoints.inl"
#undef VULKAN_MODULE_ENTRY_POINT

		if (required_functions_missing)
		{
			ResetVulkanLibraryFunctionPointers();
			s_vulkan_library.Close();
			Error::SetStringFmt(error, "Loading {} failed: required Vulkan loader functions are missing", filename);
			return false;
		}

		return true;
	};

	const char* libvulkan_env = getenv("LIBVULKAN_PATH");
#if defined(__ANDROID__)
	const bool use_direct_wrapper_icd = std::getenv("EMUCOREX_VULKAN_WRAPPER_ACTIVE") != nullptr;
	if (use_direct_wrapper_icd)
	{
		if (!libvulkan_env || libvulkan_env[0] == '\0')
		{
			Error::SetStringView(error, "Vulkan wrapper is enabled, but wrapper library path is empty");
			return false;
		}

		if (!s_vulkan_library.Open(libvulkan_env, error))
			return false;

		PFN_vkGetInstanceProcAddr icd_get_instance_proc_addr = nullptr;
		if (!s_vulkan_library.GetSymbol("vk_icdGetInstanceProcAddr", &icd_get_instance_proc_addr))
		{
			ResetVulkanLibraryFunctionPointers();
			s_vulkan_library.Close();
			Error::SetStringFmt(error, "Loading {} failed: vk_icdGetInstanceProcAddr is missing", libvulkan_env);
			return false;
		}

		bool required_functions_missing = false;
#define VULKAN_MODULE_ENTRY_POINT(name, required) \
		if (std::strcmp(#name, "vkDestroyInstance") != 0) \
		{ \
			name = reinterpret_cast<PFN_##name>(icd_get_instance_proc_addr(VK_NULL_HANDLE, #name)); \
			if (!name && std::strcmp(#name, "vkEnumerateInstanceLayerProperties") == 0) \
				name = EmuCoreXGetWrapperModuleEntryPointFallback(#name, name); \
			if (!name && required) \
			{ \
				ERROR_LOG("Vulkan wrapper: Failed to load required ICD function {}", #name); \
				required_functions_missing = true; \
			} \
		}

#include "VKEntryPoints.inl"
#undef VULKAN_MODULE_ENTRY_POINT

		if (required_functions_missing)
		{
			ResetVulkanLibraryFunctionPointers();
			s_vulkan_library.Close();
			Error::SetStringFmt(error, "Loading {} failed: required Vulkan wrapper ICD functions are missing", libvulkan_env);
			return false;
		}

		s_using_direct_wrapper_icd = true;
		return true;
	}
#endif

	if (libvulkan_env && libvulkan_env[0] != '\0')
	{
		Error selected_error;
		if (!load_and_validate(libvulkan_env, &selected_error))
			WARNING_LOG("Vulkan: Selected Vulkan library '{}' could not be used, falling back to system Vulkan: {}", libvulkan_env, selected_error.GetDescription());
	}

#ifdef __APPLE__
	// Check if a path to a specific Vulkan library has been specified.
	if (!s_vulkan_library.IsOpen() &&
		!load_and_validate(DynamicLibrary::GetVersionedFilename("MoltenVK").c_str(), error))
	{
		return false;
	}
#else
	// try versioned first, then unversioned.
	if (!s_vulkan_library.IsOpen() &&
		!load_and_validate(DynamicLibrary::GetVersionedFilename("vulkan", 1).c_str(), error) &&
		!load_and_validate(DynamicLibrary::GetVersionedFilename("vulkan").c_str(), error))
	{
		return false;
	}
#endif

	return true;
}

void Vulkan::UnloadVulkanLibrary()
{
	ResetVulkanLibraryFunctionPointers();
	s_using_direct_wrapper_icd = false;
	s_vulkan_library.Close();
}

bool Vulkan::LoadVulkanInstanceFunctions(VkInstance instance)
{
	bool required_functions_missing = false;

	if (s_using_direct_wrapper_icd && !vkDestroyInstance)
	{
		vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(vkGetInstanceProcAddr(instance, "vkDestroyInstance"));
		if (!vkDestroyInstance)
		{
			std::fprintf(stderr, "Vulkan: Failed to load required instance function vkDestroyInstance\n");
			required_functions_missing = true;
		}
	}

	auto LoadFunction = [&required_functions_missing, instance](PFN_vkVoidFunction* func_ptr, const char* name, bool is_required) {
		*func_ptr = vkGetInstanceProcAddr(instance, name);
		if (!(*func_ptr) && is_required)
		{
			std::fprintf(stderr, "Vulkan: Failed to load required instance function %s\n", name);
			required_functions_missing = true;
		}
	};

#define VULKAN_INSTANCE_ENTRY_POINT(name, required) \
	LoadFunction(reinterpret_cast<PFN_vkVoidFunction*>(&name), #name, required);
#include "VKEntryPoints.inl"
#undef VULKAN_INSTANCE_ENTRY_POINT

	return !required_functions_missing;
}

bool Vulkan::LoadVulkanDeviceFunctions(VkDevice device)
{
	bool required_functions_missing = false;
	auto LoadFunction = [&required_functions_missing, device](PFN_vkVoidFunction* func_ptr, const char* name, bool is_required) {
		*func_ptr = vkGetDeviceProcAddr(device, name);
		if (!(*func_ptr) && is_required)
		{
			std::fprintf(stderr, "Vulkan: Failed to load required device function %s\n", name);
			required_functions_missing = true;
		}
	};

#define VULKAN_DEVICE_ENTRY_POINT(name, required) \
	LoadFunction(reinterpret_cast<PFN_vkVoidFunction*>(&name), #name, required);
#include "VKEntryPoints.inl"
#undef VULKAN_DEVICE_ENTRY_POINT

	return !required_functions_missing;
}
