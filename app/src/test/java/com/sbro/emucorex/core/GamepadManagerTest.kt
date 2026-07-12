package com.sbro.emucorex.core

import android.view.KeyEvent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class GamepadManagerTest {
    @Test
    fun defaultButtonMappingIsPreservedWithoutOverrides() {
        assertEquals(
            "square",
            GamepadManager.resolveMappedActionIdForKeyCode(KeyEvent.KEYCODE_BUTTON_X, emptyMap())
        )
        assertEquals(
            "r2",
            GamepadManager.resolveMappedActionIdForTriggerAxis("r2", emptyMap())
        )
    }

    @Test
    fun movingActionSuppressesItsOldDefaultButton() {
        val bindings = mapOf("r2" to KeyEvent.KEYCODE_BUTTON_X)

        assertEquals(
            "r2",
            GamepadManager.resolveMappedActionIdForKeyCode(KeyEvent.KEYCODE_BUTTON_X, bindings)
        )
        assertNull(
            GamepadManager.resolveMappedActionIdForKeyCode(KeyEvent.KEYCODE_BUTTON_R2, bindings)
        )
        assertNull(GamepadManager.resolveMappedActionIdForTriggerAxis("r2", bindings))
    }

    @Test
    fun squareAndR2SwapAppliesToButtonsAndAnalogTriggerAxis() {
        val bindings = mapOf(
            "square" to KeyEvent.KEYCODE_BUTTON_R2,
            "r2" to KeyEvent.KEYCODE_BUTTON_X
        )

        assertEquals(
            "square",
            GamepadManager.resolveMappedActionIdForKeyCode(KeyEvent.KEYCODE_BUTTON_R2, bindings)
        )
        assertEquals(
            "r2",
            GamepadManager.resolveMappedActionIdForKeyCode(KeyEvent.KEYCODE_BUTTON_X, bindings)
        )
        assertEquals("square", GamepadManager.resolveMappedActionIdForTriggerAxis("r2", bindings))
    }

    @Test
    fun analogTriggerRecognizesAndroidAlternateButtonCode() {
        val bindings = mapOf(
            "square" to KeyEvent.KEYCODE_BUTTON_8,
            "r2" to KeyEvent.KEYCODE_BUTTON_X
        )

        assertEquals("square", GamepadManager.resolveMappedActionIdForTriggerAxis("r2", bindings))
    }
}
