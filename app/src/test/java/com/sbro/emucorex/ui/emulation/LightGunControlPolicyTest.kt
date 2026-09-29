package com.sbro.emucorex.ui.emulation

import com.sbro.emucorex.data.AppPreferences
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LightGunControlPolicyTest {
    @Test
    fun normalArcadeCabinetNeverUsesGunBindingsEvenWithGlobalUsbGun() {
        assertFalse(usesLightGunControls(1, AppPreferences.USB_DEVICE_GUNCON2, AppPreferences.USB_DEVICE_NONE, true))
        assertFalse(usesLightGunControls(-1, AppPreferences.USB_DEVICE_GUNCON2, AppPreferences.USB_DEVICE_NONE, true))
    }

    @Test
    fun lightGunArcadeCabinetUsesGunBindingsWithoutGlobalUsbSetting() {
        assertTrue(usesLightGunControls(2, AppPreferences.USB_DEVICE_NONE, AppPreferences.USB_DEVICE_NONE, false))
    }

    @Test
    fun ordinaryPs2KeepsPadBindingsEvenWhenUsbGunIsConfiguredGlobally() {
        assertFalse(usesLightGunControls(0, AppPreferences.USB_DEVICE_GUNCON2, AppPreferences.USB_DEVICE_NONE, false))
        assertFalse(usesLightGunControls(0, AppPreferences.USB_DEVICE_NONE, AppPreferences.USB_DEVICE_NONE, true))
        assertTrue(usesLightGunControls(0, AppPreferences.USB_DEVICE_GUNCON2, AppPreferences.USB_DEVICE_NONE, true))
        assertTrue(usesLightGunControls(0, AppPreferences.USB_DEVICE_NONE, AppPreferences.USB_DEVICE_GUNCON2, true))
    }

    @Test
    fun perGameGunProfileFollowsTheEffectiveGyroMode() {
        assertTrue(isPerGameGunProfileActive(true, AppPreferences.GYRO_MODE_LIGHT_GUN))
        assertFalse(isPerGameGunProfileActive(false, AppPreferences.GYRO_MODE_LIGHT_GUN))
        assertFalse(isPerGameGunProfileActive(true, AppPreferences.GYRO_MODE_OFF))
        assertFalse(isPerGameGunProfileActive(true, AppPreferences.GYRO_MODE_AIM))
    }

    @Test
    fun lightGunGyroModeStaysProvidedEvenWhenItMatchesTheGlobalMode() {
        assertTrue(perGameGyroModeIsProvided(AppPreferences.GYRO_MODE_LIGHT_GUN, AppPreferences.GYRO_MODE_LIGHT_GUN))
        assertTrue(perGameGyroModeIsProvided(AppPreferences.GYRO_MODE_LIGHT_GUN, AppPreferences.GYRO_MODE_OFF))
        assertTrue(perGameGyroModeIsProvided(AppPreferences.GYRO_MODE_AIM, AppPreferences.GYRO_MODE_OFF))
        assertFalse(perGameGyroModeIsProvided(AppPreferences.GYRO_MODE_OFF, AppPreferences.GYRO_MODE_OFF))
    }
}
