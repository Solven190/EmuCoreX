package com.sbro.emucorex.data

import android.view.KeyEvent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test

class GamepadBindingRulesTest {
    @Test
    fun assigningArcadeButtonKeepsPs2AndShortcutMappingsOnSamePhysicalButton() {
        val bindings = mapOf(
            "square" to KeyEvent.KEYCODE_BUTTON_X,
            "quick_save" to KeyEvent.KEYCODE_BUTTON_Y
        )

        val gun = GamepadBindingRules.assign(bindings, "gun_trigger", KeyEvent.KEYCODE_BUTTON_X)
        val coin = GamepadBindingRules.assign(gun, "coin", KeyEvent.KEYCODE_BUTTON_Y)

        assertEquals(KeyEvent.KEYCODE_BUTTON_X, coin["square"])
        assertEquals(KeyEvent.KEYCODE_BUTTON_X, coin["gun_trigger"])
        assertEquals(KeyEvent.KEYCODE_BUTTON_Y, coin["quick_save"])
        assertEquals(KeyEvent.KEYCODE_BUTTON_Y, coin["coin"])
    }

    @Test
    fun reassigningWithinOneContextRemovesOnlyItsConflictingAction() {
        val bindings = mapOf(
            "square" to KeyEvent.KEYCODE_BUTTON_X,
            "cross" to KeyEvent.KEYCODE_BUTTON_B,
            "gun_trigger" to KeyEvent.KEYCODE_BUTTON_X
        )

        val moved = GamepadBindingRules.assign(bindings, "cross", KeyEvent.KEYCODE_BUTTON_X)

        assertFalse(moved.containsKey("square"))
        assertEquals(KeyEvent.KEYCODE_BUTTON_X, moved["cross"])
        assertEquals(KeyEvent.KEYCODE_BUTTON_X, moved["gun_trigger"])
    }

    @Test
    fun perGameOverrideChangesOnlySelectedPlayerAndAction() {
        val global = mapOf(
            0 to mapOf("square" to KeyEvent.KEYCODE_BUTTON_X),
            1 to mapOf(
                "square" to KeyEvent.KEYCODE_BUTTON_X,
                "gun_pedal" to KeyEvent.KEYCODE_BUTTON_L1
            )
        )
        val override = mapOf(1 to mapOf("gun_trigger" to KeyEvent.KEYCODE_BUTTON_X))

        val effective = GamepadBindingRules.merge(global, override)

        assertEquals(global.getValue(0), effective.getValue(0))
        assertEquals(KeyEvent.KEYCODE_BUTTON_X, effective.getValue(1)["square"])
        assertEquals(KeyEvent.KEYCODE_BUTTON_X, effective.getValue(1)["gun_trigger"])
        assertEquals(KeyEvent.KEYCODE_BUTTON_L1, effective.getValue(1)["gun_pedal"])
        assertFalse(global.getValue(1).containsKey("gun_trigger"))
    }
}
