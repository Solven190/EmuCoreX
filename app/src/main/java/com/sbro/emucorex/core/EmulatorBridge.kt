package com.sbro.emucorex.core

import android.content.Context
import android.os.ParcelFileDescriptor
import android.util.Log
import android.view.Surface
import androidx.core.net.toUri
import com.sbro.emucorex.data.AppPreferences
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.lang.ref.WeakReference
import java.util.Locale

object EmulatorBridge {
    private const val TAG = "EmulatorBridge"
    const val AUTO_RENDERER = -1
    const val OPENGL_RENDERER = 12
    const val VULKAN_RENDERER = 14
    const val DEFAULT_RENDERER = VULKAN_RENDERER
    private const val BOOT_SMOKE_PROBE_STEPS = 67_108_864

    private val aspectRatioSettingValues = mapOf(
        0 to "Stretch",
        1 to "Auto 4:3/3:2",
        2 to "4:3",
        3 to "16:9",
        4 to "10:7"
    )

    private val serialDispatcher = Dispatchers.IO.limitedParallelism(1)
    private val serialScope = CoroutineScope(SupervisorJob() + serialDispatcher)

    @Volatile
    var isNativeLoaded: Boolean = false
        private set

    @Volatile
    private var isVmActive: Boolean = false

    @Volatile
    private var lastSurface: Surface? = null

    @Volatile
    private var lastSurfaceWidth: Int = 0

    @Volatile
    private var lastSurfaceHeight: Int = 0

    @Volatile
    private var surfaceEventVersion: Long = 0

    @Volatile
    private var shutdownRequested: Boolean = false

    private var contextRef: WeakReference<Context>? = null
    private val settingsCache = HashMap<String, String>()

    init {
        try {
            System.loadLibrary("emucore")
            isNativeLoaded = true
            Log.i(TAG, "libemucore loaded")
        } catch (error: UnsatisfiedLinkError) {
            isNativeLoaded = false
            Log.e(TAG, "libemucore load failed", error)
        }
    }

    private data class RuntimeOp(val kind: String, val fields: List<String>)
    private data class PreparedMetadataPath(
        val path: String,
        val descriptor: ParcelFileDescriptor? = null
    )

    private fun settingOp(section: String, key: String, type: String, value: String) =
        RuntimeOp("setting", listOf(section, key, type, value))

    private fun rendererOp(renderer: Int) = RuntimeOp("renderer", listOf(renderer.toString()))

    private fun upscaleOp(value: Float) = RuntimeOp("upscale", listOf(value.toString()))

    private fun normalizeAspectRatio(type: Int): Int {
        return if (type in aspectRatioSettingValues.keys) type else 1
    }

    private fun aspectOp(type: Int) = RuntimeOp(
        "aspect",
        normalizeAspectRatio(type).let { listOf(it.toString(), aspectRatioSettingValues.getValue(it)) }
    )

    private fun customDriverOp(path: String) = RuntimeOp("custom_driver", listOf(path))

    private fun prepareCustomDriverLibrary(path: String): String {
        if (path.isBlank()) return ""
        val file = File(path)
        if (file.isFile) {
            file.setReadable(true, true)
            file.setWritable(true, true)
            file.setExecutable(true, true)
        }
        return path
    }

    private fun refreshBiosOp() = RuntimeOp("refresh_bios", emptyList())

    private fun resetTargetFpsOp() = RuntimeOp("reset_target_fps", emptyList())

    private fun memoryCardSlotOp(slot: Int, fileName: String?) = RuntimeOp(
        "memory_card_slot",
        listOf(slot.toString(), fileName.orEmpty())
    )

    private suspend fun <T> runSerial(block: () -> T): T = withContext(serialDispatcher) { block() }

    private fun launchSerial(block: suspend () -> Unit) {
        serialScope.launch { block() }
    }

    private suspend fun performRuntimeOps(ops: List<RuntimeOp>) {
        if (!isNativeLoaded || ops.isEmpty()) return
        runSerial {
            NativeApp.beginSettingsBatch()
            try {
                ops.forEach { op ->
                    when (op.kind) {
                        "setting" -> {
                            val section = op.fields.getOrNull(0) ?: return@forEach
                            val key = op.fields.getOrNull(1) ?: return@forEach
                            val type = op.fields.getOrNull(2) ?: return@forEach
                            val value = toCoreSettingValue(section, key, op.fields.getOrNull(3) ?: return@forEach)
                            NativeApp.setSetting(section, key, type, value)
                        }
                        "renderer" -> {
                            val renderer = op.fields.firstOrNull()?.toIntOrNull() ?: return@forEach
                            NativeApp.renderGpu(if (renderer == AUTO_RENDERER) 0 else renderer)
                        }
                        "upscale" -> {
                            val value = op.fields.firstOrNull()?.toFloatOrNull() ?: return@forEach
                            NativeApp.renderUpscalemultiplier(normalizeUpscale(value))
                        }
                        "aspect" -> {
                            val type = op.fields.firstOrNull()?.toIntOrNull() ?: return@forEach
                            NativeApp.setAspectRatio(type)
                            NativeApp.setSetting(
                                "EmuCore/GS",
                                "AspectRatio",
                                "string",
                                op.fields.getOrNull(1) ?: aspectRatioSettingValues.getValue(1)
                            )
                        }
                        "custom_driver" -> {
                            NativeApp.setCustomDriverPath(op.fields.firstOrNull().orEmpty())
                        }
                        "refresh_bios" -> {
                            NativeApp.refreshBIOS()
                        }
                        "reset_target_fps" -> {
                            NativeApp.setSetting("EmuCore/GS", "FramerateNTSC", "float", "59.94")
                            NativeApp.setSetting("EmuCore/GS", "FrameratePAL", "float", "50.0")
                        }
                        "memory_card_slot" -> {
                            val slot = op.fields.getOrNull(0)?.toIntOrNull() ?: return@forEach
                            val slotIndex = slot.coerceIn(1, 2)
                            val fileName = op.fields.getOrNull(1).orEmpty()
                            val hasCard = fileName.isNotBlank()
                            NativeApp.setSetting("MemoryCards", "Slot${slotIndex}_Enable", "bool", hasCard.toString())
                            NativeApp.setSetting("MemoryCards", "Slot${slotIndex}_Filename", "string", fileName)
                        }
                    }
                }
            } finally {
                NativeApp.endSettingsBatch()
            }
        }
    }

