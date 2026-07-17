package com.sbro.emucorex.data

import com.sbro.emucorex.ui.emulation.EmulationUiState
import com.sbro.emucorex.ui.settings.SettingsUiState
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ThreadPinningDefaultsTest {
    @Test
    fun threadPinningIsEnabledAcrossFreshGlobalAndGameStates() {
        assertTrue(AppPreferences.DEFAULT_THREAD_PINNING)
        assertTrue(SettingsSnapshot().enableThreadPinning)
        assertTrue(SettingsUiState().enableThreadPinning)
        assertTrue(EmulationUiState().enableThreadPinning)
        assertTrue(PerGameSettings(gameKey = "game", gameTitle = "Game").enableThreadPinning)
    }

    @Test
    fun explicitUserOptOutRemainsSupported() {
        assertFalse(
            PerGameSettings(
                gameKey = "game",
                gameTitle = "Game",
                enableThreadPinning = false
            ).enableThreadPinning
        )
    }
}
