// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/Common/GSGPUProfilePrivate.h"

namespace GpuProfileDetail
{
static bool LooksLikeMediaTek(std::string_view lowered_hints)
{
	return ContainsAny(lowered_hints, {
		"mediatek", "mtk", "dimensity", "helio",
		"mt67", "mt68", "mt69",
	});
}

bool LooksLikeMali(std::string_view lowered_hints)
{
	return ContainsAny(lowered_hints, {"mali", "valhall", "bifrost", "midgard"}) ||
		LooksLikeMediaTek(lowered_hints);
}

MobileGpuTier ResolveMaliTier(std::string_view lowered_hints)
{
	if (ContainsAny(lowered_hints,
			{"immortalis", "mali-g925", "mali-g720", "mali-g715", "mali-g710", "mt6989", "mt6985", "mt6983"}))
	{
		return MobileGpuTier::High;
	}

	if (ContainsAny(lowered_hints,
			{"mali-g610", "mali-g615", "mali-g78", "mali-g77", "mt6897", "mt6896", "mt6895", "mt6893", "mt6879"}))
	{
		return MobileGpuTier::Mid;
	}

	return MobileGpuTier::Low;
}

MobileGsTuning GetMaliTuning(MobileGpuTier tier)
{
	return MakeMobileGsTuning(tier);
}
} // namespace GpuProfileDetail
