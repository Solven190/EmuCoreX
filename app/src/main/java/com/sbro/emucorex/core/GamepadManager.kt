package com.sbro.emucorex.core

import android.annotation.SuppressLint
import android.content.Context
import android.os.Build
import android.os.CombinedVibration
import android.os.SystemClock
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.util.Log
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import com.sbro.emucorex.data.AppPreferences
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import java.util.Locale
import kotlin.math.abs
import kotlin.math.roundToInt
import kotlin.math.sign

object GamepadManager {
    data class MappableButtonAction(
        val id: String,
        val padKey: Int,
        val defaultKeyCodes: List<Int>
    )

    data class ConnectedGamepad(
        val padIndex: Int,
        val deviceId: Int,
        val name: String
    )

    private data class BindingCaptureState(
        val padIndex: Int,
        val onCaptured: (Int) -> Unit
    )

    private data class AnalogState(
        var prevLeftX: Float = 0f,
        var prevLeftY: Float = 0f,
        var prevRightX: Float = 0f,
        var prevRightY: Float = 0f,
        var prevLT: Float = 0f,
        var prevRT: Float = 0f,
        var prevHatX: Float = 0f,
        var prevHatY: Float = 0f
    )

    private data class RumbleState(
        var lastAmplitude: Int = 0,
        var lastUpdateElapsedMs: Long = 0L
    )

    private sealed class VibrationTarget {
        data class Single(val vibrator: Vibrator) : VibrationTarget()
        data class Managed(val manager: VibratorManager) : VibrationTarget()
    }

    private const val TAG = "GamepadManager"
    private const val MAX_PAD_SLOTS = 2
    private const val RUMBLE_UPDATE_INTERVAL_MS = 40L
    private const val RUMBLE_PULSE_DURATION_MS = 80L
    private const val TEST_RUMBLE_DURATION_MS = 260L
    private const val MISSING_VIBRATOR_LOG_INTERVAL_MS = 1500L
    private val FINGERPRINT_UINPUT_DEVICE_TOKENS = setOf(
        "uinput-fpc",
        "uinput-goodix",
        "uinput-synaptics",
        "uinput-elan",
        "uinput-vfs",
        "uinput-atrus"
    )
    private val GAMEPAD_BUTTON_KEY_CODES = intArrayOf(
        KeyEvent.KEYCODE_BUTTON_A,
        KeyEvent.KEYCODE_BUTTON_B,
        KeyEvent.KEYCODE_BUTTON_X,
        KeyEvent.KEYCODE_BUTTON_Y,
        KeyEvent.KEYCODE_BUTTON_L1,
        KeyEvent.KEYCODE_BUTTON_R1,
        KeyEvent.KEYCODE_BUTTON_L2,
        KeyEvent.KEYCODE_BUTTON_R2,
        KeyEvent.KEYCODE_BUTTON_THUMBL,
        KeyEvent.KEYCODE_BUTTON_THUMBR,
        KeyEvent.KEYCODE_BUTTON_SELECT,
        KeyEvent.KEYCODE_BUTTON_START,
        KeyEvent.KEYCODE_BUTTON_1,
        KeyEvent.KEYCODE_BUTTON_2,
        KeyEvent.KEYCODE_BUTTON_3,
        KeyEvent.KEYCODE_BUTTON_4,
        KeyEvent.KEYCODE_BUTTON_5,
        KeyEvent.KEYCODE_BUTTON_6,
        KeyEvent.KEYCODE_BUTTON_7,
        KeyEvent.KEYCODE_BUTTON_8,
        KeyEvent.KEYCODE_BUTTON_9,
        KeyEvent.KEYCODE_BUTTON_10,
        KeyEvent.KEYCODE_DPAD_UP,
        KeyEvent.KEYCODE_DPAD_DOWN,
        KeyEvent.KEYCODE_DPAD_LEFT,
        KeyEvent.KEYCODE_DPAD_RIGHT
    )
    private val JOYSTICK_AXIS_CODES = intArrayOf(
        MotionEvent.AXIS_X,
        MotionEvent.AXIS_Y,
        MotionEvent.AXIS_Z,
        MotionEvent.AXIS_RX,
        MotionEvent.AXIS_RY,
        MotionEvent.AXIS_RZ,
        MotionEvent.AXIS_LTRIGGER,
        MotionEvent.AXIS_RTRIGGER,
        MotionEvent.AXIS_BRAKE,
        MotionEvent.AXIS_GAS,
        MotionEvent.AXIS_HAT_X,
        MotionEvent.AXIS_HAT_Y
    )

