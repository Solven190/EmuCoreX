package com.sbro.emucorex.ui.emulation

import com.sbro.emucorex.core.ShaderPreparationStatus
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Test

class ShaderPreparationRunCoordinatorTest {
    @Test
    fun abortBetweenPublishingDecisionAndAwaitCompletesThePendingChoice() = runBlocking {
        val gate = ShaderPreparationChoiceGate()
        gate.reset()
        var published = false

        val choice = gate.await {
            published = true
            gate.abort()
        }

        assertEquals(true, published)
        assertEquals(LaunchPreparationChoice.Abort, choice)
    }

    @Test
    fun abortBeforeDecisionPublicationDoesNotCreateAnUncompletableWait() = runBlocking {
        val gate = ShaderPreparationChoiceGate()
        gate.reset()
        gate.abort()
        var published = false

        val choice = gate.await { published = true }

        assertEquals(LaunchPreparationChoice.Abort, choice)
        assertEquals(false, published)
    }

    @Test
    fun rejectedStartWithIdleSnapshotReturnsWithoutTreatingIdleAsRunning() = runBlocking {
        var reads = 0
        var waits = 0
        val result = ShaderPreparationRunCoordinator.awaitKnownTerminal(
            startAccepted = false,
            readStatus = { reads++; idleStatus() },
            cancellationRequested = { false },
            cancel = { error("No job should be cancelled") },
            waitBetweenPolls = { waits++ }
        )

        assertSame(ShaderPreparationWaitResult.NoWorker, result)
        assertEquals(1, reads)
        assertEquals(0, waits)
    }

    @Test
    fun acceptedStartDoesNotReleaseLaunchUntilAParsedTerminalSnapshot() = runBlocking {
        val running = status(
            state = ShaderPreparationStatus.State.RUNNING,
            total = 2,
            completed = 1,
            alreadyAvailable = 1,
            finished = false
        )
        val completed = status(
            state = ShaderPreparationStatus.State.COMPLETED,
            total = 2,
            completed = 2,
            finished = true,
            checkpointSaved = true
        )
        val snapshots = listOf<ShaderPreparationStatus?>(null, running, completed)
        var reads = 0
        var waits = 0
        val observed = mutableListOf<ShaderPreparationStatus>()
        val result = ShaderPreparationRunCoordinator.awaitKnownTerminal(
            startAccepted = true,
            readStatus = { snapshots[reads++] },
            cancellationRequested = { false },
            cancel = { error("Preparation was not cancelled") },
            waitBetweenPolls = { waits++ },
            onStatus = observed::add
        )

        assertEquals(ShaderPreparationWaitResult.Terminal(completed), result)
        assertEquals(listOf(running, completed), observed)
        assertEquals(2, waits)
    }

    @Test
    fun abortWhileStatusIsUnknownCancelsOnceKnownAndStillWaitsForTerminal() = runBlocking {
        val active = status(state = ShaderPreparationStatus.State.RUNNING, total = 1)
        val cancelling = active.copy(
            state = ShaderPreparationStatus.State.CANCELLING,
            cancelRequested = true
        )
        val cancelled = active.copy(
            state = ShaderPreparationStatus.State.CANCELLED,
            cancelled = 1,
            finished = true,
            cancelRequested = true
        )
        val snapshots = listOf<ShaderPreparationStatus?>(null, active, cancelling, cancelled)
        var reads = 0
        var waits = 0
        val cancelledIds = mutableListOf<Long>()
        val result = ShaderPreparationRunCoordinator.awaitKnownTerminal(
            startAccepted = true,
            readStatus = { snapshots[reads++] },
            cancellationRequested = { true },
            cancel = { cancelledIds += it },
            waitBetweenPolls = { waits++ }
        )

        assertEquals(ShaderPreparationWaitResult.Terminal(cancelled), result)
        assertEquals(listOf(42L), cancelledIds)
        assertEquals(3, waits)
    }

    @Test
    fun rejectedStartWithMissingSnapshotWaitsUntilAnExistingWorkerIsKnownAndTerminal() = runBlocking {
        val active = status(state = ShaderPreparationStatus.State.RUNNING, total = 1)
        val completed = active.copy(
            state = ShaderPreparationStatus.State.COMPLETED,
            completed = 1,
            finished = true,
            checkpointSaved = true
        )
        val snapshots = listOf<ShaderPreparationStatus?>(null, active, completed)
        var reads = 0
        var waits = 0
        val result = ShaderPreparationRunCoordinator.awaitKnownTerminal(
            startAccepted = false,
            readStatus = { snapshots[reads++] },
            cancellationRequested = { false },
            cancel = { error("Preparation was not cancelled") },
            waitBetweenPolls = { waits++ }
        )

        assertEquals(ShaderPreparationWaitResult.Terminal(completed), result)
        assertEquals(2, waits)
    }

