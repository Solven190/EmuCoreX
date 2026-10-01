package com.sbro.emucorex.core

import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class StringsLocalizationTest {
    private val requiredKeys = listOf(
        "hardware_warning_title",
        "hardware_warning_body",
        "hardware_warning_gpu",
        "hardware_warning_recommended",
        "hardware_warning_continue"
    )

    @Test
    fun `hardware warning strings exist in every locale`() {
        val resDir = resolveResDir()
        val localeDirs = resDir
            .listFiles { file -> file.isDirectory && file.name.startsWith("values") }
            .orEmpty()
            .filter { dir -> File(dir, "strings.xml").isFile }
        assertTrue("Expected at least 18 locale string files", localeDirs.size >= 18)
        localeDirs.forEach { dir ->
            val content = File(dir, "strings.xml").readText()
            requiredKeys.forEach { key ->
                assertTrue("Missing $key in ${dir.name}/strings.xml", content.contains("name=\"$key\""))
            }
        }
    }

    private fun resolveResDir(): File {
        val candidates = listOf(
            File("src/main/res"),
            File("app/src/main/res"),
            File(System.getProperty("user.dir").orEmpty(), "src/main/res"),
            File(System.getProperty("user.dir").orEmpty(), "app/src/main/res")
        )
        return candidates.firstOrNull { it.isDirectory }
            ?: error("Could not locate the Android res directory")
    }
}
