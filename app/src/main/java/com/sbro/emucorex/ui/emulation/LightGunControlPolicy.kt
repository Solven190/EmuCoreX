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

/**
 * True when the running game has a per-game profile whose effective gyro mode is Light Gun.
 * The effective mode already folds in the profile's provided keys, so a per-game Light Gun
 * choice that happens to equal the global setting still activates the gun controls instead of
 * being treated as "not chosen for this game".
 */
internal fun isPerGameGunProfileActive(hasPerGameProfile: Boolean, effectiveGyroMode: Int): Boolean =
    hasPerGameProfile && effectiveGyroMode == AppPreferences.GYRO_MODE_LIGHT_GUN
