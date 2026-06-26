package com.sbro.emucorex.ui.profile

import android.app.Activity
import android.content.Context
import android.content.ContextWrapper
import android.widget.Toast
import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.animateContentSize
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.BorderStroke
import androidx.compose.material.icons.automirrored.rounded.Login
import androidx.compose.material.icons.automirrored.rounded.Logout
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsIgnoringVisibility
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.AccountCircle
import androidx.compose.material.icons.rounded.Edit
import androidx.compose.material.icons.rounded.Email
import androidx.compose.material.icons.rounded.EmojiEvents
import androidx.compose.material.icons.rounded.Leaderboard
import androidx.compose.material.icons.rounded.Person
import androidx.compose.material.icons.rounded.Schedule
import androidx.compose.material.icons.rounded.SportsEsports
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.sbro.emucorex.R
import com.sbro.emucorex.data.PlayerGamePlayStat
import com.sbro.emucorex.data.PlayerLeaderboardEntry
import com.sbro.emucorex.data.PlayerProfile
import com.sbro.emucorex.ui.common.BitmapPathImage
import com.sbro.emucorex.ui.common.GameCoverArt
import com.sbro.emucorex.ui.common.ScreenTopBar
import com.sbro.emucorex.ui.common.shimmer
import com.sbro.emucorex.ui.theme.ScreenHorizontalPadding
import java.text.DateFormat
import java.util.Date
import java.util.Locale

private enum class ProfileTab {
    Overview,
    Games,
    Leaderboard
}

