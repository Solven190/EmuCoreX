package com.sbro.emucorex.core

import android.content.Context
import android.content.Intent
import android.os.SystemClock
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

@RunWith(AndroidJUnit4::class)
class DriverPerformanceDeviceTest {
    @Test
    fun compareGodOfWarIntroWithTurbo() {
        val context = ApplicationProvider.getApplicationContext<Context>()
        val label = InstrumentationRegistry.getArguments().getString("label") ?: "comparison"
        require(label.matches(Regex("[a-zA-Z0-9_-]+")))
        val pkg = context.packageName
        val intent = Intent(pkg + ".action.LAUNCH_GAME").apply {
            setClassName(pkg, pkg + ".MainActivity")
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            putExtra(pkg + ".extra.GAME_PATH", "/sdcard/PS2Games/Games/God of War II (USA).iso")
            putExtra(pkg + ".extra.SAVE_SLOT", 1)
            putExtra(pkg + ".extra.AUTOTEST_MODE", true)
        }
        context.startActivity(intent)
        val timeout = SystemClock.elapsedRealtime() + 45000
        while ((!NativeApp.hasValidVm() || NativeApp.getPerformanceMetricsSnapshot().isNullOrBlank()) && SystemClock.elapsedRealtime() < timeout)
            Thread.sleep(100)
        assertTrue("Game must start before performance measurement", NativeApp.hasValidVm())
        Thread.sleep(7000)
        NativeApp.setPerformanceMetricsEnabled(true, true, false)
        fun cross() {
            NativeApp.setPadButton(0, 96, 0, true)
            Thread.sleep(150)
            NativeApp.setPadButton(0, 96, 0, false)
        }
        cross() // New Game from the unchanged original menu state.
        Thread.sleep(2000)
        cross() // Accept the default difficulty.
        Thread.sleep(3000)
        NativeApp.setTurboModeEnabled(true)
        val samples = JSONArray()
        try {
            repeat(35) {
                samples.put(JSONObject().apply {
                    put("elapsed", it)
                    put("snapshot", NativeApp.getPerformanceMetricsSnapshot().orEmpty())
                    val clocks = JSONObject()
                    listOf(0, 4, 7).forEach { policy ->
                        clocks.put("policy$policy", runCatching {
                            File("/sys/devices/system/cpu/cpufreq/policy$policy/scaling_cur_freq").readText().trim()
                        }.getOrNull() ?: JSONObject.NULL)
                    }
                    put("clocks", clocks)
                })
                Thread.sleep(1000)
            }
            val output = File(context.getExternalFilesDir(null), "logs/driver-perf-$label.json")
            output.parentFile?.mkdirs()
            output.writeText(samples.toString(2), Charsets.UTF_8)
            assertTrue("Snapshots must include live frame data", samples.length() == 35)
        } finally {
            NativeApp.setTurboModeEnabled(false)
            NativeApp.shutdown()
        }
    }
}
