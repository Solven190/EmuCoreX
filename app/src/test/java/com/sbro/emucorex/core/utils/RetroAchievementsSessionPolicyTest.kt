package com.sbro.emucorex.core.utils

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class RetroAchievementsSessionPolicyTest {
    @Test
    fun `cached profile is restored only with a complete session`() {
        assertTrue(RetroAchievementsSessionPolicy.hasStoredSession("player", "verified-token"))
    }

    @Test
    fun `username without token is not treated as a signed in account`() {
        assertFalse(RetroAchievementsSessionPolicy.hasStoredSession("player", null))
        assertFalse(RetroAchievementsSessionPolicy.hasStoredSession("player", ""))
        assertFalse(RetroAchievementsSessionPolicy.hasStoredSession("player", "   "))
    }

    @Test
    fun `token without username is not treated as a signed in account`() {
        assertFalse(RetroAchievementsSessionPolicy.hasStoredSession(null, "verified-token"))
        assertFalse(RetroAchievementsSessionPolicy.hasStoredSession("", "verified-token"))
        assertFalse(RetroAchievementsSessionPolicy.hasStoredSession("  ", "verified-token"))
    }
}
