package com.sbro.emucorex.ui.emulation

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.async
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeout
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ShaderPreparationReadinessCoordinatorTest {
    @Test
    fun validatedNativeReceiptAloneSkipsTheOfferAndProbeFailuresOfferConservatively() = runBlocking {
        val gate = ShaderPreparationLifecycleGate()

        assertEquals(
            ShaderPreparationReadinessResult.Prepared,
            ShaderPreparationReadinessCoordinator.check(gate, { false }) { true }
        )
        assertEquals(
            ShaderPreparationReadinessResult.Offer,
            ShaderPreparationReadinessCoordinator.check(gate, { false }) { false }
        )
        assertEquals(
            ShaderPreparationReadinessResult.Offer,
            ShaderPreparationReadinessCoordinator.check(gate, { false }) {
                error("receipt validation unavailable")
            }
        )
    }

    @Test
    fun abortDuringProbeWaitsForProbeAndNeverAllowsGameLaunch() = runBlocking {
        val gate = ShaderPreparationLifecycleGate()
        val probeEntered = CompletableDeferred<Unit>()
        val releaseProbe = CompletableDeferred<Unit>()
        var abortRequested = false
        var cleanupSawActiveOwner: Boolean? = null

        val check = async {
            ShaderPreparationReadinessCoordinator.check(gate, { abortRequested }) {
                probeEntered.complete(Unit)
                releaseProbe.await()
                true
            }
        }
        withTimeout(2_000L) { probeEntered.await() }
        assertTrue("the synchronous native probe must own the lifecycle gate", gate.mayBeActive)

        abortRequested = true
        val cleanup = async {
            gate.withCleanup { cleanupSawActiveOwner = it }
        }
        assertFalse("route cleanup must wait until the probe returns", cleanup.isCompleted)

        releaseProbe.complete(Unit)
        val result = check.await()
        assertEquals(ShaderPreparationReadinessResult.Aborted, result)
        val allowsGameLaunch = result == ShaderPreparationReadinessResult.Prepared ||
            result == ShaderPreparationReadinessResult.Offer
        assertFalse("an aborted readiness result must never allow game launch", allowsGameLaunch)
        cleanup.await()
        assertEquals(false, cleanupSawActiveOwner)
    }
}
