package com.sbro.emucorex.core

object GpuHardwareProfiles {
    const val ADRENO = 0
    const val MALI = 1
    const val POWERVR = 2

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
}
