package com.sbro.emucorex.data

import android.content.Context
import android.net.Uri
import com.sbro.emucorex.core.EmulatorStorage
import org.json.JSONObject
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipInputStream
import java.util.zip.ZipOutputStream
import kotlinx.coroutines.flow.first

class SettingsBackupRepository(
    private val context: Context,
    private val preferences: AppPreferences,
    private val perGameSettingsRepository: PerGameSettingsRepository,
    private val cheatRepository: CheatRepository
) {
    private val homeBackgroundRepository = HomeBackgroundRepository(context)
    private val customFontRepository = CustomFontRepository(context)

    suspend fun backup(destination: Uri): Boolean {
        return runCatching {
            context.contentResolver.openOutputStream(destination)?.use { output ->
                ZipOutputStream(output).use { zip ->
                    zip.writeJsonEntry("settings.json", preferences.exportJson())
                    zip.writeJsonEntry("per-game-settings.json", perGameSettingsRepository.exportJson())
                    zip.writeJsonEntry("cheats.json", cheatRepository.exportJson())
                    zip.writeDirectory("memory-cards", memoryCardsDir())
                    homeBackgroundRepository.installedBackground()?.let { (type, file) ->
                        zip.writeFileEntry("customization/home-background.${type.backupExtension}", file)
                    }
                    customFontRepository.installedFile()?.let { file ->
                        zip.writeFileEntry(CustomFontRepository.BACKUP_ENTRY, file)
                    }
                }
            } != null
        }.getOrDefault(false)
    }

    suspend fun restore(source: Uri): Boolean {
        return runCatching {
            var restoredBackgroundType: HomeBackgroundType? = null
            var restoredCustomFont = false
            context.contentResolver.openInputStream(source)?.use { input ->
                ZipInputStream(input).use { zip ->
                    generateSequence { zip.nextEntry }.forEach { entry ->
                        when (entry.name) {
                            "settings.json" -> preferences.importJson(JSONObject(zip.readBytes().decodeToString()))
                            "per-game-settings.json" -> perGameSettingsRepository.importJson(JSONObject(zip.readBytes().decodeToString()))
                            "cheats.json" -> cheatRepository.importJson(JSONObject(zip.readBytes().decodeToString()))
                            CustomFontRepository.BACKUP_ENTRY -> {
                                customFontRepository.restoreFromBackup(zip).getOrThrow()
                                restoredCustomFont = true
                            }
                            else -> {
                                val backgroundType = entry.name.toBackgroundBackupType()
                                if (backgroundType != null && !entry.isDirectory) {
                                    homeBackgroundRepository.restoreFromBackup(backgroundType, zip).getOrThrow()
                                    restoredBackgroundType = backgroundType
                                } else if (entry.name.startsWith("memory-cards/") && !entry.isDirectory) {
                                    val relative = entry.name.removePrefix("memory-cards/")
                                    val root = memoryCardsDir().canonicalFile
                                    val target = File(root, relative).canonicalFile
                                    require(target.path.startsWith(root.path + File.separator)) {
                                        "Invalid memory-card backup path"
                                    }
                                    target.parentFile?.mkdirs()
                                    target.outputStream().use { output -> zip.copyTo(output) }
                                }
                            }
                        }
                        zip.closeEntry()
                    }
                }
            } != null && restoredBackgroundType.let { restoredType ->
                if (restoredType != null) preferences.setHomeBackgroundType(restoredType)
                if (restoredCustomFont) {
                    preferences.setCustomFontInstalled(
                        preferences.customFontName.first() ?: CustomFontRepository.DEFAULT_DISPLAY_NAME
                    )
                }
                true
            }
        }.getOrDefault(false)
    }

    private fun memoryCardsDir(): File {
        return EmulatorStorage.memoryCardsDir(context, preferences.getEmulatorDataPathSync())
    }
}

private val HomeBackgroundType.backupExtension: String
    get() = when (this) {
        HomeBackgroundType.IMAGE -> "image"
        HomeBackgroundType.GIF -> "gif"
        HomeBackgroundType.VIDEO -> "video"
        HomeBackgroundType.NONE -> error("Default background has no backup file")
    }

private fun String.toBackgroundBackupType(): HomeBackgroundType? = when (this) {
    "customization/home-background.image" -> HomeBackgroundType.IMAGE
    "customization/home-background.gif" -> HomeBackgroundType.GIF
    "customization/home-background.video" -> HomeBackgroundType.VIDEO
    else -> null
}

private fun ZipOutputStream.writeJsonEntry(name: String, json: JSONObject) {
    putNextEntry(ZipEntry(name))
    write(json.toString(2).toByteArray())
    closeEntry()
}

private fun ZipOutputStream.writeDirectory(prefix: String, directory: File) {
    directory.listFiles().orEmpty().forEach { file ->
        if (file.isDirectory) {
            writeDirectory("$prefix/${file.name}", file)
        } else {
            putNextEntry(ZipEntry("$prefix/${file.name}"))
            file.inputStream().use { it.copyTo(this) }
            closeEntry()
        }
    }
}

private fun ZipOutputStream.writeFileEntry(name: String, file: File) {
    putNextEntry(ZipEntry(name))
    file.inputStream().use { it.copyTo(this) }
    closeEntry()
}
