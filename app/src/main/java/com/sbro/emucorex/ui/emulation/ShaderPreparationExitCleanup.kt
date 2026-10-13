package com.sbro.emucorex.ui.emulation

import kotlinx.coroutines.Job
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch

internal object ShaderPreparationExitCleanup {
    private val processScope = CoroutineScope(SupervisorJob() + Dispatchers.IO)

    fun isRequired(hasActiveWorker: Boolean, phase: ShaderPreparationUiPhase): Boolean =
        hasActiveWorker || phase != ShaderPreparationUiPhase.Hidden

    /** The process scope deliberately does not inherit the ViewModel's Job. */
    @Suppress("UNUSED_PARAMETER")
    fun scheduleIfRequired(
        required: Boolean,
        ownerScope: CoroutineScope,
        cleanup: suspend () -> Unit
    ): Job? = if (required) processScope.launch { cleanup() } else null
}
