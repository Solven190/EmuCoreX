// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS/Renderers/Common/GSGPUProfile.h"

#include <initializer_list>
#include <string>
#include <string_view>

namespace GpuProfileDetail
{
std::string ToLowerASCII(std::string_view value);
bool ContainsAny(std::string_view haystack, std::initializer_list<const char*> needles);

MobileGsTuning MakeMobileGsTuning(MobileGpuTier tier);

bool LooksLikeAdreno(std::string_view lowered_hints);
MobileGpuTier ResolveAdrenoTier(std::string_view lowered_hints);
MobileGsTuning GetAdrenoTuning(MobileGpuTier tier);

bool LooksLikeMali(std::string_view lowered_hints);
MobileGpuTier ResolveMaliTier(std::string_view lowered_hints);
MobileGsTuning GetMaliTuning(MobileGpuTier tier);

bool LooksLikePowerVR(std::string_view lowered_hints);
MobileGpuTier ResolvePowerVRTier(std::string_view lowered_hints);
MobileGsTuning GetPowerVRTuning(MobileGpuTier tier);
} // namespace GpuProfileDetail
