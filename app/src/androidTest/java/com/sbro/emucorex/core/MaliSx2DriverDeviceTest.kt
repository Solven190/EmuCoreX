package com.sbro.emucorex.core

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.sbro.emucorex.data.AppPreferences
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.runBlocking
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File
import java.security.MessageDigest

@RunWith(AndroidJUnit4::class)
class MaliSx2DriverDeviceTest {
    private val context = ApplicationProvider.getApplicationContext<Context>()
    private val savedSelection get() = File(context.cacheDir, "malisx2-device-test-original-selection.json")

    @Test
    fun useSystemDriverForComparison() = runBlocking {
        assertFalse(NativeApp.hasValidVm())
        AppPreferences(context).setGpuDriverType(0)
    }

    @Test
    fun useSelectedMaliDriverForComparison() = runBlocking {
        assertFalse(NativeApp.hasValidVm())
        val prefs = AppPreferences(context)
        val path = requireNotNull(prefs.customDriverPath.first())
        assertEquals("libvulkan_malisx2.so", File(path).name)
        assertEquals(path, GpuDriverManager(context).resolveUsableDriverPath(path))
        prefs.setGpuDriverType(1)
    }

    @Test
    fun installOfficialArchiveAndSelectForDeviceValidation() = runBlocking {
        assertFalse("Close games before validating a driver package", NativeApp.hasValidVm())
        val prefs = AppPreferences(context)
        // Keep the original selection until the explicit cleanup test runs.
        if (!savedSelection.exists()) {
            savedSelection.writeText(JSONObject().apply {
                put("type", prefs.gpuDriverType.first())
                put("path", prefs.customDriverPath.first() ?: JSONObject.NULL)
            }.toString(), Charsets.UTF_8)
        }
        val driver = RemoteGpuDriver(
            id = "mali-sx2-0.1.4", name = "MaliSX2 0.1.4", variant = "MaliSX2",
            gpu = "Mali Valhall v9/v11", description = "Official device validation fixture",
            recommended = false,
            downloadUrl = "https://github.com/bmdhacks/malisx2/releases/download/v0.1.4/malisx2-0.1.4-fdbf88e508.zip",
            sourceUrl = "https://github.com/bmdhacks/malisx2/releases/tag/v0.1.4",
            credits = "ARMSX2 / bmdhacks (MIT)", sizeBytes = 3786355L
        )
        val archive = GpuDriverCatalogRepository(context).downloadDriver(driver) {}
        assertEquals("35cec77da26ba9bc23531f423f34ba741f0fe82e212feb72ce0967023d6ee3d9", sha256(archive))
        val manager = GpuDriverManager(context)
        val installedName = manager.installFromArchive(archive, GpuDriverVendor.MALI)
        val path = requireNotNull(manager.readMainLibraryPath(installedName))
        assertEquals("libvulkan_malisx2.so", File(path).name)
        assertEquals("b27bfa701a9e727aa4bd52ecc858320ae930a22c2343635d58925f5f632dd7de", sha256(File(path)))
        assertEquals(GpuDriverVendor.MALI, GpuDriverCompatibility.installedDriverVendor(File(path)))
        assertEquals(path, manager.resolveUsableDriverPath(path))
        prefs.setCustomDriverPath(path)
        prefs.setGpuDriverType(1)
        assertEquals(1, prefs.gpuDriverType.first())
        assertEquals(path, prefs.customDriverPath.first())
    }

    @Test
    fun restoreOriginalSelectionAfterDeviceValidation() = runBlocking {
        assertFalse("Close games before restoring driver selection", NativeApp.hasValidVm())
        if (!savedSelection.exists()) return@runBlocking
        val original = JSONObject(savedSelection.readText(Charsets.UTF_8))
        val prefs = AppPreferences(context)
        prefs.setCustomDriverPath(if (original.isNull("path")) null else original.getString("path"))
        prefs.setGpuDriverType(original.getInt("type"))
        assertTrue(savedSelection.delete())
    }

    private fun sha256(file: File): String {
        val digest = MessageDigest.getInstance("SHA-256")
        file.inputStream().use { input ->
            val buffer = ByteArray(65536)
            while (true) {
                val count = input.read(buffer)
                if (count < 0) break
                digest.update(buffer, 0, count)
            }
        }
        return digest.digest().joinToString("") { "%02x".format(it.toInt() and 255) }
    }
}
