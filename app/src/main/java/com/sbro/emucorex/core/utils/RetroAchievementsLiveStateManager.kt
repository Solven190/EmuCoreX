package com.sbro.emucorex.core.utils

import com.sbro.emucorex.core.EmulatorBridge
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch

data class RetroAchievementsLiveUiState(
    val isSupported: Boolean = EmulatorBridge.isNativeLoaded,
    val enabled: Boolean = false,
    val hardcorePreference: Boolean = false,
    val hardcoreActive: Boolean = false,
    val user: RetroAchievementsUserState? = null,
    val game: RetroAchievementsGameState? = null
)

object RetroAchievementsLiveStateManager {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private val _state = MutableStateFlow(RetroAchievementsLiveUiState())
    val state: StateFlow<RetroAchievementsLiveUiState> = _state.asStateFlow()

    fun refreshFromNative() {
        if (!EmulatorBridge.isNativeLoaded) {
            _state.update { it.copy(isSupported = false) }
            return
        }

        scope.launch(Dispatchers.IO) {
            runCatching { RetroAchievementsBridge.nativeRequestState() }
        }
    }

    internal fun onStateChanged(
        enabled: Boolean,
        haveUser: Boolean,
        username: String?,
        displayName: String?,
        avatar: String?,
        points: Int,
        scPoints: Int,
        unreadMessages: Int,
        hardcorePreference: Boolean,
        hardcoreActive: Boolean,
        haveGame: Boolean,
        gameTitle: String?,
        richPresence: String?,
        iconPath: String?,
        gameId: Int,
        numAchievements: Int,
        earnedAchievements: Int,
        score: Int,
        scScore: Int,
        hardcoreMode: Boolean,
        leaderboardEnabled: Boolean,
        richPresenceEnabled: Boolean
    ) {
        _state.update { current ->
            current.copy(
                isSupported = true,
                enabled = enabled,
                hardcorePreference = hardcorePreference,
                hardcoreActive = hardcoreActive,
                user = if (haveUser && !username.isNullOrBlank()) {
                    RetroAchievementsUserState(
                        username = username,
                        displayName = displayName?.takeIf { it.isNotBlank() } ?: username,
                        avatarPath = avatar?.takeIf { it.isNotBlank() } ?: current.user?.avatarPath,
                        points = points,
                        softcorePoints = scPoints,
                        unreadMessages = unreadMessages
                    )
                } else {
                    current.user
                },
                game = if (haveGame && !gameTitle.isNullOrBlank()) {
                    RetroAchievementsGameState(
                        title = gameTitle,
                        richPresence = richPresence,
                        iconPath = iconPath,
                        gameId = gameId,
                        earnedAchievements = earnedAchievements,
                        totalAchievements = numAchievements,
                        earnedPoints = score,
                        totalPoints = scScore,
                        hardcoreMode = hardcoreMode,
                        leaderboardEnabled = leaderboardEnabled,
                        richPresenceEnabled = richPresenceEnabled
                    )
                } else {
                    null
                }
            )
        }
    }

    internal fun onLoginSuccess(username: String?, points: Int, scPoints: Int, unreadMessages: Int) {
        if (username.isNullOrBlank()) return
        _state.update { current ->
            current.copy(
                user = current.user?.copy(
                    username = username,
                    displayName = current.user.displayName.ifBlank { username },
                    points = points,
                    softcorePoints = scPoints,
                    unreadMessages = unreadMessages
                ) ?: RetroAchievementsUserState(
                    username = username,
                    displayName = username,
                    avatarPath = null,
                    points = points,
                    softcorePoints = scPoints,
                    unreadMessages = unreadMessages
                )
            )
        }
    }

    internal fun onHardcoreModeChanged(enabled: Boolean) {
        _state.update { it.copy(hardcoreActive = enabled) }
    }
}
