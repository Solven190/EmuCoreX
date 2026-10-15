package com.sbro.emucorex.ui.settings

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class SettingsUiStateGsOwnershipTest {
    @Test
    fun compactOwnershipFieldRetainsBothBooleanViews() {
        val defaults = SettingsUiState()
        assertFalse(defaults.hwDownloadModeUserOverride)
        assertFalse(defaults.blendingAccuracyUserOverride)

        val hwOwned = SettingsUiState(
            gsSettingOverrideFlags = settingsGsOverrideFlags(hwDownloadMode = true, blendingAccuracy = false)
        )
        assertTrue(hwOwned.hwDownloadModeUserOverride)
        assertFalse(hwOwned.blendingAccuracyUserOverride)

        val bothOwned = hwOwned.copy(
            gsSettingOverrideFlags = settingsGsOverrideFlags(hwDownloadMode = true, blendingAccuracy = true)
        )
        assertTrue(bothOwned.hwDownloadModeUserOverride)
        assertTrue(bothOwned.blendingAccuracyUserOverride)
    }
}