    @Volatile
    private var emulationInputEnabled = false
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    @Volatile
    private var initialized = false
    @Volatile
    private var bindingCaptureState: BindingCaptureState? = null
    @Volatile
    private var vibrationEnabled = true
    @Volatile
    private var vibrationStrength = AppPreferences.DEFAULT_PAD_VIBRATION_STRENGTH / 100f
    @Volatile
    private var vibrationFallbackEnabled = true
    @Volatile
    private var customBindingsByPad: Map<Int, Map<String, Int>> = emptyMap()
    @Volatile
    private var customBindingsByPadAndKeyCode: Map<Int, Map<Int, Int>> = emptyMap()
    @Volatile
    private var analogDeadzone = AppPreferences.DEFAULT_GAMEPAD_STICK_DEADZONE / 100f
    @Volatile
    private var leftStickSensitivity = AppPreferences.DEFAULT_GAMEPAD_STICK_SENSITIVITY / 100f
    @Volatile
    private var rightStickSensitivity = AppPreferences.DEFAULT_GAMEPAD_STICK_SENSITIVITY / 100f
    @Volatile
    private var invertLeftStick = false
    @Volatile
    private var invertRightStick = false
    @Volatile
    private var singleGamepadReplacesTouch = true

    private val connectionLock = Any()
    private var appContext: Context? = null
    private val deviceToPadIndex = linkedMapOf<Int, Int>()
    private val analogStatesByDeviceId = mutableMapOf<Int, AnalogState>()
    private val rumbleStatesByPad = mutableMapOf<Int, RumbleState>()
    private var lastMissingVibratorLogElapsedMs = 0L

    private object PadKey {
        const val Up = 19
        const val Right = 22
        const val Down = 20
        const val Left = 21
        const val Triangle = 100
        const val Circle = 97
        const val Cross = 96
        const val Square = 99
        const val Select = 109
        const val Start = 108
        const val L1 = 102
        const val L2 = 104
        const val R1 = 103
        const val R2 = 105
        const val L3 = 106
        const val R3 = 107
        const val LeftStickUp = 110
        const val LeftStickRight = 111
        const val LeftStickDown = 112
        const val LeftStickLeft = 113
        const val RightStickUp = 120
        const val RightStickRight = 121
        const val RightStickDown = 122
        const val RightStickLeft = 123
    }

    private val mappableActions = listOf(
        MappableButtonAction("cross", PadKey.Cross, listOf(KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_BUTTON_1)),
        MappableButtonAction("circle", PadKey.Circle, listOf(KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BUTTON_2)),
        MappableButtonAction("square", PadKey.Square, listOf(KeyEvent.KEYCODE_BUTTON_X, KeyEvent.KEYCODE_BUTTON_3)),
        MappableButtonAction("triangle", PadKey.Triangle, listOf(KeyEvent.KEYCODE_BUTTON_Y, KeyEvent.KEYCODE_BUTTON_4)),
        MappableButtonAction("l1", PadKey.L1, listOf(KeyEvent.KEYCODE_BUTTON_L1, KeyEvent.KEYCODE_BUTTON_5)),
        MappableButtonAction("r1", PadKey.R1, listOf(KeyEvent.KEYCODE_BUTTON_R1, KeyEvent.KEYCODE_BUTTON_6)),
        MappableButtonAction("l2", PadKey.L2, listOf(KeyEvent.KEYCODE_BUTTON_L2, KeyEvent.KEYCODE_BUTTON_7)),
        MappableButtonAction("r2", PadKey.R2, listOf(KeyEvent.KEYCODE_BUTTON_R2, KeyEvent.KEYCODE_BUTTON_8)),
        MappableButtonAction("l3", PadKey.L3, listOf(KeyEvent.KEYCODE_BUTTON_THUMBL)),
        MappableButtonAction("r3", PadKey.R3, listOf(KeyEvent.KEYCODE_BUTTON_THUMBR)),
        MappableButtonAction("select", PadKey.Select, listOf(KeyEvent.KEYCODE_BUTTON_SELECT, KeyEvent.KEYCODE_BUTTON_9)),
        MappableButtonAction("start", PadKey.Start, listOf(KeyEvent.KEYCODE_BUTTON_START, KeyEvent.KEYCODE_BUTTON_10)),
        MappableButtonAction("dpad_up", PadKey.Up, listOf(KeyEvent.KEYCODE_DPAD_UP)),
        MappableButtonAction("dpad_down", PadKey.Down, listOf(KeyEvent.KEYCODE_DPAD_DOWN)),
        MappableButtonAction("dpad_left", PadKey.Left, listOf(KeyEvent.KEYCODE_DPAD_LEFT)),
        MappableButtonAction("dpad_right", PadKey.Right, listOf(KeyEvent.KEYCODE_DPAD_RIGHT))
    )
    private val actionsById = mappableActions.associateBy { it.id }
    private val _connectedGamepadCountState = MutableStateFlow(0)
    val connectedGamepadCountState: StateFlow<Int> = _connectedGamepadCountState
    private var connectedGamepadSnapshot: List<ConnectedGamepad> = emptyList()

