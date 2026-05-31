package com.sbro.emucorex.core.utils

import com.sbro.emucorex.core.EmulatorBridge
import com.sbro.emucorex.data.RetroAchievementsRepository
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch

enum class RetroAchievementsLoginRequestReason {
    USER_INITIATED,
    TOKEN_INVALID,
    UNKNOWN
}

data class RetroAchievementsUserState(
    val username: String,
    val displayName: String,
    val avatarPath: String?,
    val points: Int,
    val softcorePoints: Int,
    val unreadMessages: Int
)

data class RetroAchievementsGameState(
    val title: String,
    val richPresence: String?,
    val iconPath: String?,
    val gameId: Int,
    val earnedAchievements: Int,
    val totalAchievements: Int,
    val earnedPoints: Int,
    val totalPoints: Int,
    val hardcoreMode: Boolean,
    val leaderboardEnabled: Boolean,
    val richPresenceEnabled: Boolean
)

data class RetroAchievementsUiState(
    val isSupported: Boolean = EmulatorBridge.isNativeLoaded,
    val isLoading: Boolean = false,
    val isAuthenticating: Boolean = false,
    val enabled: Boolean = false,
    val hardcorePreference: Boolean = false,
    val hardcoreActive: Boolean = false,
    val storedUsername: String? = null,
    val user: RetroAchievementsUserState? = null,
    val game: RetroAchievementsGameState? = null,
    val errorMessage: String? = null,
    val loginRequestReason: RetroAchievementsLoginRequestReason? = null
)

object RetroAchievementsStateManager {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private val _state = MutableStateFlow(RetroAchievementsUiState())
    val state: StateFlow<RetroAchievementsUiState> = _state.asStateFlow()

    @Volatile
    private var initialized = false

    fun initialize() {
        if (initialized) return
        initialized = true
        val prefsEnabled = loadStoredEnabled()
        val prefsHardcore = loadStoredHardcore()
        _state.update {
            it.copy(
                isSupported = EmulatorBridge.isNativeLoaded,
                storedUsername = loadStoredUsername(),
                enabled = prefsEnabled,
                hardcorePreference = prefsHardcore,
                isLoading = true
            )
        }
        refreshState(invalidateCaches = false)
    }

    fun refreshState(invalidateCaches: Boolean = true) {
        if (invalidateCaches) {
            RetroAchievementsRepository.invalidateUnlockedAchievementsCache()
        }
        if (!EmulatorBridge.isNativeLoaded) {
            _state.update {
                it.copy(
                    isSupported = false,
                    isLoading = false,
                    isAuthenticating = false,
                    errorMessage = null
                )
            }
            return
        }

        _state.update {
            it.copy(
                isSupported = true,
                isLoading = true,
                enabled = loadStoredEnabled(),
                hardcorePreference = loadStoredHardcore(),
                storedUsername = loadStoredUsername() ?: it.storedUsername
            )
        }
        scope.launch(Dispatchers.IO) {
            runCatching { RetroAchievementsBridge.nativeRequestState() }
                .onFailure { handleNativeFailure(it) }
        }
    }