    private fun rendererName(renderer: Int): String = when (renderer) {
        AUTO_RENDERER -> "Vulkan"
        0 -> "Vulkan"
        OPENGL_RENDERER -> "OpenGL"
        13 -> "Software"
        VULKAN_RENDERER -> "Vulkan"
        15 -> "D3D12"
        3 -> "D3D11"
        else -> "Unknown($renderer)"
    }

    private fun normalizeRenderer(renderer: Int): Int {
        return if (renderer <= 0) DEFAULT_RENDERER else renderer
    }

    private fun toCoreSettingValue(section: String, key: String, value: String): String {
        if (section == "EmuCore/GS" && key == "TriFilter") {
            return when (value.toIntOrNull()) {
                0 -> "-1"
                else -> value
            }
        }
        return value
    }

    private fun fromCoreSettingValue(section: String, key: String, value: String?): String? {
        if (value == null) return null
        if (section == "EmuCore/GS" && key == "TriFilter") {
            return when (value.toIntOrNull()) {
                -1 -> "0"
                else -> value
            }
        }
        return value
    }

    fun initializeOnce(context: Context) {
        contextRef = WeakReference(context)
        if (!isNativeLoaded) {
            Log.e(TAG, "initializeOnce skipped: native library is not loaded")
            return
        }

        try {
            NativeApp.setNativeLibraryDir(context.applicationInfo.nativeLibraryDir ?: "")
            NativeApp.initializeOnce(context.applicationContext)
            val jitSmokeOk = runCatching { NativeApp.runJitExecutableMemorySmokeTest() }.getOrDefault(false)
            Log.i(TAG, "JIT executable-memory smoke result=$jitSmokeOk")
            val preferEnglishTitles = runBlocking {
                AppPreferences(context.applicationContext).preferEnglishGameTitles.first()
            }
            NativeApp.setSetting("UI", "PreferEnglishGameTitles", "bool", preferEnglishTitles.toString())
            Log.i(TAG, "initializeOnce completed")
        } catch (error: Exception) {
            Log.e(TAG, "initializeOnce failed", error)
        }
    }

    fun getContext(): Context? = contextRef?.get()

