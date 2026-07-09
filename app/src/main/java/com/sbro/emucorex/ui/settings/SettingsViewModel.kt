package com.sbro.emucorex.ui.settings

import android.app.Activity
import android.app.Application
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.sbro.emucorex.core.AndroidTouchHaptics
import com.sbro.emucorex.core.AppUpdateRelease
import com.sbro.emucorex.core.AppUpdateRepository
import com.sbro.emucorex.core.DocumentPathResolver
import com.sbro.emucorex.core.EmulatorBridge
import com.sbro.emucorex.core.EmulatorStorage
import com.sbro.emucorex.core.GpuHardwareProfiles
import com.sbro.emucorex.core.GamepadManager
import com.sbro.emucorex.core.GsHackDefaults
import com.sbro.emucorex.core.PerformanceProfiles
import com.sbro.emucorex.core.PerformancePresets
import com.sbro.emucorex.core.ProPurchaseManager
import com.sbro.emucorex.core.SetupValidator
import com.sbro.emucorex.core.StorageAccess
import com.sbro.emucorex.core.normalizeUpscale
import com.sbro.emucorex.data.AppPreferences
import com.sbro.emucorex.data.AppPreferences.Companion.FPS_OVERLAY_MODE_DETAILED
import com.sbro.emucorex.data.CoverArtRepository
import com.sbro.emucorex.data.SettingsSnapshot
import com.sbro.emucorex.ui.theme.ThemeMode
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.withContext
import com.sbro.emucorex.core.GpuDriverCatalogRepository
import com.sbro.emucorex.core.GpuDriverManager
import com.sbro.emucorex.core.InstalledGpuDriver
import com.sbro.emucorex.core.RemoteGpuDriver
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

data class SettingsUiState(
    val isLoaded: Boolean = false,
    val themeMode: ThemeMode = ThemeMode.SYSTEM,
    val isProUnlocked: Boolean = false,
    val proPrice: String? = null,
    val isProProductLoading: Boolean = false,
    val isProProductAvailable: Boolean = false,
    val isProPurchaseInProgress: Boolean = false,
    val proPurchaseMessageResId: Int? = null,
    val languageTag: String? = null,
    val renderer: Int = EmulatorBridge.DEFAULT_RENDERER,
    val upscaleMultiplier: Float = 1f,
    val aspectRatio: Int = 1,
    val autoProgressiveScan: Boolean = false,
    val padVibration: Boolean = true,
    val padVibrationStrength: Int = AppPreferences.DEFAULT_PAD_VIBRATION_STRENGTH,
    val padVibrationFallback: Boolean = true,
    val showFps: Boolean = true,
    val fpsOverlayMode: Int = FPS_OVERLAY_MODE_DETAILED,
    val fpsOverlayCorner: Int = AppPreferences.FPS_OVERLAY_CORNER_TOP_RIGHT,
    val confirmSaveLoadActions: Boolean = true,
    val compactControls: Boolean = true,
    val keepScreenOn: Boolean = true,
    val showRecentGames: Boolean = true,
    val showHomeSearch: Boolean = false,
    val showDebugOptions: Boolean = false,
    val preferEnglishGameTitles: Boolean = false,
    val biosPath: String? = null,
    val gamePath: String? = null,
    val emulatorDataPath: String? = null,
    val coverDownloadBaseUrl: String? = null,
    val coverArtStyle: Int = AppPreferences.COVER_ART_STYLE_DEFAULT,
    val biosValid: Boolean = false,
    val setupComplete: Boolean = false,
    val appVersion: String = "1.0.0",
    val performanceProfile: Int = PerformanceProfiles.SAFE,
    // Extended settings
    val eeCycleRate: Int = 0,
    val eeCycleSkip: Int = 0,
    val enableEeRecompiler: Boolean = true,
    val enableIopRecompiler: Boolean = true,
    val enableVu0Recompiler: Boolean = true,
    val enableVu1Recompiler: Boolean = true,
    val eeFpuRoundMode: Int = AppPreferences.DEFAULT_EE_FPU_ROUND_MODE,
    val vu0RoundMode: Int = AppPreferences.DEFAULT_VU_ROUND_MODE,
    val vu1RoundMode: Int = AppPreferences.DEFAULT_VU_ROUND_MODE,
    val eeFpuClampingMode: Int = AppPreferences.DEFAULT_EE_FPU_CLAMPING_MODE,
    val vu0ClampingMode: Int = AppPreferences.DEFAULT_VU0_CLAMPING_MODE,
    val vu1ClampingMode: Int = AppPreferences.DEFAULT_VU1_CLAMPING_MODE,
    val enableGameFixes: Boolean = true,
    val enableWaitLoopSpeedhack: Boolean = true,
    val enableIntcStatSpeedhack: Boolean = true,
    val enableVuFlagHack: Boolean = true,
    val enableInstantVu1: Boolean = true,
    val enableMtvu: Boolean = true,
    val enableThreadPinning: Boolean = false,
    val enableFastBoot: Boolean = true,
    val enableFastCdvd: Boolean = false,
    val enableCheats: Boolean = false,
    val hwDownloadMode: Int = 0,
    val frameSkip: Int = 0,
    val skipDuplicateFrames: Boolean = true,
    val textureFiltering: Int = GsHackDefaults.BILINEAR_FILTERING_DEFAULT,
    val trilinearFiltering: Int = GsHackDefaults.TRILINEAR_FILTERING_DEFAULT,
    val blendingAccuracy: Int = GsHackDefaults.BLENDING_ACCURACY_DEFAULT,
    val texturePreloading: Int = GsHackDefaults.TEXTURE_PRELOADING_DEFAULT,
    val enableFxaa: Boolean = false,
    val casMode: Int = 0,
    val casSharpness: Int = 50,
    val tvShader: Int = GsHackDefaults.TV_SHADER_DEFAULT,
    val shadeBoostEnabled: Boolean = false,
    val shadeBoostBrightness: Int = 50,
    val shadeBoostContrast: Int = 50,
    val shadeBoostSaturation: Int = 50,
    val shadeBoostGamma: Int = 50,
    val enableWidescreenPatches: Boolean = false,
    val enableNoInterlacingPatches: Boolean = false,
    val anisotropicFiltering: Int = 0,
    val enableHwMipmapping: Boolean = GsHackDefaults.HW_MIPMAPPING_DEFAULT,
    val antiBlur: Boolean = GsHackDefaults.ANTI_BLUR_DEFAULT,
    val cpuSpriteRenderSize: Int = GsHackDefaults.CPU_SPRITE_RENDER_SIZE_DEFAULT,
    val cpuSpriteRenderLevel: Int = GsHackDefaults.CPU_SPRITE_RENDER_LEVEL_DEFAULT,
    val softwareClutRender: Int = GsHackDefaults.SOFTWARE_CLUT_RENDER_DEFAULT,
    val gpuTargetClutMode: Int = GsHackDefaults.GPU_TARGET_CLUT_DEFAULT,
    val skipDrawStart: Int = 0,
    val skipDrawEnd: Int = 0,
    val autoFlushHardware: Int = GsHackDefaults.AUTO_FLUSH_DEFAULT,
    val cpuFramebufferConversion: Boolean = false,
    val disableDepthConversion: Boolean = false,
    val disableSafeFeatures: Boolean = false,
    val disableRenderFixes: Boolean = false,
    val preloadFrameData: Boolean = false,
    val disablePartialInvalidation: Boolean = false,
    val textureInsideRt: Int = GsHackDefaults.TEXTURE_INSIDE_RT_DEFAULT,
    val readTargetsOnClose: Boolean = false,
    val estimateTextureRegion: Boolean = false,
    val gpuPaletteConversion: Boolean = false,
    val halfPixelOffset: Int = GsHackDefaults.HALF_PIXEL_OFFSET_DEFAULT,
    val nativeScaling: Int = GsHackDefaults.NATIVE_SCALING_DEFAULT,
    val roundSprite: Int = GsHackDefaults.ROUND_SPRITE_DEFAULT,
    val bilinearUpscale: Int = GsHackDefaults.BILINEAR_UPSCALE_DEFAULT,
    val textureOffsetX: Int = 0,
    val textureOffsetY: Int = 0,
    val alignSprite: Boolean = false,
    val mergeSprite: Boolean = false,
    val forceEvenSpritePosition: Boolean = false,
    val nativePaletteDraw: Boolean = false,
    val performancePreset: Int = PerformancePresets.CUSTOM,
    // Overlay
    val overlayScale: Int = 100,
    val overlayOpacity: Int = 80,
    val overlayShow: Boolean = true,
    val racingMode: Boolean = false,
    val touchHaptics: Boolean = false,
    val touchHapticsStrength: Int = AppPreferences.DEFAULT_TOUCH_HAPTICS_STRENGTH,
    val leftStickSensitivity: Int = AppPreferences.DEFAULT_STICK_SENSITIVITY,
    val rightStickSensitivity: Int = AppPreferences.DEFAULT_STICK_SENSITIVITY,
    val invertLeftStick: Boolean = false,
    val invertRightStick: Boolean = false,
    val invertLeftStickHorizontal: Boolean = false,
    val invertRightStickHorizontal: Boolean = false,
    // Gamepad
    val enableAutoGamepad: Boolean = true,
    val hideOverlayOnGamepad: Boolean = true,
    val gamepadStickDeadzone: Int = AppPreferences.DEFAULT_GAMEPAD_STICK_DEADZONE,
    val gamepadLeftStickSensitivity: Int = AppPreferences.DEFAULT_GAMEPAD_STICK_SENSITIVITY,
    val gamepadRightStickSensitivity: Int = AppPreferences.DEFAULT_GAMEPAD_STICK_SENSITIVITY,
    val gamepadRightStickUpToR2: Boolean = false,
    val gamepadRightStickDownToL2: Boolean = false,
    val gamepadButtonHaptics: Boolean = false,
    val gamepadBindings: Map<String, Int> = emptyMap(),
    val gamepadBindingsByPad: Map<Int, Map<String, Int>> = emptyMap(),
    val gpuDriverType: Int = 0,
    val mediatekAngleOpenGl: Boolean = false,
    val customDriverPath: String? = null,
    val installedGpuDrivers: List<InstalledGpuDriver> = emptyList(),
    val remoteGpuDrivers: List<RemoteGpuDriver> = emptyList(),
    val gpuDriverCatalogLoading: Boolean = false,
    val gpuDriverCatalogError: String? = null,
    val gpuDriverDownloads: Map<String, Float> = emptyMap(),
    val appUpdate: AppUpdateUiState = AppUpdateUiState(),
    val frameLimitEnabled: Boolean = true,
    val vSyncEnabled: Boolean = false,
    val fastForwardSpeed: Float = AppPreferences.DEFAULT_FAST_FORWARD_SPEED,
    val targetFps: Int = 0,
    val ntscFramerate: Float = AppPreferences.DEFAULT_NTSC_FRAMERATE,
    val palFramerate: Float = AppPreferences.DEFAULT_PAL_FRAMERATE
)

