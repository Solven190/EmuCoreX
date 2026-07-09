// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/Common/GSGPUProfilePrivate.h"

namespace GpuProfileDetail
{
bool LooksLikePowerVR(std::string_view lowered_hints)
{
	return ContainsAny(lowered_hints, {"imagination", "powervr", "img"});
}

MobileGpuTier ResolvePowerVRTier(std::string_view lowered_hints)
{
	if (ContainsAny(lowered_hints, {"b-series", "bxs", "bxt", "mt6877", "mt6878"}))
		return MobileGpuTier::Mid;

	return MobileGpuTier::Low;
}

MobileGsTuning GetPowerVRTuning(MobileGpuTier tier)
{
	return MakeMobileGsTuning(tier);
}
} // namespace GpuProfileDetail
