package com.sbro.emucorex.core

import android.content.Context
import java.io.File

object EmulatorStorage {

    private fun defaultRoot(context: Context): File {
        return context.getExternalFilesDir(null) ?: context.filesDir
    }

    private fun root(context: Context, customRootPath: String? = null): File {
        val customRoot = customRootPath
            ?.takeIf { it.isNotBlank() }
            ?.let(::File)
        return (customRoot ?: defaultRoot(context)).apply { mkdirs() }
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

    fun backupsDir(context: Context): File = File(root(context), "backups").apply { mkdirs() }

    fun appStateDir(context: Context): File = File(root(context), "app-state").apply { mkdirs() }

    fun importedCheatsDir(context: Context): File = File(appStateDir(context), "imported-cheats").apply { mkdirs() }

    fun dataRoot(context: Context, customRootPath: String? = null): File = root(context, customRootPath)

    fun defaultDataRoot(context: Context): File = defaultRoot(context).apply { mkdirs() }
}
