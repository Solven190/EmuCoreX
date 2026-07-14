package com.sbro.emucorex.core

import android.content.Context
import java.io.File
import java.io.FileOutputStream

object EmulatorStorage {
    private val runtimeSubdirectories = listOf(
        "sstates",
        "memcards",
        "textures",
        "cheats",
        "patches",
        "logs"
    )

    private fun defaultRoot(context: Context): File {
        return context.getExternalFilesDir(null) ?: context.filesDir
    }

    private fun root(context: Context, customRootPath: String? = null): File {
        val customRoot = customRootPath
            ?.takeIf { it.isNotBlank() }
            ?.let(::File)
        if (customRoot != null && prepareRoot(customRoot)) {
            return customRoot
        }

        return defaultRoot(context).apply { prepareRoot(this) }
    }

    fun prepareCustomDataRoot(customRootPath: String?): Boolean {
        val customRoot = customRootPath
            ?.takeIf { it.isNotBlank() }
            ?.let(::File)
            ?: return false
        return prepareRoot(customRoot)
    }

    fun saveStatesDir(context: Context, customRootPath: String? = null): File =
        File(root(context, customRootPath), "sstates").apply { mkdirs() }

    fun memoryCardsDir(context: Context, customRootPath: String? = null): File =
        File(root(context, customRootPath), "memcards").apply { mkdirs() }

    fun texturesDir(context: Context, customRootPath: String? = null): File =
        File(root(context, customRootPath), "textures").apply { mkdirs() }

    fun cheatsDir(context: Context, customRootPath: String? = null): File =
        File(root(context, customRootPath), "cheats").apply { mkdirs() }

    fun patchesDir(context: Context, customRootPath: String? = null): File =
        File(root(context, customRootPath), "patches").apply { mkdirs() }

    fun logDir(context: Context, customRootPath: String? = null): File =
        File(root(context, customRootPath), "logs").apply { mkdirs() }

    fun appStateDir(context: Context): File = File(root(context), "app-state").apply { mkdirs() }

    fun importedCheatsDir(context: Context): File = File(appStateDir(context), "imported-cheats").apply { mkdirs() }

    private fun prepareRoot(root: File): Boolean {
        return runCatching {
            if (!root.exists() && !root.mkdirs()) return false
            if (!root.isDirectory) return false

            for (subdirectory in runtimeSubdirectories) {
                val dir = File(root, subdirectory)
                if (!dir.exists() && !dir.mkdirs()) return false
                if (!dir.isDirectory) return false
            }

            if (!canWriteProbe(root)) return false
            runtimeSubdirectories.forEach { subdirectory ->
                if (!canWriteProbe(File(root, subdirectory))) return false
            }
            true
        }.getOrDefault(false)
    }

    private fun canWriteProbe(directory: File): Boolean {
        val probe = File(directory, ".emucorex-write-test")
        return runCatching {
            FileOutputStream(probe, false).use { stream ->
                stream.write(1)
            }
            if (probe.exists() && !probe.delete()) {
                probe.deleteOnExit()
            }
            true
        }.getOrDefault(false)
    }
}