    fun login(username: String, password: String, rememberPassword: Boolean = false) {
        val cleanUsername = username.trim()
        if (cleanUsername.isBlank() || password.isBlank()) {
            _state.update {
                it.copy(errorMessage = "Username and password are required.")
            }
            return
        }

        _state.update {
            it.copy(
                isAuthenticating = true,
                isLoading = true,
                errorMessage = null,
                loginRequestReason = null,
                storedUsername = cleanUsername
            )
        }
        RetroAchievementsRepository.invalidateUnlockedAchievementsCache()
        scope.launch(Dispatchers.IO) {
            runCatching { RetroAchievementsBridge.nativeLogin(cleanUsername, password) }
                .onSuccess { error ->
                    if (!error.isNullOrBlank()) {
                        _state.update {
                            it.copy(
                                isAuthenticating = false,
                                isLoading = false,
                                errorMessage = error
                            )
                        }
                    } else {
                        EmulatorBridge.getContext()?.let { context ->
                            val preferences = com.sbro.emucorex.data.AppPreferences(context)
                            val previousUsername = preferences.getAchievementsUsernameSync()
                            preferences.setAchievementsUsername(cleanUsername)
                            preferences.setAchievementsRememberPassword(rememberPassword)
                            preferences.setAchievementsPassword(password.takeIf { rememberPassword })
                            if (!previousUsername.equals(cleanUsername, ignoreCase = true)) {
                                preferences.setAchievementsAvatarPath(null)
                            }
                            preferences.setAchievementsAccountProgressJson(null)
                        }
                        _state.update { current ->
                            current.copy(
                                isAuthenticating = false,
                                isLoading = false,
                                errorMessage = null,
                                loginRequestReason = null,
                                storedUsername = cleanUsername,
                                user = current.user ?: RetroAchievementsUserState(
                                    username = cleanUsername,
                                    displayName = cleanUsername,
                                    avatarPath = current.user?.takeIf { it.username == cleanUsername }?.avatarPath,
                                    points = 0,
                                    softcorePoints = 0,
                                    unreadMessages = 0
                                )
                            )
                        }
                    }
                }
                .onFailure { handleNativeFailure(it) }
        }
    }

    fun logout() {
        RetroAchievementsRepository.invalidateUnlockedAchievementsCache()
        _state.update {
            it.copy(
                isLoading = true,
                isAuthenticating = false,
                errorMessage = null,
                loginRequestReason = null
            )
        }
        scope.launch(Dispatchers.IO) {
            runCatching { RetroAchievementsBridge.nativeLogout() }
                .onFailure { handleNativeFailure(it) }
                .also {
                    EmulatorBridge.getContext()?.let { context ->
                        com.sbro.emucorex.data.AppPreferences(context).run {
                            setAchievementsAvatarPath(null)
                            setAchievementsAccountProgressJson(null)
                        }
                    }
                }
        }
    }

    fun setEnabled(enabled: Boolean) {
        RetroAchievementsRepository.invalidateUnlockedAchievementsCache()
        _state.update {
            it.copy(
                enabled = enabled,
                isLoading = true,
                errorMessage = null
            )
        }
        scope.launch(Dispatchers.IO) {
            runCatching {
                EmulatorBridge.getContext()
                    ?.let { com.sbro.emucorex.data.AppPreferences(it).setAchievementsEnabled(enabled) }
                RetroAchievementsBridge.nativeSetEnabled(enabled)
            }
                .onFailure { handleNativeFailure(it) }
        }
    }

    fun setHardcore(enabled: Boolean) {
        _state.update {
            it.copy(
                hardcorePreference = enabled,
                isLoading = true,
                errorMessage = null
            )
        }
        scope.launch(Dispatchers.IO) {
            runCatching {
                EmulatorBridge.getContext()
                    ?.let { com.sbro.emucorex.data.AppPreferences(it).setAchievementsHardcore(enabled) }
                RetroAchievementsBridge.nativeSetHardcore(enabled)
            }
                .onFailure { handleNativeFailure(it) }
        }
    }

    fun clearTransientState() {
        _state.update {
            it.copy(
                errorMessage = null,
                loginRequestReason = null
            )
        }
    }

    internal fun onLoginRequested(reason: Int) {
        _state.update {
            it.copy(
                isAuthenticating = false,
                isLoading = false,
                loginRequestReason = when (reason) {
                    0 -> RetroAchievementsLoginRequestReason.USER_INITIATED
                    1 -> RetroAchievementsLoginRequestReason.TOKEN_INVALID
                    else -> RetroAchievementsLoginRequestReason.UNKNOWN
                }
            )
        }
    }

