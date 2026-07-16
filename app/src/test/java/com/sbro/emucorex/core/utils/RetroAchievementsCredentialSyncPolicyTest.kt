package com.sbro.emucorex.core.utils

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class RetroAchievementsCredentialSyncPolicyTest {
    @Test
    fun `verified native credential values are accepted`() {
        assertTrue(RetroAchievementsCredentialSyncPolicy.shouldPersistNativeValue("player"))
        assertTrue(RetroAchievementsCredentialSyncPolicy.shouldPersistNativeValue("token-value"))
        assertTrue(RetroAchievementsCredentialSyncPolicy.shouldPersistNativeValue("1780000000"))
    }

    @Test
    fun `missing native settings cannot erase verified credentials`() {
        assertFalse(RetroAchievementsCredentialSyncPolicy.shouldPersistNativeValue(null))
        assertFalse(RetroAchievementsCredentialSyncPolicy.shouldPersistNativeValue(""))
        assertFalse(RetroAchievementsCredentialSyncPolicy.shouldPersistNativeValue("   \t"))
    }
}
