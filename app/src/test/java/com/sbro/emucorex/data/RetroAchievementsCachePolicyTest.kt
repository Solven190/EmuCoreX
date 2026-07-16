package com.sbro.emucorex.data

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class RetroAchievementsCachePolicyTest {

    @Test
    fun freshness_acceptsExactBoundaryAndRejectsExpiredFutureOrMissingTimestamps() {
        val now = 10_000_000L
        val ttl = 900_000L

        assertTrue(RetroAchievementsCachePolicy.isFresh(now, now, ttl))
        assertTrue(RetroAchievementsCachePolicy.isFresh(now - ttl, now, ttl))
        assertFalse(RetroAchievementsCachePolicy.isFresh(now - ttl - 1L, now, ttl))
        assertFalse(RetroAchievementsCachePolicy.isFresh(0L, now, ttl))
        assertFalse(RetroAchievementsCachePolicy.isFresh(now + 1L, now, ttl))
        assertFalse(RetroAchievementsCachePolicy.isFresh(now, now, -1L))
    }

    @Test
    fun ownership_isCaseInsensitiveButNeverAcceptsBlankOrDifferentAccounts() {
        assertTrue(RetroAchievementsCachePolicy.isSameAccount("  PlayerOne ", "playerone"))
        assertFalse(RetroAchievementsCachePolicy.isSameAccount("PlayerOne", "PlayerTwo"))
        assertFalse(RetroAchievementsCachePolicy.isSameAccount("", "PlayerOne"))
        assertFalse(RetroAchievementsCachePolicy.isSameAccount("PlayerOne", "  "))
        assertFalse(RetroAchievementsCachePolicy.isSameAccount(null, "PlayerOne"))
        assertFalse(RetroAchievementsCachePolicy.isSameAccount("PlayerOne", null))
    }
}
