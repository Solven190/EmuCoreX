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

        assertTrue(stored.achievementsNotifications)
        assertTrue(stored.achievementsLeaderboardNotifications)
        assertTrue(stored.achievementsIndicators)
        assertTrue(stored.achievementsLeaderboardTrackers)
        assertTrue(stored.achievementsSoundEffects)
        assertTrue(ui.notifications)
        assertTrue(ui.leaderboardNotifications)
        assertTrue(ui.achievementIndicators)
        assertTrue(ui.leaderboardTrackers)
        assertTrue(ui.soundEffects)
    }

    @Test
    fun achievementAndLeaderboardIndicatorsRemainIndependent() {
        val initial = RetroAchievementsUiState(isSupported = false)

        val notificationsDisabled = initial.copy(notifications = false)
        assertFalse(notificationsDisabled.notifications)
        assertTrue(notificationsDisabled.leaderboardNotifications)
        assertTrue(notificationsDisabled.achievementIndicators)
        assertTrue(notificationsDisabled.leaderboardTrackers)
        assertTrue(notificationsDisabled.soundEffects)

        val achievementsDisabled = initial.copy(achievementIndicators = false)
        assertFalse(achievementsDisabled.achievementIndicators)
        assertTrue(achievementsDisabled.leaderboardTrackers)

        val leaderboardsDisabled = initial.copy(leaderboardTrackers = false)
        assertTrue(leaderboardsDisabled.achievementIndicators)
        assertFalse(leaderboardsDisabled.leaderboardTrackers)

        val soundsDisabled = initial.copy(soundEffects = false)
        assertTrue(soundsDisabled.notifications)
        assertTrue(soundsDisabled.leaderboardNotifications)
        assertTrue(soundsDisabled.achievementIndicators)
        assertTrue(soundsDisabled.leaderboardTrackers)
        assertFalse(soundsDisabled.soundEffects)
    }
}
