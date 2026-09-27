package com.sbro.emucorex.ui.emulation

/** Apply the same touch-stick inversion and sensitivity used by ordinary analog input. */
internal fun adjustLightGunStickInput(
    x: Float,
    y: Float,
    invertX: Boolean,
    invertY: Boolean,
    sensitivity: Float
): Pair<Float, Float> {
    val scale = sensitivity.coerceIn(0.5f, 2f)
    return ((if (invertX) -x else x) * scale).coerceIn(-1f, 1f) to
        ((if (invertY) -y else y) * scale).coerceIn(-1f, 1f)
}
