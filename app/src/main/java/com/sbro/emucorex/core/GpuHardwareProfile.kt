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

    fun coreOverrideFor(profile: Int): String = when (normalize(profile)) {
        MALI -> "mali"
        POWERVR -> "powervr"
        else -> "adreno"
    }

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

        val profile = when {
            listOf("mediatek", "mtk", "mt").any(hints::contains) -> MALI
            listOf("qualcomm", "qcom", "snapdragon", "adreno").any(hints::contains) -> ADRENO
            else -> ADRENO
        }
        cachedDetectedProfile = profile
        return profile
    }
}
