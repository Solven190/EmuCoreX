package com.sbro.emucorex.core

import android.content.Context
import android.media.AudioAttributes
import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.view.HapticFeedbackConstants
import android.view.View
import kotlin.math.roundToInt

object AndroidTouchHaptics {
    private val hapticAudioAttributes: AudioAttributes by lazy {
        AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_GAME)
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
            .build()
    }

    @Suppress("DEPRECATION")
    fun play(
        context: Context,
        view: View? = null,
        strengthPercent: Int,
        durationMs: Long
    ) {
        performViewHaptic(view)

        val amplitude = ((strengthPercent.coerceIn(10, 100) / 100f) * 255f)
            .roundToInt()
            .coerceIn(1, 255)
        val duration = durationMs.coerceIn(80L, 260L)
        val effect = createTouchEffect(amplitude, duration)

        collectVibrators(context).forEach { vibrator ->
            runCatching {
                vibrator.vibrate(effect, hapticAudioAttributes)
            }
        }
    }

    @Suppress("DEPRECATION")
    private fun performViewHaptic(view: View?) {
        view ?: return
        val flags = HapticFeedbackConstants.FLAG_IGNORE_VIEW_SETTING or
            HapticFeedbackConstants.FLAG_IGNORE_GLOBAL_SETTING
        val feedback = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            intArrayOf(
                HapticFeedbackConstants.CONFIRM,
                HapticFeedbackConstants.GESTURE_START,
                HapticFeedbackConstants.VIRTUAL_KEY
            )
        } else {
            intArrayOf(
                HapticFeedbackConstants.VIRTUAL_KEY,
                HapticFeedbackConstants.KEYBOARD_TAP,
                HapticFeedbackConstants.CLOCK_TICK
            )
        }
        feedback.forEach { type ->
            runCatching { view.performHapticFeedback(type, flags) }
        }
    }

    private fun createTouchEffect(amplitude: Int, durationMs: Long): VibrationEffect {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && durationMs <= 120L) {
            return VibrationEffect.createPredefined(
                if (amplitude >= 180) {
                    VibrationEffect.EFFECT_HEAVY_CLICK
                } else {
                    VibrationEffect.EFFECT_CLICK
                }
            )
        }

        val firstPulse = (durationMs * 0.62f).roundToInt().coerceIn(50, 170).toLong()
        val secondPulse = (durationMs * 0.30f).roundToInt().coerceIn(28, 90).toLong()
        val secondAmplitude = (amplitude * 0.72f).roundToInt().coerceIn(1, 255)
        return VibrationEffect.createWaveform(
            longArrayOf(0L, firstPulse, 22L, secondPulse),
            intArrayOf(0, amplitude, 0, secondAmplitude),
            -1
        )
    }

    private fun collectVibrators(context: Context): List<Vibrator> {
        val appContext = context.applicationContext
        val vibrators = buildList {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                val manager = appContext.getSystemService(VibratorManager::class.java)
                manager?.defaultVibrator?.let(::add)
                manager?.vibratorIds?.forEach { id ->
                    runCatching { manager.getVibrator(id) }
                        .getOrNull()
                        ?.let(::add)
                }
            } else {
                @Suppress("DEPRECATION")
                (appContext.getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator)?.let(::add)
            }
        }
        return vibrators
            .filter { vibrator -> runCatching { vibrator.hasVibrator() }.getOrDefault(false) }
            .distinctBy { System.identityHashCode(it) }
    }
}
