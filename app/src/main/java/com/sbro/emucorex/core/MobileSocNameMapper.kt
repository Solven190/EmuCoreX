package com.sbro.emucorex.core

import android.os.Build

/**
 * Temporary local stand-in.
 *
 * The upstream repository (sashkinbro/EmuCoreX) currently references this
 * class from FeedbackUploadWorker.kt and EmulationViewModel.kt, but the
 * class file itself is missing from a recent commit on the `main` branch
 * upstream. This is a minimal, safe implementation that provides the same
 * public API (currentDeviceName(): String) so the fork can compile until
 * the upstream maintainer restores the original file.
 *
 * NOTE: Delete this file (or replace it) once upstream fixes the missing
 * class, so a future sync does not silently keep this simplified version
 * instead of the maintainer's real implementation.
 */
object MobileSocNameMapper {

    // Known hardware/board codename -> human-readable chipset name.
    // Not exhaustive; unrecognized devices fall back to the raw codename.
    private val knownChipsets: Map<String, String> = mapOf(
        // Qualcomm Snapdragon
        "lahaina" to "Snapdragon 888",
        "lito" to "Snapdragon 865",
        "kona" to "Snapdragon 865",
        "taro" to "Snapdragon 8 Gen 1",
        "kalama" to "Snapdragon 8 Gen 2",
        "pineapple" to "Snapdragon 8 Gen 3",
        "sun" to "Snapdragon 8 Elite",
        "sm8250" to "Snapdragon 865",
        "sm8350" to "Snapdragon 888",
        "sm8450" to "Snapdragon 8 Gen 1",
        "sm8550" to "Snapdragon 8 Gen 2",
        "sm8650" to "Snapdragon 8 Gen 3",
        "sdm855" to "Snapdragon 855",

        // MediaTek Dimensity
        "mt6893" to "Dimensity 1200",
        "mt6891" to "Dimensity 1100",
        "mt6889" to "Dimensity 1000",
        "mt6895" to "Dimensity 8100",
        "mt6896" to "Dimensity 8200",
        "mt6897" to "Dimensity 8300",
        "mt6983" to "Dimensity 9000",
        "mt6985" to "Dimensity 9200",
        "mt6989" to "Dimensity 9300",
        "mt6991" to "Dimensity 9400"
    )

    /**
     * Returns a human-readable name for the device's chipset (SoC).
     * Never returns null or throws; falls back to the raw hardware/board
     * codename, or "Unknown" if nothing usable is available.
     */
    fun currentDeviceName(): String {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val socModel = Build.SOC_MODEL?.trim().orEmpty()
            if (socModel.isNotBlank() && !socModel.equals("unknown", ignoreCase = true)) {
                return socModel
            }
        }

        val hardware = Build.HARDWARE?.trim()?.lowercase().orEmpty()
        val board = Build.BOARD?.trim()?.lowercase().orEmpty()

        knownChipsets[hardware]?.let { return it }
        knownChipsets[board]?.let { return it }

        return when {
            hardware.isNotBlank() -> hardware
            board.isNotBlank() -> board
            else -> "Unknown"
        }
    }
}