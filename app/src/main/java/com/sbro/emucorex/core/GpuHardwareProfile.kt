package com.sbro.emucorex.core

import android.os.Build

object GpuHardwareProfiles {
    const val ADRENO = 0
    const val MALI = 1
    const val POWERVR = 2

    private var cachedDetectedProfile: Int? = null

    fun normalize(profile: Int): Int = when (profile) {
        MALI -> MALI
        POWERVR -> POWERVR
        else -> ADRENO
    }

    fun isMediatekProfile(profile: Int): Boolean {
        return normalize(profile) != ADRENO
    }

    // Build properties can identify the SoC vendor, but not the actual GPU reliably (some MediaTek
    // generations use Mali and others use PowerVR). Let the native renderer use GL_RENDERER or
    // VkPhysicalDeviceProperties::deviceName, which is the authoritative signal.
    fun coreOverrideFor(@Suppress("UNUSED_PARAMETER") profile: Int): String = "auto"

    fun isMediaTekHardware(): Boolean {
        return detectHardwareProfile() == MALI
    }

    fun detectHardwareProfile(): Int {
        cachedDetectedProfile?.let { return it }
        val socManufacturer = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            Build.SOC_MANUFACTURER.orEmpty()
        } else { "" }
        val socModel = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            Build.SOC_MODEL.orEmpty()
        } else { "" }
        val hardware = listOf(Build.HARDWARE, Build.BOARD, Build.DEVICE)
            .filterNot { it.isNullOrBlank() }
            .joinToString(" / ")
        val deviceModel = listOf(Build.MANUFACTURER, Build.MODEL)
            .filterNot { it.isNullOrBlank() }
            .joinToString(" ")
        val hints = listOf(socManufacturer, socModel, hardware, deviceModel)
            .joinToString(" ")
            .lowercase()

        val profile = classifyHardwareProfile(hints)
        cachedDetectedProfile = profile
        return profile
    }

    internal fun classifyHardwareProfile(rawHints: String): Int {
        val hints = rawHints.lowercase()
        return when {
            listOf("powervr", "imgtec", "imagination technologies").any(hints::contains) -> POWERVR
            listOf("qualcomm", "qcom", "snapdragon", "adreno").any(hints::contains) -> ADRENO
            listOf("mediatek", "mtk", "dimensity", "helio").any(hints::contains) ||
                Regex("""\bmt\d{4}\b""").containsMatchIn(hints) -> MALI
            else -> ADRENO
        }
    }
}
