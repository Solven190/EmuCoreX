package com.sbro.emucorex.core

import org.junit.Assert.assertEquals
import org.junit.Test

class AndroidGamePerformanceTest {
    @Test
    fun reportsLoadingWhileEmulationStarts() {
        assertEquals(
            AndroidGamePhase.Loading,
            resolveAndroidGamePhase(
                isStarting = true,
                isRunning = false,
                isPaused = false,
                showMenu = false
            )
        )
    }

    @Test
    fun reportsGameplayOnlyForActiveUninterruptedEmulation() {
        assertEquals(
            AndroidGamePhase.Gameplay,
            resolveAndroidGamePhase(
                isStarting = false,
                isRunning = true,
                isPaused = false,
                showMenu = false
            )
        )
        assertEquals(
            AndroidGamePhase.Idle,
            resolveAndroidGamePhase(
                isStarting = false,
                isRunning = true,
                isPaused = true,
                showMenu = false
            )
        )
        assertEquals(
            AndroidGamePhase.Idle,
            resolveAndroidGamePhase(
                isStarting = false,
                isRunning = true,
                isPaused = false,
                showMenu = true
            )
        )
    }
}