    suspend fun applyRuntimeConfig(
        biosPath: String?,
        renderer: Int,
        upscaleMultiplier: Float,
        gpuDriverType: Int = 0,
        customDriverPath: String? = null,
        aspectRatio: Int = 1,
        enableEeRecompiler: Boolean = true,
        enableIopRecompiler: Boolean = true,
        enableVu0Recompiler: Boolean = true,
        enableVu1Recompiler: Boolean = true,
        enableFastmem: Boolean = true,
        mtvu: Boolean = true,
        fastCdvd: Boolean = false,
        enableCheats: Boolean = false,
        hwDownloadMode: Int = 0,
        eeCycleRate: Int = 0,
        eeCycleSkip: Int = 0,
        frameSkip: Int = 0,
        skipDuplicateFrames: Boolean = false,
        frameLimitEnabled: Boolean = true,
        targetFps: Int = 0,
        textureFiltering: Int = GsHackDefaults.BILINEAR_FILTERING_DEFAULT,
        trilinearFiltering: Int = GsHackDefaults.TRILINEAR_FILTERING_DEFAULT,
        blendingAccuracy: Int = GsHackDefaults.BLENDING_ACCURACY_DEFAULT,
        texturePreloading: Int = GsHackDefaults.TEXTURE_PRELOADING_DEFAULT,
        enableFxaa: Boolean = false,
        casMode: Int = 0,
        casSharpness: Int = 50,
        shadeBoostEnabled: Boolean = false,
        shadeBoostBrightness: Int = 50,
        shadeBoostContrast: Int = 50,
        shadeBoostSaturation: Int = 50,
        shadeBoostGamma: Int = 50,
        anisotropicFiltering: Int = 0,
        enableHwMipmapping: Boolean = GsHackDefaults.HW_MIPMAPPING_DEFAULT,
        widescreenPatches: Boolean = false,
        noInterlacingPatches: Boolean = false,
        cpuSpriteRenderSize: Int = GsHackDefaults.CPU_SPRITE_RENDER_SIZE_DEFAULT,
        cpuSpriteRenderLevel: Int = GsHackDefaults.CPU_SPRITE_RENDER_LEVEL_DEFAULT,
        softwareClutRender: Int = GsHackDefaults.SOFTWARE_CLUT_RENDER_DEFAULT,
        gpuTargetClutMode: Int = GsHackDefaults.GPU_TARGET_CLUT_DEFAULT,
        skipDrawStart: Int = 0,
        skipDrawEnd: Int = 0,
        autoFlushHardware: Int = GsHackDefaults.AUTO_FLUSH_DEFAULT,
        cpuFramebufferConversion: Boolean = false,
        disableDepthConversion: Boolean = false,
        disableSafeFeatures: Boolean = false,
        disableRenderFixes: Boolean = false,
        preloadFrameData: Boolean = false,
        disablePartialInvalidation: Boolean = false,
        textureInsideRt: Int = GsHackDefaults.TEXTURE_INSIDE_RT_DEFAULT,
        readTargetsOnClose: Boolean = false,
        estimateTextureRegion: Boolean = false,
        gpuPaletteConversion: Boolean = false,
        halfPixelOffset: Int = GsHackDefaults.HALF_PIXEL_OFFSET_DEFAULT,
        nativeScaling: Int = GsHackDefaults.NATIVE_SCALING_DEFAULT,
        roundSprite: Int = GsHackDefaults.ROUND_SPRITE_DEFAULT,
        bilinearUpscale: Int = GsHackDefaults.BILINEAR_UPSCALE_DEFAULT,
        textureOffsetX: Int = 0,
        textureOffsetY: Int = 0,
        alignSprite: Boolean = false,
        mergeSprite: Boolean = false,
        forceEvenSpritePosition: Boolean = false,
        nativePaletteDraw: Boolean = false,
        memoryCardSlot1: String? = null,
        memoryCardSlot2: String? = null,
        autotestMode: Boolean = false,
        fpuClampMode: Int = 1,
        disableHardwareReadbacks: Boolean = false,
        fpuCorrectAddSub: Boolean = true
    ) {
        if (!isNativeLoaded) return

        val context = getContext() ?: return
        val resolvedRenderer = normalizeRenderer(renderer)
        val preparedBios = DocumentPathResolver.prepareBiosSelection(context, biosPath)
        val resolvedBiosPath = preparedBios?.directoryPath
            ?: biosPath?.let(DocumentPathResolver::resolveDirectoryPath)
        val preferredBiosFile = preparedBios?.fileName
            ?: DocumentPathResolver.findPreferredBiosFileName(resolvedBiosPath)
        val savestatesDir = File(context.getExternalFilesDir(null) ?: context.filesDir, "sstates").apply { mkdirs() }
        val memcardsDir = File(context.getExternalFilesDir(null) ?: context.filesDir, "memcards").apply { mkdirs() }
        val cheatsDir = File(context.getExternalFilesDir(null) ?: context.filesDir, "cheats").apply { mkdirs() }
        val patchesDir = File(context.getExternalFilesDir(null) ?: context.filesDir, "patches").apply { mkdirs() }
        val logDir = EmulatorStorage.logDir(context)
        val manualHardwareFixes = GsHackDefaults.shouldEnableManualHardwareFixes(
            cpuSpriteRenderSize = cpuSpriteRenderSize,
            cpuSpriteRenderLevel = cpuSpriteRenderLevel,
            softwareClutRender = softwareClutRender,
            gpuTargetClutMode = gpuTargetClutMode,
            skipDrawStart = skipDrawStart,
            skipDrawEnd = skipDrawEnd,
            autoFlushHardware = autoFlushHardware,
            cpuFramebufferConversion = cpuFramebufferConversion,
            disableDepthConversion = disableDepthConversion,
            disableSafeFeatures = disableSafeFeatures,
            disableRenderFixes = disableRenderFixes,
            preloadFrameData = preloadFrameData,
            disablePartialInvalidation = disablePartialInvalidation,
            textureInsideRt = textureInsideRt,
            readTargetsOnClose = readTargetsOnClose,
            estimateTextureRegion = estimateTextureRegion,
            gpuPaletteConversion = gpuPaletteConversion,
            halfPixelOffset = halfPixelOffset,
            nativeScaling = nativeScaling,
            roundSprite = roundSprite,
            bilinearUpscale = bilinearUpscale,
            textureOffsetX = textureOffsetX,
            textureOffsetY = textureOffsetY,
            alignSprite = alignSprite,
            mergeSprite = mergeSprite,
            forceEvenSpritePosition = forceEvenSpritePosition,
            nativePaletteDraw = nativePaletteDraw
        )

        val customDriverSupported = GpuDriverCompatibility.supportsAdrenoToolsCustomDrivers()
        val effectiveGpuDriverType = if (gpuDriverType == 1 && customDriverSupported) 1 else 0
        NativeApp.setCrashContextString("emu_renderer_name", rendererName(resolvedRenderer))
        NativeApp.setCrashContextString("emu_gpu_driver_mode", if (effectiveGpuDriverType == 1) "custom" else "system")
        val resolvedCustomDriverPath = if (effectiveGpuDriverType == 1) {
            prepareCustomDriverLibrary(customDriverPath.orEmpty())
        } else {
            ""
        }
        val directEeRecompiler = enableEeRecompiler
        val directIopRecompiler = enableIopRecompiler
        val directVu0Recompiler = enableVu0Recompiler
        val directVu1Recompiler = enableVu1Recompiler
        val directMtvu = mtvu && directVu1Recompiler
        Log.i(
            "EmuCoreX",
            "android jit: requested={ee:$enableEeRecompiler iop:$enableIopRecompiler vu0:$enableVu0Recompiler vu1:$enableVu1Recompiler fastmem:$enableFastmem} mtvuRequested=$mtvu direct={ee:$directEeRecompiler iop:$directIopRecompiler vu0:$directVu0Recompiler vu1:$directVu1Recompiler mtvu:$directMtvu fastmem:$enableFastmem}"
        )
        NativeApp.logCrashBreadcrumb(
            "applyRuntimeConfig renderer=${rendererName(resolvedRenderer)}($resolvedRenderer) driverType=$effectiveGpuDriverType requestedDriverType=$gpuDriverType hwDownload=$hwDownloadMode mtvuRequested=$mtvu directJit={ee:$directEeRecompiler iop:$directIopRecompiler vu0:$directVu0Recompiler vu1:$directVu1Recompiler mtvu:$directMtvu fastmem:$enableFastmem} fastCdvd=$fastCdvd jitRequested={ee:$enableEeRecompiler iop:$enableIopRecompiler vu0:$enableVu0Recompiler vu1:$enableVu1Recompiler fastmem:$enableFastmem}"
        )
        val prefs = AppPreferences(context)
        val padVibrationEnabled = prefs.padVibration.first()

        performRuntimeOps(
            buildList {
                add(rendererOp(resolvedRenderer))
                add(upscaleOp(upscaleMultiplier))
                add(aspectOp(aspectRatio))
                add(settingOp("Folders", "Bios", "string", resolvedBiosPath.orEmpty()))
                add(settingOp("Folders", "Savestates", "string", savestatesDir.absolutePath))
                add(settingOp("Folders", "MemoryCards", "string", memcardsDir.absolutePath))
                add(settingOp("Folders", "Cheats", "string", cheatsDir.absolutePath))
                add(settingOp("Folders", "Patches", "string", patchesDir.absolutePath))
                add(settingOp("Folders", "Logs", "string", logDir.absolutePath))
                add(memoryCardSlotOp(1, memoryCardSlot1))
                add(memoryCardSlotOp(2, memoryCardSlot2))
                add(settingOp("Filenames", "BIOS", "string", preferredBiosFile.orEmpty()))
                add(refreshBiosOp())
                add(settingOp("EmuCoreX", "OpenGLTextureDebugLog", "bool", (resolvedRenderer == 12).toString()))
                add(settingOp("EmuCore/CPU/Recompiler", "EnableEE", "bool", directEeRecompiler.toString()))
                add(settingOp("EmuCore/CPU/Recompiler", "EnableIOP", "bool", directIopRecompiler.toString()))
                add(settingOp("EmuCore/CPU/Recompiler", "EnableVU0", "bool", directVu0Recompiler.toString()))
                add(settingOp("EmuCore/CPU/Recompiler", "EnableVU1", "bool", directVu1Recompiler.toString()))
                add(settingOp("EmuCore/CPU/Recompiler", "EnableFastmem", "bool", enableFastmem.toString()))
                add(settingOp("EmuCore/Speedhacks", "WaitLoop", "bool", "true"))
                add(settingOp("EmuCore/Speedhacks", "IntcStat", "bool", "true"))
                add(settingOp("EmuCore/Speedhacks", "vuFlagHack", "bool", "true"))
                add(settingOp("EmuCore/Speedhacks", "vuThread", "bool", directMtvu.toString()))
                add(settingOp("EmuCore/Speedhacks", "vu1Instant", "bool", "true"))
                add(settingOp("EmuCore/Speedhacks", "fastCDVD", "bool", fastCdvd.toString()))
                add(settingOp("EmuCore", "EnableCheats", "bool", enableCheats.toString()))
                add(settingOp("EmuCore/GS", "HWDownloadMode", "int", hwDownloadMode.toString()))
                add(settingOp("EmuCore/Speedhacks", "EECycleRate", "int", eeCycleRate.toString()))
                add(settingOp("EmuCore/Speedhacks", "EECycleSkip", "int", eeCycleSkip.toString()))
                add(settingOp("EmuCore/GS", "FrameLimitEnable", "bool", frameLimitEnabled.toString()))
                addAll(targetFpsOps(targetFps))
                add(settingOp("EmuCore/Framerate", "NominalScalar", "float", "1.0"))
                add(settingOp("EmuCore/CPU/Recompiler", "FPUClampMode", "int", fpuClampMode.toString()))
                add(settingOp("EmuCore/GS", "disable_hw_readbacks", "bool", disableHardwareReadbacks.toString()))
                add(settingOp("EmuCore/CPU/Recompiler", "fpuCorrectAddSub", "bool", fpuCorrectAddSub.toString()))
                add(settingOp("EmuCore/GS", "FrameSkip", "int", frameSkip.toString()))
                add(settingOp("EmuCore/GS", "SkipDuplicateFrames", "bool", skipDuplicateFrames.toString()))
                add(settingOp("EmuCore/GS", "filter", "int", textureFiltering.toString()))
                add(settingOp("EmuCore/GS", "TriFilter", "int", trilinearFiltering.toString()))
                add(settingOp("EmuCore/GS", "accurate_blending_unit", "int", blendingAccuracy.toString()))
                add(settingOp("EmuCore/GS", "texture_preloading", "int", texturePreloading.toString()))
                add(settingOp("EmuCore/GS", "DisableShaderCache", "bool", "false"))
                add(settingOp("EmuCore/GS", "fxaa", "bool", enableFxaa.toString()))
                add(settingOp("EmuCore/GS", "CASMode", "int", casMode.toString()))
                add(settingOp("EmuCore/GS", "CASSharpness", "int", casSharpness.toString()))
                add(settingOp("EmuCore/GS", "ShadeBoost", "bool", shadeBoostEnabled.toString()))
                add(settingOp("EmuCore/GS", "ShadeBoost_Brightness", "int", shadeBoostBrightness.toString()))
                add(settingOp("EmuCore/GS", "ShadeBoost_Contrast", "int", shadeBoostContrast.toString()))
                add(settingOp("EmuCore/GS", "ShadeBoost_Saturation", "int", shadeBoostSaturation.toString()))
                add(settingOp("EmuCore/GS", "ShadeBoost_Gamma", "int", shadeBoostGamma.toString()))
                add(settingOp("EmuCore/GS", "MaxAnisotropy", "int", anisotropicFiltering.toString()))
                add(settingOp("EmuCore/GS", "hw_mipmap", "bool", enableHwMipmapping.toString()))
                add(settingOp("EmuCore", "EnableWideScreenPatches", "bool", widescreenPatches.toString()))
                add(settingOp("EmuCore", "EnableNoInterlacingPatches", "bool", noInterlacingPatches.toString()))
                add(settingOp("EmuCore/GS", "UserHacks", "bool", manualHardwareFixes.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_CPUSpriteRenderBW", "int", cpuSpriteRenderSize.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_CPUSpriteRenderLevel", "int", cpuSpriteRenderLevel.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_CPUCLUTRender", "int", softwareClutRender.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_GPUTargetCLUTMode", "int", gpuTargetClutMode.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_SkipDraw_Start", "int", skipDrawStart.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_SkipDraw_End", "int", skipDrawEnd.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_AutoFlushLevel", "int", autoFlushHardware.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_CPU_FB_Conversion", "bool", cpuFramebufferConversion.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_DisableDepthSupport", "bool", disableDepthConversion.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_Disable_Safe_Features", "bool", disableSafeFeatures.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_DisableRenderFixes", "bool", disableRenderFixes.toString()))
                add(settingOp("EmuCore/GS", "preload_frame_with_gs_data", "bool", preloadFrameData.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_DisablePartialInvalidation", "bool", disablePartialInvalidation.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_TextureInsideRt", "int", textureInsideRt.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_ReadTCOnClose", "bool", readTargetsOnClose.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_EstimateTextureRegion", "bool", estimateTextureRegion.toString()))
                add(settingOp("EmuCore/GS", "paltex", "bool", gpuPaletteConversion.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_HalfPixelOffset", "int", halfPixelOffset.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_native_scaling", "int", nativeScaling.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_round_sprite_offset", "int", roundSprite.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_BilinearHack", "int", bilinearUpscale.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_TCOffsetX", "int", textureOffsetX.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_TCOffsetY", "int", textureOffsetY.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_align_sprite_X", "bool", alignSprite.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_merge_pp_sprite", "bool", mergeSprite.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_ForceEvenSpritePosition", "bool", forceEvenSpritePosition.toString()))
                add(settingOp("EmuCore/GS", "UserHacks_NativePaletteDraw", "bool", nativePaletteDraw.toString()))
                add(settingOp("EmuCoreX", "BiosSource", "string", biosPath.orEmpty()))
                add(settingOp("EmuCoreX", "Renderer", "int", resolvedRenderer.toString()))
                add(settingOp("EmuCoreX", "UpscaleMultiplier", "float", upscaleMultiplier.toString()))
                add(settingOp("EmuCoreX", "HasContext", "bool", (context.applicationContext != null).toString()))
                add(settingOp("EmuCoreX", "AutotestMode", "bool", autotestMode.toString()))
                add(settingOp("EmuCore", "WarnAboutUnsafeSettings", "bool", "false"))
                add(settingOp("EmuCore/GS", "OsdMessagesPos", "int", "0"))
                add(settingOp("EmuCore/GS", "OsdPerformancePos", "int", "0"))
                add(settingOp("InputSources", "PadVibration", "bool", padVibrationEnabled.toString()))
                add(settingOp("Achievements", "Enabled", "bool", prefs.getAchievementsEnabledSync().toString()))
                add(settingOp("Achievements", "ChallengeMode", "bool", prefs.getAchievementsHardcoreSync().toString()))
                add(settingOp("Achievements", "Username", "string", prefs.getAchievementsUsernameSync().orEmpty()))
                add(settingOp("Achievements", "Token", "string", prefs.getAchievementsTokenSync().orEmpty()))
                add(customDriverOp(resolvedCustomDriverPath))
            }
        )
    }

    suspend fun setMemoryCardAssignments(slot1: String?, slot2: String?) {
        performRuntimeOps(
            listOf(
                memoryCardSlotOp(1, slot1),
                memoryCardSlotOp(2, slot2)
            )
        )
    }

    suspend fun startEmulation(
        path: String,
        bootSmokeProbe: Boolean = false,
        allowBiosBoot: Boolean = false
    ): Boolean {
        if (!isNativeLoaded) {
            Log.e(TAG, "startEmulation skipped: native library is not loaded")
            return false
        }
        if (path.isBlank() && !allowBiosBoot && !bootSmokeProbe) {
            Log.e(TAG, "startEmulation rejected blank game path")
            return false
        }
        val isElf = when {
            path.substringAfterLast('.', "").equals("elf", ignoreCase = true) -> true
            path.startsWith("content://") -> {
                val context = getContext()
                val displayName = context?.let { DocumentPathResolver.getDisplayName(it, path) }.orEmpty()
                displayName.substringAfterLast('.', "").equals("elf", ignoreCase = true)
            }
            else -> false
        }
        val isIrx = when {
            path.substringAfterLast('.', "").equals("irx", ignoreCase = true) -> true
            path.startsWith("content://") -> {
                val context = getContext()
                val displayName = context?.let { DocumentPathResolver.getDisplayName(it, path) }.orEmpty()
                displayName.substringAfterLast('.', "").equals("irx", ignoreCase = true)
            }
            else -> false
        }
        val pathType = when {
            path.startsWith("content://") -> "content"
            path.isBlank() -> "bios"
            else -> "file"
        }
        NativeApp.logCrashBreadcrumb("startEmulation requested pathType=$pathType vmActive=$isVmActive")
        Log.i(TAG, "startEmulation requested pathType=$pathType bootSmoke=$bootSmokeProbe vmActive=$isVmActive")

        return runSerial {
            isVmActive = true
            shutdownRequested = false
            val result = try {
                NativeApp.logCrashBreadcrumb(
                    "startEmulation entering native ${
                        when {
                            bootSmokeProbe -> "runBootSmokeProbe"
                            isElf -> "bootElf"
                            isIrx -> "bootIrx"
                            else -> "runVMThread"
                        }
                    }"
                )
                when {
                    bootSmokeProbe -> NativeApp.runBootSmokeProbe(path, BOOT_SMOKE_PROBE_STEPS) != 0
                    isElf -> NativeApp.bootElf(path)
                    isIrx -> NativeApp.bootIrx(path)
                    else -> NativeApp.runVMThread(path)
                }
            } catch (error: Exception) {
                NativeApp.logCrashBreadcrumb("startEmulation exception before native start returned")
                Log.e(TAG, "startEmulation native call failed", error)
                false
            }
            if (bootSmokeProbe) {
                isVmActive = false
                DocumentPathResolver.releasePreparedLaunchHandles()
            } else if (!result || !runCatching { NativeApp.hasValidVm() }.getOrDefault(false)) {
                isVmActive = false
                DocumentPathResolver.releasePreparedLaunchHandles()
            }
            NativeApp.logCrashBreadcrumb("startEmulation finished result=$result")
            Log.i(TAG, "startEmulation finished result=$result")
            result
        }
    }

    suspend fun pause() {
        if (!isNativeLoaded || !isVmActive) return
        runSerial {
            try {
                NativeApp.pause()
            } catch (_: Exception) { }
        }
    }

    suspend fun resume() {
        if (!isNativeLoaded || !isVmActive) return
        runSerial {
            try {
                rebindSurface()
                NativeApp.resume()
            } catch (_: Exception) { }
        }
    }

    suspend fun shutdown() {
        if (!isNativeLoaded || !isVmActive || shutdownRequested) return
        runSerial {
            if (!isVmActive || shutdownRequested) return@runSerial
            shutdownRequested = true
            try {
                NativeApp.shutdown()
            } catch (_: Exception) { }
            isVmActive = false
            shutdownRequested = false
            DocumentPathResolver.releasePreparedLaunchHandles()
        }
    }

    fun hasValidVm(): Boolean {
        if (!isNativeLoaded) return false
        return try {
            isVmActive && NativeApp.hasValidVm()
        } catch (_: Exception) {
            false
        }
    }

    fun isVmActive(): Boolean {
        if (isVmActive && isNativeLoaded && !runCatching { NativeApp.hasValidVm() }.getOrDefault(false)) {
            isVmActive = false
            DocumentPathResolver.releasePreparedLaunchHandles()
        }
        return isVmActive
    }

    fun getGameTitle(path: String): String = getGameMetadata(path).title

    fun getGameMetadata(path: String): GameMetadata {
        val inferredMetadata = when {
            path.startsWith("content://") -> {
                val context = getContext()
                val displayName = context?.let { DocumentPathResolver.getDisplayName(it, path) } ?: path
                parseMetadataFromName(displayName)
            }
            else -> parseMetadataFromName(File(path).nameWithoutExtension)
        }
        val extensionSource = if (path.startsWith("content://")) {
            getContext()?.let { DocumentPathResolver.getDisplayName(it, path) } ?: path
        } else {
            path
        }
        val extension = extensionSource.substringAfterLast('.', "").lowercase()

        if (!isNativeLoaded) return inferredMetadata
        if (isVmActive) return inferredMetadata
        if (extension == "elf") return inferredMetadata

        val preparedPath = prepareMetadataPathForNative(path) ?: return inferredMetadata
        return try {
            val rawTitle = NativeApp.getGameTitle(preparedPath.path).orEmpty()
            val segments = rawTitle.split('|')
            val nativeTitle = segments.getOrNull(0).orEmpty()
            val nativeSerial = segments.getOrNull(1)?.takeIf { it.isNotBlank() }
            val nativeSerialWithCrc = segments.getOrNull(2)?.takeIf { it.isNotBlank() }
            if (isFdMetadataArtifact(preparedPath.path, nativeTitle, nativeSerial)) {
                return inferredMetadata
            }
            val title = nativeTitle
                .takeIf { it.isNotBlank() }
                ?.let { normalizeNativeGameTitle(it, inferredMetadata.title) }
                ?: inferredMetadata.title
            GameMetadata(
                title = title,
                serial = nativeSerial ?: inferredMetadata.serial,
                serialWithCrc = nativeSerialWithCrc ?: inferredMetadata.serialWithCrc
            )
        } catch (_: Exception) {
            inferredMetadata
        } finally {
            preparedPath.descriptor?.close()
        }
    }

    private fun prepareMetadataPathForNative(path: String): PreparedMetadataPath? {
        if (!path.startsWith("content://")) return PreparedMetadataPath(path)
        val context = getContext() ?: return null
        val directPath = DocumentPathResolver.resolveFilePath(context, path)
            ?.let(::File)
            ?.takeIf { it.isFile && it.canRead() }
            ?.absolutePath
        if (!directPath.isNullOrBlank()) return PreparedMetadataPath(directPath)

        return PreparedMetadataPath(path)
    }

    private fun isFdMetadataArtifact(path: String, nativeTitle: String, nativeSerial: String?): Boolean {
        if (!path.startsWith("/proc/self/fd/")) return false
        if (!nativeSerial.isNullOrBlank()) return false
        val trimmedTitle = nativeTitle.substringBefore('|').trim()
        return trimmedTitle.isBlank() || trimmedTitle.all(Char::isDigit)
    }

    private fun normalizeNativeGameTitle(rawTitle: String, fallbackTitle: String): String {
        return cleanGameDisplayTitle(rawTitle, fallbackTitle)
    }

    fun cleanGameDisplayTitle(rawTitle: String?, fallbackNameOrPath: String? = null): String {
        val fallbackDisplay = fallbackNameOrPath
            ?.takeIf { it.isNotBlank() }
            ?.let { value ->
                if (value.startsWith("content://")) {
                    getContext()?.let { context -> DocumentPathResolver.getDisplayName(context, value) }
                        ?: DocumentPathResolver.normalizeDisplayName(value)
                } else {
                    DocumentPathResolver.normalizeDisplayName(value)
                }
            }
            .orEmpty()
        val fallbackTitle = parseMetadataFromName(fallbackDisplay).title
        val trimmed = rawTitle.orEmpty().trim()
        val source = if (trimmed.isBlank() || looksLikeStoragePath(trimmed)) {
            fallbackDisplay.ifBlank { trimmed }
        } else {
            trimmed
        }
        val normalized = if (looksLikeStoragePath(source)) {
            DocumentPathResolver.normalizeDisplayName(source)
        } else {
            source
        }
        return parseMetadataFromName(normalized).title.ifBlank { fallbackTitle }
    }

    private fun looksLikeStoragePath(value: String): Boolean {
        val trimmed = value.trim()
        return trimmed.contains("%2F", ignoreCase = true) ||
            trimmed.contains("%3A", ignoreCase = true) ||
            trimmed.startsWith("content://", ignoreCase = true) ||
            trimmed.startsWith("primary:", ignoreCase = true) ||
            trimmed.startsWith("home:", ignoreCase = true) ||
            trimmed.startsWith("raw:", ignoreCase = true) ||
            trimmed.contains("/storage/", ignoreCase = true)
    }

    fun parseMetadataFromName(rawName: String): GameMetadata {
        val ext = rawName.substringAfterLast('.', "").lowercase()
        val cleanName = if (ext in setOf("iso", "bin", "chd", "cso", "gz", "elf")) {
            rawName.substringBeforeLast('.').trim()
        } else {
            rawName.trim()
        }
        val serial = extractSerialFromName(cleanName)
        val title = cleanName
            .replace(Regex("""(?i)\b([A-Z]{4})[-_. ]?(\d{3})[-_. ]?(\d{2})\b"""), " ")
            .replace(Regex("""(?i)\b([A-Z]{4})[-_. ]?(\d{5})\b"""), " ")
            .replace(Regex("""\[[^]]*]|\([^)]*\)"""), " ")
            .replace(Regex("""\b(disc|disk|cd|dvd)\s*\d+\b""", RegexOption.IGNORE_CASE), " ")
            .replace('_', ' ')
            .replace(Regex("""\s+"""), " ")
            .trim()
            .ifBlank { cleanName }
        return GameMetadata(title = title, serial = serial, serialWithCrc = serial)
    }

    private fun extractSerialFromName(value: String): String? {
        val normalized = value.uppercase(Locale.ROOT)
        val fullPattern = Regex("""\b([A-Z]{4})[-_. ]?(\d{3})[-_. ]?(\d{2})\b""")
        val compactPattern = Regex("""\b([A-Z]{4})[-_. ]?(\d{5})\b""")
        return fullPattern.find(normalized)?.let { match ->
            "${match.groupValues[1]}-${match.groupValues[2]}${match.groupValues[3]}"
        } ?: compactPattern.find(normalized)?.let { match ->
            "${match.groupValues[1]}-${match.groupValues[2]}"
        }
    }

    suspend fun saveState(slot: Int): Boolean {
        if (!isNativeLoaded || !isVmActive) return false
        return runSerial {
            try {
                NativeApp.saveStateToSlot(slot)
            } catch (_: Exception) {
                false
            }
        }
    }

    suspend fun loadState(slot: Int): Boolean {
        if (!isNativeLoaded || !isVmActive) return false
        return runSerial {
            try {
                val success = NativeApp.loadStateFromSlot(slot)
                if (success) {
                    runCatching { rebindSurface() }
                    runCatching { NativeApp.resume() }
                }
                success
            } catch (_: Exception) {
                false
            }
        }
    }

    fun hasSaveStateForGame(path: String, slot: Int): Boolean {
        if (!isNativeLoaded) return false
        if (path.startsWith("/") && !File(path).exists()) return false
        val statePath = try {
            NativeApp.getSaveStatePathForFile(path, slot)
        } catch (_: Exception) {
            null
        } ?: return false
        return File(statePath).exists()
    }

    suspend fun setRenderer(gpuType: Int) {
        val resolvedRenderer = normalizeRenderer(gpuType)
        settingsCache["EmuCore/GS:Renderer"] = resolvedRenderer.toString()
        performRuntimeOps(listOf(rendererOp(resolvedRenderer), settingOp("EmuCore/GS", "Renderer", "int", resolvedRenderer.toString())))
    }

    suspend fun setUpscaleMultiplier(multiplier: Float) {
        val normalized = normalizeUpscale(multiplier)
        settingsCache["EmuCore/GS:upscale_multiplier"] = normalized.toString()
        performRuntimeOps(listOf(upscaleOp(normalized), settingOp("EmuCore/GS", "upscale_multiplier", "float", normalized.toString())))
    }

    suspend fun setAspectRatio(type: Int) {
        val normalizedType = normalizeAspectRatio(type)
        val value = aspectRatioSettingValues.getValue(normalizedType)
        settingsCache["EmuCore/GS:AspectRatio"] = value
        performRuntimeOps(listOf(aspectOp(normalizedType)))
    }

    suspend fun setCustomDriverPath(path: String) {
        val resolvedPath = if (GpuDriverCompatibility.supportsAdrenoToolsCustomDrivers()) {
            prepareCustomDriverLibrary(path)
        } else {
            ""
        }
        settingsCache["EmuCore/GS:CustomDriverPath"] = resolvedPath
        performRuntimeOps(listOf(customDriverOp(resolvedPath)))
    }

    suspend fun setFrameLimitEnabled(enabled: Boolean) {
        setSetting("EmuCore/GS", "FrameLimitEnable", "bool", enabled.toString())
    }

    suspend fun setSkipDuplicateFrames(enabled: Boolean) {
        setSetting("EmuCore/GS", "SkipDuplicateFrames", "bool", enabled.toString())
    }

    suspend fun setTargetFps(targetFps: Int) {
        performRuntimeOps(
            buildList {
                addAll(targetFpsOps(targetFps))
                add(settingOp("EmuCore/Framerate", "NominalScalar", "float", "1.0"))
            }
        )
    }

    fun setPadButton(index: Int, range: Int, pressed: Boolean) {
        setPadButton(0, index, range, pressed)
    }

    fun setPadButton(padIndex: Int, index: Int, range: Int, pressed: Boolean) {
        if (!isNativeLoaded) return
        try {
            NativeApp.setPadButton(padIndex, index, range, pressed)
        } catch (_: Exception) { }
    }

    fun resetKeyStatus() {
        if (!isNativeLoaded || !isVmActive) return
        launchSerial {
            if (!isVmActive || !runCatching { NativeApp.hasValidVm() }.getOrDefault(false)) return@launchSerial
            try {
                NativeApp.resetKeyStatus()
            } catch (_: Exception) { }
        }
    }

    fun resetPadState(padIndex: Int) {
        if (!isNativeLoaded || !isVmActive) return
        launchSerial {
            if (!isVmActive || !runCatching { NativeApp.hasValidVm() }.getOrDefault(false)) return@launchSerial
            try {
                NativeApp.resetPadState(padIndex)
            } catch (_: Exception) { }
        }
    }

    suspend fun setPadVibration(enabled: Boolean) {
        setSetting("InputSources", "PadVibration", "bool", enabled.toString())
        runCatching { NativeApp.setPadVibration(enabled) }
    }

    fun onSurfaceCreated() {
        if (!isNativeLoaded) return
        NativeApp.setCrashContextString("emu_surface_state", "created")
        NativeApp.logCrashBreadcrumb("surfaceCreated")
        launchSerial {
            try {
                NativeApp.onNativeSurfaceCreated()
            } catch (_: Exception) { }
        }
    }

    fun onSurfaceChanged(surface: Surface, width: Int, height: Int) {
        if (!isNativeLoaded) return
        val eventVersion = ++surfaceEventVersion
        lastSurface = surface
        lastSurfaceWidth = width
        lastSurfaceHeight = height
        NativeApp.setCrashContextString("emu_surface_state", "changed")
        NativeApp.setCrashContextInt("emu_surface_width", width)
        NativeApp.setCrashContextInt("emu_surface_height", height)
        NativeApp.setCrashContextBool("emu_surface_valid", surface.isValid)
        NativeApp.logCrashBreadcrumb("surfaceChanged width=$width height=$height valid=${surface.isValid}")
        launchSerial {
            if (surfaceEventVersion != eventVersion) return@launchSerial
            try {
                NativeApp.onNativeSurfaceChanged(surface, width, height)
            } catch (_: Exception) { }
        }
    }

    fun onSurfaceDestroyed() {
        if (!isNativeLoaded) return
        val eventVersion = ++surfaceEventVersion
        NativeApp.setCrashContextString("emu_surface_state", "destroyed")
        NativeApp.logCrashBreadcrumb("surfaceDestroyed")
        runCatching { NativeApp.pause() }
        launchSerial {
            delay(250)
            if (surfaceEventVersion != eventVersion) return@launchSerial
            lastSurface = null
            lastSurfaceWidth = 0
            lastSurfaceHeight = 0
            try {
                NativeApp.onNativeSurfaceDestroyed()
            } catch (_: Exception) { }
        }
    }

    private fun rebindSurface() {
        val surface = lastSurface ?: return
        val width = lastSurfaceWidth
        val height = lastSurfaceHeight
        if (!surface.isValid || width <= 0 || height <= 0) return
        NativeApp.logCrashBreadcrumb("rebindSurface width=$width height=$height")
        try {
            NativeApp.onNativeSurfaceChanged(surface, width, height)
        } catch (_: Exception) { }
    }

    suspend fun setSetting(section: String, key: String, type: String, value: String) {
        if (!isNativeLoaded) return
        val cacheKey = "$section:$key"
        if (settingsCache[cacheKey] == value) return
        performRuntimeOps(listOf(settingOp(section, key, type, value)))
        settingsCache[cacheKey] = value
    }

    suspend fun reloadPatches() {
        if (!isNativeLoaded) return
        runSerial { NativeApp.reloadPatches() }
    }

    fun getSetting(section: String, key: String, type: String): String? {
        if (!isNativeLoaded) return null
        return try {
            fromCoreSettingValue(section, key, NativeApp.getSetting(section, key, type))
        } catch (_: Exception) {
            null
        }
    }

    private fun targetFpsOps(targetFps: Int): List<RuntimeOp> {
        if (targetFps <= 0) {
            return listOf(resetTargetFpsOp())
        }

        val manualFps = targetFps.coerceIn(20, 120).toFloat()
        return listOf(
            settingOp("EmuCore/GS", "FramerateNTSC", "float", manualFps.toString()),
            settingOp("EmuCore/GS", "FrameratePAL", "float", manualFps.toString())
        )
    }
}
