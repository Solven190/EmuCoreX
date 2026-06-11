package com.sbro.emucorex.core.utils

import com.sbro.emucorex.core.NativeApp
import com.sbro.emucorex.data.AppPreferences
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking

object RetroAchievementsBridge {
    @JvmStatic
    external fun nativeRequestState()

    @JvmStatic
    external fun nativeLogin(username: String, password: String): String?

    @JvmStatic
    external fun nativeLogout()

    @JvmStatic
    external fun nativeSetEnabled(enabled: Boolean)

    @JvmStatic
    external fun nativeSetHardcore(enabled: Boolean)

    @JvmStatic
    fun notifyLoginRequested(reason: Int) {
        RetroAchievementsStateManager.onLoginRequested(reason)
    }

    @JvmStatic
    fun notifyLoginSuccess(username: String?, points: Int, scPoints: Int, unreadMessages: Int) {
        RetroAchievementsLiveStateManager.onLoginSuccess(username, points, scPoints, unreadMessages)
        RetroAchievementsStateManager.onLoginSuccess(username, points, scPoints, unreadMessages)
    }

    @JvmStatic
    fun notifyStateChanged(
        enabled: Boolean, haveUser: Boolean, username: String?, displayName: String?,
        avatar: String?, points: Int, scPoints: Int, unreadMessages: Int,
        hardcorePreference: Boolean, hardcoreActive: Boolean, haveGame: Boolean,
        gameTitle: String?, richPresence: String?, iconPath: String?,
        gameId: Int, numAchievements: Int, earnedAchievements: Int, score: Int, scScore: Int,
        hardcoreMode: Boolean, leaderboardEnabled: Boolean, richPresenceEnabled: Boolean
    ) {
        RetroAchievementsLiveStateManager.onStateChanged(
            enabled = enabled,
            haveUser = haveUser,
            username = username,
            displayName = displayName,
            avatar = avatar,
            points = points,
            scPoints = scPoints,
            unreadMessages = unreadMessages,
            hardcorePreference = hardcorePreference,
            hardcoreActive = hardcoreActive,
            haveGame = haveGame,
            gameTitle = gameTitle,
            richPresence = richPresence,
            iconPath = iconPath,
            gameId = gameId,
            numAchievements = numAchievements,
            earnedAchievements = earnedAchievements,
            score = score,
            scScore = scScore,
            hardcoreMode = hardcoreMode,
            leaderboardEnabled = leaderboardEnabled,
            richPresenceEnabled = richPresenceEnabled
        )
        RetroAchievementsStateManager.onStateChanged(
            enabled = enabled,
            haveUser = haveUser,
            username = username,
            displayName = displayName,
            avatar = avatar,
            points = points,
            scPoints = scPoints,
            unreadMessages = unreadMessages,
            hardcorePreference = hardcorePreference,
            hardcoreActive = hardcoreActive,
            haveGame = haveGame,
            gameTitle = gameTitle,
            richPresence = richPresence,
            iconPath = iconPath,
            gameId = gameId,
            numAchievements = numAchievements,
            earnedAchievements = earnedAchievements,
            score = score,
            scScore = scScore,
            hardcoreMode = hardcoreMode,
            leaderboardEnabled = leaderboardEnabled,
            richPresenceEnabled = richPresenceEnabled
        )
    }

    @JvmStatic
    fun notifyHardcoreModeChanged(enabled: Boolean) {
        RetroAchievementsLiveStateManager.onHardcoreModeChanged(enabled)
        RetroAchievementsStateManager.onHardcoreModeChanged(enabled)
    }

    @JvmStatic
    fun notifyNotification(kind: String?, title: String?, message: String?, imagePath: String?) {
        RetroAchievementsLiveStateManager.onNotification(kind, title, message, imagePath)
    }

    @JvmStatic
    fun notifySettingsChanged(section: String, key: String, value: String) {
        val context = NativeApp.getContext() ?: return
        val prefs = AppPreferences(context)
        if (section == "Achievements" && (key == "Username" || key == "Token" || key == "LoginTimestamp")) {
            runBlocking(Dispatchers.IO) {
                when (key) {
                    "Username" -> prefs.setAchievementsUsername(value.ifBlank { null })
                    "Token" -> prefs.setAchievementsToken(value.ifBlank { null })
                    "LoginTimestamp" -> prefs.setAchievementsLoginTimestamp(value.ifBlank { null })
                }
            }
            return
        }

        CoroutineScope(Dispatchers.IO).launch {
            if (section == "Achievements") {
                when (key) {
                    "Enabled" -> prefs.setAchievementsEnabled(value.toBoolean())
                    "ChallengeMode" -> prefs.setAchievementsHardcore(value.toBoolean())
                }
            }
        }
    }
}
