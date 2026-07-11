package com.sbro.emucorex.core

import android.content.Context
import androidx.core.net.toUri
import androidx.documentfile.provider.DocumentFile
import java.io.File

object SetupValidator {
    private val supportedGameExtensions = setOf("iso", "bin", "img", "mdf", "gz", "cso", "zso", "chd", "elf")
    private const val MAX_GAME_READ_PROBE_FILES = 24
    private const val MAX_GAME_READ_PROBE_DIRECTORIES = 96

    fun isGameFolderPresentForStartup(context: Context, rawPath: String?): Boolean {
        if (rawPath.isNullOrBlank()) return false

        return if (rawPath.startsWith("content://")) {
            val root = DocumentFile.fromTreeUri(context, rawPath.toUri()) ?: return false
            runCatching { root.exists() && root.isDirectory }.getOrDefault(false)
        } else {
            val dir = File(rawPath)
            if (DocumentPathResolver.findAccessibleTreeUriForRawPath(context, rawPath) != null) {
                return true
            }
            dir.exists() && dir.isDirectory
        }
    }

    fun isAnyGameFolderPresentForStartup(context: Context, rawPaths: List<String>): Boolean =
        rawPaths.any { isGameFolderPresentForStartup(context, it) }

    fun hasCoreReadableGameFile(context: Context, rawPaths: List<String>): Boolean =
        rawPaths.any { hasCoreReadableGameFile(context, it) }

    fun isGameFolderAccessible(context: Context, rawPath: String?): Boolean {
        if (rawPath.isNullOrBlank()) return false

        return if (rawPath.startsWith("content://")) {
            val root = DocumentFile.fromTreeUri(context, rawPath.toUri()) ?: return false
            runCatching { root.isDirectory && root.exists() }.getOrDefault(false)
        } else {
            val dir = File(rawPath)
            if (DocumentPathResolver.isScopedStorageExternalPath(rawPath)) {
                val migratedUri = DocumentPathResolver.findAccessibleTreeUriForRawPath(context, rawPath)
                if (migratedUri != null) {
                    val root = DocumentFile.fromTreeUri(context, migratedUri) ?: return false
                    return runCatching { root.isDirectory && root.exists() }.getOrDefault(false)
                }
                return false
            }
            dir.exists() && dir.isDirectory
        }
    }

    fun hasCoreReadableGameFile(context: Context, rawPath: String?): Boolean {
        if (!isGameFolderAccessible(context, rawPath)) return false
        rawPath ?: return false

        return if (rawPath.startsWith("content://")) {
            val root = DocumentFile.fromTreeUri(context, rawPath.toUri()) ?: return false
            findReadableDocumentGame(context, root, ProbeBudget()) != null
        } else {
            val dir = File(rawPath)
            if (dir.isDirectory) {
                findReadableLocalGame(context, dir, ProbeBudget()) != null
            } else {
                false
            }
        }
    }

    private fun findReadableLocalGame(context: Context, dir: File, budget: ProbeBudget): String? {
        if (!budget.tryEnterDirectory()) return null
        val children = dir.listFiles().orEmpty()
        for (child in children) {
            if (child.isDirectory) {
                findReadableLocalGame(context, child, budget)?.let { return it }
            } else if (child.isFile && child.extension.lowercase() in supportedGameExtensions) {
                if (!budget.tryCheckFile()) return null
                if (isLaunchPathReadable(context, child.absolutePath)) return child.absolutePath
            }
        }
        return null
    }

    private fun findReadableDocumentGame(context: Context, root: DocumentFile, budget: ProbeBudget): String? {
        if (!budget.tryEnterDirectory()) return null
        val children = runCatching { root.listFiles() }.getOrDefault(emptyArray())
        for (child in children) {
            if (child.isDirectory) {
                findReadableDocumentGame(context, child, budget)?.let { return it }
            } else if (child.isFile) {
                val displayName = child.name.orEmpty().ifBlank {
                    DocumentPathResolver.getDisplayName(context, child.uri.toString())
                }
                val extension = displayName.substringAfterLast('.', "").lowercase()
                if (extension !in supportedGameExtensions) continue
                if (!budget.tryCheckFile()) return null
                val uriPath = child.uri.toString()
                if (isLaunchPathReadable(context, uriPath)) return uriPath
            }
        }
        return null
    }

    private fun isLaunchPathReadable(context: Context, rawGamePath: String): Boolean {
        val preparedPath = DocumentPathResolver.prepareGameLaunchPath(context, rawGamePath) ?: return false
        return if (preparedPath.startsWith("content://")) {
            runCatching {
                context.contentResolver.openInputStream(preparedPath.toUri())?.use { stream ->
                    stream.read(ByteArray(1))
                    true
                } ?: false
            }.getOrDefault(false)
        } else {
            val file = File(preparedPath)
            file.isFile && file.canRead() && file.length() > 0L
        }
    }

    private class ProbeBudget {
        private var checkedFiles = 0
        private var checkedDirectories = 0

        fun tryCheckFile(): Boolean {
            if (checkedFiles >= MAX_GAME_READ_PROBE_FILES) return false
            checkedFiles++
            return true
        }

        fun tryEnterDirectory(): Boolean {
            if (checkedDirectories >= MAX_GAME_READ_PROBE_DIRECTORIES) return false
            checkedDirectories++
            return true
        }
    }
}
