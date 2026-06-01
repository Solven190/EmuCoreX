package com.sbro.emucorex.navigation

import android.content.Intent
import android.net.Uri
import android.util.Log
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.EnterTransition
import androidx.compose.animation.ExitTransition
import androidx.compose.animation.SizeTransform
import androidx.compose.animation.core.EaseIn
import androidx.compose.animation.core.EaseOut
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.scaleOut
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.NavDestination.Companion.hasRoute
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import androidx.navigation.toRoute
import com.sbro.emucorex.R
import com.sbro.emucorex.core.BiosValidator
import com.sbro.emucorex.core.DocumentPathResolver
import com.sbro.emucorex.core.GameLaunchShortcut
import com.sbro.emucorex.core.SetupValidator
import com.sbro.emucorex.data.AppPreferences
import com.sbro.emucorex.data.GameItem
import com.sbro.emucorex.data.SaveStateRepository
import com.sbro.emucorex.ui.achievements.AccountUnlockedAchievementsScreen
import com.sbro.emucorex.ui.achievements.AchievementsHubScreen
import com.sbro.emucorex.ui.achievements.GameAchievementsScreen
import com.sbro.emucorex.ui.catalog.CatalogSearchScreen
import com.sbro.emucorex.ui.detail.GameDetailScreen
import com.sbro.emucorex.ui.emulation.EmulationScreen
import com.sbro.emucorex.ui.formats.SupportedFormatsScreen
import com.sbro.emucorex.ui.home.HomeScreen
import com.sbro.emucorex.ui.memorycards.MemoryCardManagerScreen
import com.sbro.emucorex.ui.onboarding.OnboardingScreen
import com.sbro.emucorex.ui.saves.SaveManagerScreen
import com.sbro.emucorex.ui.settings.LanguageSettingsScreen
import com.sbro.emucorex.ui.settings.PerGameSettingsManagerScreen
import com.sbro.emucorex.ui.settings.PerGameSettingsQuickEditorDialog
import com.sbro.emucorex.ui.settings.AppUpdateAvailableDialog
import com.sbro.emucorex.ui.settings.GpuDriverScreen
import com.sbro.emucorex.ui.settings.SettingsScreen
import com.sbro.emucorex.ui.settings.SettingsViewModel
import com.sbro.emucorex.ui.common.PremiumLoadingAnimation
import com.sbro.emucorex.ui.common.ProvideGamepadUiNavigation
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.ui.Alignment
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import kotlinx.serialization.Serializable

@Serializable
object HomeRoute

@Serializable
data class GameDetailRoute(val catalogGameId: Long)

@Serializable
data class EmulationRoute(
    val gamePath: String? = null,
    val saveSlot: Int? = null,
    val bootBios: Boolean = false,
    val bootSmokeProbe: Boolean = false,
    val autotestMode: Boolean = false,
    val enableEeRecompiler: Boolean? = null,
    val enableIopRecompiler: Boolean? = null,
    val enableVu0Recompiler: Boolean? = null,
    val enableVu1Recompiler: Boolean? = null,
    val enableFastmem: Boolean? = null,
    val enableMtvu: Boolean? = null,
    val renderer: Int? = null,
    val gsDumpFrames: Int? = null,
    val gsDumpDelayMs: Int? = null
)

@Serializable
data class SettingsRoute(val tab: String = "general")

@Serializable
object LanguageSettingsRoute

@Serializable
object GpuDriverSettingsRoute

@Serializable
object OnboardingRoute

@Serializable
object CatalogSearchRoute

@Serializable
object SupportedFormatsRoute

@Serializable
object ControlsEditorRoute

@Serializable
data class SaveManagerRoute(val gamePath: String? = null, val gameTitle: String? = null)

@Serializable
object MemoryCardManagerRoute

@Serializable
object AchievementsRoute

@Serializable
object AccountUnlockedAchievementsRoute

@Serializable
object GameSettingsManagerRoute

@Serializable
data class GameAchievementsRoute(val gamePath: String, val gameTitle: String? = null)

private enum class StartupDestination {
    HOME,
    ONBOARDING
}

private const val TAG = "AppNavigation"

private fun appScreenEnterTransition(): EnterTransition = appScreenPopEnterTransition()

private fun appScreenExitTransition(): ExitTransition = ExitTransition.None