@OptIn(ExperimentalMaterial3Api::class, ExperimentalLayoutApi::class)
@Composable
fun ProfileScreen(
    onBackClick: () -> Unit,
    onOpenGameDetails: (Long) -> Unit,
    viewModel: ProfileViewModel = viewModel()
) {
    val uiState by viewModel.uiState.collectAsState()
    val context = LocalContext.current
    val topInset = WindowInsets.statusBarsIgnoringVisibility.asPaddingValues().calculateTopPadding()
    val bottomInset = WindowInsets.navigationBars.asPaddingValues().calculateBottomPadding()
    val selectedTab = rememberSaveable { mutableIntStateOf(0) }
    val isViewingLeaderboardProfile = uiState.viewedProfile != null || uiState.isViewedProfileLoading

    BackHandler(enabled = isViewingLeaderboardProfile) {
        viewModel.closeViewedProfile()
    }

    LaunchedEffect(uiState.messageKey, uiState.errorMessage) {
        uiState.messageKey?.let { key ->
            Toast.makeText(context, profileMessageRes(key), Toast.LENGTH_SHORT).show()
            viewModel.clearTransientMessages()
        }
        uiState.errorMessage?.let { message ->
            Toast.makeText(context, message, Toast.LENGTH_LONG).show()
            viewModel.clearTransientMessages()
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(MaterialTheme.colorScheme.background)
    ) {
        if (uiState.account == null) {
            AuthContent(
                isLoading = uiState.isAuthLoading,
                topInset = topInset,
                onBackClick = onBackClick,
                onSignIn = viewModel::signIn,
                onCreateAccount = viewModel::createAccount,
                onResetPassword = viewModel::sendPasswordReset,
                onGoogleSignIn = {
                    context.findActivity()?.let(viewModel::signInWithGoogle)
                        ?: Toast.makeText(context, R.string.profile_google_failed, Toast.LENGTH_SHORT).show()
                },
                modifier = Modifier.weight(1f)
            )
        } else {
            val tabs = ProfileTab.entries
            if (isViewingLeaderboardProfile) {
                ViewedPlayerProfile(
                    profile = uiState.viewedProfile,
                    isLoading = uiState.isViewedProfileLoading,
                    topInset = topInset,
                    onBack = viewModel::closeViewedProfile,
                    onGameClick = { game -> viewModel.openGameDetails(game, onOpenGameDetails) },
                    modifier = Modifier.weight(1f)
                )
            } else {
                Box(modifier = Modifier.weight(1f)) {
                    LazyColumn(
                        modifier = Modifier.fillMaxSize(),
                        contentPadding = PaddingValues(
                            start = ScreenHorizontalPadding,
                            end = ScreenHorizontalPadding,
                            top = topInset + 8.dp,
                            bottom = bottomInset + 110.dp
                        ),
                        verticalArrangement = Arrangement.spacedBy(12.dp)
                    ) {
                        item {
                            ScreenTopBar(
                                title = stringResource(R.string.profile_title),
                                onBackClick = onBackClick
                            )
                        }
                        when (tabs[selectedTab.intValue]) {
                            ProfileTab.Overview -> {
                                item {
                                    ProfileOverview(
                                        profile = uiState.profile,
                                        isLoading = uiState.isProfileLoading,
                                        photoURL = uiState.account?.photoURL,
                                        email = uiState.account?.email,
                                        isActionLoading = uiState.isAuthLoading,
                                        onUpdateName = viewModel::updateDisplayName,
                                        onSignOut = {
                                            viewModel.signOut()
                                        }
                                    )
                                }
                                if (uiState.isProfileLoading && uiState.profile == null) {
                                    item { RecentGamesSkeletonCard() }
                                } else {
                                    uiState.profile?.games
                                        .orEmpty()
                                        .filter { (it.lastPlayedAtMs ?: 0L) > 0L }
                                        .sortedByDescending { it.lastPlayedAtMs ?: 0L }
                                        .take(8)
                                        .takeIf { it.isNotEmpty() }
                                        ?.let { recentGames ->
                                            item {
                                                RecentGamesCard(
                                                    games = recentGames,
                                                    onGameClick = { game -> viewModel.openGameDetails(game, onOpenGameDetails) }
                                                )
                                            }
                                        }
                                }
                            }

                            ProfileTab.Games -> {
                                if (uiState.isProfileLoading && uiState.games.isEmpty()) {
                                    items(4) { GamePlayStatSkeletonRow() }
                                } else if (uiState.games.isEmpty()) {
                                    item { EmptyProfileState(text = stringResource(R.string.profile_games_empty)) }
                                } else {
                                    items(uiState.games, key = { it.gameKey }) { game ->
                                        GamePlayStatRow(
                                            game = game,
                                            onClick = { viewModel.openGameDetails(game, onOpenGameDetails) }
                                        )
                                    }
                                }
                            }

                            ProfileTab.Leaderboard -> {
                                if (uiState.isLeaderboardLoading) {
                                    item { LoadingRow(text = stringResource(R.string.profile_leaderboard_loading)) }
                                } else if (uiState.leaderboard.isEmpty()) {
                                    item { EmptyProfileState(text = stringResource(R.string.profile_leaderboard_empty)) }
                                } else {
                                    items(uiState.leaderboard, key = { it.uid }) { entry ->
                                        LeaderboardRow(
                                            entry = entry,
                                            currentUid = uiState.account?.uid,
                                            onClick = { viewModel.viewLeaderboardProfile(entry.uid) }
                                        )
                                    }
                                }
                            }
                        }
                    }
                    ProfileBottomNav(
                        tabs = tabs,
                        selectedIndex = selectedTab.intValue,
                        onSelect = { selectedTab.intValue = it },
                        modifier = Modifier
                            .align(Alignment.BottomCenter)
                            .padding(horizontal = ScreenHorizontalPadding)
                            .padding(bottom = bottomInset + 12.dp)
                    )
                }
            }
        }
    }
}

@Composable
private fun ProfileBottomNav(
    tabs: List<ProfileTab>,
    selectedIndex: Int,
    onSelect: (Int) -> Unit,
    modifier: Modifier = Modifier
) {
    Surface(
        modifier = modifier.fillMaxWidth(),
        shape = RoundedCornerShape(28.dp),
        color = MaterialTheme.colorScheme.surface,
        tonalElevation = 4.dp,
        border = profileCardBorder()
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(6.dp),
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            tabs.forEachIndexed { index, tab ->
                val selected = selectedIndex == index
                Surface(
                    onClick = { onSelect(index) },
                    modifier = Modifier
                        .weight(1f)
                        .height(56.dp),
                    shape = RoundedCornerShape(22.dp),
                    color = if (selected) MaterialTheme.colorScheme.primary.copy(alpha = 0.16f) else Color.Transparent
                ) {
                    Column(
                        modifier = Modifier.fillMaxSize(),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.Center
                    ) {
                        Icon(
                            imageVector = tab.icon(),
                            contentDescription = null,
                            tint = if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.size(22.dp)
                        )
                        Spacer(Modifier.height(3.dp))
                        Text(
                            text = stringResource(tab.titleRes()),
                            style = MaterialTheme.typography.labelMedium.copy(fontWeight = if (selected) FontWeight.Bold else FontWeight.Medium),
                            color = if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun AuthContent(
    isLoading: Boolean,
    topInset: androidx.compose.ui.unit.Dp,
    onBackClick: () -> Unit,
    onSignIn: (String, String) -> Unit,
    onCreateAccount: (String, String, String) -> Unit,
    onResetPassword: (String) -> Unit,
    onGoogleSignIn: () -> Unit,
    modifier: Modifier = Modifier
) {
    var email by rememberSaveable { mutableStateOf("") }
    var password by rememberSaveable { mutableStateOf("") }
    var displayName by rememberSaveable { mutableStateOf("") }
    var createMode by rememberSaveable { mutableStateOf(false) }

    LazyColumn(
        modifier = modifier,
        contentPadding = PaddingValues(
            start = ScreenHorizontalPadding,
            end = ScreenHorizontalPadding,
            top = topInset + 8.dp,
            bottom = 28.dp
        ),
        verticalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        item {
            ScreenTopBar(
                title = stringResource(R.string.profile_title),
                onBackClick = onBackClick
            )
        }
        item {
            Surface(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(22.dp),
                color = MaterialTheme.colorScheme.surface,
                tonalElevation = 2.dp,
                border = profileCardBorder()
            ) {
                Column(
                    modifier = Modifier
                        .padding(18.dp)
                        .animateContentSize(
                            animationSpec = tween(
                                durationMillis = 260,
                                easing = FastOutSlowInEasing
                            )
                        ),
                    verticalArrangement = Arrangement.spacedBy(14.dp)
                ) {
                    Text(
                        text = stringResource(R.string.profile_auth_title),
                        style = MaterialTheme.typography.titleLarge.copy(fontWeight = FontWeight.Bold),
                        color = MaterialTheme.colorScheme.onSurface
                    )
                    AnimatedVisibility(
                        visible = createMode,
                        enter = fadeIn(animationSpec = tween(160)) + expandVertically(
                            animationSpec = tween(260, easing = FastOutSlowInEasing)
                        ),
                        exit = fadeOut(animationSpec = tween(120)) + shrinkVertically(
                            animationSpec = tween(220, easing = FastOutSlowInEasing)
                        )
                    ) {
                        OutlinedTextField(
                            value = displayName,
                            onValueChange = { displayName = it },
                            modifier = Modifier.fillMaxWidth(),
                            singleLine = true,
                            shape = RoundedCornerShape(20.dp),
                            label = { Text(stringResource(R.string.profile_display_name)) },
                            leadingIcon = { Icon(Icons.Rounded.Person, contentDescription = null) }
                        )
                    }
                    OutlinedTextField(
                        value = email,
                        onValueChange = { email = it },
                        modifier = Modifier.fillMaxWidth(),
                        singleLine = true,
                        shape = RoundedCornerShape(20.dp),
                        label = { Text(stringResource(R.string.profile_email)) },
                        leadingIcon = { Icon(Icons.Rounded.Email, contentDescription = null) }
                    )
                    OutlinedTextField(
                        value = password,
                        onValueChange = { password = it },
                        modifier = Modifier.fillMaxWidth(),
                        singleLine = true,
                        shape = RoundedCornerShape(20.dp),
                        label = { Text(stringResource(R.string.profile_password)) },
                        visualTransformation = PasswordVisualTransformation(),
                        leadingIcon = { Icon(Icons.AutoMirrored.Rounded.Login, contentDescription = null) }
                    )
                    Button(
                        enabled = !isLoading,
                        onClick = {
                            if (createMode) {
                                onCreateAccount(email, password, displayName)
                            } else {
                                onSignIn(email, password)
                            }
                        },
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        if (isLoading) {
                            CircularProgressIndicator(modifier = Modifier.size(18.dp), strokeWidth = 2.dp)
                            Spacer(Modifier.width(10.dp))
                        }
                        Text(stringResource(if (createMode) R.string.profile_create_account else R.string.profile_sign_in))
                    }
                    OutlinedButton(
                        enabled = !isLoading,
                        onClick = onGoogleSignIn,
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Icon(Icons.Rounded.AccountCircle, contentDescription = null)
                        Spacer(Modifier.width(8.dp))
                        Text(stringResource(R.string.profile_google_sign_in))
                    }
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        TextButton(onClick = { createMode = !createMode }) {
                            Text(stringResource(if (createMode) R.string.profile_have_account else R.string.profile_need_account))
                        }
                        TextButton(enabled = email.isNotBlank() && !isLoading, onClick = { onResetPassword(email) }) {
                            Text(stringResource(R.string.profile_reset_password))
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun ViewedPlayerProfile(
    profile: PlayerProfile?,
    isLoading: Boolean,
    topInset: androidx.compose.ui.unit.Dp,
    onBack: () -> Unit,
    onGameClick: (PlayerGamePlayStat) -> Unit,
    modifier: Modifier = Modifier
) {
    LazyColumn(
        modifier = modifier,
        contentPadding = PaddingValues(
            start = ScreenHorizontalPadding,
            end = ScreenHorizontalPadding,
            top = topInset + 8.dp,
            bottom = 18.dp
        ),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        item {
            ScreenTopBar(
                title = stringResource(R.string.profile_title),
                onBackClick = onBack
            )
        }

        if (isLoading) {
            item { ReadOnlyProfileSkeletonCard() }
            items(3) { GamePlayStatSkeletonRow() }
        } else if (profile == null) {
            item { EmptyProfileState(text = stringResource(R.string.profile_player_not_found)) }
        } else {
            item {
                ReadOnlyProfileCard(profile = profile)
            }
            if (profile.games.isNotEmpty()) {
                item {
                    Text(
                        text = stringResource(R.string.profile_tab_games),
                        style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.Bold),
                        color = MaterialTheme.colorScheme.onBackground,
                        modifier = Modifier.padding(top = 4.dp)
                    )
                }
                items(profile.games, key = { it.gameKey }) { game ->
                    GamePlayStatRow(game = game, onClick = { onGameClick(game) })
                }
            }
        }
    }
}

@Composable
private fun ReadOnlyProfileCard(profile: PlayerProfile) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(28.dp),
        color = MaterialTheme.colorScheme.surface,
        tonalElevation = 2.dp,
        border = profileCardBorder()
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(12.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Box(
                    modifier = Modifier
                        .size(72.dp)
                        .clip(CircleShape)
                        .background(MaterialTheme.colorScheme.primary.copy(alpha = 0.12f)),
                    contentAlignment = Alignment.Center
                ) {
                    BitmapPathImage(
                        imagePath = profile.photoURL,
                        contentDescription = profile.displayName,
                        modifier = Modifier.fillMaxSize(),
                        contentScale = ContentScale.Crop,
                        fallback = {
                            Icon(
                                imageVector = Icons.Rounded.Person,
                                contentDescription = null,
                                tint = MaterialTheme.colorScheme.primary,
                                modifier = Modifier.size(38.dp)
                            )
                        }
                    )
                }
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        text = profile.displayName,
                        style = MaterialTheme.typography.titleLarge.copy(fontWeight = FontWeight.Bold),
                        color = MaterialTheme.colorScheme.onSurface,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                    profile.lastPlayedTitle?.takeIf { it.isNotBlank() }?.let { title ->
                        Text(
                            text = stringResource(R.string.profile_last_played_format, title),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis
                        )
                    }
                }
            }

            HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.5f))

            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                StatChip(
                    icon = Icons.Rounded.Schedule,
                    label = stringResource(R.string.profile_total_time),
                    value = formatDuration(profile.totalPlayTimeMs),
                    modifier = Modifier.weight(1f)
                )
                StatChip(
                    icon = Icons.Rounded.SportsEsports,
                    label = stringResource(R.string.profile_games_played),
                    value = profile.gamesPlayed.toString(),
                    modifier = Modifier.weight(1f)
                )
            }
        }
    }
}

@Composable
private fun ProfileOverview(
    profile: PlayerProfile?,
    isLoading: Boolean,
    photoURL: String?,
    email: String?,
    isActionLoading: Boolean,
    onUpdateName: (String) -> Unit,
    onSignOut: () -> Unit
) {
    var editingName by rememberSaveable(profile?.displayName) { mutableStateOf(profile?.displayName.orEmpty()) }
    var isEditingName by rememberSaveable { mutableStateOf(false) }
    val avatarUrl = photoURL ?: profile?.photoURL

    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(28.dp),
        color = MaterialTheme.colorScheme.surface,
        tonalElevation = 2.dp,
        border = profileCardBorder()
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            if (isLoading && profile == null) {
                ProfileOverviewSkeletonContent()
            } else {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(12.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Box(
                        modifier = Modifier
                            .size(72.dp)
                            .clip(CircleShape)
                            .background(MaterialTheme.colorScheme.primary.copy(alpha = 0.12f)),
                        contentAlignment = Alignment.Center
                    ) {
                        BitmapPathImage(
                            imagePath = avatarUrl,
                            contentDescription = profile?.displayName,
                            modifier = Modifier.fillMaxSize(),
                            contentScale = ContentScale.Crop,
                            fallback = {
                                Icon(
                                    imageVector = Icons.Rounded.Person,
                                    contentDescription = null,
                                    tint = MaterialTheme.colorScheme.primary,
                                    modifier = Modifier.size(38.dp)
                                )
                            }
                        )
                    }
                    Column(modifier = Modifier.weight(1f)) {
                        Text(
                            text = profile?.displayName ?: stringResource(R.string.profile_player),
                            style = MaterialTheme.typography.titleLarge.copy(fontWeight = FontWeight.Bold),
                            color = MaterialTheme.colorScheme.onSurface,
                            maxLines = 2,
                            overflow = TextOverflow.Ellipsis
                        )
                        Text(
                            text = email.orEmpty(),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            maxLines = 2,
                            overflow = TextOverflow.Ellipsis
                        )
                    }
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(10.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    OutlinedButton(
                        enabled = !isActionLoading,
                        onClick = {
                            editingName = profile?.displayName.orEmpty()
                            isEditingName = !isEditingName
                        },
                        modifier = Modifier.weight(1f)
                    ) {
                        Icon(
                            imageVector = Icons.Rounded.Edit,
                            contentDescription = null,
                            modifier = Modifier.size(18.dp)
                        )
                        Spacer(Modifier.width(8.dp))
                        Text(stringResource(R.string.profile_edit_name), maxLines = 1, overflow = TextOverflow.Ellipsis)
                    }
                    OutlinedButton(
                        enabled = !isActionLoading,
                        onClick = onSignOut,
                        modifier = Modifier.weight(1f)
                    ) {
                        Icon(
                            imageVector = Icons.AutoMirrored.Rounded.Logout,
                            contentDescription = null,
                            modifier = Modifier.size(18.dp)
                        )
                        Spacer(Modifier.width(8.dp))
                        Text(stringResource(R.string.profile_sign_out), maxLines = 1, overflow = TextOverflow.Ellipsis)
                    }
                }

                HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.5f))

                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    StatChip(
                        icon = Icons.Rounded.Schedule,
                        label = stringResource(R.string.profile_total_time),
                        value = formatDuration(profile?.totalPlayTimeMs ?: 0L),
                        modifier = Modifier.weight(1f)
                    )
                    StatChip(
                        icon = Icons.Rounded.SportsEsports,
                        label = stringResource(R.string.profile_games_played),
                        value = (profile?.gamesPlayed ?: 0).toString(),
                        modifier = Modifier.weight(1f)
                    )
                }

                if (isEditingName) {
                    OutlinedTextField(
                        value = editingName,
                        onValueChange = { editingName = it },
                        modifier = Modifier.fillMaxWidth(),
                        singleLine = true,
                        shape = RoundedCornerShape(20.dp),
                        label = { Text(stringResource(R.string.profile_display_name)) },
                        leadingIcon = { Icon(Icons.Rounded.Edit, contentDescription = null) }
                    )
                    Row(
                        horizontalArrangement = Arrangement.spacedBy(10.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Button(
                            enabled = !isActionLoading && editingName.isNotBlank() && editingName != profile?.displayName,
                            onClick = {
                                onUpdateName(editingName)
                                isEditingName = false
                            }
                        ) {
                            Text(stringResource(R.string.save))
                        }
                        TextButton(
                            enabled = !isActionLoading,
                            onClick = {
                                editingName = profile?.displayName.orEmpty()
                                isEditingName = false
                            }
                        ) {
                            Text(stringResource(R.string.cancel))
                        }
                    }
                } else {
                    profile?.lastPlayedTitle?.takeIf { it.isNotBlank() }?.let { title ->
                        Text(
                            text = stringResource(R.string.profile_last_played_format, title),
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun ProfileOverviewSkeletonContent() {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        SkeletonBlock(
            modifier = Modifier
                .size(72.dp)
                .clip(CircleShape)
        )
        Column(
            modifier = Modifier.weight(1f),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            SkeletonBlock(
                modifier = Modifier
                    .fillMaxWidth(0.72f)
                    .height(24.dp)
                    .clip(RoundedCornerShape(10.dp))
            )
            SkeletonBlock(
                modifier = Modifier
                    .fillMaxWidth(0.9f)
                    .height(16.dp)
                    .clip(RoundedCornerShape(8.dp))
            )
        }
    }

    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        SkeletonBlock(
            modifier = Modifier
                .weight(1f)
                .height(44.dp)
                .clip(RoundedCornerShape(18.dp))
        )
        SkeletonBlock(
            modifier = Modifier
                .weight(1f)
                .height(44.dp)
                .clip(RoundedCornerShape(18.dp))
        )
    }

    HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.5f))

    Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
        SkeletonBlock(
            modifier = Modifier
                .weight(1f)
                .height(108.dp)
                .clip(RoundedCornerShape(16.dp))
        )
        SkeletonBlock(
            modifier = Modifier
                .weight(1f)
                .height(108.dp)
                .clip(RoundedCornerShape(16.dp))
        )
    }
}

@Composable
private fun ReadOnlyProfileSkeletonCard() {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(28.dp),
        color = MaterialTheme.colorScheme.surface,
        tonalElevation = 2.dp,
        border = profileCardBorder()
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            ProfileOverviewSkeletonContent()
        }
    }
}

@Composable
private fun GamePlayStatRow(
    game: PlayerGamePlayStat,
    onClick: (() -> Unit)? = null
) {
    val content: @Composable () -> Unit = {
        Row(
            modifier = Modifier.padding(12.dp),
            horizontalArrangement = Arrangement.spacedBy(12.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            GameCoverArt(
                coverPath = game.coverArtPath,
                fallbackTitle = game.title,
                modifier = Modifier
                    .size(width = 54.dp, height = 78.dp)
                    .clip(RoundedCornerShape(10.dp)),
                contentScale = ContentScale.Crop
            )
            Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(5.dp)) {
                Text(
                    text = game.title,
                    style = MaterialTheme.typography.titleSmall.copy(fontWeight = FontWeight.SemiBold),
                    color = MaterialTheme.colorScheme.onSurface,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis
                )
                Text(
                    text = stringResource(R.string.profile_game_sessions_format, game.sessions),
                    style = MaterialTheme.typography.labelMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                game.lastPlayedAtMs?.let { lastPlayed ->
                    Text(
                        text = stringResource(R.string.profile_game_last_played_format, formatDate(lastPlayed)),
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
            Text(
                text = formatDuration(game.totalPlayTimeMs),
                style = MaterialTheme.typography.titleSmall.copy(fontWeight = FontWeight.Bold),
                color = MaterialTheme.colorScheme.primary
            )
        }
    }
    if (onClick != null) {
        Surface(
            onClick = onClick,
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(18.dp),
            color = MaterialTheme.colorScheme.surface,
            tonalElevation = 1.dp,
            border = profileCardBorder(alpha = 0.48f),
            content = content
        )
    } else {
        Surface(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(18.dp),
            color = MaterialTheme.colorScheme.surface,
            tonalElevation = 1.dp,
            border = profileCardBorder(alpha = 0.48f),
            content = content
        )
    }
}

@Composable
private fun GamePlayStatSkeletonRow() {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(18.dp),
        color = MaterialTheme.colorScheme.surface,
        tonalElevation = 1.dp,
        border = profileCardBorder(alpha = 0.48f)
    ) {
        Row(
            modifier = Modifier.padding(12.dp),
            horizontalArrangement = Arrangement.spacedBy(12.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            SkeletonBlock(
                modifier = Modifier
                    .size(width = 54.dp, height = 78.dp)
                    .clip(RoundedCornerShape(10.dp))
            )
            Column(
                modifier = Modifier.weight(1f),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                SkeletonBlock(
                    modifier = Modifier
                        .fillMaxWidth(0.82f)
                        .height(18.dp)
                        .clip(RoundedCornerShape(8.dp))
                )
                SkeletonBlock(
                    modifier = Modifier
                        .fillMaxWidth(0.46f)
                        .height(14.dp)
                        .clip(RoundedCornerShape(7.dp))
                )
                SkeletonBlock(
                    modifier = Modifier
                        .fillMaxWidth(0.58f)
                        .height(12.dp)
                        .clip(RoundedCornerShape(6.dp))
                )
            }
            SkeletonBlock(
                modifier = Modifier
                    .width(44.dp)
                    .height(18.dp)
                    .clip(RoundedCornerShape(8.dp))
            )
        }
    }
}

@Composable
private fun RecentGamesCard(
    games: List<PlayerGamePlayStat>,
    onGameClick: (PlayerGamePlayStat) -> Unit
) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(28.dp),
        color = MaterialTheme.colorScheme.surface,
        tonalElevation = 2.dp,
        border = profileCardBorder()
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Text(
                text = stringResource(R.string.profile_recently_played),
                style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.Bold),
                color = MaterialTheme.colorScheme.onSurface
            )
            LazyRow(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                items(games, key = { it.gameKey }) { game ->
                    Column(
                        modifier = Modifier.width(96.dp),
                        verticalArrangement = Arrangement.spacedBy(7.dp)
                    ) {
                        Surface(
                            onClick = { onGameClick(game) },
                            shape = RoundedCornerShape(16.dp),
                            color = Color.Transparent
                        ) {
                            GameCoverArt(
                                coverPath = game.coverArtPath,
                                fallbackTitle = game.title,
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .height(136.dp)
                                    .clip(RoundedCornerShape(16.dp)),
                                contentScale = ContentScale.Crop
                            )
                        }
                        Text(
                            text = game.title,
                            style = MaterialTheme.typography.labelMedium.copy(fontWeight = FontWeight.SemiBold),
                            color = MaterialTheme.colorScheme.onSurface,
                            maxLines = 2,
                            overflow = TextOverflow.Ellipsis
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun RecentGamesSkeletonCard() {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(28.dp),
        color = MaterialTheme.colorScheme.surface,
        tonalElevation = 2.dp,
        border = profileCardBorder()
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            SkeletonBlock(
                modifier = Modifier
                    .width(148.dp)
                    .height(22.dp)
                    .clip(RoundedCornerShape(10.dp))
            )
            LazyRow(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                items(4) {
                    Column(
                        modifier = Modifier.width(96.dp),
                        verticalArrangement = Arrangement.spacedBy(7.dp)
                    ) {
                        SkeletonBlock(
                            modifier = Modifier
                                .fillMaxWidth()
                                .height(136.dp)
                                .clip(RoundedCornerShape(16.dp))
                        )
                        SkeletonBlock(
                            modifier = Modifier
                                .fillMaxWidth()
                                .height(14.dp)
                                .clip(RoundedCornerShape(7.dp))
                        )
                        SkeletonBlock(
                            modifier = Modifier
                                .fillMaxWidth(0.68f)
                                .height(12.dp)
                                .clip(RoundedCornerShape(6.dp))
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun LeaderboardRow(
    entry: PlayerLeaderboardEntry,
    currentUid: String?,
    onClick: () -> Unit
) {
    val isCurrentUser = entry.uid == currentUid
    Surface(
        onClick = onClick,
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(24.dp),
        color = if (isCurrentUser) MaterialTheme.colorScheme.primary.copy(alpha = 0.12f) else MaterialTheme.colorScheme.surface,
        tonalElevation = 2.dp,
        border = if (isCurrentUser) {
            BorderStroke(1.dp, MaterialTheme.colorScheme.primary.copy(alpha = 0.28f))
        } else {
            profileCardBorder(alpha = 0.5f)
        }
    ) {
        Row(
            modifier = Modifier.padding(14.dp),
            horizontalArrangement = Arrangement.spacedBy(12.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            RankBadge(rank = entry.rank)
            Box(
                modifier = Modifier
                    .size(48.dp)
                    .clip(CircleShape)
                    .background(MaterialTheme.colorScheme.primary.copy(alpha = 0.12f)),
                contentAlignment = Alignment.Center
            ) {
                BitmapPathImage(
                    imagePath = entry.photoURL,
                    contentDescription = entry.displayName,
                    modifier = Modifier.fillMaxSize(),
                    contentScale = ContentScale.Crop,
                    fallback = {
                        Icon(
                            imageVector = Icons.Rounded.Person,
                            contentDescription = null,
                            tint = MaterialTheme.colorScheme.primary,
                            modifier = Modifier.size(25.dp)
                        )
                    }
                )
            }
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = entry.displayName,
                    style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.Bold),
                    color = MaterialTheme.colorScheme.onSurface,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                Text(
                    text = stringResource(R.string.profile_leaderboard_games_format, entry.gamesPlayed),
                    style = MaterialTheme.typography.labelMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
            Text(
                text = formatDuration(entry.totalPlayTimeMs),
                style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.Bold),
                color = MaterialTheme.colorScheme.primary
            )
        }
    }
}

@Composable
private fun RankBadge(rank: Int) {
    val topRank = rank <= 3
    Surface(
        shape = RoundedCornerShape(18.dp),
        color = if (topRank) colorForRank(rank).copy(alpha = 0.18f) else MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.5f)
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 10.dp, vertical = 8.dp),
            horizontalArrangement = Arrangement.spacedBy(5.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            if (topRank) {
                Icon(
                    imageVector = Icons.Rounded.EmojiEvents,
                    contentDescription = null,
                    tint = colorForRank(rank),
                    modifier = Modifier.size(18.dp)
                )
            }
            Text(
                text = "#$rank",
                style = MaterialTheme.typography.labelLarge.copy(fontWeight = FontWeight.Bold),
                color = if (topRank) colorForRank(rank) else MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}


@Composable
private fun SkeletonBlock(modifier: Modifier = Modifier) {
    Box(modifier = modifier.shimmer())
}

@Composable
private fun LoadingRow(text: String) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(18.dp),
        horizontalArrangement = Arrangement.spacedBy(12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        CircularProgressIndicator(modifier = Modifier.size(22.dp), strokeWidth = 2.dp)
        Text(text = text, color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}

@Composable
private fun EmptyProfileState(text: String) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(18.dp),
        color = MaterialTheme.colorScheme.surface,
        tonalElevation = 1.dp,
        border = profileCardBorder(alpha = 0.48f)
    ) {
        Text(
            text = text,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(18.dp)
        )
    }
}

@Composable
private fun StatChip(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    label: String,
    value: String,
    modifier: Modifier = Modifier
) {
    Surface(
        modifier = modifier,
        shape = RoundedCornerShape(16.dp),
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.36f),
        border = profileCardBorder(alpha = 0.34f)
    ) {
        Column(modifier = Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Icon(icon, contentDescription = null, tint = MaterialTheme.colorScheme.primary)
            Text(text = value, style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.Bold))
            Text(text = label, style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

private fun ProfileTab.titleRes(): Int = when (this) {
    ProfileTab.Overview -> R.string.profile_tab_overview
    ProfileTab.Games -> R.string.profile_tab_games
    ProfileTab.Leaderboard -> R.string.profile_tab_leaderboard
}

private fun ProfileTab.icon() = when (this) {
    ProfileTab.Overview -> Icons.Rounded.Person
    ProfileTab.Games -> Icons.Rounded.SportsEsports
    ProfileTab.Leaderboard -> Icons.Rounded.Leaderboard
}

@Composable
private fun profileCardBorder(alpha: Float = 0.58f): BorderStroke {
    return BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant.copy(alpha = alpha))
}

private fun profileMessageRes(key: String): Int = when (key) {
    "profile_signed_in" -> R.string.profile_signed_in
    "profile_account_created" -> R.string.profile_account_created
    "profile_password_reset_sent" -> R.string.profile_password_reset_sent
    "profile_name_updated" -> R.string.profile_name_updated
    "profile_signed_out" -> R.string.profile_signed_out
    else -> R.string.profile_done
}

private tailrec fun Context.findActivity(): Activity? = when (this) {
    is Activity -> this
    is ContextWrapper -> baseContext.findActivity()
    else -> null
}

private fun colorForRank(rank: Int) = when (rank) {
    1 -> Color(0xFFFFC857)
    2 -> Color(0xFFB6C2D9)
    else -> Color(0xFFD89C64)
}

private fun formatDuration(durationMs: Long): String {
    val totalMinutes = (durationMs / 60_000L).coerceAtLeast(0L)
    val hours = totalMinutes / 60L
    val minutes = totalMinutes % 60L
    return when {
        hours > 0L -> String.format(Locale.US, "%dh %02dm", hours, minutes)
        else -> String.format(Locale.US, "%dm", minutes)
    }
}

private fun formatDate(timestampMs: Long): String {
    return DateFormat.getDateInstance(DateFormat.MEDIUM).format(Date(timestampMs))
}
