package com.sbro.emucorex.ui.emulation

import com.sbro.emucorex.data.PerformanceOverlayMetrics
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class PerformanceOverlayLayoutTest {
    private val fullSnapshot = """
        FPS: 59.94 [P] | VPS: 60.00
        Speed: 100% | Target: 100%
        Vulkan HW
        VRAM: 24 MB | TGT: 8 MB | SRC: 4 MB
        Frame: 15.20 / 16.67 / 18.40 ms
        Queue: 2
        Res: 1280x896 NTSC Progressive
        EE: 55.2% (11.04 ms)
        GS: 57.7% (11.53 ms)
        VU: 57.4% (11.49 ms)
        SW-0: 20.0% (4.00 ms)
    """.trimIndent()

    @Test
    fun defaultMaskPreservesPreviousOverlayAndKeepsQueueHidden() {
        val layout = buildPerformanceOverlayLayout(fullSnapshot, PerformanceOverlayMetrics.DEFAULT)

        assertEquals("FPS: 59.94 [P] | VPS: 60.00", layout.mainLines[0])
        assertEquals("Speed: 100% | Target: 100%", layout.mainLines[1])
        assertTrue(layout.mainLines.any { it.startsWith("EE:") })
        assertTrue(layout.mainLines.any { it.startsWith("SW-0:") })
        assertEquals("Vulkan HW | VRAM: 24 MB | TGT: 8 MB | SRC: 4 MB", layout.bottomLines[0])
        assertFalse(layout.bottomLines.any { it.startsWith("GS Queue:") })
    }

    @Test
    fun compositeLinesAllowIndependentMetricSelection() {
        val mask = PerformanceOverlayMetrics.VPS or PerformanceOverlayMetrics.TARGET
        val layout = buildPerformanceOverlayLayout(fullSnapshot, mask)

        assertEquals(listOf("VPS: 60.00", "Target: 100%"), layout.mainLines)
        assertTrue(layout.bottomLines.isEmpty())
    }

    @Test
    fun rendererAndVramCanBeSelectedIndependently() {
        val rendererOnly = buildPerformanceOverlayLayout(fullSnapshot, PerformanceOverlayMetrics.RENDERER)
        val vramOnly = buildPerformanceOverlayLayout(fullSnapshot, PerformanceOverlayMetrics.VRAM)

        assertEquals(listOf("Vulkan HW"), rendererOnly.bottomLines)
        assertEquals(listOf("VRAM: 24 MB | TGT: 8 MB | SRC: 4 MB"), vramOnly.bottomLines)
    }

    @Test
    fun queueCanBeEnabledExplicitly() {
        val layout = buildPerformanceOverlayLayout(fullSnapshot, PerformanceOverlayMetrics.QUEUE)

        assertEquals(listOf("GS Queue: 2"), layout.bottomLines)
        assertTrue(layout.mainLines.isEmpty())
    }

    @Test
    fun unsupportedMaskBitsAreDiscarded() {
        assertEquals(0, PerformanceOverlayMetrics.sanitize(Int.MIN_VALUE))
        assertEquals(
            PerformanceOverlayMetrics.FPS,
            PerformanceOverlayMetrics.sanitize(Int.MIN_VALUE or PerformanceOverlayMetrics.FPS)
        )
    }
}
