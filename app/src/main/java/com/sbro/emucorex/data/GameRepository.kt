package com.sbro.emucorex.data

import android.content.Context
import android.net.Uri
import android.provider.DocumentsContract
import android.util.Log
import androidx.core.net.toUri
import androidx.documentfile.provider.DocumentFile
import com.sbro.emucorex.core.BiosValidator
import com.sbro.emucorex.core.DocumentPathResolver
import com.sbro.emucorex.core.EmulatorBridge
import com.sbro.emucorex.data.pcsx2.Pcsx2CompatibilityEntry
import com.sbro.emucorex.data.pcsx2.Pcsx2CompatibilityRepository
import java.io.File

data class GameItem(
    val title: String,
    val path: String,
    val fileName: String,
    val fileSize: Long,
    val lastModified: Long,
    val coverArtPath: String? = null,
    val serial: String? = null,
    val pcsx2Compatibility: Pcsx2CompatibilityEntry? = null
)

class GameRepository {

    companion object {
        private const val TAG = "GameRepository"
        private val SUPPORTED_EXTENSIONS = setOf("iso", "bin", "img", "mdf", "gz", "cso", "zso", "chd", "elf")
        private val COVER_EXTENSIONS = setOf("jpg", "jpeg", "png", "webp")
        private val COVER_DIRECTORY_NAMES = setOf("covers", "cover", "art", "artwork", "boxart", "box art")

        internal fun parentDocumentId(documentId: String): String? {
            val separatorIndex = documentId.lastIndexOf('/')
            if (separatorIndex >= 0) return documentId.substring(0, separatorIndex)

            val volumeSeparatorIndex = documentId.indexOf(':')
            return if (volumeSeparatorIndex >= 0 && volumeSeparatorIndex < documentId.lastIndex) {
                documentId.substring(0, volumeSeparatorIndex + 1)
            } else {
                null
            }
        }

        internal fun relativeDocumentSegments(rootDocumentId: String, targetDocumentId: String): List<String>? {
            if (targetDocumentId == rootDocumentId) return emptyList()
            val prefix = if (rootDocumentId.endsWith(':')) rootDocumentId else "$rootDocumentId/"
            if (!targetDocumentId.startsWith(prefix)) return null
            return targetDocumentId.removePrefix(prefix).split('/').filter(String::isNotBlank)
        }
    }

    fun scanDirectory(path: String, context: Context): List<GameItem> {
        return scanDirectory(path, context, emptyMap())
    }

    fun scanDirectory(
        path: String,
        context: Context,
        cachedGamesByPath: Map<String, GameItem>,
        shouldAbort: () -> Boolean = { false }
    ): List<GameItem> {
        val dir = File(path)
        if (!dir.exists() || !dir.isDirectory) return emptyList()
        return scanLocalDirectory(dir, context, cachedGamesByPath, shouldAbort).sortedBy { it.title.lowercase() }
    }

    fun scanDirectoryFromUri(uri: Uri, context: Context): List<GameItem> {
        return scanDirectoryFromUri(uri, context, emptyMap())
    }

    fun scanDirectoryFromUri(
        uri: Uri,
        context: Context,
        cachedGamesByPath: Map<String, GameItem>,
        shouldAbort: () -> Boolean = { false }
    ): List<GameItem> {
        val docFile = DocumentFile.fromTreeUri(context, uri)
            ?: return emptyList()
        return scanDocumentFile(docFile, context, cachedGamesByPath, shouldAbort).sortedBy { it.title.lowercase() }
    }

    fun findCoverForGame(
        path: String,
        context: Context,
        serial: String? = null,
        title: String? = null
    ): String? {
        return if (path.startsWith("content://")) {
            runCatching { findDocumentCover(path, context, serial, title) }
                .onFailure { error -> Log.w(TAG, "Unable to find cover for document URI: $path", error) }
                .getOrNull()
        } else {
            findLocalCover(path, context, serial, title)
        }
    }

    fun downloadCoverForGame(game: GameItem, context: Context): String? {
        if (!game.coverArtPath.isNullOrBlank()) {
            val coverFile = File(game.coverArtPath)
            if (coverFile.exists()) {
                return game.coverArtPath
            }
        }

        return game.serial?.let { serial ->
            CoverArtRepository(context).downloadCover(serial)
        }
    }

