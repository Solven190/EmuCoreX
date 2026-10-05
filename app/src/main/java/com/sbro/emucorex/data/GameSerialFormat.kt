package com.sbro.emucorex.data

import java.util.Locale

/** Validation and normalization for manually entered game serial numbers. */
object GameSerialFormat {

    private val PS2_SERIAL = Regex("^[A-Z]{4}\\d{5}$")
    private val ARCADE_SERIAL = Regex("^NM\\d{5}$")

    /**
     * Returns the canonical serial for [raw] (`XXXX-#####` for PS2 discs, compact for arcade),
     * or null when the value is blank or does not match a supported format.
     */
    fun normalize(raw: String?): String? {
        val compact = raw
            ?.trim()
            ?.uppercase(Locale.ROOT)
            ?.replace(Regex("[^A-Z0-9]"), "")
            ?.takeIf { it.isNotBlank() }
            ?: return null
        return when {
            ARCADE_SERIAL.matches(compact) -> compact
            PS2_SERIAL.matches(compact) -> "${compact.substring(0, 4)}-${compact.substring(4)}"
            else -> null
        }
    }

    /** True when [raw] can be saved, including an empty value that clears the override. */
    fun isValidOrEmpty(raw: String?): Boolean = raw.isNullOrBlank() || normalize(raw) != null
}
