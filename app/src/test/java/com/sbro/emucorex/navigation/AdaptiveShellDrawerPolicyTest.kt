package com.sbro.emucorex.navigation

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AdaptiveShellDrawerPolicyTest {
    @Test
    fun feedbackWithBackNavigationDoesNotUseModalDrawer() {
        assertFalse(
            shouldUseCompactModalDrawer(
                drawerEnabled = true,
                selected = PrimaryDestination.Feedback,
                hasBackClick = true
            )
        )
    }

    @Test
    fun settingsWithBackNavigationDoesNotUseModalDrawer() {
        assertFalse(
            shouldUseCompactModalDrawer(
                drawerEnabled = true,
                selected = PrimaryDestination.Settings,
                hasBackClick = true
            )
        )
    }

    @Test
    fun homeKeepsModalDrawer() {
        assertTrue(
            shouldUseCompactModalDrawer(
                drawerEnabled = true,
                selected = PrimaryDestination.Home,
                hasBackClick = false
            )
        )
    }

    @Test
    fun disabledDrawerCannotBeCreated() {
        assertFalse(
            shouldUseCompactModalDrawer(
                drawerEnabled = false,
                selected = PrimaryDestination.Home,
                hasBackClick = false
            )
        )
    }
}
