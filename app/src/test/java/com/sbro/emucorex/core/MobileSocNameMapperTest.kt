package com.sbro.emucorex.core

import org.junit.Assert.assertEquals
import org.junit.Test

class MobileSocNameMapperTest {
    @Test
    fun resolvesCurrentFlagshipPlatformCodes() {
        assertEquals("Snapdragon 8 Elite Gen 5", MobileSocNameMapper.resolve("SM8850-AC"))
        assertEquals("Dimensity 9500", MobileSocNameMapper.resolve("MT6993"))
        assertEquals("Exynos 2600", MobileSocNameMapper.resolve("s5e9965"))
        assertEquals("Google Tensor G5", MobileSocNameMapper.resolve("laguna"))
    }

    @Test
    fun resolvesOlderAndAlternativeVendorCodes() {
        assertEquals("Snapdragon 835", MobileSocNameMapper.resolve("MSM8998"))
        assertEquals("Helio G99/G100", MobileSocNameMapper.resolve("mt6789v/cd"))
        assertEquals("Kirin 970", MobileSocNameMapper.resolve("hi3670"))
        assertEquals("Unisoc T618", MobileSocNameMapper.resolve("T618"))
        assertEquals("Rockchip RK3588", MobileSocNameMapper.resolve("rk3588s"))
    }

    @Test
    fun keepsAlreadyReadableMarketingNames() {
        assertEquals("Snapdragon 8 Gen 3", MobileSocNameMapper.resolve("Snapdragon 8 Gen 3"))
        assertEquals("Exynos 2400", MobileSocNameMapper.resolve("Exynos 2400"))
    }

    @Test
    fun neverShowsAnUnmappedRawPlatformCode() {
        assertEquals("Snapdragon", MobileSocNameMapper.resolve("SM9999", manufacturer = "Qualcomm"))
        assertEquals("MediaTek", MobileSocNameMapper.resolve("MT9999"))
        assertEquals("Unknown SoC", MobileSocNameMapper.resolve("future_platform"))
    }
}
