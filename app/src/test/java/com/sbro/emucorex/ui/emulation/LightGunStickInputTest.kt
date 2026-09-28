package com.sbro.emucorex.ui.emulation

import org.junit.Assert.assertEquals
import org.junit.Test

class LightGunStickInputTest {
    @Test
    fun touchAimRespectsBothStickInversions() {
        assertEquals(
            -0.4f to 0.7f,
            adjustLightGunStickInput(0.4f, -0.7f, invertX = true, invertY = true, sensitivity = 1f)
        )
    }

    @Test
    fun touchAimUsesSensitivityWithoutExceedingAnalogRange() {
        assertEquals(
            1f to -0.5f,
            adjustLightGunStickInput(0.8f, -0.25f, invertX = false, invertY = false, sensitivity = 2f)
        )
    }
}
