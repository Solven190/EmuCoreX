package com.sbro.emucorex.data

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import com.sbro.emucorex.core.EmulatorStorage
import java.io.File
import java.util.Locale
import java.util.zip.ZipInputStream

data class TexturePackInfo(
    val serial: String,
    val replacementCount: Int,
    val dumpCount: Int,
    val sizeBytes: Long,
    val lastModifiedAt: Long
)

data class TexturePackSummary(
    val rootPath: String,
    val packs: List<TexturePackInfo>,
    val totalReplacementCount: Int,
    val totalDumpCount: Int,
    val totalSizeBytes: Long
)

data class TextureImportResult(
    val success: Boolean,
    val importedFiles: Int = 0,
    val importedSerials: Set<String> = emptySet(),
    val message: String? = null
)

class TexturePackRepository(
    private val context: Context,
    private val preferences: AppPreferences
) {
    private val textureExtensions = setOf("png", "dds")
    private val serialPattern = Regex("[A-Z]{4}[-_ ]?\\d{5}", RegexOption.IGNORE_CASE)

    fun listPacks(): TexturePackSummary {
        val root = texturesRoot()
        val packs = root.listFiles()
            ?.asSequence()
            ?.filter { it.isDirectory }
            ?.mapNotNull(::buildPackInfo)
            ?.sortedBy { it.serial.lowercase(Locale.US) }
            ?.toList()
            .orEmpty()

        return TexturePackSummary(
            rootPath = root.absolutePath,
            packs = packs,
            totalReplacementCount = packs.sumOf { it.replacementCount },
            totalDumpCount = packs.sumOf { it.dumpCount },
            totalSizeBytes = packs.sumOf { it.sizeBytes }
        )
    }

    fun importPackZip(uri: Uri): TextureImportResult {
        val displayName = displayName(uri)
        val fallbackSerial = findSerial(displayName)
        val importedSerials = linkedSetOf<String>()
        var importedFiles = 0

        val input = context.contentResolver.openInputStream(uri)
            ?: return TextureImportResult(success = false, message = "Unable to open selected file.")

        input.use { stream ->
            ZipInputStream(stream.buffered()).use { zip ->
                while (true) {
                    val entry = zip.nextEntry ?: break
                    try {
                        if (entry.isDirectory) continue
                        val cleanParts = cleanZipPath(entry.name)
                        if (cleanParts.isEmpty()) continue
                        if (!isTextureFile(cleanParts.last())) continue

                        val serial = serialFromParts(cleanParts) ?: fallbackSerial ?: continue
                        val relativeParts = replacementRelativePath(cleanParts, serial)
                        if (relativeParts.isEmpty()) continue

                        val target = safeChild(replacementsDir(serial), relativeParts)
                            ?: return TextureImportResult(success = false, message = "Invalid texture path in archive.")
                        target.parentFile?.mkdirs()
                        target.outputStream().use { out -> zip.copyTo(out) }
                        importedFiles++
                        importedSerials += serial
                    } finally {
                        zip.closeEntry()
                    }
                }
            }
        }

        return if (importedFiles > 0) {
            TextureImportResult(
                success = true,
                importedFiles = importedFiles,
                importedSerials = importedSerials
            )
        } else {
            TextureImportResult(
                success = false,
                message = "No PNG or DDS replacement textures were found in the archive."
            )
        }
    }

    fun deletePack(serial: String): Boolean {
        return gameDir(serial).deleteRecursively()
    }

    fun clearDumps(serial: String): Boolean {
        val dumps = dumpsDir(serial)
        if (!dumps.exists()) return true
        return dumps.deleteRecursively() && dumps.mkdirs()
    }

    private fun buildPackInfo(folder: File): TexturePackInfo? {
        val serial = normalizeSerial(folder.name) ?: return null
        val replacementDir = File(folder, "replacements")
        val dumpDir = File(folder, "dumps")
        val replacementFiles = textureFiles(replacementDir)
        val dumpFiles = textureFiles(dumpDir)
        val allFiles = replacementFiles + dumpFiles
        return TexturePackInfo(
            serial = serial,
            replacementCount = replacementFiles.size,
            dumpCount = dumpFiles.size,
            sizeBytes = allFiles.sumOf { it.length() },
            lastModifiedAt = allFiles.maxOfOrNull { it.lastModified() } ?: folder.lastModified()
        )
    }

    private fun texturesRoot(): File {
        return EmulatorStorage.texturesDir(context, preferences.getEmulatorDataPathSync())
    }

    private fun gameDir(serial: String): File {
        return File(texturesRoot(), serial).apply { mkdirs() }
    }

    private fun replacementsDir(serial: String): File {
        return File(gameDir(serial), "replacements").apply { mkdirs() }
    }

    private fun dumpsDir(serial: String): File {
        return File(gameDir(serial), "dumps").apply { mkdirs() }
    }

    private fun textureFiles(root: File): List<File> {
        if (!root.exists()) return emptyList()
        return root.walkTopDown()
            .filter { it.isFile && isTextureFile(it.name) }
            .toList()
    }

    private fun isTextureFile(name: String): Boolean {
        return name.substringAfterLast('.', "").lowercase(Locale.US) in textureExtensions
    }

    private fun cleanZipPath(path: String): List<String> {
        return path.replace('\\', '/')
            .split('/')
            .map { it.trim() }
            .filter { it.isNotEmpty() && it != "." && it != ".." && !it.contains(':') }
    }

    private fun serialFromParts(parts: List<String>): String? {
        return parts.firstNotNullOfOrNull(::findSerial)
    }

    private fun findSerial(text: String?): String? {
        if (text.isNullOrBlank()) return null
        val match = serialPattern.find(text) ?: return null
        return normalizeSerial(match.value)
    }

    private fun normalizeSerial(raw: String): String? {
        val compact = raw.trim().uppercase(Locale.US).replace('_', '-').replace(' ', '-')
        val match = serialPattern.find(compact) ?: return null
        val value = match.value.replace('_', '-').replace(' ', '-')
        return if ('-' in value) value else "${value.take(4)}-${value.drop(4)}"
    }

    private fun replacementRelativePath(parts: List<String>, serial: String): List<String> {
        val normalizedParts = parts.map { part -> normalizeSerial(part) ?: part }
        val serialIndex = normalizedParts.indexOfFirst { it.equals(serial, ignoreCase = true) }
        val replacementIndex = normalizedParts.indexOfFirst { it.equals("replacements", ignoreCase = true) }
        val startIndex = when {
            replacementIndex >= 0 -> replacementIndex + 1
            serialIndex >= 0 -> serialIndex + 1
            else -> 0
        }
        return parts.drop(startIndex)
    }

    private fun safeChild(root: File, relativeParts: List<String>): File? {
        val rootCanonical = root.canonicalFile
        val target = relativeParts.fold(rootCanonical) { current, part -> File(current, part) }.canonicalFile
        return if (target.path.startsWith(rootCanonical.path + File.separator)) target else null
    }

    private fun displayName(uri: Uri): String? {
        return runCatching {
            context.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)
                ?.use { cursor ->
                    if (cursor.moveToFirst()) {
                        cursor.getString(cursor.getColumnIndexOrThrow(OpenableColumns.DISPLAY_NAME))
                    } else {
                        null
                    }
                }
        }.getOrNull() ?: uri.lastPathSegment
    }
}