private fun appScreenPopEnterTransition(): EnterTransition {
    return fadeIn(animationSpec = tween(durationMillis = 260, delayMillis = 70, easing = EaseOut)) +
        scaleIn(initialScale = 0.96f, animationSpec = tween(260, delayMillis = 70, easing = EaseOut))
}

private fun appScreenPopExitTransition(): ExitTransition {
    return fadeOut(animationSpec = tween(durationMillis = 110, easing = EaseIn)) +
        scaleOut(targetScale = 1.0f, animationSpec = tween(110, easing = EaseIn))
}

@Composable
fun AppNavigation(
    launchIntentVersion: Int = 0,
    restoredFromSavedState: Boolean = false,
    onStartupReady: () -> Unit = {}
) {
    val context = LocalContext.current
    val activity = context as? ComponentActivity
    val preferences = AppPreferences(context)
    val saveStateRepository = SaveStateRepository(context)
    var quickGameSettingsTarget by remember { mutableStateOf<GameItem?>(null) }
    LaunchedEffect(Unit) {
        onStartupReady()
    }
    val startupDestination by produceState<StartupDestination?>(
        initialValue = null,
        key1 = preferences,
        key2 = launchIntentVersion
    ) {
        val launchRequest = GameLaunchShortcut.parseLaunchRequest(activity?.intent)
        if (launchRequest?.autotestMode == true) {
            Log.i(TAG, "Startup destination forced HOME for autotest launch path=${launchRequest.gamePath}")
            value = StartupDestination.HOME
            return@produceState
        }
        value = combine(
            preferences.onboardingCompleted,
            preferences.biosPath,
            preferences.gamePath
        ) { onboardingCompleted, biosPath, gamePath ->
            val hasUsableBios = BiosValidator.hasUsableBiosFiles(context, biosPath)
            val hasGameFolder = SetupValidator.isGameFolderPresentForStartup(context, gamePath)
            val shouldOpenHome = onboardingCompleted && hasUsableBios && hasGameFolder
            Log.i(
                TAG,
                "Startup destination onboarding=$onboardingCompleted bios=$hasUsableBios gameFolder=$hasGameFolder launch=${launchRequest != null}"
            )
            if (shouldOpenHome) StartupDestination.HOME else StartupDestination.ONBOARDING
        }.first()
    }
    if (startupDestination == null) {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(MaterialTheme.colorScheme.background),
            contentAlignment = Alignment.Center
        )
        {
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(24.dp)
            ) {
                PremiumLoadingAnimation(size = 80.dp)
            }
        }
        return
    }

    val navController = rememberNavController()
    val currentBackStackEntry by navController.currentBackStackEntryAsState()
    val settingsViewModel: SettingsViewModel = viewModel()
    val settingsUiState by settingsViewModel.uiState.collectAsState()
    val scope = rememberCoroutineScope()
    var homeDrawerEnabled by remember { mutableStateOf(true) }
    var blockRestoredEmulationRoute by remember(restoredFromSavedState) {
        mutableStateOf(restoredFromSavedState)
    }
    val unsupportedGameImageMessage = stringResource(R.string.shell_launch_game_unsupported)
    val continueUnavailableMessage = stringResource(R.string.home_game_menu_continue_unavailable)
    LaunchedEffect(currentBackStackEntry?.destination, restoredFromSavedState) {
        if (!restoredFromSavedState) return@LaunchedEffect
        val destination = currentBackStackEntry?.destination ?: return@LaunchedEffect
        if (!destination.hasRoute<EmulationRoute>()) {
            blockRestoredEmulationRoute = false
        }
    }
    val launchGamePicker = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenDocument()
    ) { uri: Uri? ->
        uri ?: return@rememberLauncherForActivityResult
        runCatching {
            context.contentResolver.takePersistableUriPermission(
                uri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION
            )
        }
        val rawPath = uri.toString()
        val displayName = DocumentPathResolver.getDisplayName(context, rawPath)
        if (!isSupportedGameImage(displayName)) {
            Toast.makeText(context, unsupportedGameImageMessage, Toast.LENGTH_SHORT).show()
            return@rememberLauncherForActivityResult
        }
        navController.navigate(EmulationRoute(gamePath = rawPath)) {
            launchSingleTop = true
        }
    }
    val startDestination = when (startupDestination) {
        StartupDestination.ONBOARDING -> OnboardingRoute
        StartupDestination.HOME -> HomeRoute
        else -> {}
    }
    val navigateCheats: () -> Unit = {
        navController.navigate(SettingsRoute(tab = "cheats")) {
            launchSingleTop = true
        }
    }
    val navigateGameSettingsManager: () -> Unit = {
        navController.navigate(GameSettingsManagerRoute) {
            launchSingleTop = true
        }
    }
    val navigateDataTransfer: () -> Unit = {
        navController.navigate(SettingsRoute(tab = "data_transfer")) {
            launchSingleTop = true
        }
    }
    val navigateMemoryCardManager: () -> Unit = {
        navController.navigate(MemoryCardManagerRoute) {
            launchSingleTop = true
        }
    }
    val launchGamePickerAction: () -> Unit = {
        launchGamePicker.launch(arrayOf("*/*"))
    }
    val resetAllSettingsAndOpenOnboarding: () -> Unit = {
        scope.launch {
            preferences.resetAllSettings()
            navController.navigate(OnboardingRoute) {
                launchSingleTop = true
                popUpTo(navController.graph.id) { inclusive = true }
            }
        }
    }
    val canShowStartupUpdateDialog = currentBackStackEntry?.destination?.hasRoute<HomeRoute>() == true
    LaunchedEffect(startupDestination) {
        if (startupDestination == StartupDestination.HOME) {
            settingsViewModel.checkForStartupAppUpdates()
        }
    }
    LaunchedEffect(currentBackStackEntry?.destination, settingsUiState.appUpdate.startupDialogVisible) {
        val destination = currentBackStackEntry?.destination ?: return@LaunchedEffect
        if (settingsUiState.appUpdate.startupDialogVisible && !destination.hasRoute<HomeRoute>()) {
            settingsViewModel.dismissStartupUpdateDialog()
        }
    }
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(MaterialTheme.colorScheme.background)
    ) {
        ProvideGamepadUiNavigation(
            enabled = true,
            onBack = {
                if (navController.previousBackStackEntry != null) {
                    navController.popBackStack()
                } else {
                    false
                }
            }
        )
        NavHost(
            navController = navController,
            startDestination = startDestination,
            modifier = Modifier
                .fillMaxSize()
                .background(MaterialTheme.colorScheme.background),
            enterTransition = { appScreenEnterTransition() },
            exitTransition = { appScreenExitTransition() },
            popEnterTransition = { appScreenPopEnterTransition() },
            popExitTransition = { appScreenPopExitTransition() },
            sizeTransform = { SizeTransform(clip = false) }
        ) {
            composable<OnboardingRoute> {
                OnboardingScreen(
                    onComplete = {
                        navController.navigate(HomeRoute) {
                            popUpTo(OnboardingRoute) { inclusive = true }
                        }
                    }
                )
            }

            composable<HomeRoute> {
                AdaptiveShell(
                    selected = PrimaryDestination.Home,
                    drawerEnabled = homeDrawerEnabled,
                    onNavigateHome = { },
                    onNavigateSearch = {
                        navController.navigate(CatalogSearchRoute) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateFormats = {
                        navController.navigate(SupportedFormatsRoute) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateSettings = {
                        navController.navigate(SettingsRoute()) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateAchievements = {
                        navController.navigate(AchievementsRoute) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateCheats = navigateCheats,
                    onNavigateGameSettingsManager = navigateGameSettingsManager,
                    onNavigateDataTransfer = navigateDataTransfer,
                    onResetAllSettings = resetAllSettingsAndOpenOnboarding,
                    onNavigateSaveManager = {
                        navController.navigate(SaveManagerRoute()) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateMemoryCardManager = navigateMemoryCardManager,
                    onOpenManageFolders = {
                        navController.navigate(SettingsRoute(tab = "paths")) {
                            launchSingleTop = true
                        }
                    },
                    onLaunchGame = launchGamePickerAction,
                    onLaunchBios = {
                        navController.navigate(EmulationRoute(bootBios = true)) {
                            launchSingleTop = true
                        }
                    }
                ) { openDrawer ->
                    HomeScreen(
                        onGameClick = { game ->
                            navController.navigate(EmulationRoute(gamePath = game.path)) {
                                launchSingleTop = true
                            }
                        },
                        onContinueGame = { game ->
                            val saveSlot = saveStateRepository.findLatestSlot(game.path)
                            if (saveSlot == null) {
                                Toast.makeText(context, continueUnavailableMessage, Toast.LENGTH_SHORT).show()
                                return@HomeScreen
                            }
                            navController.navigate(EmulationRoute(gamePath = game.path, saveSlot = saveSlot)) {
                                launchSingleTop = true
                            }
                        },
                        onLoadSaveClick = { game ->
                            navController.navigate(SaveManagerRoute(gamePath = game.path, gameTitle = game.title)) {
                                launchSingleTop = true
                            }
                        },
                        onManageGameClick = { game ->
                            quickGameSettingsTarget = game
                        },
                        onCreateShortcutClick = { game ->
                            GameLaunchShortcut.requestPinnedShortcut(context, game)
                        },
                        onMenuClick = openDrawer,
                        onShelfModeChanged = { isShelfMode ->
                            homeDrawerEnabled = !isShelfMode
                        }
                    )
                }
            }

            composable<GameDetailRoute> { backStackEntry ->
                val route = backStackEntry.toRoute<GameDetailRoute>()
                GameDetailScreen(
                    catalogGameId = route.catalogGameId,
                    onBackClick = { navController.popBackStack() }
                )
            }

            composable<CatalogSearchRoute> {
                AdaptiveShell(
                    selected = PrimaryDestination.Search,
                    onNavigateHome = {
                        navController.navigate(HomeRoute) {
                            launchSingleTop = true
                            popUpTo(HomeRoute) { inclusive = false }
                        }
                    },
                    onNavigateSearch = { },
                    onNavigateFormats = {
                        navController.navigate(SupportedFormatsRoute) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateSettings = {
                        navController.navigate(SettingsRoute()) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateAchievements = {
                        navController.navigate(AchievementsRoute) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateCheats = navigateCheats,
                    onNavigateGameSettingsManager = navigateGameSettingsManager,
                    onNavigateDataTransfer = navigateDataTransfer,
                    onResetAllSettings = resetAllSettingsAndOpenOnboarding,
                    onNavigateSaveManager = {
                        navController.navigate(SaveManagerRoute()) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateMemoryCardManager = navigateMemoryCardManager,
                    onBackClick = { navController.popBackStack() },
                    onOpenManageFolders = {
                        navController.navigate(SettingsRoute(tab = "paths")) {
                            launchSingleTop = true
                        }
                    },
                    onLaunchGame = launchGamePickerAction
                ) {
                    CatalogSearchScreen(
                        onGameClick = { igdbId ->
                            navController.navigate(GameDetailRoute(catalogGameId = igdbId)) {
                                launchSingleTop = true
                            }
                        },
                        onBackClick = { navController.popBackStack() }
                    )
                }
            }

            composable<EmulationRoute> { backStackEntry ->
                val route = backStackEntry.toRoute<EmulationRoute>()
                EmulationScreen(
                    gamePath = route.gamePath,
                    bootToBios = route.bootBios,
                    bootSmokeProbe = route.bootSmokeProbe,
                    saveSlot = route.saveSlot,
                    autotestMode = route.autotestMode,
                    enableEeRecompilerOverride = route.enableEeRecompiler,
                    enableIopRecompilerOverride = route.enableIopRecompiler,
                    enableVu0RecompilerOverride = route.enableVu0Recompiler,
                    enableVu1RecompilerOverride = route.enableVu1Recompiler,
                    enableFastmemOverride = route.enableFastmem,
                    enableMtvuOverride = route.enableMtvu,
                    rendererOverride = route.renderer,
                    gsDumpFrames = route.gsDumpFrames,
                    gsDumpDelayMs = route.gsDumpDelayMs,
                    restoredAfterProcessDeath = blockRestoredEmulationRoute,
                    onExit = {
                        if (!navController.popBackStack(HomeRoute, inclusive = false)) {
                            navController.navigate(HomeRoute) {
                                launchSingleTop = true
                                popUpTo(navController.graph.startDestinationId) { inclusive = false }
                            }
                        }
                    }
                )
            }

            composable<SupportedFormatsRoute> {
                AdaptiveShell(
                    selected = PrimaryDestination.Formats,
                    onNavigateHome = {
                        navController.navigate(HomeRoute) {
                            launchSingleTop = true
                            popUpTo(HomeRoute) { inclusive = false }
                        }
                    },
                    onNavigateSearch = {
                        navController.navigate(CatalogSearchRoute) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateFormats = { },
                    onNavigateSettings = {
                        navController.navigate(SettingsRoute()) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateAchievements = {
                        navController.navigate(AchievementsRoute) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateCheats = navigateCheats,
                    onNavigateGameSettingsManager = navigateGameSettingsManager,
                    onNavigateDataTransfer = navigateDataTransfer,
                    onResetAllSettings = resetAllSettingsAndOpenOnboarding,
                    onNavigateSaveManager = {
                        navController.navigate(SaveManagerRoute()) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateMemoryCardManager = navigateMemoryCardManager,
                    onBackClick = { navController.popBackStack() },
                    onOpenManageFolders = {
                        navController.navigate(SettingsRoute(tab = "paths")) {
                            launchSingleTop = true
                        }
                    },
                    onLaunchGame = launchGamePickerAction
                ) {
                    SupportedFormatsScreen(
                        onBackClick = { navController.popBackStack() }
                    )
                }
            }

            composable<SettingsRoute> { backStackEntry ->
                val route = backStackEntry.toRoute<SettingsRoute>()
                AdaptiveShell(
                    selected = PrimaryDestination.Settings,
                    onNavigateHome = {
                        navController.navigate(HomeRoute) {
                            launchSingleTop = true
                            popUpTo(HomeRoute) { inclusive = false }
                        }
                    },
                    onNavigateSearch = {
                        navController.navigate(CatalogSearchRoute) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateFormats = {
                        navController.navigate(SupportedFormatsRoute) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateSettings = { },
                    onNavigateAchievements = {
                        navController.navigate(AchievementsRoute) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateCheats = navigateCheats,
                    onNavigateGameSettingsManager = navigateGameSettingsManager,
                    onNavigateDataTransfer = navigateDataTransfer,
                    onResetAllSettings = resetAllSettingsAndOpenOnboarding,
                    onNavigateSaveManager = {
                        navController.navigate(SaveManagerRoute()) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateMemoryCardManager = navigateMemoryCardManager,
                    onBackClick = { navController.popBackStack() },
                    onOpenManageFolders = {
                        navController.navigate(SettingsRoute(tab = "paths")) {
                            launchSingleTop = true
                        }
                    },
                    onLaunchGame = launchGamePickerAction
                ) {
                    SettingsScreen(
                        initialTab = route.tab,
                        onBackClick = { navController.popBackStack() },
                        onOpenMemoryCardManager = navigateMemoryCardManager,
                        onOpenGpuDriverManager = {
                            navController.navigate(GpuDriverSettingsRoute) {
                                launchSingleTop = true
                            }
                        },
                        onOpenLanguageScreen = {
                            navController.navigate(LanguageSettingsRoute) {
                                launchSingleTop = true
                            }
                        },
                        viewModel = settingsViewModel
                    )
                }
            }

            composable<LanguageSettingsRoute> {
                LanguageSettingsScreen(
                    onBackClick = { navController.popBackStack() }
                )
            }

            composable<GpuDriverSettingsRoute> {
                GpuDriverScreen(
                    onBackClick = { navController.popBackStack() },
                    viewModel = settingsViewModel
                )
            }

            composable<GameSettingsManagerRoute> {
                PerGameSettingsManagerScreen(
                    onBackClick = { navController.popBackStack() }
                )
            }

            composable<AchievementsRoute> {
                AdaptiveShell(
                    selected = PrimaryDestination.Achievements,
                    onNavigateHome = {
                        navController.navigate(HomeRoute) {
                            launchSingleTop = true
                            popUpTo(HomeRoute) { inclusive = false }
                        }
                    },
                    onNavigateSearch = {
                        navController.navigate(CatalogSearchRoute) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateFormats = {
                        navController.navigate(SupportedFormatsRoute) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateSettings = {
                        navController.navigate(SettingsRoute()) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateAchievements = { },
                    onNavigateCheats = navigateCheats,
                    onNavigateGameSettingsManager = navigateGameSettingsManager,
                    onNavigateDataTransfer = navigateDataTransfer,
                    onResetAllSettings = resetAllSettingsAndOpenOnboarding,
                    onNavigateSaveManager = {
                        navController.navigate(SaveManagerRoute()) {
                            launchSingleTop = true
                        }
                    },
                    onNavigateMemoryCardManager = navigateMemoryCardManager,
                    onBackClick = { navController.popBackStack() },
                    onOpenManageFolders = {
                        navController.navigate(SettingsRoute(tab = "paths")) {
                            launchSingleTop = true
                        }
                    },
                    onLaunchGame = launchGamePickerAction
                ) {
                    AchievementsHubScreen(
                        onOpenGameAchievements = { path, title ->
                            navController.navigate(GameAchievementsRoute(gamePath = path, gameTitle = title)) {
                                launchSingleTop = true
                            }
                        },
                        onOpenUnlockedAchievements = {
                            navController.navigate(AccountUnlockedAchievementsRoute) {
                                launchSingleTop = true
                            }
                        },
                        onBackClick = { navController.popBackStack() }
                    )
                }
            }

            composable<AccountUnlockedAchievementsRoute> {
                AccountUnlockedAchievementsScreen(
                    onOpenGameAchievements = { path, title ->
                        navController.navigate(GameAchievementsRoute(gamePath = path, gameTitle = title)) {
                            launchSingleTop = true
                        }
                    },
                    onBackClick = { navController.popBackStack() }
                )
            }

            composable<GameAchievementsRoute> { backStackEntry ->
                val route = backStackEntry.toRoute<GameAchievementsRoute>()
                GameAchievementsScreen(
                    gamePath = route.gamePath,
                    gameTitle = route.gameTitle,
                    onBackClick = { navController.popBackStack() }
                )
            }

            composable<SaveManagerRoute> { backStackEntry ->
                val route = backStackEntry.toRoute<SaveManagerRoute>()
                SaveManagerScreen(
                    gamePath = route.gamePath,
                    gameTitle = route.gameTitle,
                    onLoadClick = { path, slot ->
                        navController.navigate(EmulationRoute(gamePath = path, saveSlot = slot)) {
                            launchSingleTop = true
                        }
                    },
                    onBackClick = { navController.popBackStack() }
                )
            }

            composable<MemoryCardManagerRoute> {
                MemoryCardManagerScreen(
                    onBackClick = { navController.popBackStack() }
                )
            }
        }

        LaunchedEffect(navController, startupDestination, launchIntentVersion) {
            val launchRequest = GameLaunchShortcut.parseLaunchRequest(activity?.intent) ?: return@LaunchedEffect
            Log.i(
                TAG,
                "Handling launch request destination=$startupDestination path=${launchRequest.gamePath} bios=${launchRequest.bootBios} autotest=${launchRequest.autotestMode}"
            )
            if (startupDestination != StartupDestination.HOME) return@LaunchedEffect
            navController.navigate(
                EmulationRoute(
                    gamePath = launchRequest.gamePath,
                    saveSlot = launchRequest.saveSlot,
                    bootBios = launchRequest.bootBios,
                    bootSmokeProbe = launchRequest.bootSmokeProbe,
                    autotestMode = launchRequest.autotestMode,
                    enableEeRecompiler = launchRequest.enableEeRecompiler,
                    enableIopRecompiler = launchRequest.enableIopRecompiler,
                    enableVu0Recompiler = launchRequest.enableVu0Recompiler,
                    enableVu1Recompiler = launchRequest.enableVu1Recompiler,
                    enableFastmem = launchRequest.enableFastmem,
                    enableMtvu = launchRequest.enableMtvu,
                    renderer = launchRequest.renderer,
                    gsDumpFrames = launchRequest.gsDumpFrames,
                    gsDumpDelayMs = launchRequest.gsDumpDelayMs
                )
            ) {
                launchSingleTop = true
            }
            GameLaunchShortcut.clearLaunchRequest(activity?.intent)
        }

        quickGameSettingsTarget?.let { game ->
            PerGameSettingsQuickEditorDialog(
                game = game,
                onDismiss = { quickGameSettingsTarget = null }
            )
        }

        val startupUpdateRelease = settingsUiState.appUpdate.latestRelease
        if (
            canShowStartupUpdateDialog &&
            settingsUiState.appUpdate.startupDialogVisible &&
            startupUpdateRelease != null
        ) {
            AppUpdateAvailableDialog(
                release = startupUpdateRelease,
                onDismiss = settingsViewModel::dismissStartupUpdateDialog,
                onSkipUpdate = settingsViewModel::skipStartupUpdateDialog,
                onOpenUpdates = {
                    settingsViewModel.dismissStartupUpdateDialog()
                    navController.navigate(SettingsRoute(tab = "updates")) {
                        launchSingleTop = true
                    }
                }
            )
        }
    }
}

private fun isSupportedGameImage(fileName: String): Boolean {
    val extension = fileName.substringAfterLast('.', "").lowercase()
    return extension in setOf("iso", "bin", "chd", "cso", "gz", "elf")
}
