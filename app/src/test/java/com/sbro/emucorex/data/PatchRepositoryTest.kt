package com.sbro.emucorex.data

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class PatchRepositoryTest {

    @Test
    fun `serial prefixed patch files are matched`() {
        assertTrue(PatchRepository.patchFileNameMatchesSerial("SLUS-20062_ABCD1234.pnach", "SLUS-20062"))
        assertTrue(PatchRepository.patchFileNameMatchesSerial("slus-20062.pnach", "SLUS-20062"))
        assertTrue(PatchRepository.patchFileNameMatchesSerial("SLUS-20062_any_label.pnach", "SLUS-20062"))
    }

    @Test
    fun `punctuation variants of the serial are matched`() {
        assertTrue(PatchRepository.patchFileNameMatchesSerial("SLUS_200.62.pnach", "SLUS-20062"))
        assertTrue(PatchRepository.patchFileNameMatchesSerial("SLUS_200.62_ABCD1234.pnach", "SLUS-20062"))
    }

    @Test
    fun `other games and unrelated files are not matched`() {
        assertFalse(PatchRepository.patchFileNameMatchesSerial("SLUS-20063_ABCD1234.pnach", "SLUS-20062"))
        assertFalse(PatchRepository.patchFileNameMatchesSerial("SLUS-200620_ABCD1234.pnach", "SLUS-20062"))
        assertFalse(PatchRepository.patchFileNameMatchesSerial("ABCD1234.pnach", "SLUS-20062"))
        assertFalse(PatchRepository.patchFileNameMatchesSerial("SLUS-20062.txt", "SLUS-20062"))
        assertFalse(PatchRepository.patchFileNameMatchesSerial("readme.txt", "SLUS-20062"))
    }

    @Test
    fun `blank serial never matches`() {
        assertFalse(PatchRepository.patchFileNameMatchesSerial("SLUS-20062.pnach", ""))
        assertFalse(PatchRepository.patchFileNameMatchesSerial("SLUS-20062.pnach", "   "))
        assertEquals(null, PatchRepository.normalizeSerial("   "))
    }

    @Test
    fun `crc only patch files are matched through the disc crc`() {
        assertTrue(PatchRepository.patchFileNameMatchesGame("ABCD1234.pnach", null, "ABCD1234"))
        assertTrue(PatchRepository.patchFileNameMatchesGame("ABCD1234_extra.pnach", null, "ABCD1234"))
        assertTrue(PatchRepository.patchFileNameMatchesGame("SLUS-20062_ABCD1234.pnach", "SLUS-20062", "ABCD1234"))
        assertFalse(PatchRepository.patchFileNameMatchesGame("DEADBEEF.pnach", null, "ABCD1234"))
        assertEquals("ABCD1234", PatchRepository.normalizeCrc("abcd1234"))
        assertEquals(null, PatchRepository.normalizeCrc("XYZ"))
    }

    @Test
    fun `patch notice reports anything only when something is active`() {
        val notice = ActivePatchNotice(
            widescreen = false,
            noInterlacing = false,
            cheats = false,
            userPatchCount = 0
        )
        assertFalse(notice.hasAnything)
        assertTrue(notice.copy(userPatchCount = 2).hasAnything)
        assertTrue(notice.copy(widescreen = true).hasAnything)
        assertTrue(notice.copy(cheats = true).hasAnything)
    }
}
