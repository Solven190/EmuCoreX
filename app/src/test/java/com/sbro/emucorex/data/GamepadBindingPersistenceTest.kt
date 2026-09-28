package com.sbro.emucorex.data

import android.view.KeyEvent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class GamepadBindingPersistenceTest {
    @Test
    fun changingOneGameBindingKeepsUnrelatedProfileSettings() {
        val original = PerGameSettings(
            gameKey = "game.acgame",
            gameTitle = "Arcade game",
            renderer = 14,
            gyroMode = AppPreferences.GYRO_MODE_LIGHT_GUN,
            gamepadBindingsByPad = mapOf(0 to mapOf("gun_pedal" to KeyEvent.KEYCODE_BUTTON_L1)),
            providedKeys = setOf("renderer", "gyroMode", "gamepadBindingsByPad")
        )
        val updated = original.withGamepadBindingsByPad(
            mapOf(0 to mapOf(
                "gun_pedal" to KeyEvent.KEYCODE_BUTTON_L1,
                "gun_trigger" to KeyEvent.KEYCODE_BUTTON_X
            ))
        )

        assertEquals(original.renderer, updated.renderer)
        assertEquals(original.gyroMode, updated.gyroMode)
        assertEquals(original.providedKeys, updated.providedKeys)
        assertEquals(KeyEvent.KEYCODE_BUTTON_X, updated.gamepadBindingsByPad[0]?.get("gun_trigger"))
        assertNull(original.gamepadBindingsByPad[0]?.get("gun_trigger"))
    }

    @Test
    fun addingAndClearingGameBindingsUpdatesSelectiveKeys() {
        val profile = PerGameSettings(
            gameKey = "game.iso",
            gameTitle = "PS2 game",
            renderer = 14,
            providedKeys = setOf("renderer")
        )
        val added = profile.withGamepadBindingsByPad(
            mapOf(0 to mapOf("square" to KeyEvent.KEYCODE_BUTTON_B))
        )
        val cleared = added.withGamepadBindingsByPad(emptyMap())

        assertEquals(setOf("renderer", "gamepadBindingsByPad"), added.providedKeys)
        assertEquals(setOf("renderer"), cleared.providedKeys)
        assertEquals(emptyMap<Int, Map<String, Int>>(), cleared.gamepadBindingsByPad)
        assertEquals(14, cleared.renderer)
    }

    @Test
    fun oldFullProfilesKeepFullProfileSemanticsAfterBindingChange() {
        val oldProfile = PerGameSettings(
            gameKey = "old.iso",
            gameTitle = "Old game",
            providedKeys = null
        )

        assertNull(oldProfile.withGamepadBindingsByPad(
            mapOf(1 to mapOf("gun_trigger" to KeyEvent.KEYCODE_BUTTON_R2))
        ).providedKeys)
        assertNull(oldProfile.withGamepadBindingsByPad(emptyMap()).providedKeys)
    }
}
