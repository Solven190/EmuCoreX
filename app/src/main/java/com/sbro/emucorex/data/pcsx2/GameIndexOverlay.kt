package com.sbro.emucorex.data.pcsx2

import android.content.Context
import java.util.Locale

/** The small YAML subset used by the bundled mobile GameIndex overlay. */
data class GameIndexOverlayEntry(
    val serial: String,
    val scalars: Map<String, String>,
    val sections: Map<String, GameIndexOverlaySection>
)

data class GameIndexOverlaySection(
    val values: Map<String, String> = emptyMap(),
    val items: List<String> = emptyList()
)

object GameIndexOverlay {
    private const val ASSET_PATH = "resources/emucorex_overrides.yaml"

    fun load(context: Context): Map<String, GameIndexOverlayEntry> = runCatching {
        context.assets.open(ASSET_PATH).bufferedReader().use { parse(it.readLines()) }
    }.getOrDefault(emptyMap())

    /** Public for focused parser tests; no Android asset access is required. */
    fun parse(lines: Iterable<String>): Map<String, GameIndexOverlayEntry> {
        data class MutableSection(
            val values: LinkedHashMap<String, String> = linkedMapOf(),
            val items: MutableList<String> = mutableListOf()
        )
        data class MutableEntry(
            val serial: String,
            val scalars: LinkedHashMap<String, String> = linkedMapOf(),
            val sections: LinkedHashMap<String, MutableSection> = linkedMapOf()
        )

        val parsed = linkedMapOf<String, MutableEntry>()
        var current: MutableEntry? = null
        var section: MutableSection? = null
        var sectionName: String? = null

        for (line in lines) {
            if (line.isBlank() || line.trimStart().startsWith('#')) continue
            val indent = line.indexOfFirst { !it.isWhitespace() }.coerceAtLeast(0)
            val text = stripComment(line.trim()).trim()
            if (text.isEmpty()) continue
            if (indent == 0 && text.endsWith(':')) {
                val serial = text.removeSuffix(":").trim().uppercase(Locale.ROOT)
                current = MutableEntry(serial).also { parsed[serial] = it }
                section = null
                sectionName = null
                continue
            }
            val entry = current ?: continue
            if (indent == 2) {
                val key = text.substringBefore(':').trim()
                val value = text.substringAfter(':', "").trim()
                if (key.isBlank()) continue
                if (value.isEmpty() || value == "{}" || value == "[]") {
                    sectionName = key
                    section = MutableSection().also { entry.sections[key] = it }
                } else {
                    sectionName = null
                    section = null
                    entry.scalars[key] = scalar(value)
                }
                continue
            }
            if (indent < 4 || sectionName == null) continue
            val active = section ?: continue
            if (text.startsWith('-')) {
                scalar(text.removePrefix("-").trim()).takeIf { it.isNotBlank() }?.let(active.items::add)
            } else if (text.contains(':')) {
                val key = text.substringBefore(':').trim()
                val value = text.substringAfter(':').trim()
                if (key.isNotBlank()) active.values[key] = scalar(value)
            }
        }
        return parsed.mapValues { (_, entry) ->
            GameIndexOverlayEntry(
                serial = entry.serial,
                scalars = entry.scalars.toMap(),
                sections = entry.sections.mapValues { (_, section) ->
                    GameIndexOverlaySection(section.values.toMap(), section.items.toList())
                }
            )
        }
    }

    private fun scalar(raw: String): String = raw.trim()
        .removeSurrounding("\"")
        .removeSurrounding("'")
        .replace("\\\"", "\"")

    private fun stripComment(line: String): String {
        var single = false
        var double = false
        line.forEachIndexed { index, char ->
            when (char) {
                '\'' -> if (!double) single = !single
                '"' -> if (!single && (index == 0 || line[index - 1] != '\\')) double = !double
                '#' -> if (!single && !double) return line.substring(0, index)
            }
        }
        return line
    }
}