    internal fun onLoginSuccess(username: String?, points: Int, scPoints: Int, unreadMessages: Int) {
        val storedAvatar = loadStoredAvatarPath()
        _state.update { current ->
            val resolvedUsername = username?.takeIf { it.isNotBlank() }
                ?: current.user?.username
                ?: current.storedUsername
                ?: loadStoredUsername()
            current.copy(
                isAuthenticating = false,
                isLoading = false,
                errorMessage = null,
                loginRequestReason = null,
                storedUsername = resolvedUsername ?: current.storedUsername,
                user = if (!resolvedUsername.isNullOrBlank()) {
                    current.user?.copy(
                        username = resolvedUsername,
                        displayName = username?.takeIf { it.isNotBlank() } ?: current.user.displayName,
                        points = points,
                        softcorePoints = scPoints,
                        unreadMessages = unreadMessages
                    ) ?: RetroAchievementsUserState(
                        username = resolvedUsername,
                        displayName = username?.takeIf { it.isNotBlank() } ?: resolvedUsername,
                        avatarPath = storedAvatar,
                        points = points,
                        softcorePoints = scPoints,
                        unreadMessages = unreadMessages
                    )
                } else {
                    current.user?.copy(
                        points = points,
                        softcorePoints = scPoints,
                        unreadMessages = unreadMessages
                    )
                }
            )
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
        val resolvedStoredUsername = username ?: _state.value.storedUsername ?: loadStoredUsername()
        val storedEnabled = loadStoredEnabled()
        val storedHardcore = loadStoredHardcore()
        val storedAvatar = loadStoredAvatarPath()
        val resolvedAvatar = avatar?.takeIf { it.isNotBlank() } ?: storedAvatar
        if (!avatar.isNullOrBlank()) {
            persistStoredAvatarPath(avatar)
        }
        _state.update { current ->
            current.copy(
                isSupported = true,
                isLoading = false,
                isAuthenticating = false,
                enabled = storedEnabled,
                hardcorePreference = storedHardcore,
                hardcoreActive = hardcoreActive,
                storedUsername = resolvedStoredUsername,
                user = if (haveUser && !username.isNullOrBlank()) {
                    RetroAchievementsUserState(
                        username = username,
                        displayName = displayName?.takeIf { it.isNotBlank() } ?: username,
                        avatarPath = resolvedAvatar,
                        points = points,
                        softcorePoints = scPoints,
                        unreadMessages = unreadMessages
                    )
                } else if (!resolvedStoredUsername.isNullOrBlank()) {
                    current.user?.takeIf { it.username == resolvedStoredUsername } ?: RetroAchievementsUserState(
                        username = resolvedStoredUsername,
                        displayName = resolvedStoredUsername,
                        avatarPath = storedAvatar,
                        points = 0,
                        softcorePoints = 0,
                        unreadMessages = 0
                    )
                } else {
                    null
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
                },
                errorMessage = null,
                loginRequestReason = null
            )
        }
    }

    internal fun onHardcoreModeChanged(enabled: Boolean) {
        _state.update {
            it.copy(
                hardcoreActive = enabled,
                isLoading = false
            )
        }
    }

    private fun loadStoredUsername(): String? {
        val context = EmulatorBridge.getContext() ?: return null
        return com.sbro.emucorex.data.AppPreferences(context).getAchievementsUsernameSync()
    }

    private fun loadStoredEnabled(): Boolean {
        val context = EmulatorBridge.getContext() ?: return _state.value.enabled
        return com.sbro.emucorex.data.AppPreferences(context).getAchievementsEnabledSync()
    }

    private fun loadStoredHardcore(): Boolean {
        val context = EmulatorBridge.getContext() ?: return _state.value.hardcorePreference
        return com.sbro.emucorex.data.AppPreferences(context).getAchievementsHardcoreSync()
    }

    private fun loadStoredAvatarPath(): String? {
        val context = EmulatorBridge.getContext() ?: return null
        return com.sbro.emucorex.data.AppPreferences(context).getAchievementsAvatarPathSync()
    }

    private fun persistStoredAvatarPath(avatarPath: String) {
        val context = EmulatorBridge.getContext() ?: return
        scope.launch(Dispatchers.IO) {
            com.sbro.emucorex.data.AppPreferences(context).setAchievementsAvatarPath(avatarPath)
        }
    }

    private fun handleNativeFailure(error: Throwable) {
        _state.update {
            it.copy(
                isLoading = false,
                isAuthenticating = false,
                errorMessage = error.message ?: "RetroAchievements is unavailable right now."
            )
        }
    }
}
