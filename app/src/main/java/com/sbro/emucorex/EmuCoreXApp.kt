package com.sbro.emucorex

import android.app.Application
import com.sbro.emucorex.core.AppIconManager
import com.sbro.emucorex.core.EmulatorBridge
import com.sbro.emucorex.core.utils.RetroAchievementsStateManager
import com.sbro.emucorex.data.AppPreferences

class EmuCoreXApp : Application() {

    override fun onCreate() {
        super.onCreate()
        AppIconManager.applyProIcon(this, AppPreferences(this).getProUnlockedSync())
        EmulatorBridge.initializeOnce(this)
        RetroAchievementsStateManager.initialize()
    }
}
