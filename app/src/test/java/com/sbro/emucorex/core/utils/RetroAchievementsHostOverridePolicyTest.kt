package com.sbro.emucorex.core.utils

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class RetroAchievementsHostOverridePolicyTest {
    @Test
    fun acceptsOnlyCanonicalLoopbackHttpHostsWithExplicitPorts() {
        val cases = mapOf(
            "127.0.0.1:1" to "http://127.0.0.1:1",
            " http://127.0.0.1:8080/ " to "http://127.0.0.1:8080",
            "LOCALHOST:65535" to "http://localhost:65535",
            "http://localhost:1234" to "http://localhost:1234",
            "HTTP://LOCALHOST:4321" to "http://localhost:4321"
        )

        cases.forEach { (input, expected) ->
            assertEquals(input, expected, RetroAchievementsHostOverridePolicy.normalizeHost(input))
        }
    }

    @Test
    fun rejectsRemoteEncryptedMalformedAndAmbiguousHosts() {
        val rejected = listOf(
            null,
            "",
            " ",
            "https://127.0.0.1:8080",
            "http://192.168.1.10:8080",
            "http://example.com:8080",
            "http://127.0.0.1",
            "http://127.0.0.1:0",
            "http://127.0.0.1:65536",
            "http://user@127.0.0.1:8080",
            "http://127.0.0.1:8080/api",
            "http://127.0.0.1:8080//",
            "http://127.0.0.1:8080/%2e",
            "http://127.0.0.1:8080?query=1",
            "http://127.0.0.1:8080?",
            "http://127.0.0.1:8080#fragment",
            "http://127.0.0.1:8080#",
            "http://127.0.0.1:bad",
            "127.0.0.1:8080@evil.example",
            "http://localhost.:8080",
            "http://[::1]:8080",
            "http://2130706433:8080",
            "http://0177.0.0.1:8080",
            "http://127.0.0.1:8080\\evil",
            "http://127.0.0.1:8080\nHost: evil.example",
            "ftp://localhost:8080"
        )

        rejected.forEach { input ->
            assertNull(input, RetroAchievementsHostOverridePolicy.normalizeHost(input))
        }
    }

    @Test
    fun proxyAlwaysForcesEffectiveHardcoreOff() {
        assertTrue(RetroAchievementsHostOverridePolicy.effectiveHardcore(true, overrideActive = false))
        assertFalse(RetroAchievementsHostOverridePolicy.effectiveHardcore(false, overrideActive = false))
        assertFalse(RetroAchievementsHostOverridePolicy.effectiveHardcore(true, overrideActive = true))
        assertFalse(RetroAchievementsHostOverridePolicy.effectiveHardcore(false, overrideActive = true))
    }

    @Test
    fun clearEncodesExactSavedHardcoreValueAcrossColdStarts() {
        assertEquals(1, RetroAchievementsHostOverridePolicy.hardcoreRestoreMode(true))
        assertEquals(0, RetroAchievementsHostOverridePolicy.hardcoreRestoreMode(false))
        assertEquals(-1, RetroAchievementsHostOverridePolicy.hardcoreRestoreMode(null))
    }
}
