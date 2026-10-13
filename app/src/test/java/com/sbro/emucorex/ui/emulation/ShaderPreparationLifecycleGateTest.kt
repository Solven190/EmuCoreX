package com.sbro.emucorex.ui.emulation

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.async
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeout
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ShaderPreparationLifecycleGateTest {
    @Test
    fun ownershipIsVisibleBeforeSuspendingNativeAdmission() = runBlocking {
        val gate = ShaderPreparationLifecycleGate()
        val admissionEntered = CompletableDeferred<Unit>()
        val finishAdmission = CompletableDeferred<Unit>()
        val admission = async {
            gate.admitAndObserve(
                admit = {
                    admissionEntered.complete(Unit)
                    finishAdmission.await()
                    true
                },
                observe = { accepted -> accepted }
            )
        }

        withTimeout(2_000L) { admissionEntered.await() }
        assertTrue("an in-flight start must already be owned", gate.mayBeActive)
        finishAdmission.complete(Unit)
        assertTrue(admission.await())
    }

    @Test
    fun cleanupCannotPassAQueuedStartOrResumeAdmission() = runBlocking {
        val gate = ShaderPreparationLifecycleGate()
        val admissionEntered = CompletableDeferred<Unit>()
        val finishObservation = CompletableDeferred<Unit>()
        val observed = CompletableDeferred<Unit>()
        val admission = async {
            gate.admitAndObserve(
                admit = {
                    admissionEntered.complete(Unit)
                    true
                },
                observe = {
                    finishObservation.await()
                    observed.complete(Unit)
                }
            )
        }
        withTimeout(2_000L) { admissionEntered.await() }

        var cleanupSawOwner: Boolean? = null
        val cleanup = async {
            gate.withCleanup { cleanupSawOwner = it }
        }
        assertEquals(null, cleanupSawOwner)
        finishObservation.complete(Unit)
        withTimeout(2_000L) { observed.await() }
        admission.await()
        cleanup.await()

        assertEquals(true, cleanupSawOwner)
    }

    @Test
    fun routeDisposalPreservesEmulationAcrossConfigurationChanges() {
        assertFalse(ShaderPreparationRouteExitPolicy.shouldStopEmulation(isChangingConfigurations = true))
        assertTrue(ShaderPreparationRouteExitPolicy.shouldStopEmulation(isChangingConfigurations = false))
    }

    @Test
    fun recreatedSameLaunchIsIgnoredWhileStartingRunningOrPreparing() {
        val current = launchRequest("/games/current.iso")
        assertTrue(ShaderPreparationLaunchRetention.shouldIgnoreRecreatedLaunch(
            current, current, isStarting = true, isRunning = false, preparationVisible = false
        ))
        assertTrue(ShaderPreparationLaunchRetention.shouldIgnoreRecreatedLaunch(
            current, current, isStarting = false, isRunning = true, preparationVisible = false
        ))
        assertTrue(ShaderPreparationLaunchRetention.shouldIgnoreRecreatedLaunch(
            current, current, isStarting = false, isRunning = false, preparationVisible = true
        ))
    }

    @Test
    fun launchRetentionDoesNotBlockDifferentIntentOrAnInactiveSameIntent() {
        val current = launchRequest("/games/current.iso")
        assertFalse(ShaderPreparationLaunchRetention.shouldIgnoreRecreatedLaunch(
            current,
            launchRequest("/games/next.iso"),
            isStarting = false,
            isRunning = true,
            preparationVisible = false
        ))
        assertFalse(ShaderPreparationLaunchRetention.shouldIgnoreRecreatedLaunch(
            current,
            current,
            isStarting = false,
            isRunning = false,
            preparationVisible = false
        ))
    }

    private fun launchRequest(path: String) = ShaderPreparationLaunchRequest(
        path = path,
        slotToLoad = null,
        bootToBios = false,
        bootSmokeProbe = false,
        autotestMode = false,
        enableEeRecompilerOverride = null,
        enableIopRecompilerOverride = null,
        enableVu0RecompilerOverride = null,
        enableVu1RecompilerOverride = null,
        enableFastmemOverride = null,
        enableMtvuOverride = null,
        rendererOverride = null,
        gsDumpFrames = null,
        gsDumpDelayMs = null,
        vu0FamilyMaskOverride = null,
        vu1FamilyMaskOverride = null,
        restoredAfterProcessDeath = false
    )
}
