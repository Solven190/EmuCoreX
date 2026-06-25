// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <string>
#include <string_view>

enum class GpuProfileOverride : u8
{
	Auto,
	Mali,
	Adreno,
	PowerVR,
};

enum class RuntimeGpuProfile : u8
{
	Mali,
	Adreno,
	PowerVR,
};

enum class MobileGpuTier : u8
{
	Low,
	Mid,
	High,
};

struct MobileGsTuning
{
	MobileGpuTier tier = MobileGpuTier::High;
	bool constrained = false;
	bool prefer_new_textures = true;
	bool force_partial_texture_preloading = false;
	bool prefer_mobile_light_gs = false;
	bool prefer_mobile_sw_blend = false;
	u32 pooled_targets = 160;
	u32 target_age = 12;
	u32 pooled_textures = 160;
	u32 texture_age = 8;
};

struct GpuProfileSelection
{
	GpuProfileOverride override_mode = GpuProfileOverride::Auto;
	RuntimeGpuProfile runtime_profile = RuntimeGpuProfile::Adreno;
	MobileGsTuning gs_tuning;
	std::string hints;
};

class GpuProfileDetector
{
public:
	static GpuProfileOverride ParseOverride(std::string_view value);
	static const char* OverrideToConfigString(GpuProfileOverride value);
	static const char* OverrideToString(GpuProfileOverride value);
	static const char* RuntimeProfileToString(RuntimeGpuProfile value);
	static const char* MobileTierToString(MobileGpuTier value);

	static GpuProfileSelection Resolve(std::string_view override_value, std::string_view gpu_vendor,
		std::string_view gpu_renderer_or_name);
};
