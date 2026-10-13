package com.sbro.emucorex.ui.emulation

import com.sbro.emucorex.core.ShaderPreparationStatus

internal sealed class ShaderPreparationWaitResult {
    data class Terminal(val status: ShaderPreparationStatus) : ShaderPreparationWaitResult()
    object NoWorker : ShaderPreparationWaitResult()
    object StatusUnavailable : ShaderPreparationWaitResult()
    object RuntimeShutdown : ShaderPreparationWaitResult()
}

internal enum class LaunchPreparationChoice {
    Prepare,
    Skip,
    Resume,
    ContinueToGame,
    Abort
}

/** Makes route-abort completion safe even when it races with publishing the decision UI. */
internal class ShaderPreparationChoiceGate {
    private val lock = Any()
    private var aborted = false
    private var pending: kotlinx.coroutines.CompletableDeferred<LaunchPreparationChoice>? = null

    fun reset() {
        synchronized(lock) {
            check(pending == null) { "Cannot reset a pending shader preparation choice" }
            aborted = false
        }
    }

    fun choose(choice: LaunchPreparationChoice) {
        synchronized(lock) {
            pending?.complete(choice)
        }
    }

    fun abort() {
        synchronized(lock) {
            aborted = true
            pending?.complete(LaunchPreparationChoice.Abort)
        }
    }

    suspend fun await(publish: () -> Unit): LaunchPreparationChoice {
        val deferred = synchronized(lock) {
            if (aborted) null else kotlinx.coroutines.CompletableDeferred<LaunchPreparationChoice>().also {
                pending = it
            }
        } ?: return LaunchPreparationChoice.Abort

        try {
            publish()
            synchronized(lock) {
                if (aborted) deferred.complete(LaunchPreparationChoice.Abort)
            }
            return deferred.await()
        } finally {
            synchronized(lock) {
                if (pending === deferred) pending = null
            }
        }
    }
}

/** Keeps a pending game launch behind a known native job terminal state. */
internal object ShaderPreparationRunCoordinator {
    suspend fun awaitKnownTerminal(
        startAccepted: Boolean,
        readStatus: suspend () -> ShaderPreparationStatus?,
        cancellationRequested: () -> Boolean,
        cancel: suspend (Long) -> Unit,
        waitBetweenPolls: suspend () -> Unit,
        onStatus: suspend (ShaderPreparationStatus) -> Unit = {},
        shutdownIfStatusRemainsUnknown: suspend () -> Boolean = { false },
        maximumUnknownPolls: Int = 24
    ): ShaderPreparationWaitResult {
        var unknownPolls = 0
        var cancelIssuedFor = 0L
        while (true) {
            val status = readStatus()
            if (status == null) {
                unknownPolls++
            } else {
                onStatus(status)
                if (status.state == ShaderPreparationStatus.State.IDLE && status.jobId == 0L) {
                    if (!startAccepted) return ShaderPreparationWaitResult.NoWorker
                    unknownPolls++
                } else {
                    unknownPolls = 0
                    val terminal = status.state == ShaderPreparationStatus.State.COMPLETED ||
                        status.state == ShaderPreparationStatus.State.CANCELLED ||
                        status.state == ShaderPreparationStatus.State.FAILED ||
                        status.state == ShaderPreparationStatus.State.UNSUPPORTED
                    if (terminal) return ShaderPreparationWaitResult.Terminal(status)

                    if (cancellationRequested() && status.jobId > 0L &&
                        status.state != ShaderPreparationStatus.State.CANCELLING &&
                        cancelIssuedFor != status.jobId) {
                        cancel(status.jobId)
                        cancelIssuedFor = status.jobId
                    }
                }
            }

            if (unknownPolls >= maximumUnknownPolls) {
                if (cancellationRequested()) {
                    if (shutdownIfStatusRemainsUnknown()) {
                        return ShaderPreparationWaitResult.RuntimeShutdown
                    }
                }
                return ShaderPreparationWaitResult.StatusUnavailable
            }
            waitBetweenPolls()
        }
    }
}
