// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/DynamicLibrary.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/SmallString.h"
#include "common/StringUtil.h"

#include "fmt/format.h"

#include <cstring>
#include <dlfcn.h>

DynamicLibrary::DynamicLibrary() = default;

DynamicLibrary::DynamicLibrary(const char* filename)
{
	Error error;
	if (!Open(filename, &error))
		Console.ErrorFmt("DynamicLibrary open failed: {}", error.GetDescription());
}

DynamicLibrary::DynamicLibrary(DynamicLibrary&& move)
	: m_handle(move.m_handle)
{
	move.m_handle = nullptr;
}

DynamicLibrary::~DynamicLibrary()
{
	Close();
}

std::string DynamicLibrary::GetUnprefixedFilename(const char* filename)
{
	return std::string(filename) + ".so";
}

std::string DynamicLibrary::GetVersionedFilename(const char* libname, int, int)
{
	const char* prefix = std::strncmp(libname, "lib", 3) ? "lib" : "";
	return fmt::format("{}{}.so", prefix, libname);
}

bool DynamicLibrary::Open(const char* filename, Error* error)
{
	m_handle = dlopen(filename, RTLD_NOW);
	if (!m_handle)
	{
		const char* err = dlerror();
		Error::SetStringFmt(error, "Loading {} failed: {}", filename, err ? err : "<UNKNOWN>");
		return false;
	}

	return true;
}

void DynamicLibrary::Adopt(void* handle)
{
	pxAssertRel(handle, "Handle is valid");

	Close();

	m_handle = handle;
}

void DynamicLibrary::Close()
{
	if (!IsOpen())
		return;

	dlclose(m_handle);
	m_handle = nullptr;
}

void* DynamicLibrary::GetSymbolAddress(const char* name) const
{
	return reinterpret_cast<void*>(dlsym(m_handle, name));
}

DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& move)
{
	Close();
	m_handle = move.m_handle;
	move.m_handle = nullptr;
	return *this;
}