    private fun scanLocalDirectory(
        dir: File,
        context: Context,
        cachedGamesByPath: Map<String, GameItem>,
        shouldAbort: () -> Boolean
    ): List<GameItem> {
        val items = mutableListOf<GameItem>()
        val children = dir.listFiles().orEmpty()
        val coverCandidates = buildLocalCoverCandidates(children)
        val coverRepository = CoverArtRepository(context)
        val customCoverRepository = CustomGameCoverRepository(context)
        val compatibilityRepository = Pcsx2CompatibilityRepository(context)

        children.forEach { file ->
            if (shouldAbort()) return items
            when {
                file.isDirectory && normalizeBaseName(file.name) !in COVER_DIRECTORY_NAMES -> {
                    items.addAll(scanLocalDirectory(file, context, cachedGamesByPath, shouldAbort))
                }

                file.isFile && file.extension.lowercase() in SUPPORTED_EXTENSIONS -> {
                    val cachedGame = cachedGamesByPath[file.absolutePath]
                    val canReuseCachedMetadata = cachedGame != null &&
                        cachedGame.fileSize == file.length() &&
                        cachedGame.lastModified == file.lastModified() &&
                        cachedGame.fileName == file.name &&
                        !cachedGame.serial.isNullOrBlank()
                    val metadata = if (canReuseCachedMetadata) {
                        com.sbro.emucorex.core.GameMetadata(cachedGame.title, cachedGame.serial)
                    } else {
                        EmulatorBridge.getGameMetadata(file.absolutePath)
                    }
                    if (BiosValidator.isLikelyBiosLibraryEntry(file.name, metadata.title, metadata.serial, file.length())) {
                        return@forEach
                    }

                    val cleanTitle = EmulatorBridge.cleanGameDisplayTitle(metadata.title, file.name)
                    val compatibility = compatibilityRepository.findBest(metadata.serial, cleanTitle)
                    val serial = metadata.serial ?: compatibility?.serial
                    val title = compatibility?.title ?: cleanTitle
                    items += GameItem(
                        title = title,
                        path = file.absolutePath,
                        fileName = file.name,
                        fileSize = file.length(),
                        lastModified = file.lastModified(),
                        coverArtPath = customCoverRepository.findCustomCoverPath(file.absolutePath)
                            ?: coverRepository.findCachedCoverPath(serial)
                            ?: cachedGame?.coverArtPath?.takeIf { File(it).exists() }
                            ?: coverCandidates[normalizeBaseName(file.nameWithoutExtension)]?.absolutePath
                            ?: coverCandidates[normalizeBaseName(cleanGameName(title))]?.absolutePath,
                        serial = serial,
                        pcsx2Compatibility = compatibility
                    )
                }
            }
        }

        return items
    }

    private fun scanDocumentFile(
        docFile: DocumentFile,
        context: Context,
        cachedGamesByPath: Map<String, GameItem>,
        shouldAbort: () -> Boolean
    ): List<GameItem> {
        val items = mutableListOf<GameItem>()
        val children = docFile.listFiles()
        val coverCandidates = buildDocumentCoverCandidates(children)
        val coverRepository = CoverArtRepository(context)
        val customCoverRepository = CustomGameCoverRepository(context)
        val compatibilityRepository = Pcsx2CompatibilityRepository(context)

        for (file in children) {
            if (shouldAbort()) return items
            if (file.isDirectory) {
                val folderName = DocumentPathResolver.normalizeDisplayName(file.name.orEmpty())
                if (normalizeBaseName(folderName) !in COVER_DIRECTORY_NAMES) {
                    items.addAll(scanDocumentFile(file, context, cachedGamesByPath, shouldAbort))
                }
            } else if (file.isFile) {
                val rawName = file.name ?: continue
                val uriPath = file.uri.toString()
                val name = DocumentPathResolver.getDisplayName(context, uriPath)
                    .ifBlank { DocumentPathResolver.normalizeDisplayName(rawName) }
                val ext = name.substringAfterLast('.', "").lowercase()
                if (ext !in SUPPORTED_EXTENSIONS) continue

                val cachedGame = cachedGamesByPath[uriPath]
                val canReuseCachedMetadata = cachedGame != null &&
                    cachedGame.fileSize == file.length() &&
                    cachedGame.lastModified == file.lastModified() &&
                    cachedGame.fileName == name &&
                    !cachedGame.serial.isNullOrBlank()
                val metadata = if (canReuseCachedMetadata) {
                    com.sbro.emucorex.core.GameMetadata(cachedGame.title, cachedGame.serial)
                } else {
                    EmulatorBridge.getGameMetadata(uriPath)
                }

                if (BiosValidator.isLikelyBiosLibraryEntry(name, metadata.title, metadata.serial, file.length())) {
                    continue
                }

                val cleanTitle = cleanScannedTitle(metadata.title, name)
                val compatibility = compatibilityRepository.findBest(metadata.serial, cleanTitle)
                val serial = metadata.serial ?: compatibility?.serial
                val title = compatibility?.title ?: cleanTitle
                items += GameItem(
                    title = title,
                    path = uriPath,
                    fileName = name,
                    fileSize = file.length(),
                    lastModified = file.lastModified(),
                    coverArtPath = customCoverRepository.findCustomCoverPath(uriPath)
                        ?: coverRepository.findCachedCoverUri(serial)
                        ?: cachedGame?.coverArtPath
                        ?: coverCandidates[normalizeBaseName(name.substringBeforeLast('.'))]?.uri?.toString()
                        ?: coverCandidates[normalizeBaseName(cleanGameName(title))]?.uri?.toString(),
                    serial = serial,
                    pcsx2Compatibility = compatibility
                )
            }
        }

        return items
    }

