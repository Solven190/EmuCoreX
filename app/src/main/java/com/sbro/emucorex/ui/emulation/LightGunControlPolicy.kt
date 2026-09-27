package com.sbro.emucorex.ui.emulation

import com.sbro.emucorex.data.AppPreferences

/** Cabinet type from the running core: -1 pending, 0 PS2, 1 arcade, 2 arcade gun. */
internal fun usesLightGunControls(
    arcadeInputMode: Int,
    usbPort1Device: Int,
    usbPort2Device: Int,
    ps2GunProfileActive: Boolean
): Boolean =
    arcadeInputMode == 2 || (arcadeInputMode == 0 && (
        usbPort1Device == AppPreferences.USB_DEVICE_GUNCON2 ||
            usbPort2Device == AppPreferences.USB_DEVICE_GUNCON2) && ps2GunProfileActive)
