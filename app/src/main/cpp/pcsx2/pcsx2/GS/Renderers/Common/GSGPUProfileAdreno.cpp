// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/Common/GSGPUProfilePrivate.h"

namespace GpuProfileDetail
{
bool LooksLikeAdreno(std::string_view lowered_hints)
{
	const bool has_adreno = ContainsAny(lowered_hints, {"adreno"});
	const bool has_qualcomm = ContainsAny(lowered_hints, {"qualcomm", "qcom", "snapdragon"});
	return (has_adreno || has_qualcomm);
}

MobileGpuTier ResolveAdrenoTier(std::string_view /*lowered_hints*/)
{
	return MobileGpuTier::High;
}

MobileGsTuning GetAdrenoTuning(MobileGpuTier tier)
{
	return MakeMobileGsTuning(tier);
}
} // namespace GpuProfileDetail