    @Test
    fun acceptedStartWithIdleSnapshotDoesNotClaimThereIsNoWorker() = runBlocking {
        val running = status(state = ShaderPreparationStatus.State.RUNNING, total = 1)
        val completed = running.copy(
            state = ShaderPreparationStatus.State.COMPLETED,
            completed = 1,
            finished = true,
            checkpointSaved = true
        )
        val snapshots = listOf<ShaderPreparationStatus?>(idleStatus(), running, completed)
        var reads = 0
        var waits = 0
        val result = ShaderPreparationRunCoordinator.awaitKnownTerminal(
            startAccepted = true,
            readStatus = { snapshots[reads++] },
            cancellationRequested = { false },
            cancel = { error("Preparation was not cancelled") },
            waitBetweenPolls = { waits++ }
        )

        assertEquals(ShaderPreparationWaitResult.Terminal(completed), result)
        assertEquals(2, waits)
    }

    @Test
    fun exhaustedUnknownStatusCannotBeTreatedAsPermissionToLaunch() = runBlocking {
        var shutdowns = 0
        val result = ShaderPreparationRunCoordinator.awaitKnownTerminal(
            startAccepted = true,
            readStatus = { null },
            cancellationRequested = { false },
            cancel = { error("No job ID is known") },
            waitBetweenPolls = {},
            shutdownIfStatusRemainsUnknown = { shutdowns++; true },
            maximumUnknownPolls = 3
        )

        assertSame(ShaderPreparationWaitResult.StatusUnavailable, result)
        assertEquals(0, shutdowns)
    }

    @Test
    fun failedRuntimeShutdownDoesNotReleaseUnknownWorkerOwnership() = runBlocking {
        var shutdowns = 0
        val result = ShaderPreparationRunCoordinator.awaitKnownTerminal(
            startAccepted = true,
            readStatus = { null },
            cancellationRequested = { true },
            cancel = { error("No job ID is known") },
            waitBetweenPolls = {},
            shutdownIfStatusRemainsUnknown = { shutdowns++; false },
            maximumUnknownPolls = 1
        )

        assertSame(ShaderPreparationWaitResult.StatusUnavailable, result)
        assertEquals(1, shutdowns)
    }

    @Test
    fun confirmedRuntimeShutdownIsTheOnlyUnknownStatusCleanupCompletion() = runBlocking {
        val result = ShaderPreparationRunCoordinator.awaitKnownTerminal(
            startAccepted = true,
            readStatus = { null },
            cancellationRequested = { true },
            cancel = { error("No job ID is known") },
            waitBetweenPolls = {},
            shutdownIfStatusRemainsUnknown = { true },
            maximumUnknownPolls = 1
        )

        assertSame(ShaderPreparationWaitResult.RuntimeShutdown, result)
    }

    private fun idleStatus() = ShaderPreparationStatus(
        jobId = 0L,
        attempt = 0,
        state = ShaderPreparationStatus.State.IDLE,
        error = ShaderPreparationStatus.Error.NONE,
        total = 0,
        completed = 0,
        alreadyAvailable = 0,
        failed = 0,
        cancelled = 0,
        inFlight = 0,
        finished = false,
        checkpointSaved = false,
        cancelRequested = false,
        adapterName = ""
    )

    private fun status(
        state: ShaderPreparationStatus.State,
        total: Int,
        completed: Int = 0,
        alreadyAvailable: Int = 0,
        cancelled: Int = 0,
        finished: Boolean = false,
        checkpointSaved: Boolean = false,
        cancelRequested: Boolean = false
    ) = ShaderPreparationStatus(
        jobId = 42L,
        attempt = 1,
        state = state,
        error = ShaderPreparationStatus.Error.NONE,
        total = total,
        completed = completed,
        alreadyAvailable = alreadyAvailable,
        failed = 0,
        cancelled = cancelled,
        inFlight = 0,
        finished = finished,
        checkpointSaved = checkpointSaved,
        cancelRequested = cancelRequested,
        adapterName = "test adapter"
    )
}