data class AppUpdateUiState(
    val releaseHistory: List<AppUpdateRelease> = emptyList(),
    val historyLoading: Boolean = false,
    val historyErrorMessage: String? = null
)

class SettingsViewModel(application: Application) : AndroidViewModel(application) {

    private val preferences = AppPreferences(application)
    private val appUpdateRepository = AppUpdateRepository(application)
    private val gpuDriverManager = GpuDriverManager(application)
    private val gpuDriverCatalogRepository = GpuDriverCatalogRepository(application)
    private val proPurchaseManager = ProPurchaseManager.getInstance(application)
    private val _uiState = MutableStateFlow(SettingsUiState())
    val uiState: StateFlow<SettingsUiState> = _uiState.asStateFlow()

    init {
        refreshInstalledGpuDrivers()
        viewModelScope.launch {
            preferences.cleanupLegacyClampingPreferencesIfNeeded()
            preferences.settingsSnapshot.collect { snapshot ->
                applySettingsSnapshot(snapshot)
            }
        }

                viewModelScope.launch {
            proPurchaseManager.state.collect { proState ->
                _uiState.value = _uiState.value.copy(
                    isProUnlocked = proState.isProUnlocked,
                    proPrice = proState.productPrice,
                    isProProductLoading = proState.isProductLoading,
                    isProProductAvailable = proState.isProductAvailable,
                    isProPurchaseInProgress = proState.isPurchaseInProgress,
                    proPurchaseMessageResId = proState.messageResId
                )
            }
        }

        try {
            val pInfo = application.packageManager.getPackageInfo(application.packageName, 0)
            _uiState.value = _uiState.value.copy(appVersion = pInfo.versionName ?: "1.0.0")
        } catch (_: Exception) { }
    }

