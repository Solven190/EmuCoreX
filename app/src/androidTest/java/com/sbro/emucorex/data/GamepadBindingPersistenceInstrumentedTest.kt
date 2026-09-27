package com.sbro.emucorex.data

import android.content.Context
import android.view.KeyEvent
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class GamepadBindingPersistenceInstrumentedTest {
    private val context = ApplicationProvider.getApplicationContext<Context>()

    @Test
    fun globalPs2AndGunBindingsSurvivePreferencesRecreation() = runBlocking {
        // The instrumentation APK has its own data directory, separate from the user's app.
        val testContext = InstrumentationRegistry.getInstrumentation().context
        val preferences = AppPreferences(testContext)
        preferences.resetGamepadBindingsForPad(0)
        try {
            preferences.setGamepadBinding(0, "square", KeyEvent.KEYCODE_BUTTON_X)
            preferences.setGamepadBinding(0, "gun_trigger", KeyEvent.KEYCODE_BUTTON_X)

            val restored = AppPreferences(testContext).gamepadBindingsByPad.first().getValue(0)
            assertEquals(KeyEvent.KEYCODE_BUTTON_X, restored["square"])
            assertEquals(KeyEvent.KEYCODE_BUTTON_X, restored["gun_trigger"])

            preferences.setGamepadBinding(0, "gun_trigger", KeyEvent.KEYCODE_BUTTON_B)
            val rebound = AppPreferences(testContext).gamepadBindingsByPad.first().getValue(0)
            assertEquals(KeyEvent.KEYCODE_BUTTON_X, rebound["square"])
            assertEquals(KeyEvent.KEYCODE_BUTTON_B, rebound["gun_trigger"])
        } finally {
            preferences.resetGamepadBindingsForPad(0)
        }
    }

    @Test
    fun rebindingAndClearingSurviveRepositoryRecreationWithoutLosingOtherOverrides() {
        val gameKey = "gamepad-binding-test-${System.nanoTime()}"
        val repository = PerGameSettingsRepository(context)
        try {
            repository.save(PerGameSettings(
                gameKey = gameKey,
                gameTitle = "Controller binding test",
                renderer = 14,
                providedKeys = setOf("renderer")
            ))

            val saved = requireNotNull(repository.get(gameKey)).withGamepadBindingsByPad(
                mapOf(0 to mapOf(
                    "square" to KeyEvent.KEYCODE_BUTTON_X,
                    "gun_trigger" to KeyEvent.KEYCODE_BUTTON_X
                ))
            )
            repository.save(saved)

            val reopened = requireNotNull(PerGameSettingsRepository(context).get(gameKey))
            assertEquals(KeyEvent.KEYCODE_BUTTON_X, reopened.gamepadBindingsByPad[0]?.get("square"))
            assertEquals(KeyEvent.KEYCODE_BUTTON_X, reopened.gamepadBindingsByPad[0]?.get("gun_trigger"))
            assertEquals(setOf("renderer", "gamepadBindingsByPad"), reopened.providedKeys)
            assertEquals(14, reopened.renderer)

            repository.save(reopened.withGamepadBindingsByPad(emptyMap()))
            val cleared = requireNotNull(PerGameSettingsRepository(context).get(gameKey))
            assertEquals(emptyMap<Int, Map<String, Int>>(), cleared.gamepadBindingsByPad)
            assertEquals(setOf("renderer"), cleared.providedKeys)
            assertNull(cleared.gamepadBindingsByPad[0]?.get("gun_trigger"))
        } finally {
            repository.delete(gameKey)
        }
    }
}
