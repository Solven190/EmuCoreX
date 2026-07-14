package com.sbro.emucorex.ui.home

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class HomeGridLayoutTest {
    @Test
    fun portraitPhoneColumnsTrackCoverScale() {
        val compact = calculateHomeGridColumnCount(390, 844, 390, 0.60f)
        val default = calculateHomeGridColumnCount(390, 844, 390, 1.00f)
        val large = calculateHomeGridColumnCount(390, 844, 390, 1.60f)

        assertEquals(5, compact)
        assertEquals(3, default)
        assertEquals(2, large)
        assertTrue(compact > default && default > large)
    }

    @Test
    fun wideLayoutExcludesNavigationRailFromGridWidth() {
        val columns = calculateHomeGridColumnCount(1200, 800, 800, 1.00f)

        assertEquals(8, columns)
    }

    @Test
    fun gridAlwaysKeepsAtLeastOneColumn() {
        assertEquals(1, calculateHomeGridColumnCount(1, 1, 1, 10f))
    }
}
