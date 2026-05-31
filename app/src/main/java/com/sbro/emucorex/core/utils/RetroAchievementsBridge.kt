package com.sbro.emucorex.core.utils

import android.util.Log
import com.sbro.emucorex.core.NativeApp
import com.sbro.emucorex.data.AppPreferences
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

object RetroAchievementsBridge {
    private const val TAG = "RetroAchievementsBridge"

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
        Log.d(TAG, "notifyLoginRequested reason=$reason")
        RetroAchievementsStateManager.onLoginRequested(reason)
    }

    @JvmStatic
    fun notifyLoginSuccess(username: String?, points: Int, scPoints: Int, unreadMessages: Int) {
        Log.d(TAG, "notifyLoginSuccess user=$username")
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
        Log.d(TAG, "notifyHardcoreModeChanged enabled=$enabled")
        RetroAchievementsStateManager.onHardcoreModeChanged(enabled)
    }

    @JvmStatic
    fun notifySettingsChanged(section: String, key: String, value: String) {
        Log.d(TAG, "notifySettingsChanged section=$section, key=$key, value=${if (key == "Token") "********" else value}")
        val context = NativeApp.getContext() ?: return
        val prefs = AppPreferences(context)
        CoroutineScope(Dispatchers.IO).launch {
            if (section == "Achievements") {
                when (key) {
                    "Username" -> prefs.setAchievementsUsername(value.ifBlank { null })
                    "Token" -> prefs.setAchievementsToken(value.ifBlank { null })
                    "Enabled" -> prefs.setAchievementsEnabled(value.toBoolean())
                    "ChallengeMode" -> prefs.setAchievementsHardcore(value.toBoolean())
                }
            }
        }
    }
}
