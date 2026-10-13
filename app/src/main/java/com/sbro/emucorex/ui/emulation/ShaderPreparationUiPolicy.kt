package com.sbro.emucorex.ui.emulation

import com.sbro.emucorex.core.RendererDefaults

enum class ShaderPreparationUiPhase {
    Hidden,
    Offer,
    WaitingForLifecycle,
    Starting,
    Running,
    Cancelling,
    Completed,
    Cancelled,
    Failed,
    Unsupported
}

enum class ShaderPreparationUiAction {
    Prepare,
    Skip,
    Cancel,
    Resume,
    ContinueToGame
}

enum class ShaderPreparationUiError {
    LifecycleBusy,
    UnsupportedRenderer,
    CacheDisabled,
    MissingSource,
    DeviceCreationFailed,
    PipelineFailures,
    CheckpointFailed,
    InternalError
}

data class ShaderPreparationUiSnapshot(
    val phase: ShaderPreparationUiPhase,
    val total: Int,
    val completed: Int,
    val alreadyAvailable: Int,
    val failed: Int,
    val cancelled: Int,
    val inFlight: Int,
    val finished: Boolean,
    val checkpointSaved: Boolean,
    val jobId: Long = 0L,
    val attempt: Int = 0,
    val error: ShaderPreparationUiError? = null,
    val adapterName: String? = null,
    val cancelRequested: Boolean = false,
    val ownerKnown: Boolean = true
)

data class ShaderPreparationUiProgress(
    val ready: Int,
    val total: Int,
    val fraction: Float?
)

enum class ShaderPreparationReadiness {
    ValidatedPrepared,
    NeedsPreparation,
    Unavailable
}

object ShaderPreparationUiPolicy {
    fun shouldOffer(
        renderer: Int,
        bootToBios: Boolean,
        bootSmokeProbe: Boolean,
        autotestMode: Boolean,
        restoredAfterProcessDeath: Boolean,
        readiness: ShaderPreparationReadiness = ShaderPreparationReadiness.NeedsPreparation
    ): Boolean = renderer == RendererDefaults.VULKAN &&
        !bootToBios &&
        !bootSmokeProbe &&
        !autotestMode &&
        !restoredAfterProcessDeath &&
        readiness != ShaderPreparationReadiness.ValidatedPrepared

    fun progress(snapshot: ShaderPreparationUiSnapshot): ShaderPreparationUiProgress {
        val ready = snapshot.completed + snapshot.alreadyAvailable
        val fraction = if (snapshot.total > 0) {
            (ready.toFloat() / snapshot.total).coerceIn(0f, 1f)
        } else {
            null
        }
        return ShaderPreparationUiProgress(ready = ready, total = snapshot.total, fraction = fraction)
    }

    fun actions(phase: ShaderPreparationUiPhase): Set<ShaderPreparationUiAction> = when (phase) {
        ShaderPreparationUiPhase.Offer -> setOf(ShaderPreparationUiAction.Prepare, ShaderPreparationUiAction.Skip)
        ShaderPreparationUiPhase.WaitingForLifecycle,
        ShaderPreparationUiPhase.Starting,
        ShaderPreparationUiPhase.Running -> setOf(ShaderPreparationUiAction.Cancel)
        ShaderPreparationUiPhase.Cancelled,
        ShaderPreparationUiPhase.Failed -> setOf(
            ShaderPreparationUiAction.Resume,
            ShaderPreparationUiAction.ContinueToGame
        )
        ShaderPreparationUiPhase.Unsupported,
        ShaderPreparationUiPhase.Completed -> setOf(ShaderPreparationUiAction.ContinueToGame)
        ShaderPreparationUiPhase.Hidden,
        ShaderPreparationUiPhase.Cancelling -> emptySet()
    }

    fun actions(snapshot: ShaderPreparationUiSnapshot): Set<ShaderPreparationUiAction> =
        if (!snapshot.ownerKnown) {
            setOf(ShaderPreparationUiAction.Cancel)
        } else if (snapshot.phase in setOf(ShaderPreparationUiPhase.Cancelled, ShaderPreparationUiPhase.Failed) &&
            snapshot.jobId <= 0L) {
            setOf(ShaderPreparationUiAction.ContinueToGame)
        } else {
            actions(snapshot.phase)
        }

    fun isDurablyReady(snapshot: ShaderPreparationUiSnapshot): Boolean =
        snapshot.phase == ShaderPreparationUiPhase.Completed &&
            snapshot.finished &&
            snapshot.checkpointSaved
}