    fun ensureInitialized(context: android.content.Context) {
        if (initialized) return
        initialized = true
        appContext = context.applicationContext
        val preferences = AppPreferences(context.applicationContext)
        scope.launch {
            preferences.gamepadBindingsByPad.collectLatest { bindingsByPad ->
                customBindingsByPad = bindingsByPad
                customBindingsByPadAndKeyCode = bindingsByPad.mapValues { (_, bindings) ->
                    bindings.entries.mapNotNull { (actionId, keyCode) ->
                        actionsById[actionId]?.let { keyCode to it.padKey }
                    }.toMap()
                }
            }
        }
        scope.launch {
            preferences.padVibration.collectLatest { enabled ->
                vibrationEnabled = enabled
                if (!enabled) {
                    stopAllGamepadVibrations()
                }
            }
        }
        scope.launch {
            preferences.padVibrationStrength.collectLatest { value ->
                vibrationStrength = value.coerceIn(0, 150) / 100f
            }
        }
        scope.launch {
            preferences.padVibrationFallback.collectLatest { enabled ->
                vibrationFallbackEnabled = enabled
                if (!enabled) {
                    stopAllGamepadVibrations()
                }
            }
        }
        scope.launch {
            preferences.gamepadStickDeadzone.collectLatest { value ->
                analogDeadzone = (value.coerceIn(0, 35) / 100f)
            }
        }
        scope.launch {
            preferences.gamepadLeftStickSensitivity.collectLatest { value ->
                leftStickSensitivity = value.coerceIn(50, 200) / 100f
            }
        }
        scope.launch {
            preferences.gamepadRightStickSensitivity.collectLatest { value ->
                rightStickSensitivity = value.coerceIn(50, 200) / 100f
            }
        }
        scope.launch {
            preferences.invertLeftStick.collectLatest { enabled ->
                invertLeftStick = enabled
            }
        }
        scope.launch {
            preferences.invertRightStick.collectLatest { enabled ->
                invertRightStick = enabled
            }
        }
        scope.launch {
            preferences.enableAutoGamepad.collectLatest { enabled ->
                singleGamepadReplacesTouch = enabled
                refreshConnectedGamepads()
            }
        }
        scope.launch {
            while (true) {
                refreshConnectedGamepads()
                delay(750)
            }
        }
        refreshConnectedGamepads()
    }

    fun mappableButtonActions(): List<MappableButtonAction> = mappableActions

    fun resolveBindingForAction(actionId: String, customBindings: Map<String, Int>): Int? {
        return customBindings[actionId] ?: actionsById[actionId]?.defaultKeyCodes?.firstOrNull()
    }

    fun keyCodeLabel(keyCode: Int): String {
        return when (keyCode) {
            KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_BUTTON_1 -> "A"
            KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BUTTON_2 -> "B"
            KeyEvent.KEYCODE_BUTTON_X, KeyEvent.KEYCODE_BUTTON_3 -> "X"
            KeyEvent.KEYCODE_BUTTON_Y, KeyEvent.KEYCODE_BUTTON_4 -> "Y"
            KeyEvent.KEYCODE_BUTTON_L1, KeyEvent.KEYCODE_BUTTON_5 -> "L1"
            KeyEvent.KEYCODE_BUTTON_R1, KeyEvent.KEYCODE_BUTTON_6 -> "R1"
            KeyEvent.KEYCODE_BUTTON_L2, KeyEvent.KEYCODE_BUTTON_7 -> "L2"
            KeyEvent.KEYCODE_BUTTON_R2, KeyEvent.KEYCODE_BUTTON_8 -> "R2"
            KeyEvent.KEYCODE_BUTTON_THUMBL -> "L3"
            KeyEvent.KEYCODE_BUTTON_THUMBR -> "R3"
            KeyEvent.KEYCODE_BUTTON_SELECT, KeyEvent.KEYCODE_BUTTON_9 -> "Select"
            KeyEvent.KEYCODE_BUTTON_START, KeyEvent.KEYCODE_BUTTON_10 -> "Start"
            KeyEvent.KEYCODE_DPAD_UP -> "D-Pad Up"
            KeyEvent.KEYCODE_DPAD_DOWN -> "D-Pad Down"
            KeyEvent.KEYCODE_DPAD_LEFT -> "D-Pad Left"
            KeyEvent.KEYCODE_DPAD_RIGHT -> "D-Pad Right"
            else -> KeyEvent.keyCodeToString(keyCode).removePrefix("KEYCODE_").replace('_', ' ')
        }
    }

