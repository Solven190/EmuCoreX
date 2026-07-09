// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/Common/GSGPUProfilePrivate.h"

#include <array>
#include <cctype>

#if defined(__ANDROID__)
#include <sys/system_properties.h>
#endif

namespace GpuProfileDetail
{
std::string ToLowerASCII(std::string_view value)
{
	std::string lowered;
	lowered.reserve(value.size());

	for (const char ch : value)
		lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));

	return lowered;
}

static bool Contains(std::string_view haystack, std::string_view needle)
{
	return (haystack.find(needle) != std::string_view::npos);
}

bool ContainsAny(std::string_view haystack, std::initializer_list<const char*> needles)
{
	for (const char* needle : needles)
	{
		if (Contains(haystack, needle))
			return true;
	}

	return false;
}

MobileGsTuning MakeMobileGsTuning(MobileGpuTier tier)
{
	MobileGsTuning tuning;
	tuning.tier = tier;

	switch (tier)
	{
		case MobileGpuTier::Low:
			tuning.constrained = true;
			tuning.prefer_new_textures = false;
			tuning.force_partial_texture_preloading = true;
			tuning.prefer_mobile_light_gs = false;
			tuning.prefer_mobile_sw_blend = false;
			tuning.pooled_targets = 72;
			tuning.target_age = 6;
			tuning.pooled_textures = 72;
			tuning.texture_age = 5;
			break;

		case MobileGpuTier::Mid:
			tuning.constrained = true;
			tuning.prefer_new_textures = false;
			tuning.force_partial_texture_preloading = true;
			tuning.prefer_mobile_light_gs = false;
			tuning.pooled_targets = 96;
			tuning.target_age = 8;
			tuning.pooled_textures = 96;
			tuning.texture_age = 6;
			break;

		case MobileGpuTier::High:
		default:
			tuning.constrained = false;
			tuning.prefer_new_textures = true;
			tuning.force_partial_texture_preloading = false;
			tuning.pooled_targets = 160;
			tuning.target_age = 12;
			tuning.pooled_textures = 160;
			tuning.texture_age = 8;
			break;
	}

	return tuning;
}
} // namespace GpuProfileDetail

namespace
{
static void AppendHint(std::string& hints, std::string_view key, std::string_view value)
{
	if (value.empty())
		return;

	if (!hints.empty())
		hints.append(" | ");

	if (!key.empty())
	{
		hints.append(key);
		hints.push_back('=');
	}

	hints.append(value);
}

#if defined(__ANDROID__)
static std::string GetAndroidProperty(const char* name)
{
	std::array<char, PROP_VALUE_MAX> value = {};
	const int length = __system_property_get(name, value.data());
	return (length > 0) ? std::string(value.data(), static_cast<size_t>(length)) : std::string();
}
#endif

static std::string BuildHints(std::string_view gpu_vendor, std::string_view gpu_renderer_or_name)
{
	std::string hints;
	AppendHint(hints, "gpu_vendor", gpu_vendor);
	AppendHint(hints, "gpu", gpu_renderer_or_name);

#if defined(__ANDROID__)
	static constexpr const char* property_names[] = {
		"ro.soc.manufacturer",
		"ro.soc.model",
		"ro.soc.platform",
		"ro.board.platform",
		"ro.hardware",
		"ro.hardware.chipname",
		"ro.chipname",
		"ro.product.board",
		"ro.product.manufacturer",
		"ro.product.model",
		"ro.vendor.product.manufacturer",
		"ro.vendor.product.model",
		"ro.mediatek.platform",
		"ro.vendor.mediatek.platform",
		"ro.product.cpu.abi",
		"ro.vendor.product.cpu.abilist",
	};

	for (const char* property_name : property_names)
		AppendHint(hints, property_name, GetAndroidProperty(property_name));
#endif

	return hints;
}
} // namespace

GpuProfileOverride GpuProfileDetector::ParseOverride(std::string_view value)
{
	const std::string lowered = GpuProfileDetail::ToLowerASCII(value);
	if (lowered == "mali")
		return GpuProfileOverride::Mali;
	if (lowered == "adreno")
		return GpuProfileOverride::Adreno;
	if (lowered == "powervr")
		return GpuProfileOverride::PowerVR;

	return GpuProfileOverride::Auto;
}

