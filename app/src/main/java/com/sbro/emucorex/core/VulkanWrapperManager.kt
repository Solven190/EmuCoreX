package com.sbro.emucorex.core

import android.content.Context
import android.util.Log
import java.io.File
import java.security.MessageDigest

object VulkanWrapperManager {
    private const val TAG = "VulkanWrapperManager"
    private const val ASSET_PATH = "vulkan_wrappers/bionic/libvulkan_wrapper.so"
    private const val STUB_ASSET_PATH = "vulkan_wrappers/android_stub"
    private const val WRAPPER_DIR = "vulkan_wrappers/bionic-etc2-m2"
    private const val STUB_DIR = "android_stub"
    private const val STUB_MARKER_FILE = ".bionic_etc2_m2_runtime"
    private const val STUB_VERSION = "2026-06-01-android-wsi-surface-destroy"
    private const val WRAPPER_FILE = "libvulkan_wrapper.so"
    private const val ICD_FILE = "bionic_wrapper_icd.json"
    private const val WRAPPER_SHA256 = "5d6f142ad60289a0acec7a0367c209a2fcca0fa778e17102a73b8b7958734e43"
    private const val SUPPORTS_ANDROID_WSI = true

    private val requiredStubLibraries = listOf(
        "libc++_shared.so",
        "libcutils.so",
        "libhardware.so",
        "libnativewindow.so",
        "libadrenotools.so",
    )

    data class Install(
        val libraryFile: File,
        val icdFile: File
    )

    fun isSupportedOnThisBuild(): Boolean = SUPPORTS_ANDROID_WSI

    fun ensureBionicWrapper(context: Context): Install? {
        val target = File(context.filesDir, "$WRAPPER_DIR/$WRAPPER_FILE")
        val androidStubRoot = File(context.filesDir, STUB_DIR)
        return runCatching {
            ensureStubLibraries(context, androidStubRoot)

            if (!target.isFile || sha256(target) != WRAPPER_SHA256) {
                target.parentFile?.mkdirs()
                context.assets.open(ASSET_PATH).use { input ->
                    target.outputStream().use { output ->
                        input.copyTo(output)
                    }
                }
            }

            if (sha256(target) == WRAPPER_SHA256) {
                val missingLibraries = missingStubLibraries(androidStubRoot)
                if (missingLibraries.isNotEmpty()) {
                    Log.w(
                        TAG,
                        "Bundled Vulkan wrapper is present but cannot be enabled; missing runtime libraries: ${missingLibraries.joinToString()}"
                    )
                    return@runCatching null
                }
                if (!preloadStubLibraries(androidStubRoot)) {
                    return@runCatching null
                }

                val icdFile = File(target.parentFile, ICD_FILE)
                val icdJson = buildIcdJson(target)
                if (!icdFile.isFile || icdFile.readText() != icdJson) {
                    icdFile.writeText(icdJson)
                }
                Install(
                    libraryFile = target,
                    icdFile = icdFile
                )
            } else {
                target.delete()
                null
            }
        }.onFailure { error ->
            Log.e(TAG, "Could not prepare bundled Vulkan wrapper", error)
            target.delete()
        }.getOrNull()
    }

    private fun ensureStubLibraries(context: Context, targetDir: File) {
        targetDir.mkdirs()
        val marker = File(targetDir, STUB_MARKER_FILE)
        val shouldRefresh = runCatching { marker.readText() }.getOrNull() != STUB_VERSION
        requiredStubLibraries.forEach { library ->
            val target = File(targetDir, library)
            val assetPath = "$STUB_ASSET_PATH/$library"
            if (shouldRefresh || !target.isFile || target.length() == 0L) {
                context.assets.open(assetPath).use { input ->
                    target.outputStream().use { output ->
                        input.copyTo(output)
                    }
                }
            }
        }
        marker.writeText(STUB_VERSION)
    }

    private fun missingStubLibraries(targetDir: File): List<String> =
        requiredStubLibraries.filter { library -> !File(targetDir, library).isFile }

    private fun preloadStubLibraries(targetDir: File): Boolean {
        requiredStubLibraries.forEach { library ->
            val file = File(targetDir, library)
            val error = runCatching { NativeApp.loadGlobalLibrary(file.absolutePath) }
                .onFailure { throwable ->
                    Log.e(TAG, "Could not call native loader for Vulkan wrapper runtime library ${file.absolutePath}", throwable)
                }
                .getOrElse { "native loader call failed: ${it.message}" }
            if (error != null) {
                Log.e(TAG, "Could not load Vulkan wrapper runtime library ${file.absolutePath}: $error")
                return false
            }
        }
        Log.i(TAG, "Loaded Vulkan wrapper runtime libraries from ${targetDir.absolutePath}")
        return true
    }

    private fun buildIcdJson(wrapperFile: File): String {
        val libraryPath = wrapperFile.absolutePath.replace("\\", "\\\\")
        return """
            {
              "file_format_version": "1.0.0",
              "ICD": {
                "library_path": "$libraryPath",
                "api_version": "1.3.0"
              }
            }
        """.trimIndent()
    }

    private fun sha256(file: File): String {
        val digest = MessageDigest.getInstance("SHA-256")
        file.inputStream().use { input ->
            val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
            while (true) {
                val read = input.read(buffer)
                if (read <= 0) break
                digest.update(buffer, 0, read)
            }
        }
        return digest.digest().joinToString(separator = "") { byte -> "%02x".format(byte) }
    }
}
