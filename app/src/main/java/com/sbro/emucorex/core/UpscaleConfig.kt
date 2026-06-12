package com.sbro.emucorex.core

import kotlin.math.roundToInt

const val UPSCALE_MIN = 1.0f
const val UPSCALE_MAX = 5.0f

private const val UPSCALE_STEP = 0.25f
private const val UPSCALE_NATIVE_MULTIPLIER = 1.0f
private const val UPSCALE_MAX_MULTIPLIER = 5.0f

fun normalizeUpscale(value: Float, maxMultiplier: Int = UPSCALE_MAX_MULTIPLIER.roundToInt()): Float {
    val max = maxMultiplier.coerceAtLeast(UPSCALE_NATIVE_MULTIPLIER.roundToInt())
        .coerceAtMost(UPSCALE_MAX_MULTIPLIER.roundToInt())
        .toFloat()
    val stepped = (value / UPSCALE_STEP).roundToInt() * UPSCALE_STEP
    return stepped.coerceIn(UPSCALE_NATIVE_MULTIPLIER, max)
}

fun upscaleMultiplierValue(value: Float): Int = upscaleMultiplierKey(normalizeUpscale(value))

fun upscaleMultiplierKey(value: Float): Int = (normalizeUpscale(value) * 100f).roundToInt()

fun upscaleKeyToMultiplier(value: Int): Float = normalizeUpscale(value.toFloat() / 100f)

fun formatUpscaleLabel(value: Float, nativeLabel: String): String {
    val normalized = normalizeUpscale(value)
    return if (normalized == UPSCALE_NATIVE_MULTIPLIER) {
        nativeLabel
    } else if (normalized == normalized.roundToInt().toFloat()) {
        "${normalized.roundToInt()}x"
    } else {
        "${"%.2f".format(java.util.Locale.US, normalized)}x"
    }
}

fun buildUpscaleOptions(nativeLabel: String, maxMultiplier: Int = UPSCALE_MAX_MULTIPLIER.roundToInt()): List<Pair<Int, String>> {
    val max = maxMultiplier.coerceAtLeast(UPSCALE_NATIVE_MULTIPLIER.roundToInt())
        .coerceAtMost(UPSCALE_MAX_MULTIPLIER.roundToInt())
        .toFloat()
    val steps = ((max - UPSCALE_NATIVE_MULTIPLIER) / UPSCALE_STEP).roundToInt()
    return (0..steps).map { index ->
        val multiplier = UPSCALE_NATIVE_MULTIPLIER + (index * UPSCALE_STEP)
        upscaleMultiplierKey(multiplier) to formatUpscaleLabel(multiplier, nativeLabel)
    }
}