    private fun applySettingsSnapshot(snapshot: SettingsSnapshot) {
        _uiState.value = _uiState.value.copy(
            isLoaded = true,
            themeMode = snapshot.themeMode,
            isProUnlocked = snapshot.proUnlocked,
            languageTag = snapshot.languageTag,
            renderer = snapshot.renderer,
            upscaleMultiplier = snapshot.upscaleMultiplier,
            aspectRatio = snapshot.aspectRatio,
            autoProgressiveScan = snapshot.autoProgressiveScan,
            padVibration = snapshot.padVibration,
            padVibrationStrength = snapshot.padVibrationStrength,
            padVibrationFallback = snapshot.padVibrationFallback,
            showFps = snapshot.showFps,
            fpsOverlayMode = snapshot.fpsOverlayMode,
            fpsOverlayCorner = snapshot.fpsOverlayCorner,
            confirmSaveLoadActions = snapshot.confirmSaveLoadActions,
            compactControls = snapshot.compactControls,
            keepScreenOn = snapshot.keepScreenOn,
            showRecentGames = snapshot.showRecentGames,
            showHomeSearch = snapshot.showHomeSearch,
            showDebugOptions = snapshot.showDebugOptions,
            preferEnglishGameTitles = snapshot.preferEnglishGameTitles,
            biosPath = snapshot.biosPath,
            gamePath = snapshot.gamePath,
            emulatorDataPath = snapshot.emulatorDataPath,
            coverDownloadBaseUrl = snapshot.coverDownloadBaseUrl,
            coverArtStyle = snapshot.coverArtStyle,
            biosValid = snapshot.biosValid,
            setupComplete = snapshot.setupComplete,
            performanceProfile = snapshot.performanceProfile,
            eeCycleRate = snapshot.eeCycleRate,
            eeCycleSkip = snapshot.eeCycleSkip,
            enableEeRecompiler = snapshot.enableEeRecompiler,
            enableIopRecompiler = snapshot.enableIopRecompiler,
            enableVu0Recompiler = snapshot.enableVu0Recompiler,
            enableVu1Recompiler = snapshot.enableVu1Recompiler,
            eeFpuRoundMode = snapshot.eeFpuRoundMode,
            vu0RoundMode = snapshot.vu0RoundMode,
            vu1RoundMode = snapshot.vu1RoundMode,
            eeFpuClampingMode = snapshot.eeFpuClampingMode,
            vu0ClampingMode = snapshot.vu0ClampingMode,
            vu1ClampingMode = snapshot.vu1ClampingMode,
            enableGameFixes = snapshot.enableGameFixes,
            enableWaitLoopSpeedhack = snapshot.enableWaitLoopSpeedhack,
            enableIntcStatSpeedhack = snapshot.enableIntcStatSpeedhack,
            enableVuFlagHack = snapshot.enableVuFlagHack,
            enableInstantVu1 = snapshot.enableInstantVu1,
            enableMtvu = snapshot.enableMtvu,
            enableThreadPinning = snapshot.enableThreadPinning,
            enableFastBoot = snapshot.enableFastBoot,
            enableFastCdvd = snapshot.enableFastCdvd,
            enableCheats = snapshot.enableCheats,
            hwDownloadMode = snapshot.hwDownloadMode,
            frameSkip = snapshot.frameSkip,
            skipDuplicateFrames = snapshot.skipDuplicateFrames,
            textureFiltering = snapshot.textureFiltering,
            trilinearFiltering = snapshot.trilinearFiltering,
            blendingAccuracy = snapshot.blendingAccuracy,
            texturePreloading = snapshot.texturePreloading,
            enableFxaa = snapshot.enableFxaa,
            casMode = snapshot.casMode,
            casSharpness = snapshot.casSharpness,
            tvShader = snapshot.tvShader,
            shadeBoostEnabled = snapshot.shadeBoostEnabled,
            shadeBoostBrightness = snapshot.shadeBoostBrightness,
            shadeBoostContrast = snapshot.shadeBoostContrast,
            shadeBoostSaturation = snapshot.shadeBoostSaturation,
            shadeBoostGamma = snapshot.shadeBoostGamma,
            enableWidescreenPatches = snapshot.enableWidescreenPatches,
            enableNoInterlacingPatches = snapshot.enableNoInterlacingPatches,
            anisotropicFiltering = snapshot.anisotropicFiltering,
            enableHwMipmapping = snapshot.enableHwMipmapping,
            antiBlur = snapshot.antiBlur,
            cpuSpriteRenderSize = snapshot.cpuSpriteRenderSize,
            cpuSpriteRenderLevel = snapshot.cpuSpriteRenderLevel,
            softwareClutRender = snapshot.softwareClutRender,
            gpuTargetClutMode = snapshot.gpuTargetClutMode,
            skipDrawStart = snapshot.skipDrawStart,
            skipDrawEnd = snapshot.skipDrawEnd,
            autoFlushHardware = snapshot.autoFlushHardware,
            cpuFramebufferConversion = snapshot.cpuFramebufferConversion,
            disableDepthConversion = snapshot.disableDepthConversion,
            disableSafeFeatures = snapshot.disableSafeFeatures,
            disableRenderFixes = snapshot.disableRenderFixes,
            preloadFrameData = snapshot.preloadFrameData,
            disablePartialInvalidation = snapshot.disablePartialInvalidation,
            textureInsideRt = snapshot.textureInsideRt,
            readTargetsOnClose = snapshot.readTargetsOnClose,
            estimateTextureRegion = snapshot.estimateTextureRegion,
            gpuPaletteConversion = snapshot.gpuPaletteConversion,
            halfPixelOffset = snapshot.halfPixelOffset,
            nativeScaling = snapshot.nativeScaling,
            roundSprite = snapshot.roundSprite,
            bilinearUpscale = snapshot.bilinearUpscale,
            textureOffsetX = snapshot.textureOffsetX,
            textureOffsetY = snapshot.textureOffsetY,
            alignSprite = snapshot.alignSprite,
            mergeSprite = snapshot.mergeSprite,
            forceEvenSpritePosition = snapshot.forceEvenSpritePosition,
            nativePaletteDraw = snapshot.nativePaletteDraw,
            performancePreset = snapshot.performancePreset,
            overlayScale = snapshot.overlayScale,
            overlayOpacity = snapshot.overlayOpacity,
            overlayShow = snapshot.overlayShow,
            racingMode = snapshot.racingMode,
            touchHaptics = snapshot.touchHaptics,
            touchHapticsStrength = snapshot.touchHapticsStrength,
            leftStickSensitivity = snapshot.leftStickSensitivity,
            rightStickSensitivity = snapshot.rightStickSensitivity,
            invertLeftStick = snapshot.invertLeftStick,
            invertRightStick = snapshot.invertRightStick,
            invertLeftStickHorizontal = snapshot.invertLeftStickHorizontal,
            invertRightStickHorizontal = snapshot.invertRightStickHorizontal,
            enableAutoGamepad = snapshot.enableAutoGamepad,
            hideOverlayOnGamepad = snapshot.hideOverlayOnGamepad,
            gamepadStickDeadzone = snapshot.gamepadStickDeadzone,
            gamepadLeftStickSensitivity = snapshot.gamepadLeftStickSensitivity,
            gamepadRightStickSensitivity = snapshot.gamepadRightStickSensitivity,
            gamepadRightStickUpToR2 = snapshot.gamepadRightStickUpToR2,
            gamepadRightStickDownToL2 = snapshot.gamepadRightStickDownToL2,
            gamepadButtonHaptics = snapshot.gamepadButtonHaptics,
            gamepadBindings = snapshot.gamepadBindings,
            gamepadBindingsByPad = snapshot.gamepadBindingsByPad,
            gpuDriverType = snapshot.gpuDriverType,
            mediatekAngleOpenGl = snapshot.mediatekAngleOpenGl,
            customDriverPath = snapshot.customDriverPath,
            frameLimitEnabled = snapshot.frameLimitEnabled,
            vSyncEnabled = snapshot.vSyncEnabled,
            fastForwardSpeed = snapshot.fastForwardSpeed,
            targetFps = snapshot.targetFps,
            ntscFramerate = snapshot.ntscFramerate,
            palFramerate = snapshot.palFramerate
        )
    }

    fun setThemeMode(mode: ThemeMode) { viewModelScope.launch { preferences.setThemeMode(mode) } }
    fun setLanguage(tag: String?) { viewModelScope.launch { preferences.setLanguageTag(tag) } }

    fun purchasePro(activity: Activity) { proPurchaseManager.purchase(activity) }

    fun restoreProPurchases() { proPurchaseManager.restorePurchases(showMessage = true) }

    fun clearProPurchaseMessage() { proPurchaseManager.clearMessage() }


