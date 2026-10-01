package com.sbro.emucorex.core

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class HardwareWarningPolicyTest {
    @Test
    fun `shows only once for red tier devices`() {
        assertTrue(HardwareWarningPolicy.shouldShow(GpuTier.RED, warningAlreadyShown = false))
        assertFalse(HardwareWarningPolicy.shouldShow(GpuTier.RED, warningAlreadyShown = true))
    }

    @Test
    fun `never shows for yellow green or unknown tiers`() {
        assertFalse(HardwareWarningPolicy.shouldShow(GpuTier.YELLOW, warningAlreadyShown = false))
        assertFalse(HardwareWarningPolicy.shouldShow(GpuTier.GREEN, warningAlreadyShown = false))
        assertFalse(HardwareWarningPolicy.shouldShow(GpuTier.UNKNOWN, warningAlreadyShown = false))
    }
}
