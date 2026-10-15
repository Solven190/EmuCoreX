package com.sbro.emucorex.data.pcsx2

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class GameIndexOverlayParserTest {
    @Test
    fun retainsScalarOverridesAndEmptyWholeBlockPresence() {
        val parsed = GameIndexOverlay.parse(
            listOf(
                "SLUS-12345:",
                "  name: \"Overlay title\" # source note",
                "  region: PAL",
                "  gsHWFixes: {}",
                "  clampModes:",
                "    vuClampMode: 2",
                "  roundModes:",
                "    vu1RoundMode: 3",
                "  gameFixes:",
                "    - EETimingHack",
                "SLUS-12346:",
                "  gameFixes: []",
                "  gsHWFixes: {}"
            )
        )

        val entry = requireNotNull(parsed["SLUS-12345"])
        assertEquals("Overlay title", entry.scalars["name"])
        assertEquals("PAL", entry.scalars["region"])
        assertTrue(entry.sections.containsKey("gsHWFixes"))
        assertTrue(entry.sections.getValue("gsHWFixes").values.isEmpty())
        assertEquals(mapOf("vuClampMode" to "2"), entry.sections.getValue("clampModes").values)
        assertEquals(mapOf("vu1RoundMode" to "3"), entry.sections.getValue("roundModes").values)
        assertEquals(listOf("EETimingHack"), entry.sections.getValue("gameFixes").items)

        val explicitEmpty = requireNotNull(parsed["SLUS-12346"])
        assertTrue(explicitEmpty.sections.containsKey("gameFixes"))
        assertTrue(explicitEmpty.sections.getValue("gameFixes").items.isEmpty())
        assertTrue(explicitEmpty.sections.containsKey("gsHWFixes"))
    }
}
