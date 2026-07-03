package com.sbro.emucorex.ui.onboarding

import android.app.Activity
import android.app.Application
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.sbro.emucorex.core.BiosValidator
import com.sbro.emucorex.core.DocumentPathResolver
import com.sbro.emucorex.core.EmulatorBridge
import com.sbro.emucorex.core.EmulatorStorage
import com.sbro.emucorex.core.GpuHardwareProfiles
import com.sbro.emucorex.core.PerformanceProfiles
import com.sbro.emucorex.core.ProPurchaseManager
import com.sbro.emucorex.core.SetupValidator
import com.sbro.emucorex.core.StorageAccess
import com.sbro.emucorex.data.AppPreferences
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

data class OnboardingUiState(
    val performanceProfile: Int = PerformanceProfiles.SAFE,
    val gpuHardwareProfile: Int = GpuHardwareProfiles.ADRENO,
    val biosPath: String? = null,
    val gamePath: String? = null,
    val emulatorDataPath: String? = null,
    val biosValid: Boolean = false,
    val gamePathValid: Boolean = false,
    val canContinue: Boolean = false,
    val currentPage: Int = 0,
    val totalPages: Int = 6,
    val isProUnlocked: Boolean = false,
    val proPrice: String? = null,
    val isProProductLoading: Boolean = false,
    val isProProductAvailable: Boolean = false,
    val isProPurchaseInProgress: Boolean = false,
    val proPurchaseMessageResId: Int? = null
)

class OnboardingViewModel(application: Application) : AndroidViewModel(application) {

    private val preferences = AppPreferences(application)
    private val proPurchaseManager = ProPurchaseManager.getInstance(application)
    private val _uiState = MutableStateFlow(OnboardingUiState())
    val uiState: StateFlow<OnboardingUiState> = _uiState.asStateFlow()

    init {
        viewModelScope.launch {
            launch {
                proPurchaseManager.state.collect { proState ->
                    updateState(
                        isProUnlocked = proState.isProUnlocked,
                        proPrice = proState.productPrice,
                        isProProductLoading = proState.isProductLoading,
                        isProProductAvailable = proState.isProductAvailable,
                        isProPurchaseInProgress = proState.isPurchaseInProgress,
                        proPurchaseMessageResId = proState.messageResId
                    )
                }
            }
            launch {
                preferences.performanceProfile.collect { profile ->
                    updateState(performanceProfile = profile)
                }
            }
            launch {
                preferences.gpuHardwareProfile.collect { profile ->
                    updateState(gpuHardwareProfile = profile)
                }
            }
            launch {
                preferences.biosPath.collect { path ->
                    updateState(
                        biosPath = path,
                        biosValid = BiosValidator.hasUsableBiosFiles(getApplication(), path)
                    )
                }
            }
            launch {
                preferences.gamePath.collect { path ->
                    updateState(
                        gamePath = path,
                        gamePathValid = SetupValidator.hasCoreReadableGameFile(getApplication(), path)
                    )
                }
            }
            launch {
                preferences.emulatorDataPath.collect { path ->
                    updateState(emulatorDataPath = path)
                }
            }
        }
    }

    fun purchasePro(activity: Activity) {
        proPurchaseManager.purchase(activity)
    }

    fun clearProPurchaseMessage() {
        proPurchaseManager.clearMessage()
    }

    fun setBiosPath(uri: Uri) {
        val application = getApplication<Application>()
        if (!StorageAccess.takePersistableReadPermission(application, uri)) return

        viewModelScope.launch {
            preferences.setBiosPath(uri.toString())
            EmulatorBridge.applyRuntimeConfig(
                biosPath = uri.toString(),
                emulatorDataPath = _uiState.value.emulatorDataPath,
                renderer = EmulatorBridge.getSetting("EmuCoreX", "Renderer", "int")?.toIntOrNull() ?: 0,
                gpuHardwareProfile = _uiState.value.gpuHardwareProfile,
                upscaleMultiplier = EmulatorBridge.getSetting("EmuCoreX", "UpscaleMultiplier", "float")?.toFloatOrNull()
                    ?: EmulatorBridge.getSetting("EmuCoreX", "UpscaleMultiplier", "int")?.toIntOrNull()?.toFloat()
                    ?: 1f
            )
        }
    }

    fun setGamePath(uri: Uri) {
        val application = getApplication<Application>()
        if (!StorageAccess.takePersistableReadPermission(application, uri)) return

        val rawPath = uri.toString()
        if (!SetupValidator.hasCoreReadableGameFile(application, rawPath)) return

        viewModelScope.launch {
            preferences.setGamePath(rawPath)
        }
    }

    fun setEmulatorDataPath(uri: Uri) {
        val application = getApplication<Application>()
        if (!StorageAccess.takePersistableReadWritePermission(application, uri)) return

        val resolvedPath = DocumentPathResolver.resolveDirectoryPath(uri.toString()) ?: return
        if (!EmulatorStorage.prepareCustomDataRoot(resolvedPath)) return
        viewModelScope.launch {
            preferences.setEmulatorDataPath(resolvedPath)
        }
    }

    fun setPerformanceProfile(profile: Int) {
        viewModelScope.launch {
            preferences.setPerformanceProfile(profile)
        }
    }

    fun setGpuHardwareProfile(profile: Int) {
        viewModelScope.launch {
            preferences.setGpuHardwareProfile(profile)
        }
    }

    fun setCurrentPage(page: Int) {
        val currentState = _uiState.value
        _uiState.value = currentState.copy(currentPage = page.coerceIn(0, currentState.totalPages - 1))
    }

    fun completeOnboarding(onFinished: () -> Unit) {
        if (!_uiState.value.canContinue) return
        viewModelScope.launch {
            preferences.setOnboardingCompleted(true)
            onFinished()
        }
    }

    private fun updateState(
        performanceProfile: Int = _uiState.value.performanceProfile,
        gpuHardwareProfile: Int = _uiState.value.gpuHardwareProfile,
        biosPath: String? = _uiState.value.biosPath,
        gamePath: String? = _uiState.value.gamePath,
        emulatorDataPath: String? = _uiState.value.emulatorDataPath,
        biosValid: Boolean = _uiState.value.biosValid,
        gamePathValid: Boolean = _uiState.value.gamePathValid,
        currentPage: Int = _uiState.value.currentPage,
        isProUnlocked: Boolean = _uiState.value.isProUnlocked,
        proPrice: String? = _uiState.value.proPrice,
        isProProductLoading: Boolean = _uiState.value.isProProductLoading,
        isProProductAvailable: Boolean = _uiState.value.isProProductAvailable,
        isProPurchaseInProgress: Boolean = _uiState.value.isProPurchaseInProgress,
        proPurchaseMessageResId: Int? = _uiState.value.proPurchaseMessageResId
    ) {
        _uiState.value = OnboardingUiState(
            performanceProfile = performanceProfile,
            gpuHardwareProfile = gpuHardwareProfile,
            biosPath = biosPath,
            gamePath = gamePath,
            emulatorDataPath = emulatorDataPath,
            biosValid = biosValid,
            gamePathValid = gamePathValid,
            canContinue = biosValid && gamePathValid,
            currentPage = currentPage.coerceIn(0, 5),
            totalPages = 6,
            isProUnlocked = isProUnlocked,
            proPrice = proPrice,
            isProProductLoading = isProProductLoading,
            isProProductAvailable = isProProductAvailable,
            isProPurchaseInProgress = isProPurchaseInProgress,
            proPurchaseMessageResId = proPurchaseMessageResId
        )
    }
}
