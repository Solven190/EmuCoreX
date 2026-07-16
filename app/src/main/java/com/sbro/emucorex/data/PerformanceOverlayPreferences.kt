package com.sbro.emucorex.data

object PerformanceOverlayMetrics {
    const val FPS = 1 shl 0
    const val VPS = 1 shl 1
    const val SPEED = 1 shl 2
    const val TARGET = 1 shl 3
    const val RENDERER = 1 shl 4
    const val VRAM = 1 shl 5
    const val FRAME_TIME = 1 shl 6
    const val QUEUE = 1 shl 7
    const val RESOLUTION = 1 shl 8
    const val EE = 1 shl 9
    const val GS = 1 shl 10
    const val VU = 1 shl 11
    const val SOFTWARE_THREADS = 1 shl 12

    const val ALL = FPS or VPS or SPEED or TARGET or RENDERER or VRAM or FRAME_TIME or QUEUE or
        RESOLUTION or EE or GS or VU or SOFTWARE_THREADS

    // Queue was intentionally hidden by the previous overlay, so keep the default appearance unchanged.
    const val DEFAULT = ALL and QUEUE.inv()

    fun sanitize(mask: Int): Int = mask and ALL

    fun isEnabled(mask: Int, metric: Int): Boolean = sanitize(mask) and metric != 0
}
