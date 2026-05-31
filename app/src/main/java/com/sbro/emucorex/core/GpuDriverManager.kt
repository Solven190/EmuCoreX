package com.sbro.emucorex.core

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream
import java.util.zip.ZipInputStream

data class InstalledGpuDriver(
    val name: String,
    val mainLibrary: String,
    val mainLibraryPath: String,
    val isUsable: Boolean
)

class GpuDriverManager(private val context: Context) {

    fun listInstalledDrivers(): List<InstalledGpuDriver> {
        val root = driversRoot()
        if (!root.exists()) return emptyList()
        return root.listFiles()
            .orEmpty()
            .filter { it.isDirectory }
            .mapNotNull { driverDir ->
                val mainLibrary = readMainLibraryName(driverDir) ?: return@mapNotNull null
                val mainLibraryFile = File(driverDir, mainLibrary)
                InstalledGpuDriver(
                    name = driverDir.name,
                    mainLibrary = mainLibrary,
                    mainLibraryPath = mainLibraryFile.absolutePath,
                    isUsable = mainLibraryFile.isFile
                )
            }
            .sortedBy { it.name.lowercase() }
    }

    fun installFromArchive(uri: Uri): String {
        val archiveName = queryDisplayName(uri)
            ?: uri.lastPathSegment
            ?: "custom-driver.zip"
        context.contentResolver.openInputStream(uri)?.use { input ->
            return installFromArchive(input, archiveName)
        } ?: error("Could not open archive")
    }

    fun installFromArchive(file: File): String {
        file.inputStream().use { input ->
            return installFromArchive(input, file.name)
        }
    }

    fun remove(driverName: String) {
        File(driversRoot(), driverName).deleteRecursively()
    }

    fun readMainLibraryPath(driverName: String): String? {
        val driverDir = File(driversRoot(), driverName)
        val mainLibrary = readMainLibraryName(driverDir) ?: return null
        return File(driverDir, mainLibrary).absolutePath
    }

    private fun installFromArchive(input: InputStream, archiveName: String): String {
        val driverName = archiveName.substringBeforeLast('.').ifBlank { "custom-driver" }
        val targetDir = File(driversRoot(), driverName)

        if (targetDir.exists()) {
            targetDir.deleteRecursively()
        }
        targetDir.mkdirs()

        val extractedFiles = mutableListOf<String>()
        ZipInputStream(input).use { zip ->
            generateSequence { zip.nextEntry }.forEach { entry ->
                val entryName = entry.name ?: return@forEach
                if (entry.isDirectory) return@forEach
                val normalizedEntryName = entryName.replace('\\', '/').trimStart('/')
                if (normalizedEntryName.isBlank() || normalizedEntryName.contains("..")) {
                    error("Archive contains an invalid file path")
                }
                val outFile = File(targetDir, normalizedEntryName)
                val canonicalTarget = targetDir.canonicalFile
                val canonicalOutFile = outFile.canonicalFile
                if (!canonicalOutFile.toPath().startsWith(canonicalTarget.toPath())) {
                    error("Archive contains an invalid file path")
                }
                outFile.parentFile?.mkdirs()
                FileOutputStream(outFile).use { output ->
                    zip.copyTo(output)
                }
                extractedFiles += normalizedEntryName
            }
        }

        val selectedDriver = selectMainDriverFile(extractedFiles) ?: run {
            targetDir.deleteRecursively()
            error("Archive does not contain a Vulkan driver file")
        }

        File(targetDir, "driver_name.txt").writeText("$selectedDriver\n")
        return driverName
    }

    private fun driversRoot(): File = File(context.filesDir, "driver")

    private fun readMainLibraryName(driverDir: File): String? {
        val metadataFile = File(driverDir, "driver_name.txt")
        if (!metadataFile.isFile) return null
        return metadataFile.readText()
            .lineSequence()
            .map(String::trim)
            .firstOrNull { it.isNotEmpty() }
    }

    private fun selectMainDriverFile(extractedFiles: List<String>): String? {
        val sharedLibraries = extractedFiles
            .filter { it.endsWith(".so", ignoreCase = true) }
        if (sharedLibraries.isEmpty()) return null

        return sharedLibraries.firstOrNull { file ->
            val name = file.substringAfterLast('/')
            name.equals("libvulkan.so", ignoreCase = true)
        } ?: sharedLibraries.firstOrNull { file ->
            val name = file.substringAfterLast('/')
            name.startsWith("vulkan.", ignoreCase = true) || name.startsWith("libvulkan.", ignoreCase = true)
        } ?: sharedLibraries.firstOrNull { file ->
            file.contains("vulkan", ignoreCase = true)
        }
    }

    private fun queryDisplayName(uri: Uri): String? {
        val projection = arrayOf(OpenableColumns.DISPLAY_NAME)
        return context.contentResolver.query(uri, projection, null, null, null)?.use { cursor ->
            val index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            if (index >= 0 && cursor.moveToFirst()) cursor.getString(index) else null
        }
    }
}
