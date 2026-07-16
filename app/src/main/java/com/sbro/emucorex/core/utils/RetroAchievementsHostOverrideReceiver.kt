package com.sbro.emucorex.core.utils

import android.app.Activity
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import androidx.core.content.edit
import com.sbro.emucorex.core.NativeApp
import com.sbro.emucorex.data.AppPreferences
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import java.net.URI
import java.util.Locale

class RetroAchievementsHostOverrideReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent?) {
        val action = intent?.action ?: return
        val packageName = context.packageName
        val command = when (action) {
            packageName + ACTION_CLEAR_SUFFIX -> HostOverrideCommand.Clear
            packageName + ACTION_SET_SUFFIX -> {
                RetroAchievementsHostOverridePolicy.normalizeHost(intent.getStringExtra(EXTRA_HOST))
                    ?.let(HostOverrideCommand::Set)
                    ?: HostOverrideCommand.Clear
            }
            else -> return
        }

        resultCode = Activity.RESULT_OK
        val pendingResult = goAsync()
        CoroutineScope(SupervisorJob() + Dispatchers.IO).launch {
            try {
                applyCommand(context.applicationContext, command)
            } finally {
                pendingResult.finish()
            }
        }
    }

    companion object {
        private const val ACTION_SET_SUFFIX = ".action.SET_RETROACHIEVEMENTS_HOST_OVERRIDE"
        private const val ACTION_CLEAR_SUFFIX = ".action.CLEAR_RETROACHIEVEMENTS_HOST_OVERRIDE"
        private const val PREFS_NAME = "retroachievements_host_override"
        private const val KEY_ACTIVE_HOST = "active_host"
        private const val KEY_PENDING_CLEAR = "pending_clear"
        private const val KEY_SAVED_HARDCORE = "saved_hardcore"

        const val EXTRA_HOST = "host"

        /** Called immediately after NativeApp.initialize() has made settings available. */
        @JvmStatic
        fun applyAfterNativeInitialization(context: Context) {
            val prefs = preferences(context)
            if (prefs.getString(KEY_ACTIVE_HOST, null).isNullOrBlank() &&
                !prefs.getBoolean(KEY_PENDING_CLEAR, false)
            ) {
                return
            }
            runBlocking(Dispatchers.IO) {
                applyDesiredState(context.applicationContext)
            }
        }

        fun isOverrideActive(context: Context): Boolean =
            !preferences(context).getString(KEY_ACTIVE_HOST, null).isNullOrBlank()

        private suspend fun applyCommand(context: Context, command: HostOverrideCommand) {
            when (command) {
                is HostOverrideCommand.Set -> applySet(context, command.host)
                HostOverrideCommand.Clear -> applyClear(context)
            }
        }

        private suspend fun applySet(context: Context, host: String) {
            val prefs = preferences(context)
            val appPreferences = AppPreferences(context)
            if (!prefs.contains(KEY_SAVED_HARDCORE)) {
                prefs.edit(commit = true) {
                    putBoolean(KEY_SAVED_HARDCORE, appPreferences.getAchievementsHardcoreSync())
                }
            }
            prefs.edit(commit = true) {
                putString(KEY_ACTIVE_HOST, host)
                remove(KEY_PENDING_CLEAR)
            }

            appPreferences.setAchievementsHardcore(false)
            runCatching { NativeApp.setAchievementsHostOverride(host) }
        }

        private suspend fun applyClear(context: Context) {
            val prefs = preferences(context)
            val savedHardcore = readSavedHardcore(prefs)
            prefs.edit(commit = true) {
                remove(KEY_ACTIVE_HOST)
                putBoolean(KEY_PENDING_CLEAR, true)
            }

            if (savedHardcore != null) {
                AppPreferences(context).setAchievementsHardcore(savedHardcore)
            }
            val cleared = runCatching {
                NativeApp.clearAchievementsHostOverride(
                    RetroAchievementsHostOverridePolicy.hardcoreRestoreMode(savedHardcore)
                )
            }.getOrDefault(false)
            if (cleared) {
                prefs.edit(commit = true) {
                    remove(KEY_PENDING_CLEAR)
                    remove(KEY_SAVED_HARDCORE)
                }
            }
        }

        private suspend fun applyDesiredState(context: Context) {
            val prefs = preferences(context)
            val host = prefs.getString(KEY_ACTIVE_HOST, null)
            if (!host.isNullOrBlank()) {
                val appPreferences = AppPreferences(context)
                if (!prefs.contains(KEY_SAVED_HARDCORE)) {
                    prefs.edit(commit = true) {
                        putBoolean(KEY_SAVED_HARDCORE, appPreferences.getAchievementsHardcoreSync())
                    }
                }
                appPreferences.setAchievementsHardcore(false)
                NativeApp.setAchievementsHostOverride(host)
            } else if (prefs.getBoolean(KEY_PENDING_CLEAR, false)) {
                val savedHardcore = readSavedHardcore(prefs)
                if (savedHardcore != null) {
                    AppPreferences(context).setAchievementsHardcore(savedHardcore)
                }
                if (NativeApp.clearAchievementsHostOverride(
                        RetroAchievementsHostOverridePolicy.hardcoreRestoreMode(savedHardcore)
                    )
                ) {
                    prefs.edit(commit = true) {
                        remove(KEY_PENDING_CLEAR)
                        remove(KEY_SAVED_HARDCORE)
                    }
                }
            }
        }

        private fun readSavedHardcore(prefs: SharedPreferences): Boolean? =
            if (prefs.contains(KEY_SAVED_HARDCORE)) prefs.getBoolean(KEY_SAVED_HARDCORE, false) else null

        private fun preferences(context: Context) =
            context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    }
}

private sealed interface HostOverrideCommand {
    data class Set(val host: String) : HostOverrideCommand
    data object Clear : HostOverrideCommand
}

/** Pure policy kept separate so every accepted and rejected host form is JVM-tested. */
internal object RetroAchievementsHostOverridePolicy {
    fun normalizeHost(value: String?): String? {
        val trimmed = value?.trim().orEmpty()
        if (trimmed.isEmpty()) return null

        val candidate = if (trimmed.contains("://")) trimmed else "http://$trimmed"
        val uri = runCatching { URI(candidate) }.getOrNull() ?: return null
        val scheme = uri.scheme?.lowercase(Locale.US) ?: return null
        val host = uri.host?.lowercase(Locale.US) ?: return null
        if (scheme != "http") return null
        if (host != "127.0.0.1" && host != "localhost") return null
        if (uri.port !in 1..65535) return null
        if (uri.rawUserInfo != null || uri.rawQuery != null || uri.rawFragment != null) return null
        if (!uri.rawPath.isNullOrEmpty() && uri.rawPath != "/") return null

        return "$scheme://$host:${uri.port}"
    }

    fun effectiveHardcore(requested: Boolean, overrideActive: Boolean): Boolean =
        requested && !overrideActive

    // -1 means CLEAR arrived without a matching SET, so the user's mode is untouched.
    fun hardcoreRestoreMode(savedHardcore: Boolean?): Int = when (savedHardcore) {
        true -> 1
        false -> 0
        null -> -1
    }
}
