package com.sbro.emucorex.ui.emulation

import com.sbro.emucorex.data.PerformanceOverlayMetrics

internal data class PerformanceOverlayLayout(
    val mainLines: List<String>,
    val bottomLines: List<String>
)

internal fun buildPerformanceOverlayLayout(
    text: String,
    metricsMask: Int
): PerformanceOverlayLayout {
    fun isRendererLine(line: String): Boolean {
        return line.endsWith(" HW") ||
            line.endsWith(" SW") ||
            line.endsWith(" Null") ||
            line.contains(" HW |") ||
            line.contains(" SW |") ||
            line.contains(" Null |")
    }

    fun metricForSegment(segment: String): Int = when {
        segment.startsWith("FPS:") -> PerformanceOverlayMetrics.FPS
        segment.startsWith("VPS:") -> PerformanceOverlayMetrics.VPS
        segment.startsWith("Speed:") -> PerformanceOverlayMetrics.SPEED
        segment.startsWith("Target:") -> PerformanceOverlayMetrics.TARGET
        else -> 0
    }

    fun filterLine(line: String): String? {
        if (line.startsWith("FPS:") || line.startsWith("VPS:") ||
            line.startsWith("Speed:") || line.startsWith("Target:")
        ) {
            return line.split(" | ")
                .filter { segment ->
                    val metric = metricForSegment(segment)
                    metric == 0 || PerformanceOverlayMetrics.isEnabled(metricsMask, metric)
                }
                .joinToString(" | ")
                .ifBlank { null }
        }

        val metric = when {
            isRendererLine(line) -> PerformanceOverlayMetrics.RENDERER
            line.startsWith("VRAM:") -> PerformanceOverlayMetrics.VRAM
            line.startsWith("Frame:") -> PerformanceOverlayMetrics.FRAME_TIME
            line.startsWith("Queue:") -> PerformanceOverlayMetrics.QUEUE
            line.startsWith("Res:") -> PerformanceOverlayMetrics.RESOLUTION
            line.startsWith("EE:") -> PerformanceOverlayMetrics.EE
            line.startsWith("GS:") -> PerformanceOverlayMetrics.GS
            line.startsWith("VU:") -> PerformanceOverlayMetrics.VU
            line.startsWith("SW-") -> PerformanceOverlayMetrics.SOFTWARE_THREADS
            else -> 0
        }
        if (metric != 0 && !PerformanceOverlayMetrics.isEnabled(metricsMask, metric)) return null
        return if (line.startsWith("Queue:")) {
            line.replaceFirst("Queue:", "GS Queue:")
        } else {
            line
        }
    }

    val filtered = text.lineSequence()
        .map { it.trim() }
        .filter { it.isNotEmpty() }
        .mapNotNull(::filterLine)
        .toList()

    val topLines = filtered.filter { line ->
        line.startsWith("FPS:") || line.startsWith("VPS:") ||
            line.startsWith("Speed:") || line.startsWith("Target:")
    }
    val processorLines = filtered.filter { line ->
        line.startsWith("EE:") || line.startsWith("GS:") ||
            line.startsWith("VU:") || line.startsWith("SW-")
    }
    val rendererLine = filtered.firstOrNull(::isRendererLine)
    val vramLine = filtered.firstOrNull { it.startsWith("VRAM:") }
    val bottomLines = buildList {
        when {
            rendererLine != null && vramLine != null -> add("$rendererLine | $vramLine")
            rendererLine != null -> add(rendererLine)
            vramLine != null -> add(vramLine)
        }
        addAll(filtered.filter { line ->
            line.startsWith("Frame:") || line.startsWith("GS Queue:") || line.startsWith("Res:")
        })
    }
    val knownLines = (topLines + processorLines + bottomLines).toSet()
    val unknownLines = filtered.filterNot { line ->
        line in knownLines || line == rendererLine || line == vramLine
    }

    return PerformanceOverlayLayout(
        mainLines = topLines + processorLines + unknownLines,
        bottomLines = bottomLines
    )
}
