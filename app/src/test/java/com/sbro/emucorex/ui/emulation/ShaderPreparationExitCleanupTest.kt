package com.sbro.emucorex.ui.emulation

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.cancel
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeout
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class ShaderPreparationExitCleanupTest {
    @Test
    fun pendingOfferAndActiveWorkerBothRequireExitCleanup() {
        assertEquals(true, ShaderPreparationExitCleanup.isRequired(false, ShaderPreparationUiPhase.Offer))
        assertEquals(true, ShaderPreparationExitCleanup.isRequired(true, ShaderPreparationUiPhase.Hidden))
        assertEquals(false, ShaderPreparationExitCleanup.isRequired(false, ShaderPreparationUiPhase.Hidden))
    }

    @Test
    fun exitCleanupRunsOutsideTheViewModelOwnerScope(): Unit = runBlocking {
        val ownerJob = Job()
        val started = CompletableDeferred<Unit>()
        val release = CompletableDeferred<Unit>()
        val completed = CompletableDeferred<Unit>()
        val ownerScope = CoroutineScope(ownerJob + Dispatchers.IO)
        val cleanupJob = ShaderPreparationExitCleanup.scheduleIfRequired(required = true, ownerScope = ownerScope) {
            started.complete(Unit)
            release.await()
            completed.complete(Unit)
        }

        assertNotNull(cleanupJob)
        withTimeout(2_000L) { started.await() }
        ownerJob.cancel()
        release.complete(Unit)
        withTimeout(2_000L) { completed.await() }
        cleanupJob?.join()
        Unit
    }

    @Test
    fun hiddenPreparationDoesNotScheduleUnneededCleanup() {
        val job = ShaderPreparationExitCleanup.scheduleIfRequired(
            required = false,
            ownerScope = CoroutineScope(Job() + Dispatchers.IO)
        ) {
            error("No cleanup should be scheduled")
        }

        assertNull(job)
    }
}