    fun connectedGamepads(): List<ConnectedGamepad> = synchronized(connectionLock) {
        connectedGamepadSnapshot
    }

    fun connectedControllerName(padIndex: Int): String? {
        val normalizedPadIndex = normalizePadIndex(padIndex)
        return connectedGamepads().firstOrNull { it.padIndex == normalizedPadIndex }?.name
    }

    fun firstConnectedControllerName(): String? = connectedGamepads().firstOrNull()?.name

    fun startBindingCapture(onCaptured: (Int) -> Unit) {
        startBindingCapture(0, onCaptured)
    }

    fun startBindingCapture(padIndex: Int, onCaptured: (Int) -> Unit) {
        bindingCaptureState = BindingCaptureState(normalizePadIndex(padIndex), onCaptured)
    }

    fun cancelBindingCapture() {
        bindingCaptureState = null
    }

    fun handleBindingCapture(event: KeyEvent): Boolean {
        val captureState = bindingCaptureState ?: return false
        if (!isGameController(event.device)) return false
        if (event.action != KeyEvent.ACTION_DOWN || event.repeatCount != 0) return false
        captureState.onCaptured(event.keyCode)
        bindingCaptureState = null
        return true
    }

    fun isGameController(device: InputDevice?): Boolean {
        if (device == null) return false
        if (device.isVirtual) return false
        if (isFingerprintUinputDevice(device)) return false
        val sources = device.sources
        val hasControllerSource =
            (sources and InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
            (sources and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
        if (!hasControllerSource) return false

        return hasRecognizedGamepadButton(device) || hasRecognizedJoystickAxis(device)
    }

    fun isGamepadConnected(): Boolean = connectedGamepads().isNotEmpty()

    fun connectedGamepadCount(): Int = _connectedGamepadCountState.value

    fun resolveTouchPadIndex(): Int? {
        val connectedCount = synchronized(connectionLock) { deviceToPadIndex.size }
        return when {
            connectedCount <= 0 -> 0
            connectedCount == 1 && !singleGamepadReplacesTouch -> 0
            else -> null
        }
    }

    fun setEmulationInputEnabled(enabled: Boolean) {
        emulationInputEnabled = enabled
        if (!enabled) {
            resetAnalogState()
            stopAllGamepadVibrations()
            EmulatorBridge.resetKeyStatus()
        }
    }

    fun isEmulationInputEnabled(): Boolean = emulationInputEnabled

    fun handleKeyEvent(event: KeyEvent): Boolean {
        if (!emulationInputEnabled) return false
        if (!isGameController(event.device)) return false

        val padIndex = resolvePadIndexForDevice(event.deviceId) ?: return false
        val padKey = mapKeyCodeToPadKey(padIndex, event.keyCode) ?: return false
        val pressed = event.action == KeyEvent.ACTION_DOWN

        EmulatorBridge.setPadButton(padIndex, padKey, 0, pressed)
        return true
    }

    fun handleMotionEvent(event: MotionEvent): Boolean {
        if (!emulationInputEnabled) return false
        if (!isGameController(event.device)) return false
        if (event.action != MotionEvent.ACTION_MOVE) return false

        val padIndex = resolvePadIndexForDevice(event.deviceId) ?: return false
        val state = synchronized(connectionLock) {
            analogStatesByDeviceId.getOrPut(event.deviceId) { AnalogState() }
        }

        val leftX = processStickAxis(event.getAxisValue(MotionEvent.AXIS_X), leftStickSensitivity)
        val leftY = processStickAxis(event.getAxisValue(MotionEvent.AXIS_Y), leftStickSensitivity)
            .let { if (invertLeftStick) -it else it }
        if (leftX != state.prevLeftX || leftY != state.prevLeftY) {
            dispatchAnalogStick(
                padIndex = padIndex,
                x = leftX,
                y = leftY,
                upKey = PadKey.LeftStickUp,
                rightKey = PadKey.LeftStickRight,
                downKey = PadKey.LeftStickDown,
                leftKey = PadKey.LeftStickLeft
            )
            state.prevLeftX = leftX
            state.prevLeftY = leftY
        }

        val rightX = processStickAxis(
            getAxisValueWithFallback(event, MotionEvent.AXIS_Z, MotionEvent.AXIS_RX),
            rightStickSensitivity
        )
        val rightY = processStickAxis(
            getAxisValueWithFallback(event, MotionEvent.AXIS_RZ, MotionEvent.AXIS_RY),
            rightStickSensitivity
        ).let { if (invertRightStick) -it else it }
        if (rightX != state.prevRightX || rightY != state.prevRightY) {
            dispatchAnalogStick(
                padIndex = padIndex,
                x = rightX,
                y = rightY,
                upKey = PadKey.RightStickUp,
                rightKey = PadKey.RightStickRight,
                downKey = PadKey.RightStickDown,
                leftKey = PadKey.RightStickLeft
            )
            state.prevRightX = rightX
            state.prevRightY = rightY
        }

        val lt = getAxisValueWithFallback(event, MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_BRAKE)
        val rt = getAxisValueWithFallback(event, MotionEvent.AXIS_RTRIGGER, MotionEvent.AXIS_GAS)
        if (lt != state.prevLT) {
            dispatchAnalogButton(padIndex, PadKey.L2, lt)
            state.prevLT = lt
        }
        if (rt != state.prevRT) {
            dispatchAnalogButton(padIndex, PadKey.R2, rt)
            state.prevRT = rt
        }

        val hatX = event.getAxisValue(MotionEvent.AXIS_HAT_X)
        val hatY = event.getAxisValue(MotionEvent.AXIS_HAT_Y)
        if (hatX != state.prevHatX || hatY != state.prevHatY) {
            EmulatorBridge.setPadButton(padIndex, PadKey.Left, 0, hatX < -0.5f)
            EmulatorBridge.setPadButton(padIndex, PadKey.Right, 0, hatX > 0.5f)
            EmulatorBridge.setPadButton(padIndex, PadKey.Up, 0, hatY < -0.5f)
            EmulatorBridge.setPadButton(padIndex, PadKey.Down, 0, hatY > 0.5f)
            state.prevHatX = hatX
            state.prevHatY = hatY
        }

        return true
    }

    fun onPadVibration(padIndex: Int, largeMotor: Float, smallMotor: Float) {
        playPadVibration(
            padIndex = padIndex,
            largeMotor = largeMotor,
            smallMotor = smallMotor,
            strengthOverride = null,
            durationMs = RUMBLE_PULSE_DURATION_MS,
            respectEnabledSetting = true
        )
    }

    fun testPadVibration(
        padIndex: Int = 0,
        strengthPercent: Int? = null,
        durationMs: Long = TEST_RUMBLE_DURATION_MS
    ) {
        playPadVibration(
            padIndex = padIndex,
            largeMotor = 1f,
            smallMotor = 1f,
            strengthOverride = strengthPercent?.coerceIn(0, 150)?.div(100f),
            durationMs = durationMs.coerceIn(40L, 600L),
            respectEnabledSetting = false
        )
    }

    private fun playPadVibration(
        padIndex: Int,
        largeMotor: Float,
        smallMotor: Float,
        strengthOverride: Float?,
        durationMs: Long,
        respectEnabledSetting: Boolean
    ) {
        val normalizedPadIndex = normalizePadIndex(padIndex)
        if (respectEnabledSetting && !vibrationEnabled) {
            stopPadVibration(normalizedPadIndex)
            return
        }

        val connectedGamepad = connectedGamepads().firstOrNull { it.padIndex == normalizedPadIndex }
        val target = connectedGamepad
            ?.let { getBestVibrationTarget(it.deviceId) }
            ?: getFallbackVibrationTargetForPad(normalizedPadIndex)
        if (target == null) {
            logMissingVibrationTarget(normalizedPadIndex)
            return
        }

        val intensity = maxOf(largeMotor.coerceIn(0f, 1f), smallMotor.coerceIn(0f, 1f)) *
            (strengthOverride ?: vibrationStrength).coerceIn(0f, 1.5f)
        val amplitude = (intensity * 255f).roundToInt().coerceIn(0, 255)
        if (amplitude <= 0) {
            stopPadVibration(normalizedPadIndex)
            return
        }

        val now = SystemClock.elapsedRealtime()
        val shouldUpdate = synchronized(connectionLock) {
            val state = rumbleStatesByPad.getOrPut(normalizedPadIndex) { RumbleState() }
            if (state.lastAmplitude == amplitude && (now - state.lastUpdateElapsedMs) < RUMBLE_UPDATE_INTERVAL_MS) {
                false
            } else {
                state.lastAmplitude = amplitude
                state.lastUpdateElapsedMs = now
                true
            }
        }
        if (!shouldUpdate) return

        vibrate(target, amplitude, durationMs)
    }

    private fun dispatchAnalogStick(
        padIndex: Int,
        x: Float,
        y: Float,
        upKey: Int,
        rightKey: Int,
        downKey: Int,
        leftKey: Int
    ) {
        dispatchAnalogButton(padIndex, upKey, (-y).coerceAtLeast(0f))
        dispatchAnalogButton(padIndex, rightKey, x.coerceAtLeast(0f))
        dispatchAnalogButton(padIndex, downKey, y.coerceAtLeast(0f))
        dispatchAnalogButton(padIndex, leftKey, (-x).coerceAtLeast(0f))
    }

    private fun dispatchAnalogButton(padIndex: Int, key: Int, value: Float) {
        val range = (value.coerceIn(0f, 1f) * 255f).roundToInt().coerceIn(0, 255)
        EmulatorBridge.setPadButton(padIndex, key, range, range > 0)
    }

    private fun mapKeyCodeToPadKey(padIndex: Int, keyCode: Int): Int? {
        customBindingsByPadAndKeyCode[normalizePadIndex(padIndex)]?.get(keyCode)?.let { return it }
        return when (keyCode) {
            KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_BUTTON_1 -> PadKey.Cross
            KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BUTTON_2 -> PadKey.Circle
            KeyEvent.KEYCODE_BUTTON_X, KeyEvent.KEYCODE_BUTTON_3 -> PadKey.Square
            KeyEvent.KEYCODE_BUTTON_Y, KeyEvent.KEYCODE_BUTTON_4 -> PadKey.Triangle
            KeyEvent.KEYCODE_BUTTON_L1, KeyEvent.KEYCODE_BUTTON_5 -> PadKey.L1
            KeyEvent.KEYCODE_BUTTON_R1, KeyEvent.KEYCODE_BUTTON_6 -> PadKey.R1
            KeyEvent.KEYCODE_BUTTON_L2, KeyEvent.KEYCODE_BUTTON_7 -> PadKey.L2
            KeyEvent.KEYCODE_BUTTON_R2, KeyEvent.KEYCODE_BUTTON_8 -> PadKey.R2
            KeyEvent.KEYCODE_BUTTON_THUMBL -> PadKey.L3
            KeyEvent.KEYCODE_BUTTON_THUMBR -> PadKey.R3
            KeyEvent.KEYCODE_BUTTON_SELECT, KeyEvent.KEYCODE_BUTTON_9 -> PadKey.Select
            KeyEvent.KEYCODE_BUTTON_START, KeyEvent.KEYCODE_BUTTON_10 -> PadKey.Start
            KeyEvent.KEYCODE_DPAD_UP -> PadKey.Up
            KeyEvent.KEYCODE_DPAD_DOWN -> PadKey.Down
            KeyEvent.KEYCODE_DPAD_LEFT -> PadKey.Left
            KeyEvent.KEYCODE_DPAD_RIGHT -> PadKey.Right
            else -> null
        }
    }

    private fun processStickAxis(value: Float, sensitivity: Float): Float {
        val deadzoned = applyDeadzone(value)
        return (deadzoned * sensitivity.coerceIn(0.5f, 2f)).coerceIn(-1f, 1f)
    }

    private fun getAxisValueWithFallback(event: MotionEvent, primaryAxis: Int, fallbackAxis: Int): Float {
        return if (hasJoystickAxis(event.device, primaryAxis)) {
            event.getAxisValue(primaryAxis)
        } else {
            event.getAxisValue(fallbackAxis)
        }
    }

    private fun applyDeadzone(value: Float): Float {
        val deadzone = analogDeadzone.coerceIn(0f, 0.35f)
        val magnitude = abs(value)
        if (magnitude <= deadzone) return 0f
        val normalized = ((magnitude - deadzone) / (1f - deadzone)).coerceIn(0f, 1f)
        return normalized * sign(value)
    }

    private fun normalizePadIndex(padIndex: Int): Int = padIndex.coerceIn(0, MAX_PAD_SLOTS - 1)

    private fun isFingerprintUinputDevice(device: InputDevice): Boolean {
        val name = device.name.lowercase(Locale.ROOT)
        return FINGERPRINT_UINPUT_DEVICE_TOKENS.any { token ->
            name == token || name.startsWith("$token ") || name.startsWith("$token-") || name.startsWith("${token}_")
        }
    }

    private fun hasRecognizedGamepadButton(device: InputDevice): Boolean {
        return try {
            device.hasKeys(*GAMEPAD_BUTTON_KEY_CODES).any { it }
        } catch (_: Exception) {
            false
        }
    }

    private fun hasRecognizedJoystickAxis(device: InputDevice): Boolean {
        return device.motionRanges.any { range ->
            (range.source and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK &&
                range.axis in JOYSTICK_AXIS_CODES
        }
    }

    private fun hasJoystickAxis(device: InputDevice?, axis: Int): Boolean {
        return device?.motionRanges?.any { range ->
            (range.source and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK &&
                range.axis == axis
        } == true
    }

    private fun resolvePadIndexForDevice(deviceId: Int): Int? {
        refreshConnectedGamepads()
        return synchronized(connectionLock) { deviceToPadIndex[deviceId] }
    }

    private fun refreshConnectedGamepads(): List<ConnectedGamepad> {
        val releasedAssignments = mutableListOf<Pair<Int, Int>>()
        val connectedSnapshot = synchronized(connectionLock) {
            val connectedDevices = buildList<InputDevice> {
                for (deviceId in InputDevice.getDeviceIds()) {
                    val device = InputDevice.getDevice(deviceId) ?: continue
                    if (isGameController(device)) {
                        add(device)
                    }
                }
            }
            val connectedDevicesById = connectedDevices.associateBy { it.id }
            val previousAssignments = deviceToPadIndex.toMap()
            val orderedDeviceIds = buildList {
                addAll(
                    previousAssignments.entries
                        .sortedBy { it.value }
                        .map { it.key }
                        .filter { it in connectedDevicesById }
                )
                connectedDevices.map { it.id }.forEach { deviceId ->
                    if (deviceId !in previousAssignments) {
                        add(deviceId)
                    }
                }
            }
            val targetPadIndices = desiredPadIndices(orderedDeviceIds.size)
            val updatedAssignments = linkedMapOf<Int, Int>()

            orderedDeviceIds.take(targetPadIndices.size).forEachIndexed { index, deviceId ->
                updatedAssignments[deviceId] = targetPadIndices[index]
            }

            previousAssignments.forEach { (deviceId, padIndex) ->
                if (updatedAssignments[deviceId] != padIndex) {
                    analogStatesByDeviceId.remove(deviceId)
                    rumbleStatesByPad.remove(padIndex)
                    releasedAssignments += (padIndex to deviceId)
                }
            }

            deviceToPadIndex.clear()
            deviceToPadIndex.putAll(updatedAssignments)

            val snapshot = connectedDevices.mapNotNull { device ->
                val padIndex = deviceToPadIndex[device.id] ?: return@mapNotNull null
                ConnectedGamepad(
                    padIndex = padIndex,
                    deviceId = device.id,
                    name = device.name.ifBlank { "Controller ${padIndex + 1}" }
                )
            }.sortedBy { it.padIndex }
            connectedGamepadSnapshot = snapshot
            snapshot
        }

        releasedAssignments.forEach { (padIndex, deviceId) ->
            stopGamepadVibrationForDevice(deviceId)
            if (emulationInputEnabled) {
                EmulatorBridge.resetPadState(padIndex)
            }
        }

        _connectedGamepadCountState.value = connectedSnapshot.size
        return connectedSnapshot
    }

    private fun resetAnalogState() {
        synchronized(connectionLock) {
            analogStatesByDeviceId.clear()
            rumbleStatesByPad.clear()
        }
    }

    private fun stopAllGamepadVibrations() {
        val deviceIds = connectedGamepads().map { it.deviceId }
        deviceIds.forEach(::stopGamepadVibrationForDevice)
        getSystemVibrationTarget()?.cancel()
        synchronized(connectionLock) {
            rumbleStatesByPad.clear()
        }
    }

    private fun stopPadVibration(padIndex: Int) {
        val normalizedPadIndex = normalizePadIndex(padIndex)
        val deviceId = synchronized(connectionLock) {
            rumbleStatesByPad.remove(normalizedPadIndex)
            deviceToPadIndex.entries.firstOrNull { it.value == normalizedPadIndex }?.key
        }
        if (deviceId != null) {
            stopGamepadVibrationForDevice(deviceId)
        } else {
            getFallbackVibrationTargetForPad(normalizedPadIndex)?.cancel()
        }
    }

    @Suppress("DEPRECATION")
    private fun getGamepadVibrationTarget(deviceId: Int): VibrationTarget? {
        val device = InputDevice.getDevice(deviceId) ?: return null
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            VibrationTarget.Managed(device.vibratorManager)
        } else {
            VibrationTarget.Single(device.vibrator)
        }
    }

    private fun getBestVibrationTarget(deviceId: Int): VibrationTarget? {
        val gamepadTarget = getGamepadVibrationTarget(deviceId)
        if (gamepadTarget?.hasVibrator() == true) {
            return gamepadTarget
        }
        if (!vibrationFallbackEnabled) {
            return null
        }
        return getSystemVibrationTarget()?.takeIf { it.hasVibrator() }
    }

    private fun getFallbackVibrationTargetForPad(padIndex: Int): VibrationTarget? {
        if (!vibrationFallbackEnabled || padIndex != 0) {
            return null
        }
        return getSystemVibrationTarget()?.takeIf { it.hasVibrator() }
    }

    @Suppress("DEPRECATION")
    private fun getSystemVibrationTarget(): VibrationTarget? {
        val context = appContext ?: return null
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            context.getSystemService(VibratorManager::class.java)?.let(VibrationTarget::Managed)
        } else {
            (context.getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator)?.let(VibrationTarget::Single)
        }
    }

    @SuppressLint("NewApi")
    private fun VibrationTarget.hasVibrator(): Boolean {
        return when (this) {
            is VibrationTarget.Single -> vibrator.hasVibrator()
            is VibrationTarget.Managed -> {
                manager.defaultVibrator.hasVibrator() || manager.vibratorIds.isNotEmpty()
            }
        }
    }

    private fun VibrationTarget.cancel() {
        when (this) {
            is VibrationTarget.Single -> vibrator.cancel()
            is VibrationTarget.Managed -> {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    manager.cancel()
                }
            }
        }
    }

    @Suppress("DEPRECATION")
    private fun vibrate(target: VibrationTarget, amplitude: Int, durationMs: Long) {
        when (target) {
            is VibrationTarget.Single -> {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    val resolvedAmplitude = if (target.vibrator.hasAmplitudeControl()) {
                        amplitude
                    } else {
                        VibrationEffect.DEFAULT_AMPLITUDE
                    }
                    target.vibrator.vibrate(VibrationEffect.createOneShot(durationMs, resolvedAmplitude))
                } else {
                    target.vibrator.vibrate(durationMs)
                }
            }
            is VibrationTarget.Managed -> {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    val effect = VibrationEffect.createOneShot(durationMs, amplitude)
                    target.manager.vibrate(CombinedVibration.createParallel(effect))
                }
            }
        }
    }

    private fun stopGamepadVibrationForDevice(deviceId: Int) {
        val target = getBestVibrationTarget(deviceId) ?: return
        target.cancel()
    }

    private fun logMissingVibrationTarget(padIndex: Int) {
        val now = SystemClock.elapsedRealtime()
        if ((now - lastMissingVibratorLogElapsedMs) < MISSING_VIBRATOR_LOG_INTERVAL_MS) {
            return
        }
        lastMissingVibratorLogElapsedMs = now
        Log.w(TAG, "No vibration target available for pad $padIndex")
    }

    private fun desiredPadIndices(connectedGamepadCount: Int): List<Int> {
        val visibleGamepadCount = connectedGamepadCount.coerceIn(0, MAX_PAD_SLOTS)
        return when {
            visibleGamepadCount <= 0 -> emptyList()
            visibleGamepadCount == 1 && !singleGamepadReplacesTouch -> listOf(1)
            else -> List(visibleGamepadCount) { it }
        }
    }
}
