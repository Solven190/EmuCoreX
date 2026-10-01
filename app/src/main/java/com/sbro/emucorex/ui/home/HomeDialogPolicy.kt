package com.sbro.emucorex.ui.home

enum class HomeActiveDialog {
    NONE,
    WELCOME,
    CORE_RESET,
    HARDWARE_WARNING
}

object HomeDialogPolicy {
    fun activeDialog(
        canPresent: Boolean,
        welcomePending: Boolean,
        coreResetPending: Boolean,
        hardwareWarningPending: Boolean
    ): HomeActiveDialog = when {
        !canPresent -> HomeActiveDialog.NONE
        welcomePending -> HomeActiveDialog.WELCOME
        coreResetPending -> HomeActiveDialog.CORE_RESET
        hardwareWarningPending -> HomeActiveDialog.HARDWARE_WARNING
        else -> HomeActiveDialog.NONE
    }
}
