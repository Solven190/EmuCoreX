package com.sbro.emucorex.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class UpscaleConfigTest {
    @Test
    fun buildUpscaleOptions_includesQuarterStepsThroughTenX() {
        val options = buildUpscaleOptions(nativeLabel = "Native", maxMultiplier = 10)

        assertEquals(37, options.size)
        assertEquals(100 to "Native", options.first())
        assertEquals(1000 to "10x", options.last())
        assertTrue(options.contains(525 to "5.25x"))
        assertTrue(options.contains(550 to "5.50x"))
        assertTrue(options.contains(575 to "5.75x"))
    }

    @Test
    fun normalizeUpscale_clampsToTenXAndRoundsToQuarterSteps() {
        assertEquals(10.0f, normalizeUpscale(12.0f))
        assertEquals(5.25f, normalizeUpscale(5.26f))
    }
}
