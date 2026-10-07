package com.sbro.emucorex.data

import android.content.Context
import com.sbro.emucorex.core.EmulatorStorage

/**
 * Patch activity that the Android UI can report on its own. The core's OSD is disabled on
 * Android, so the frontend summarises the launch-time patch options instead.
 */
data class ActivePatchNotice(
    val widescreen: Boolean,
    val noInterlacing: Boolean,
    val cheats: Boolean,
    val userPatchCount: Int
) {
    val hasAnything: Boolean
        get() = widescreen || noInterlacing || cheats || userPatchCount > 0
}

class PatchRepository(
    private val context: Context,
    private val preferences: AppPreferences
) {
    /** Counts user .pnach files in the patches folder that belong to the running game. */
    fun countUserPatches(serial: String?, crc: String?): Int {
        val normalizedSerial = normalizeSerial(serial)
        val normalizedCrc = normalizeCrc(crc)
        if (normalizedSerial == null && normalizedCrc == null) return 0
        val directory = EmulatorStorage.patchesDir(context, preferences.getEmulatorDataPathSync())
        return runCatching {
            directory.listFiles()
                .orEmpty()
                .count { it.isFile && patchFileNameMatchesGame(it.name, normalizedSerial, normalizedCrc) }
        }.getOrDefault(0)
    }

    companion object {
        fun normalizeSerial(serial: String?): String? =
            serial?.trim()?.uppercase()?.takeIf { it.isNotBlank() }

        fun normalizeCrc(crc: String?): String? =
            crc?.trim()?.uppercase()?.takeIf { it.matches(Regex("[0-9A-F]{8}")) }

        /** Matches a pnach file against the game serial and/or the disc CRC. */
        fun patchFileNameMatchesGame(fileName: String, serial: String?, crc: String?): Boolean {
            if (serial != null && patchFileNameMatchesSerial(fileName, serial)) return true
            if (crc != null && fileName.endsWith(".pnach", ignoreCase = true)) {
                return fileName.substringBeforeLast('.').uppercase().startsWith(crc)
            }
            return false
        }

        /**
         * Mirrors the core's pnach discovery for on-disk files: a file is loaded when its
         * name starts with the serial, allowing the usual punctuation variants
         * ("SLUS-20062", "SLUS_200.62") and an optional "_CRC" suffix. CRC-only files
         * cannot be matched without the disc CRC, so they are not counted.
         */
        fun patchFileNameMatchesSerial(fileName: String, serial: String): Boolean {
            if (!fileName.endsWith(".pnach", ignoreCase = true)) return false
            val base = fileName.substringBeforeLast('.').uppercase()
            val normalizedSerial = normalizeSerial(serial) ?: return false

            // Walk the serial along the file name, allowing punctuation differences such as
            // "SLUS-20062" vs "SLUS_200.62" between the alphanumeric characters.
            var baseIndex = 0
            for (serialCharacter in normalizedSerial) {
                if (!serialCharacter.isLetterOrDigit()) continue
                while (baseIndex < base.length && !base[baseIndex].isLetterOrDigit()) {
                    baseIndex++
                }
                if (baseIndex >= base.length || base[baseIndex] != serialCharacter) return false
                baseIndex++
            }
            // The core loads "{serial}_*.pnach" or "{serial}.pnach"; anything alphanumeric
            // glued to the serial belongs to a different game or a malformed name.
            return baseIndex >= base.length || base[baseIndex] == '_'
        }
    }
}
