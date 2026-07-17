package com.sbro.emucorex.navigation

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class StartupSplashPolicyTest {
    @Test
    fun splashRemainsVisibleUntilStartupDestinationIsResolved() {
        assertFalse(shouldReleaseStartupSplash(null))
        assertTrue(shouldReleaseStartupSplash(StartupDestination.HOME))
        assertTrue(shouldReleaseStartupSplash(StartupDestination.ONBOARDING))
    }
}
