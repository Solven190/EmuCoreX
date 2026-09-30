package com.sbro.emucorex.data

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class PerGameMemoryCardPolicyTest {

    @Test
    fun `full profile without an override follows the global card`() {
        assertEquals(
            "Mcd001.ps2",
            resolvePerGameMemoryCardOverride(
                providedKeys = null,
                key = "memoryCardSlot1",
                globalCard = "Mcd001.ps2",
                override = null
            )
        )
    }

    @Test
    fun `partial profile without the key ignores the override`() {
        assertEquals(
            "Mcd001.ps2",
            resolvePerGameMemoryCardOverride(
                providedKeys = setOf("renderer"),
                key = "memoryCardSlot1",
                globalCard = "Mcd001.ps2",
                override = "Game.ps2"
            )
        )
    }

    @Test
    fun `provided card name overrides the global card`() {
        assertEquals(
            "Game.ps2",
            resolvePerGameMemoryCardOverride(
                providedKeys = setOf("memoryCardSlot1"),
                key = "memoryCardSlot1",
                globalCard = "Mcd001.ps2",
                override = "Game.ps2"
            )
        )
    }

    @Test
    fun `empty override disables the slot`() {
        assertNull(
            resolvePerGameMemoryCardOverride(
                providedKeys = setOf("memoryCardSlot1"),
                key = "memoryCardSlot1",
                globalCard = "Mcd001.ps2",
                override = ""
            )
        )
    }

    @Test
    fun `accepted card name is kept when the file exists`() {
        assertEquals(
            "Game.ps2",
            validatePerGameMemoryCardName(
                selected = "Game.ps2",
                fallback = "Mcd001.ps2",
                availableCards = setOf("Game.ps2", "Mcd001.ps2")
            )
        )
    }

    @Test
    fun `stale card name falls back to the global card`() {
        assertEquals(
            "Mcd001.ps2",
            validatePerGameMemoryCardName(
                selected = "Deleted.ps2",
                fallback = "Mcd001.ps2",
                availableCards = setOf("Mcd001.ps2")
            )
        )
    }

    @Test
    fun `explicitly empty slot stays empty`() {
        assertNull(
            validatePerGameMemoryCardName(
                selected = null,
                fallback = "Mcd001.ps2",
                availableCards = setOf("Mcd001.ps2")
            )
        )
    }
}
