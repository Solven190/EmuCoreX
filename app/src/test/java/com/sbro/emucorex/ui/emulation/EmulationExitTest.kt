package com.sbro.emucorex.ui.emulation

import org.junit.Assert.assertEquals
import org.junit.Test

class EmulationExitTest {
    @Test
    fun hostUiIsRestoredBeforeNavigationReturnsToHome() {
        val events = mutableListOf<String>()

        completeEmulationExit(
            activePlayTimeMs = 42L,
            restoreHostUi = { events += "restore" },
            navigateFromEmulation = { events += "navigate:$it" }
        )

        assertEquals(listOf("restore", "navigate:42"), events)
    }
}