    private fun findLocalCover(path: String, context: Context, serial: String?, title: String?): String? {
        val file = File(path)
        val parent = file.parentFile ?: return null
        val baseName = normalizeBaseName(file.nameWithoutExtension)
        val titleKey = normalizeBaseName(cleanGameName(title ?: EmulatorBridge.getGameTitle(path)))
        val coverCandidates = buildLocalCoverCandidates(parent.listFiles().orEmpty())
        return CustomGameCoverRepository(context).findCustomCoverPath(path)
            ?: CoverArtRepository(context).findCachedCoverPath(serial)
            ?: coverCandidates[baseName]?.absolutePath
            ?: coverCandidates[titleKey]?.absolutePath
    }

    private fun findDocumentCover(path: String, context: Context, serial: String?, title: String?): String? {
        CustomGameCoverRepository(context).findCustomCoverPath(path)?.let { return it }
        CoverArtRepository(context).findCachedCoverUri(serial)?.let { return it }

        val uri = path.toUri()
        val document = DocumentFile.fromSingleUri(context, uri) ?: return null
        val parent = findDocumentParent(context, uri) ?: return null
        val baseName = normalizeBaseName(document.name.orEmpty().substringBeforeLast('.'))
        val titleKey = normalizeBaseName(cleanGameName(title ?: document.name.orEmpty().substringBeforeLast('.')))
        val coverCandidates = buildDocumentCoverCandidates(parent.listFiles())
        return coverCandidates[baseName]?.uri?.toString()
            ?: coverCandidates[titleKey]?.uri?.toString()
    }

    private fun findDocumentParent(context: Context, documentUri: Uri): DocumentFile? {
        val documentId = DocumentsContract.getDocumentId(documentUri)
        val parentId = parentDocumentId(documentId) ?: return null
        val persistedTrees = context.contentResolver.persistedUriPermissions
            .asSequence()
            .filter { permission -> permission.isReadPermission && permission.uri.authority == documentUri.authority }
            .mapNotNull { permission ->
                val rootId = runCatching { DocumentsContract.getTreeDocumentId(permission.uri) }.getOrNull()
                    ?: return@mapNotNull null
                Triple(permission.uri, rootId, relativeDocumentSegments(rootId, parentId))
            }
            .filter { (_, _, segments) -> segments != null }
            .sortedByDescending { (_, rootId, _) -> rootId.length }
            .toList()

        for ((treeUri, _, nullableSegments) in persistedTrees) {
            var current = DocumentFile.fromTreeUri(context, treeUri) ?: continue
            var found = true
            for (segment in nullableSegments.orEmpty()) {
                current = current.findFile(segment) ?: run {
                    found = false
                    break
                }
            }
            if (found) return current
        }

        return null
    }

    private fun normalizeBaseName(value: String): String {
        return value.lowercase()
            .replace('_', ' ')
            .replace('-', ' ')
            .replace(Regex("""\[[^]]*]|\([^)]*\)"""), " ")
            .replace(Regex("""\b(disc|disk|cd|dvd)\s*[0-9]+\b"""), " ")
            .replace(Regex("""\b\(?(usa|eur|europe|japan|jpn|beta|demo|proto|prototype|rev\s*[a-z0-9]+)\)?\b"""), " ")
            .replace(Regex("\\s+"), " ")
            .trim()
    }

    private fun cleanGameName(value: String): String {
        return value.substringBeforeLast('.')
            .replace(Regex("""\s+"""), " ")
            .trim()
    }

    private fun cleanScannedTitle(rawTitle: String, displayName: String): String {
        return EmulatorBridge.cleanGameDisplayTitle(rawTitle, displayName)
    }

    private fun buildLocalCoverCandidates(children: Array<out File>): Map<String, File> {
        val directFiles = children.filter { it.isFile && it.extension.lowercase() in COVER_EXTENSIONS }
        val nestedCoverFiles = children
            .filter { it.isDirectory && normalizeBaseName(it.name) in COVER_DIRECTORY_NAMES }
            .flatMap { it.listFiles().orEmpty().asIterable() }
            .filter { it.isFile && it.extension.lowercase() in COVER_EXTENSIONS }

        return (directFiles + nestedCoverFiles).associateBy { normalizeBaseName(cleanGameName(it.nameWithoutExtension)) }
    }

    private fun buildDocumentCoverCandidates(
        children: Array<DocumentFile>
    ): Map<String, DocumentFile> {
        val directFiles = children
            .filter {
                it.isFile &&
                    DocumentPathResolver.normalizeDisplayName(it.name.orEmpty())
                        .substringAfterLast('.', "")
                        .lowercase() in COVER_EXTENSIONS
            }
        val nestedCoverFiles = children
            .filter {
                it.isDirectory &&
                    normalizeBaseName(DocumentPathResolver.normalizeDisplayName(it.name.orEmpty())) in COVER_DIRECTORY_NAMES
            }
            .flatMap { it.listFiles().asIterable() }
            .filter {
                it.isFile &&
                    DocumentPathResolver.normalizeDisplayName(it.name.orEmpty())
                        .substringAfterLast('.', "")
                        .lowercase() in COVER_EXTENSIONS
            }

        return (directFiles + nestedCoverFiles).associateBy {
            normalizeBaseName(cleanGameName(DocumentPathResolver.normalizeDisplayName(it.name.orEmpty()).substringBeforeLast('.')))
        }
    }
}
