package com.sbro.emucorex.core.utils

import com.sbro.emucorex.data.SettingsSnapshot
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class RetroAchievementsPresentationDefaultsTest {
    @Test
    fun bothIndicatorFamiliesAreEnabledByDefaultAcrossStoredAndUiState() {
        val stored = SettingsSnapshot()
        val ui = RetroAchievementsUiState(isSupported = false)

        assertTrue(stored.achievementsIndicators)
        assertTrue(stored.achievementsLeaderboardTrackers)
        assertTrue(ui.achievementIndicators)
        assertTrue(ui.leaderboardTrackers)
    }

    @Test
    fun achievementAndLeaderboardIndicatorsRemainIndependent() {
        val initial = RetroAchievementsUiState(isSupported = false)

        val achievementsDisabled = initial.copy(achievementIndicators = false)
        assertFalse(achievementsDisabled.achievementIndicators)
        assertTrue(achievementsDisabled.leaderboardTrackers)

        val leaderboardsDisabled = initial.copy(leaderboardTrackers = false)
        assertTrue(leaderboardsDisabled.achievementIndicators)
        assertFalse(leaderboardsDisabled.leaderboardTrackers)
    }
}
