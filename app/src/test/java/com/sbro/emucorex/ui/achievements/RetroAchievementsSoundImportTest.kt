package com.sbro.emucorex.ui.achievements

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class RetroAchievementsSoundImportTest {
    @Test
    fun acceptsCanonicalRiffWaveHeader() {
        val header = byteArrayOf(
            'R'.code.toByte(), 'I'.code.toByte(), 'F'.code.toByte(), 'F'.code.toByte(),
            36, 0, 0, 0,
            'W'.code.toByte(), 'A'.code.toByte(), 'V'.code.toByte(), 'E'.code.toByte()
        )

        assertTrue(isValidAchievementWavHeader(header))
    }

    @Test
    fun rejectsTruncatedAndNonWaveFiles() {
        assertFalse(isValidAchievementWavHeader(byteArrayOf('R'.code.toByte(), 'I'.code.toByte())))
        assertFalse(isValidAchievementWavHeader("RIFF0000MP3 ".encodeToByteArray()))
        assertFalse(isValidAchievementWavHeader("OggS0000WAVE".encodeToByteArray()))
    }

    @Test
    fun acceptsSizeLimitBoundaryAndRejectsOverflow() {
        val limit = 16L * 1024L * 1024L

        assertTrue(isAchievementSoundSizeAllowed(0))
        assertTrue(isAchievementSoundSizeAllowed(limit))
        assertFalse(isAchievementSoundSizeAllowed(limit + 1))
        assertFalse(isAchievementSoundSizeAllowed(-1))
    }
}
