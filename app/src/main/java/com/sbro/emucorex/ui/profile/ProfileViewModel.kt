package com.sbro.emucorex.ui.profile

import android.app.Activity
import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.sbro.emucorex.data.PlayerAccount
import com.sbro.emucorex.data.PlayerGamePlayStat
import com.sbro.emucorex.data.PlayerLeaderboardEntry
import com.sbro.emucorex.data.PlayerProfile
import com.sbro.emucorex.data.PlayerProfileRepository
import com.sbro.emucorex.data.ps2.Ps2CatalogRepository
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch

data class ProfileUiState(
    val account: PlayerAccount? = null,
    val profile: PlayerProfile? = null,
    val viewedProfile: PlayerProfile? = null,
    val games: List<PlayerGamePlayStat> = emptyList(),
    val leaderboard: List<PlayerLeaderboardEntry> = emptyList(),
    val isAuthLoading: Boolean = false,
    val isProfileLoading: Boolean = true,
    val isViewedProfileLoading: Boolean = false,
    val isLeaderboardLoading: Boolean = true,
    val messageKey: String? = null,
    val errorMessage: String? = null
)

class ProfileViewModel(application: Application) : AndroidViewModel(application) {

    private val repository = PlayerProfileRepository(application)
    private val catalogRepository = Ps2CatalogRepository(application)
    private val _uiState = MutableStateFlow(ProfileUiState())
    val uiState: StateFlow<ProfileUiState> = _uiState.asStateFlow()

    private var profileJob: Job? = null
    private var viewedProfileJob: Job? = null

    init {
        viewModelScope.launch {
            repository.observeAuthState().collect { account ->
                profileJob?.cancel()
                viewedProfileJob?.cancel()
                _uiState.update {
                    it.copy(
                        account = account,
                        profile = null,
                        viewedProfile = null,
                        games = emptyList(),
                        isProfileLoading = account != null,
                        isViewedProfileLoading = false,
                        errorMessage = null
                    )
                }
                if (account != null) {
                    observeProfile(account.uid)
                }
            }
        }
        viewModelScope.launch {
            repository.observeLeaderboard().collect { leaderboard ->
                _uiState.update {
                    it.copy(
                        leaderboard = leaderboard,
                        isLeaderboardLoading = false
                    )
                }
            }
        }
    }

    fun signIn(email: String, password: String) {
        runAuthAction {
            repository.signIn(email, password)
            "profile_signed_in"
        }
    }

    fun createAccount(email: String, password: String, displayName: String) {
        runAuthAction {
            repository.createAccount(email, password, displayName)
            "profile_account_created"
        }
    }

    fun signInWithGoogle(activity: Activity) {
        runAuthAction {
            repository.signInWithGoogle(activity)
            "profile_signed_in"
        }
    }

    fun sendPasswordReset(email: String) {
        runAuthAction {
            repository.sendPasswordReset(email)
            "profile_password_reset_sent"
        }
    }

    fun updateDisplayName(displayName: String) {
        runAuthAction {
            repository.updateDisplayName(displayName)
            "profile_name_updated"
        }
    }

    fun signOut() {
        viewedProfileJob?.cancel()
        repository.signOut()
        _uiState.update { it.copy(viewedProfile = null, isViewedProfileLoading = false, messageKey = "profile_signed_out", errorMessage = null) }
    }

    fun clearTransientMessages() {
        _uiState.update { it.copy(messageKey = null, errorMessage = null) }
    }

    fun viewLeaderboardProfile(uid: String) {
        if (uid == _uiState.value.account?.uid) {
            closeViewedProfile()
            return
        }
        viewedProfileJob?.cancel()
        _uiState.update { it.copy(viewedProfile = null, isViewedProfileLoading = true, errorMessage = null) }
        viewedProfileJob = viewModelScope.launch {
            repository.observeProfile(uid).collect { profile ->
                _uiState.update {
                    it.copy(
                        viewedProfile = profile,
                        isViewedProfileLoading = false
                    )
                }
            }
        }
    }

    fun closeViewedProfile() {
        viewedProfileJob?.cancel()
        viewedProfileJob = null
        _uiState.update { it.copy(viewedProfile = null, isViewedProfileLoading = false) }
    }

    fun openGameDetails(game: PlayerGamePlayStat, onOpen: (Long) -> Unit) {
        viewModelScope.launch {
            val catalogId = runCatching {
                catalogRepository.findBestMatchId(
                    serial = game.serial,
                    title = game.title
                )
            }.getOrNull()
            catalogId?.let(onOpen)
        }
    }

    private fun observeProfile(uid: String) {
        profileJob = viewModelScope.launch {
            repository.observeProfile(uid).collect { profile ->
                _uiState.update {
                    it.copy(
                        profile = profile,
                        games = profile?.games.orEmpty(),
                        isProfileLoading = false
                    )
                }
            }
        }
    }

    private fun runAuthAction(action: suspend () -> String) {
        viewModelScope.launch {
            _uiState.update { it.copy(isAuthLoading = true, messageKey = null, errorMessage = null) }
            runCatching { action() }
                .onSuccess { messageKey ->
                    _uiState.update { it.copy(isAuthLoading = false, messageKey = messageKey) }
                }
                .onFailure { error ->
                    _uiState.update {
                        it.copy(
                            isAuthLoading = false,
                            errorMessage = error.localizedMessage ?: "Something went wrong."
                        )
                    }
                }
        }
    }
}