const char* GpuProfileDetector::OverrideToConfigString(GpuProfileOverride value)
{
	switch (value)
	{
		case GpuProfileOverride::Mali:
			return "mali";
		case GpuProfileOverride::Adreno:
			return "adreno";
		case GpuProfileOverride::PowerVR:
			return "powervr";
		case GpuProfileOverride::Auto:
		default:
			return "auto";
	}
}

const char* GpuProfileDetector::OverrideToString(GpuProfileOverride value)
{
	switch (value)
	{
		case GpuProfileOverride::Mali:
			return "Force Mali";
		case GpuProfileOverride::Adreno:
			return "Force Adreno";
		case GpuProfileOverride::PowerVR:
			return "Force PowerVR";
		case GpuProfileOverride::Auto:
		default:
			return "Auto";
	}
}

const char* GpuProfileDetector::RuntimeProfileToString(RuntimeGpuProfile value)
{
	switch (value)
	{
		case RuntimeGpuProfile::Mali:
			return "Mali";
		case RuntimeGpuProfile::PowerVR:
			return "PowerVR";
		case RuntimeGpuProfile::Adreno:
		default:
			return "Adreno";
	}
}

const char* GpuProfileDetector::MobileTierToString(MobileGpuTier value)
{
	switch (value)
	{
		case MobileGpuTier::Low:
			return "Low";
		case MobileGpuTier::Mid:
			return "Mid";
		case MobileGpuTier::High:
		default:
			return "High";
	}
}

GpuProfileSelection GpuProfileDetector::Resolve(std::string_view override_value, std::string_view gpu_vendor,
	std::string_view gpu_renderer_or_name)
{
	GpuProfileSelection selection;
	selection.override_mode = ParseOverride(override_value);
	selection.hints = BuildHints(gpu_vendor, gpu_renderer_or_name);
	const std::string lowered_hints = GpuProfileDetail::ToLowerASCII(selection.hints);

	if (selection.override_mode == GpuProfileOverride::Mali)
	{
		selection.runtime_profile = RuntimeGpuProfile::Mali;
		selection.gs_tuning = GpuProfileDetail::GetMaliTuning(GpuProfileDetail::ResolveMaliTier(lowered_hints));
		return selection;
	}

	if (selection.override_mode == GpuProfileOverride::Adreno)
	{
		selection.runtime_profile = RuntimeGpuProfile::Adreno;
		selection.gs_tuning = GpuProfileDetail::GetAdrenoTuning(GpuProfileDetail::ResolveAdrenoTier(lowered_hints));
		return selection;
	}

	if (selection.override_mode == GpuProfileOverride::PowerVR)
	{
		selection.runtime_profile = RuntimeGpuProfile::PowerVR;
		selection.gs_tuning = GpuProfileDetail::GetPowerVRTuning(GpuProfileDetail::ResolvePowerVRTier(lowered_hints));
		return selection;
	}

#if defined(__ANDROID__)
	if (GpuProfileDetail::LooksLikeAdreno(lowered_hints))
	{
		selection.runtime_profile = RuntimeGpuProfile::Adreno;
		selection.gs_tuning = GpuProfileDetail::GetAdrenoTuning(GpuProfileDetail::ResolveAdrenoTier(lowered_hints));
	}
	else if (GpuProfileDetail::LooksLikePowerVR(lowered_hints))
	{
		selection.runtime_profile = RuntimeGpuProfile::PowerVR;
		selection.gs_tuning = GpuProfileDetail::GetPowerVRTuning(GpuProfileDetail::ResolvePowerVRTier(lowered_hints));
	}
	else if (GpuProfileDetail::LooksLikeMali(lowered_hints))
	{
		selection.runtime_profile = RuntimeGpuProfile::Mali;
		selection.gs_tuning = GpuProfileDetail::GetMaliTuning(GpuProfileDetail::ResolveMaliTier(lowered_hints));
	}
	else
	{
		selection.runtime_profile = RuntimeGpuProfile::Adreno;
		selection.gs_tuning = GpuProfileDetail::GetAdrenoTuning(MobileGpuTier::High);
	}
#else
	selection.runtime_profile = RuntimeGpuProfile::Adreno;
	selection.gs_tuning = GpuProfileDetail::GetAdrenoTuning(MobileGpuTier::High);
#endif

	return selection;
}