    fun setRenderer(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setRenderer(value)
            EmulatorBridge.setRenderer(value)
        }
    }

    fun setPerformanceProfile(value: Int) {
        viewModelScope.launch {
            preferences.setPerformanceProfile(value)
        }
    }

    fun loadAppReleaseHistory(showErrors: Boolean = true, force: Boolean = false) {
        if (_uiState.value.appUpdate.historyLoading) return
        viewModelScope.launch(Dispatchers.IO) {
            _uiState.value = _uiState.value.copy(
                appUpdate = _uiState.value.appUpdate.copy(
                    historyLoading = true,
                    historyErrorMessage = null
                )
            )
            runCatching {
                appUpdateRepository.loadReleaseHistory(force)
            }.onSuccess { releases ->
                _uiState.value = _uiState.value.copy(
                    appUpdate = _uiState.value.appUpdate.copy(
                        releaseHistory = releases,
                        historyLoading = false,
                        historyErrorMessage = null
                    )
                )
            }.onFailure { error ->
                val errorMsg = if (showErrors) {
                    if (error is com.sbro.emucorex.core.RateLimitException) {
                        val minutes = ((error.resetTimestampMs - System.currentTimeMillis()) / 60000).coerceAtLeast(1)
                        getApplication<Application>().getString(com.sbro.emucorex.R.string.settings_updates_rate_limit_error, minutes)
                    } else {
                        error.message ?: "Could not load release history"
                    }
                } else null

                _uiState.value = _uiState.value.copy(
                    appUpdate = _uiState.value.appUpdate.copy(
                        historyLoading = false,
                        historyErrorMessage = errorMsg
                    )
                )
            }
        }
    }
    fun setUpscaleMultiplier(value: Float) {
        viewModelScope.launch {
            val normalizedValue = normalizeUpscale(value)
            markPerformancePresetCustom()
            preferences.setUpscaleMultiplier(normalizedValue)
            EmulatorBridge.setUpscaleMultiplier(normalizedValue)
        }
    }

    fun setAspectRatio(value: Int) {
        viewModelScope.launch {
            preferences.setAspectRatio(value)
            EmulatorBridge.setAspectRatio(value)
        }
    }

    fun setAutoProgressiveScan(enabled: Boolean) {
        viewModelScope.launch {
            preferences.setAutoProgressiveScan(enabled)
        }
    }

    fun setPadVibration(enabled: Boolean) {
        viewModelScope.launch {
            preferences.setPadVibration(enabled)
            EmulatorBridge.setPadVibration(enabled)
        }
    }

    fun setPadVibrationStrength(value: Int) {
        viewModelScope.launch {
            preferences.setPadVibrationStrength(value)
        }
    }

    fun setPadVibrationFallback(enabled: Boolean) {
        viewModelScope.launch {
            preferences.setPadVibrationFallback(enabled)
        }
    }

    fun testPadVibration(
        strengthPercent: Int = _uiState.value.padVibrationStrength,
        durationMs: Long = 260L
    ) {
        GamepadManager.ensureInitialized(getApplication())
        GamepadManager.testPadVibration(
            padIndex = 0,
            strengthPercent = strengthPercent,
            durationMs = durationMs
        )
    }

    fun setGamepadBinding(padIndex: Int, actionId: String, keyCode: Int) {
        viewModelScope.launch {
            preferences.setGamepadBinding(padIndex, actionId, keyCode)
        }
    }

    fun clearGamepadBinding(padIndex: Int, actionId: String) {
        viewModelScope.launch {
            preferences.clearGamepadBinding(padIndex, actionId)
        }
    }

    fun resetGamepadBindingsForPad(padIndex: Int) {
        viewModelScope.launch {
            preferences.resetGamepadBindingsForPad(padIndex)
        }
    }

    fun resetAllSettings() {
        viewModelScope.launch {
            preferences.resetAllSettings()
        }
    }

    fun setEnableCheats(enabled: Boolean) {
        viewModelScope.launch {
            val effectiveEnabled = enabled && !preferences.getAchievementsHardcoreSync()
            preferences.setEnableCheats(effectiveEnabled)
            EmulatorBridge.setSetting("EmuCore", "EnableCheats", "bool", effectiveEnabled.toString())
            if (effectiveEnabled) {
                EmulatorBridge.reloadPatches()
            }
        }
    }

    fun setShowFps(enabled: Boolean) { viewModelScope.launch { preferences.setShowFps(enabled) } }
    fun setFpsOverlayMode(mode: Int) { viewModelScope.launch { preferences.setFpsOverlayMode(mode) } }
    fun setFpsOverlayCorner(corner: Int) { viewModelScope.launch { preferences.setFpsOverlayCorner(corner) } }
    fun setConfirmSaveLoadActions(enabled: Boolean) { viewModelScope.launch { preferences.setConfirmSaveLoadActions(enabled) } }
    fun setKeepScreenOn(enabled: Boolean) { viewModelScope.launch { preferences.setKeepScreenOn(enabled) } }
    fun setRacingMode(enabled: Boolean) { viewModelScope.launch { preferences.setRacingMode(enabled) } }
    fun setTouchHaptics(enabled: Boolean) { viewModelScope.launch { preferences.setTouchHaptics(enabled) } }
    fun setTouchHapticsStrength(value: Int) { viewModelScope.launch { preferences.setTouchHapticsStrength(value) } }
    fun testTouchHaptics(
        strengthPercent: Int = _uiState.value.touchHapticsStrength,
        durationMs: Long = 160L
    ) {
        AndroidTouchHaptics.play(
            context = getApplication(),
            strengthPercent = strengthPercent,
            durationMs = durationMs
        )
    }
    fun setShowRecentGames(enabled: Boolean) { viewModelScope.launch { preferences.setShowRecentGames(enabled) } }
    fun setShowHomeSearch(enabled: Boolean) { viewModelScope.launch { preferences.setShowHomeSearch(enabled) } }
    fun setShowDebugOptions(enabled: Boolean) { viewModelScope.launch { preferences.setShowDebugOptions(enabled) } }
    fun setPreferEnglishGameTitles(enabled: Boolean) {
        viewModelScope.launch {
            EmulatorBridge.setSetting("UI", "PreferEnglishGameTitles", "bool", enabled.toString())
            preferences.setPreferEnglishGameTitles(enabled)
        }
    }

    // Extended settings
    fun setEeCycleRate(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setEeCycleRate(value)
            EmulatorBridge.setSetting("EmuCore/Speedhacks", "EECycleRate", "int", value.toString())
        }
    }

    fun setEeCycleSkip(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setEeCycleSkip(value)
            EmulatorBridge.setSetting("EmuCore/Speedhacks", "EECycleSkip", "int", value.toString())
        }
    }

    fun setEnableEeRecompiler(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setEnableEeRecompiler(enabled)
            EmulatorBridge.setSetting("EmuCore/CPU/Recompiler", "EnableEE", "bool", enabled.toString())
        }
    }

    fun setEnableIopRecompiler(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setEnableIopRecompiler(enabled)
            EmulatorBridge.setSetting("EmuCore/CPU/Recompiler", "EnableIOP", "bool", enabled.toString())
        }
    }

    fun setEnableVu0Recompiler(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setEnableVu0Recompiler(enabled)
            EmulatorBridge.setSetting("EmuCore/CPU/Recompiler", "EnableVU0", "bool", enabled.toString())
        }
    }

    fun setEnableVu1Recompiler(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setEnableVu1Recompiler(enabled)
            EmulatorBridge.setSetting("EmuCore/CPU/Recompiler", "EnableVU1", "bool", enabled.toString())
            if (!enabled) {
                preferences.setEnableMtvu(false)
                EmulatorBridge.setSetting("EmuCore/Speedhacks", "vuThread", "bool", "false")
            }
        }
    }

    fun setEeFpuRoundMode(value: Int) {
        viewModelScope.launch {
            val normalized = value.coerceIn(AppPreferences.FLOAT_ROUND_NEAREST, AppPreferences.FLOAT_ROUND_CHOP)
            markPerformancePresetCustom()
            preferences.setEeFpuRoundMode(normalized)
            EmulatorBridge.setSetting("EmuCore/CPU", "FPU.Roundmode", "int", normalized.toString())
        }
    }

    fun setVu0RoundMode(value: Int) {
        viewModelScope.launch {
            val normalized = value.coerceIn(AppPreferences.FLOAT_ROUND_NEAREST, AppPreferences.FLOAT_ROUND_CHOP)
            markPerformancePresetCustom()
            preferences.setVu0RoundMode(normalized)
            EmulatorBridge.setSetting("EmuCore/CPU", "VU0.Roundmode", "int", normalized.toString())
        }
    }

    fun setVu1RoundMode(value: Int) {
        viewModelScope.launch {
            val normalized = value.coerceIn(AppPreferences.FLOAT_ROUND_NEAREST, AppPreferences.FLOAT_ROUND_CHOP)
            markPerformancePresetCustom()
            preferences.setVu1RoundMode(normalized)
            EmulatorBridge.setSetting("EmuCore/CPU", "VU1.Roundmode", "int", normalized.toString())
        }
    }

    fun setEeFpuClampingMode(value: Int) {
        viewModelScope.launch {
            val normalized = value.coerceIn(AppPreferences.CLAMPING_NONE, AppPreferences.CLAMPING_FULL)
            markPerformancePresetCustom()
            preferences.setEeFpuClampingMode(normalized)
            setEeFpuClampingModeCore(normalized)
        }
    }

    fun setVu0ClampingMode(value: Int) {
        viewModelScope.launch {
            val normalized = value.coerceIn(AppPreferences.CLAMPING_NONE, AppPreferences.CLAMPING_FULL)
            markPerformancePresetCustom()
            preferences.setVu0ClampingMode(normalized)
            setVuClampingModeCore("VU0ClampMode", "vu0", normalized)
        }
    }

    fun setVu1ClampingMode(value: Int) {
        viewModelScope.launch {
            val normalized = value.coerceIn(AppPreferences.CLAMPING_NONE, AppPreferences.CLAMPING_FULL)
            markPerformancePresetCustom()
            preferences.setVu1ClampingMode(normalized)
            setVuClampingModeCore("VU1ClampMode", "vu1", normalized)
        }
    }

    fun setEnableGameFixes(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setEnableGameFixes(enabled)
            EmulatorBridge.setSetting("EmuCore", "EnableGameFixes", "bool", enabled.toString())
        }
    }

    fun setEnableWaitLoopSpeedhack(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setEnableWaitLoopSpeedhack(enabled)
            EmulatorBridge.setSetting("EmuCore/Speedhacks", "WaitLoop", "bool", enabled.toString())
        }
    }

    private suspend fun setEeFpuClampingModeCore(value: Int) {
        EmulatorBridge.setSetting("EmuCoreX/CPU", "EEClampMode", "int", value.toString())
        EmulatorBridge.setSetting("EmuCore/CPU/Recompiler", "fpuOverflow", "bool", (value >= AppPreferences.CLAMPING_NORMAL).toString())
        EmulatorBridge.setSetting("EmuCore/CPU/Recompiler", "fpuExtraOverflow", "bool", (value >= AppPreferences.CLAMPING_EXTRA).toString())
        EmulatorBridge.setSetting("EmuCore/CPU/Recompiler", "fpuFullMode", "bool", (value >= AppPreferences.CLAMPING_FULL).toString())
    }

    private suspend fun setVuClampingModeCore(modeKey: String, prefix: String, value: Int) {
        EmulatorBridge.setSetting("EmuCoreX/CPU", modeKey, "int", value.toString())
        EmulatorBridge.setSetting("EmuCore/CPU/Recompiler", "${prefix}Overflow", "bool", (value >= AppPreferences.CLAMPING_NORMAL).toString())
        EmulatorBridge.setSetting("EmuCore/CPU/Recompiler", "${prefix}ExtraOverflow", "bool", (value >= AppPreferences.CLAMPING_EXTRA).toString())
        EmulatorBridge.setSetting("EmuCore/CPU/Recompiler", "${prefix}SignOverflow", "bool", (value >= AppPreferences.CLAMPING_FULL).toString())
    }

    fun setEnableIntcStatSpeedhack(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setEnableIntcStatSpeedhack(enabled)
            EmulatorBridge.setSetting("EmuCore/Speedhacks", "IntcStat", "bool", enabled.toString())
        }
    }

    fun setEnableVuFlagHack(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setEnableVuFlagHack(enabled)
            EmulatorBridge.setSetting("EmuCore/Speedhacks", "vuFlagHack", "bool", enabled.toString())
        }
    }

    fun setEnableInstantVu1(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setEnableInstantVu1(enabled)
            EmulatorBridge.setSetting("EmuCore/Speedhacks", "vu1Instant", "bool", enabled.toString())
        }
    }

    fun setEnableMtvu(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            val effectiveEnabled = enabled && _uiState.value.enableVu1Recompiler
            preferences.setEnableMtvu(effectiveEnabled)
            EmulatorBridge.setSetting("EmuCore/Speedhacks", "vuThread", "bool", effectiveEnabled.toString())
        }
    }

    fun setEnableThreadPinning(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setEnableThreadPinning(enabled)
            EmulatorBridge.setSetting("EmuCore", "EnableThreadPinning", "bool", enabled.toString())
        }
    }

    fun setEnableFastBoot(enabled: Boolean) {
        viewModelScope.launch {
            preferences.setEnableFastBoot(enabled)
            EmulatorBridge.setSetting("EmuCore", "EnableFastBoot", "bool", enabled.toString())
        }
    }

    fun setEnableFastCdvd(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setEnableFastCdvd(enabled)
            EmulatorBridge.setSetting("EmuCore/Speedhacks", "fastCDVD", "bool", enabled.toString())
        }
    }

    fun setHwDownloadMode(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setHwDownloadMode(value)
            EmulatorBridge.setSetting("EmuCore/GS", "HWDownloadMode", "int", value.toString())
        }
    }

    fun setFrameSkip(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setFrameSkip(value)
            EmulatorBridge.setSetting("EmuCore/GS", "FrameSkip", "int", value.toString())
        }
    }

    fun setSkipDuplicateFrames(enabled: Boolean) {
        viewModelScope.launch {
            preferences.setSkipDuplicateFrames(enabled)
            EmulatorBridge.setSkipDuplicateFrames(enabled)
        }
    }

    fun setFrameLimitEnabled(enabled: Boolean) {
        viewModelScope.launch {
            val effectiveEnabled = enabled || preferences.getAchievementsHardcoreSync()
            preferences.setFrameLimitEnabled(effectiveEnabled)
            EmulatorBridge.setFrameLimitEnabled(effectiveEnabled)
        }
    }

    fun setVSyncEnabled(enabled: Boolean) {
        viewModelScope.launch {
            preferences.setVSyncEnabled(enabled)
            EmulatorBridge.setVSyncEnabled(enabled)
        }
    }

    fun setFastForwardSpeed(value: Float) {
        viewModelScope.launch {
            preferences.setFastForwardSpeed(value)
            EmulatorBridge.setFastForwardSpeed(value)
        }
    }

    fun setTargetFps(value: Int) {
        viewModelScope.launch {
            preferences.setTargetFps(if (value <= 0) 0 else value)
            EmulatorBridge.setTargetFps(value, _uiState.value.ntscFramerate, _uiState.value.palFramerate)
        }
    }

    fun setNtscFramerate(value: Float) {
        viewModelScope.launch {
            preferences.setNtscFramerate(value)
            EmulatorBridge.setTargetFps(_uiState.value.targetFps, value, _uiState.value.palFramerate)
        }
    }

    fun setPalFramerate(value: Float) {
        viewModelScope.launch {
            preferences.setPalFramerate(value)
            EmulatorBridge.setTargetFps(_uiState.value.targetFps, _uiState.value.ntscFramerate, value)
        }
    }

    fun setTextureFiltering(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setTextureFiltering(value)
            EmulatorBridge.setSetting("EmuCore/GS", "filter", "int", value.toString())
        }
    }

    fun setTrilinearFiltering(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setTrilinearFiltering(value)
            EmulatorBridge.setSetting("EmuCore/GS", "TriFilter", "int", value.toString())
        }
    }

    fun setBlendingAccuracy(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setBlendingAccuracy(value)
            EmulatorBridge.setSetting("EmuCore/GS", "accurate_blending_unit", "int", value.toString())
        }
    }

    fun setMediatekAngleOpenGl(enabled: Boolean) {
        viewModelScope.launch {
            val effectiveEnabled = enabled &&
                GpuHardwareProfiles.isMediaTekHardware() &&
                EmulatorBridge.isBundledAngleAvailable()
            preferences.setMediatekAngleOpenGl(effectiveEnabled)
            EmulatorBridge.setSetting("EmuCore/GS", "AndroidUseAngleOpenGL", "bool", effectiveEnabled.toString())
        }
    }

    fun setTexturePreloading(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setTexturePreloading(value)
            EmulatorBridge.setSetting("EmuCore/GS", "texture_preloading", "int", value.toString())
        }
    }

    fun setEnableFxaa(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setEnableFxaa(enabled)
            EmulatorBridge.setSetting("EmuCore/GS", "fxaa", "bool", enabled.toString())
        }
    }

    fun setCasMode(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setCasMode(value)
            EmulatorBridge.setSetting("EmuCore/GS", "CASMode", "int", value.toString())
        }
    }

    fun setCasSharpness(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setCasSharpness(value)
            EmulatorBridge.setSetting("EmuCore/GS", "CASSharpness", "int", value.toString())
        }
    }

    fun setTvShader(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            val clamped = GsHackDefaults.coerceTvShader(value)
            preferences.setTvShader(clamped)
            EmulatorBridge.setSetting("EmuCore/GS", "TVShader", "int", clamped.toString())
        }
    }

    fun setShadeBoostBrightness(value: Int) {
        viewModelScope.launch {
            val clamped = value.coerceIn(1, 100)
            markPerformancePresetCustom()
            val enabled = isShadeBoostActive(
                brightness = clamped,
                contrast = _uiState.value.shadeBoostContrast,
                saturation = _uiState.value.shadeBoostSaturation,
                gamma = _uiState.value.shadeBoostGamma
            )
            preferences.setShadeBoostEnabled(enabled)
            preferences.setShadeBoostBrightness(clamped)
            EmulatorBridge.setSetting("EmuCore/GS", "ShadeBoost", "bool", enabled.toString())
            EmulatorBridge.setSetting("EmuCore/GS", "ShadeBoost_Brightness", "int", clamped.toString())
        }
    }

    fun setShadeBoostContrast(value: Int) {
        viewModelScope.launch {
            val clamped = value.coerceIn(1, 100)
            markPerformancePresetCustom()
            val enabled = isShadeBoostActive(
                brightness = _uiState.value.shadeBoostBrightness,
                contrast = clamped,
                saturation = _uiState.value.shadeBoostSaturation,
                gamma = _uiState.value.shadeBoostGamma
            )
            preferences.setShadeBoostEnabled(enabled)
            preferences.setShadeBoostContrast(clamped)
            EmulatorBridge.setSetting("EmuCore/GS", "ShadeBoost", "bool", enabled.toString())
            EmulatorBridge.setSetting("EmuCore/GS", "ShadeBoost_Contrast", "int", clamped.toString())
        }
    }

    fun setShadeBoostSaturation(value: Int) {
        viewModelScope.launch {
            val clamped = value.coerceIn(1, 100)
            markPerformancePresetCustom()
            val enabled = isShadeBoostActive(
                brightness = _uiState.value.shadeBoostBrightness,
                contrast = _uiState.value.shadeBoostContrast,
                saturation = clamped,
                gamma = _uiState.value.shadeBoostGamma
            )
            preferences.setShadeBoostEnabled(enabled)
            preferences.setShadeBoostSaturation(clamped)
            EmulatorBridge.setSetting("EmuCore/GS", "ShadeBoost", "bool", enabled.toString())
            EmulatorBridge.setSetting("EmuCore/GS", "ShadeBoost_Saturation", "int", clamped.toString())
        }
    }

    fun setShadeBoostGamma(value: Int) {
        viewModelScope.launch {
            val clamped = value.coerceIn(1, 100)
            markPerformancePresetCustom()
            val enabled = isShadeBoostActive(
                brightness = _uiState.value.shadeBoostBrightness,
                contrast = _uiState.value.shadeBoostContrast,
                saturation = _uiState.value.shadeBoostSaturation,
                gamma = clamped
            )
            preferences.setShadeBoostEnabled(enabled)
            preferences.setShadeBoostGamma(clamped)
            EmulatorBridge.setSetting("EmuCore/GS", "ShadeBoost", "bool", enabled.toString())
            EmulatorBridge.setSetting("EmuCore/GS", "ShadeBoost_Gamma", "int", clamped.toString())
        }
    }

    private fun isShadeBoostActive(
        brightness: Int,
        contrast: Int,
        saturation: Int,
        gamma: Int
    ): Boolean {
        return brightness != 50 || contrast != 50 || saturation != 50 || gamma != 50
    }

    fun setEnableWidescreenPatches(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setEnableWidescreenPatches(enabled)
            EmulatorBridge.setSetting("EmuCore", "EnableWideScreenPatches", "bool", enabled.toString())
        }
    }

    fun setEnableNoInterlacingPatches(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setEnableNoInterlacingPatches(enabled)
            EmulatorBridge.setSetting("EmuCore", "EnableNoInterlacingPatches", "bool", enabled.toString())
        }
    }

    fun setAnisotropicFiltering(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setAnisotropicFiltering(value)
            EmulatorBridge.setSetting("EmuCore/GS", "MaxAnisotropy", "int", value.toString())
        }
    }

    fun setEnableHwMipmapping(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setEnableHwMipmapping(enabled)
            EmulatorBridge.setSetting("EmuCore/GS", "hw_mipmap", "bool", enabled.toString())
        }
    }

    fun setAntiBlur(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setAntiBlur(enabled)
            EmulatorBridge.setSetting("EmuCore/GS", "pcrtc_antiblur", "bool", enabled.toString())
        }
    }

    fun setCpuSpriteRenderSize(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setCpuSpriteRenderSize(value)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_CPUSpriteRenderBW", "int", value.toString())
            refreshManualHardwareFixes(_uiState.value.copy(cpuSpriteRenderSize = value))
        }
    }

    fun setCpuSpriteRenderLevel(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setCpuSpriteRenderLevel(value)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_CPUSpriteRenderLevel", "int", value.toString())
            refreshManualHardwareFixes(_uiState.value.copy(cpuSpriteRenderLevel = value))
        }
    }

    fun setSoftwareClutRender(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setSoftwareClutRender(value)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_CPUCLUTRender", "int", value.toString())
            refreshManualHardwareFixes(_uiState.value.copy(softwareClutRender = value))
        }
    }

    fun setGpuTargetClutMode(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setGpuTargetClutMode(value)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_GPUTargetCLUTMode", "int", value.toString())
            refreshManualHardwareFixes(_uiState.value.copy(gpuTargetClutMode = value))
        }
    }

    fun setSkipDrawStart(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setSkipDrawStart(value)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_SkipDraw_Start", "int", value.toString())
            refreshManualHardwareFixes(_uiState.value.copy(skipDrawStart = value))
        }
    }

    fun setSkipDrawEnd(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setSkipDrawEnd(value)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_SkipDraw_End", "int", value.toString())
            refreshManualHardwareFixes(_uiState.value.copy(skipDrawEnd = value))
        }
    }

    fun setAutoFlushHardware(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setAutoFlushHardware(value)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_AutoFlushLevel", "int", value.toString())
            refreshManualHardwareFixes(_uiState.value.copy(autoFlushHardware = value))
        }
    }

    fun setCpuFramebufferConversion(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setCpuFramebufferConversion(enabled)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_CPU_FB_Conversion", "bool", enabled.toString())
            refreshManualHardwareFixes(_uiState.value.copy(cpuFramebufferConversion = enabled))
        }
    }

    fun setDisableDepthConversion(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setDisableDepthConversion(enabled)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_DisableDepthSupport", "bool", enabled.toString())
            refreshManualHardwareFixes(_uiState.value.copy(disableDepthConversion = enabled))
        }
    }

    fun setDisableSafeFeatures(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setDisableSafeFeatures(enabled)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_Disable_Safe_Features", "bool", enabled.toString())
            refreshManualHardwareFixes(_uiState.value.copy(disableSafeFeatures = enabled))
        }
    }

    fun setDisableRenderFixes(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setDisableRenderFixes(enabled)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_DisableRenderFixes", "bool", enabled.toString())
            refreshManualHardwareFixes(_uiState.value.copy(disableRenderFixes = enabled))
        }
    }

    fun setPreloadFrameData(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setPreloadFrameData(enabled)
            EmulatorBridge.setSetting("EmuCore/GS", "preload_frame_with_gs_data", "bool", enabled.toString())
            refreshManualHardwareFixes(_uiState.value.copy(preloadFrameData = enabled))
        }
    }

    fun setDisablePartialInvalidation(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setDisablePartialInvalidation(enabled)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_DisablePartialInvalidation", "bool", enabled.toString())
            refreshManualHardwareFixes(_uiState.value.copy(disablePartialInvalidation = enabled))
        }
    }

    fun setTextureInsideRt(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setTextureInsideRt(value)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_TextureInsideRt", "int", value.toString())
            refreshManualHardwareFixes(_uiState.value.copy(textureInsideRt = value))
        }
    }

    fun setReadTargetsOnClose(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setReadTargetsOnClose(enabled)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_ReadTCOnClose", "bool", enabled.toString())
            refreshManualHardwareFixes(_uiState.value.copy(readTargetsOnClose = enabled))
        }
    }

    fun setEstimateTextureRegion(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setEstimateTextureRegion(enabled)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_EstimateTextureRegion", "bool", enabled.toString())
            refreshManualHardwareFixes(_uiState.value.copy(estimateTextureRegion = enabled))
        }
    }

    fun setGpuPaletteConversion(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setGpuPaletteConversion(enabled)
            EmulatorBridge.setSetting("EmuCore/GS", "paltex", "bool", enabled.toString())
            refreshManualHardwareFixes(_uiState.value.copy(gpuPaletteConversion = enabled))
        }
    }

    fun setHalfPixelOffset(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setHalfPixelOffset(value)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_HalfPixelOffset", "int", value.toString())
            refreshManualHardwareFixes(_uiState.value.copy(halfPixelOffset = value))
        }
    }

    fun setNativeScaling(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setNativeScaling(value)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_native_scaling", "int", value.toString())
            refreshManualHardwareFixes(_uiState.value.copy(nativeScaling = value))
        }
    }

    fun setRoundSprite(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setRoundSprite(value)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_round_sprite_offset", "int", value.toString())
            refreshManualHardwareFixes(_uiState.value.copy(roundSprite = value))
        }
    }

    fun setBilinearUpscale(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setBilinearUpscale(value)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_BilinearHack", "int", value.toString())
            refreshManualHardwareFixes(_uiState.value.copy(bilinearUpscale = value))
        }
    }

    fun setTextureOffsetX(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setTextureOffsetX(value)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_TCOffsetX", "int", value.toString())
            refreshManualHardwareFixes(_uiState.value.copy(textureOffsetX = value))
        }
    }

    fun setTextureOffsetY(value: Int) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setTextureOffsetY(value)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_TCOffsetY", "int", value.toString())
            refreshManualHardwareFixes(_uiState.value.copy(textureOffsetY = value))
        }
    }

    fun setAlignSprite(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setAlignSprite(enabled)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_align_sprite_X", "bool", enabled.toString())
            refreshManualHardwareFixes(_uiState.value.copy(alignSprite = enabled))
        }
    }

    fun setMergeSprite(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setMergeSprite(enabled)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_merge_pp_sprite", "bool", enabled.toString())
            refreshManualHardwareFixes(_uiState.value.copy(mergeSprite = enabled))
        }
    }

    fun setForceEvenSpritePosition(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setForceEvenSpritePosition(enabled)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_ForceEvenSpritePosition", "bool", enabled.toString())
            refreshManualHardwareFixes(_uiState.value.copy(forceEvenSpritePosition = enabled))
        }
    }

    fun setNativePaletteDraw(enabled: Boolean) {
        viewModelScope.launch {
            markPerformancePresetCustom()
            preferences.setNativePaletteDraw(enabled)
            EmulatorBridge.setSetting("EmuCore/GS", "UserHacks_NativePaletteDraw", "bool", enabled.toString())
            refreshManualHardwareFixes(_uiState.value.copy(nativePaletteDraw = enabled))
        }
    }

    private suspend fun markPerformancePresetCustom() {
        if (_uiState.value.performancePreset != PerformancePresets.CUSTOM) {
            preferences.setPerformancePreset(PerformancePresets.CUSTOM)
        }
    }

    private suspend fun refreshManualHardwareFixes(state: SettingsUiState = _uiState.value) {
        val enabled = GsHackDefaults.shouldEnableManualHardwareFixes(
            cpuSpriteRenderSize = state.cpuSpriteRenderSize,
            cpuSpriteRenderLevel = state.cpuSpriteRenderLevel,
            softwareClutRender = state.softwareClutRender,
            gpuTargetClutMode = state.gpuTargetClutMode,
            skipDrawStart = state.skipDrawStart,
            skipDrawEnd = state.skipDrawEnd,
            autoFlushHardware = state.autoFlushHardware,
            cpuFramebufferConversion = state.cpuFramebufferConversion,
            disableDepthConversion = state.disableDepthConversion,
            disableSafeFeatures = state.disableSafeFeatures,
            disableRenderFixes = state.disableRenderFixes,
            preloadFrameData = state.preloadFrameData,
            disablePartialInvalidation = state.disablePartialInvalidation,
            textureInsideRt = state.textureInsideRt,
            readTargetsOnClose = state.readTargetsOnClose,
            estimateTextureRegion = state.estimateTextureRegion,
            gpuPaletteConversion = state.gpuPaletteConversion,
            halfPixelOffset = state.halfPixelOffset,
            nativeScaling = state.nativeScaling,
            roundSprite = state.roundSprite,
            bilinearUpscale = state.bilinearUpscale,
            textureOffsetX = state.textureOffsetX,
            textureOffsetY = state.textureOffsetY,
            alignSprite = state.alignSprite,
            mergeSprite = state.mergeSprite,
            forceEvenSpritePosition = state.forceEvenSpritePosition,
            nativePaletteDraw = state.nativePaletteDraw
        )
        EmulatorBridge.setSetting("EmuCore/GS", "UserHacks", "bool", enabled.toString())
    }

    // Overlay
    fun setOverlayScale(value: Int) { viewModelScope.launch { preferences.setOverlayScale(value) } }
    fun setOverlayOpacity(value: Int) { viewModelScope.launch { preferences.setOverlayOpacity(value) } }
    fun setLeftStickSensitivity(value: Int) { viewModelScope.launch { preferences.setLeftStickSensitivity(value) } }
    fun setRightStickSensitivity(value: Int) { viewModelScope.launch { preferences.setRightStickSensitivity(value) } }
    fun setInvertLeftStick(enabled: Boolean) { viewModelScope.launch { preferences.setInvertLeftStick(enabled) } }
    fun setInvertRightStick(enabled: Boolean) { viewModelScope.launch { preferences.setInvertRightStick(enabled) } }
    fun setInvertLeftStickHorizontal(enabled: Boolean) { viewModelScope.launch { preferences.setInvertLeftStickHorizontal(enabled) } }
    fun setInvertRightStickHorizontal(enabled: Boolean) { viewModelScope.launch { preferences.setInvertRightStickHorizontal(enabled) } }

    // Gamepad
    fun setEnableAutoGamepad(enabled: Boolean) { viewModelScope.launch { preferences.setEnableAutoGamepad(enabled) } }
    fun setHideOverlayOnGamepad(enabled: Boolean) { viewModelScope.launch { preferences.setHideOverlayOnGamepad(enabled) } }
    fun setGamepadStickDeadzone(value: Int) { viewModelScope.launch { preferences.setGamepadStickDeadzone(value) } }
    fun setGamepadLeftStickSensitivity(value: Int) { viewModelScope.launch { preferences.setGamepadLeftStickSensitivity(value) } }
    fun setGamepadRightStickSensitivity(value: Int) { viewModelScope.launch { preferences.setGamepadRightStickSensitivity(value) } }
    fun setGamepadRightStickUpToR2(enabled: Boolean) { viewModelScope.launch { preferences.setGamepadRightStickUpToR2(enabled) } }
    fun setGamepadRightStickDownToL2(enabled: Boolean) { viewModelScope.launch { preferences.setGamepadRightStickDownToL2(enabled) } }
    fun setGamepadButtonHaptics(enabled: Boolean) { viewModelScope.launch { preferences.setGamepadButtonHaptics(enabled) } }

    fun setBiosPath(uri: Uri) {
        val application = getApplication<Application>()
        if (!StorageAccess.takePersistableReadPermission(application, uri)) return
        viewModelScope.launch {
            preferences.setBiosPath(uri.toString())
            EmulatorBridge.applyRuntimeConfig(
                biosPath = uri.toString(),
                emulatorDataPath = _uiState.value.emulatorDataPath,
                memoryCardSlot1 = preferences.memoryCardSlot1.first(),
                memoryCardSlot2 = preferences.memoryCardSlot2.first(),
                renderer = _uiState.value.renderer,
                upscaleMultiplier = _uiState.value.upscaleMultiplier,
                gpuDriverType = _uiState.value.gpuDriverType,
                customDriverPath = _uiState.value.customDriverPath,
                gpuHardwareProfile = GpuHardwareProfiles.detectHardwareProfile(),
                mediatekAngleOpenGl = _uiState.value.mediatekAngleOpenGl,
                enableEeRecompiler = _uiState.value.enableEeRecompiler,
                enableIopRecompiler = _uiState.value.enableIopRecompiler,
                enableVu0Recompiler = _uiState.value.enableVu0Recompiler,
                enableVu1Recompiler = _uiState.value.enableVu1Recompiler,
                eeFpuRoundMode = _uiState.value.eeFpuRoundMode,
                vu0RoundMode = _uiState.value.vu0RoundMode,
                vu1RoundMode = _uiState.value.vu1RoundMode,
                eeFpuClampingMode = _uiState.value.eeFpuClampingMode,
                vu0ClampingMode = _uiState.value.vu0ClampingMode,
                vu1ClampingMode = _uiState.value.vu1ClampingMode,
                enableGameFixes = _uiState.value.enableGameFixes,
                waitLoopSpeedhack = _uiState.value.enableWaitLoopSpeedhack,
                intcStatSpeedhack = _uiState.value.enableIntcStatSpeedhack,
                vuFlagHack = _uiState.value.enableVuFlagHack,
                instantVu1 = _uiState.value.enableInstantVu1,
                mtvu = _uiState.value.enableMtvu,
                enableThreadPinning = _uiState.value.enableThreadPinning,
                enableFastBoot = _uiState.value.enableFastBoot,
                hwDownloadMode = _uiState.value.hwDownloadMode,
                frameLimitEnabled = _uiState.value.frameLimitEnabled,
                vSyncEnabled = _uiState.value.vSyncEnabled,
                targetFps = _uiState.value.targetFps,
                ntscFramerate = _uiState.value.ntscFramerate,
                palFramerate = _uiState.value.palFramerate,
                textureFiltering = _uiState.value.textureFiltering,
                trilinearFiltering = _uiState.value.trilinearFiltering,
                blendingAccuracy = _uiState.value.blendingAccuracy,
                texturePreloading = _uiState.value.texturePreloading,
                enableFxaa = _uiState.value.enableFxaa,
                casMode = _uiState.value.casMode,
                casSharpness = _uiState.value.casSharpness,
                tvShader = _uiState.value.tvShader,
                anisotropicFiltering = _uiState.value.anisotropicFiltering,
                enableHwMipmapping = _uiState.value.enableHwMipmapping,
                antiBlur = _uiState.value.antiBlur,
                cpuSpriteRenderSize = _uiState.value.cpuSpriteRenderSize,
                cpuSpriteRenderLevel = _uiState.value.cpuSpriteRenderLevel,
                softwareClutRender = _uiState.value.softwareClutRender,
                gpuTargetClutMode = _uiState.value.gpuTargetClutMode,
                skipDrawStart = _uiState.value.skipDrawStart,
                skipDrawEnd = _uiState.value.skipDrawEnd,
                autoFlushHardware = _uiState.value.autoFlushHardware,
                cpuFramebufferConversion = _uiState.value.cpuFramebufferConversion,
                disableDepthConversion = _uiState.value.disableDepthConversion,
                disableSafeFeatures = _uiState.value.disableSafeFeatures,
                disableRenderFixes = _uiState.value.disableRenderFixes,
                preloadFrameData = _uiState.value.preloadFrameData,
                disablePartialInvalidation = _uiState.value.disablePartialInvalidation,
                textureInsideRt = _uiState.value.textureInsideRt,
                readTargetsOnClose = _uiState.value.readTargetsOnClose,
                estimateTextureRegion = _uiState.value.estimateTextureRegion,
                gpuPaletteConversion = _uiState.value.gpuPaletteConversion,
                halfPixelOffset = _uiState.value.halfPixelOffset,
                nativeScaling = _uiState.value.nativeScaling,
                roundSprite = _uiState.value.roundSprite,
                bilinearUpscale = _uiState.value.bilinearUpscale,
                textureOffsetX = _uiState.value.textureOffsetX,
                textureOffsetY = _uiState.value.textureOffsetY,
                alignSprite = _uiState.value.alignSprite,
                mergeSprite = _uiState.value.mergeSprite,
                forceEvenSpritePosition = _uiState.value.forceEvenSpritePosition,
                nativePaletteDraw = _uiState.value.nativePaletteDraw
            )
        }
    }

    fun setGamePath(uri: Uri) {
        val application = getApplication<Application>()
        if (!StorageAccess.takePersistableReadPermission(application, uri)) return
        val rawPath = uri.toString()
        if (!SetupValidator.hasCoreReadableGameFile(application, rawPath)) return
        viewModelScope.launch { preferences.setGamePath(rawPath) }
    }

    fun setEmulatorDataPath(uri: Uri) {
        val application = getApplication<Application>()
        if (!StorageAccess.takePersistableReadWritePermission(application, uri)) return

        val resolvedPath = DocumentPathResolver.resolveDirectoryPath(uri.toString()) ?: return
        if (!EmulatorStorage.prepareCustomDataRoot(resolvedPath)) {
            android.widget.Toast.makeText(application, com.sbro.emucorex.R.string.error_otg_read_only, android.widget.Toast.LENGTH_LONG).show()
            return
        }
        viewModelScope.launch { preferences.setEmulatorDataPath(resolvedPath) }
    }

    fun clearEmulatorDataPath() {
        viewModelScope.launch { preferences.setEmulatorDataPath(null) }
    }

    fun setCoverDownloadBaseUrl(url: String?) {
        viewModelScope.launch {
            preferences.setCoverDownloadBaseUrl(url)
            CoverArtRepository(getApplication()).clearCache()
        }
    }

    fun setCoverArtStyle(style: Int) {
        viewModelScope.launch {
            preferences.setCoverArtStyle(style)
            CoverArtRepository(getApplication()).clearCache()
        }
    }

    fun setCustomDriverPath(path: String?) {
        viewModelScope.launch {
            preferences.setCustomDriverPath(path)
            if (path != null) {
                preferences.setGpuDriverType(1)
                EmulatorBridge.setCustomDriverPath(path)
            } else {
                preferences.setGpuDriverType(0)
                EmulatorBridge.setCustomDriverPath("")
            }
        }
    }

    fun installGpuDriver(uri: android.net.Uri, onComplete: (Result<String>) -> Unit) {
        viewModelScope.launch(Dispatchers.IO) {
            val result = runCatching {
                val driverName = gpuDriverManager.installFromArchive(uri)
                selectGpuDriverInternal(driverName)
                driverName
            }
            refreshInstalledGpuDrivers()
            withContext(Dispatchers.Main) {
                onComplete(result)
            }
        }
    }

    fun refreshGpuDriverCatalog() {
        if (_uiState.value.gpuDriverCatalogLoading) return
        viewModelScope.launch(Dispatchers.IO) {
            _uiState.value = _uiState.value.copy(
                gpuDriverCatalogLoading = true,
                gpuDriverCatalogError = null
            )
            runCatching {
                gpuDriverCatalogRepository.loadCatalog()
            }.onSuccess { drivers ->
                _uiState.value = _uiState.value.copy(
                    remoteGpuDrivers = drivers,
                    gpuDriverCatalogLoading = false,
                    gpuDriverCatalogError = null
                )
            }.onFailure { error ->
                _uiState.value = _uiState.value.copy(
                    gpuDriverCatalogLoading = false,
                    gpuDriverCatalogError = error.message ?: "Could not load GPU driver catalog"
                )
            }
        }
    }

    fun installRemoteGpuDriver(driver: RemoteGpuDriver, onComplete: (Result<String>) -> Unit) {
        if (_uiState.value.gpuDriverDownloads.containsKey(driver.id)) return
        viewModelScope.launch(Dispatchers.IO) {
            _uiState.value = _uiState.value.copy(
                gpuDriverDownloads = _uiState.value.gpuDriverDownloads + (driver.id to 0f)
            )
            val result = runCatching {
                val archive = gpuDriverCatalogRepository.downloadDriver(driver) { progress ->
                    _uiState.value = _uiState.value.copy(
                        gpuDriverDownloads = _uiState.value.gpuDriverDownloads + (driver.id to progress)
                    )
                }
                val driverName = gpuDriverManager.installFromArchive(archive)
                selectGpuDriverInternal(driverName)
                driverName
            }
            _uiState.value = _uiState.value.copy(
                gpuDriverDownloads = _uiState.value.gpuDriverDownloads - driver.id
            )
            refreshInstalledGpuDrivers()
            withContext(Dispatchers.Main) {
                onComplete(result)
            }
        }
    }

    fun useSystemGpuDriver() {
        viewModelScope.launch {
            preferences.setGpuDriverType(0)
            EmulatorBridge.setCustomDriverPath("")
        }
    }

    fun selectGpuDriver(driverName: String) {
        viewModelScope.launch(Dispatchers.IO) {
            selectGpuDriverInternal(driverName)
            refreshInstalledGpuDrivers()
        }
    }

    fun removeGpuDriver(driverName: String) {
        viewModelScope.launch(Dispatchers.IO) {
            val removedDriverPath = gpuDriverManager.readMainLibraryPath(driverName)
            gpuDriverManager.remove(driverName)
            if (removedDriverPath != null && _uiState.value.customDriverPath == removedDriverPath) {
                preferences.setGpuDriverType(0)
                preferences.setCustomDriverPath(null)
                EmulatorBridge.setCustomDriverPath("")
            }
            refreshInstalledGpuDrivers()
        }
    }

    private suspend fun selectGpuDriverInternal(driverName: String) {
        val driverPath = gpuDriverManager.readMainLibraryPath(driverName) ?: error("Driver is not installed")
        preferences.setCustomDriverPath(driverPath)
        preferences.setGpuDriverType(1)
        EmulatorBridge.setCustomDriverPath(driverPath)
    }

    fun refreshInstalledGpuDrivers() {
        _uiState.value = _uiState.value.copy(
            installedGpuDrivers = gpuDriverManager.listInstalledDrivers()
        )
    }

}
