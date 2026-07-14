package com.sbro.emucorex.core

import android.app.Activity
import android.content.Context
import android.content.pm.ApplicationInfo
import android.os.Build
import com.google.android.play.core.review.ReviewManagerFactory

object PlayInAppReviewManager {
    private const val PLAY_STORE_PACKAGE = "com.android.vending"

    fun canRequest(context: Context): Boolean {
        if (context.applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE != 0) return false

        val installerPackage = runCatching {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                context.packageManager
                    .getInstallSourceInfo(context.packageName)
                    .installingPackageName
            } else {
                @Suppress("DEPRECATION")
                context.packageManager.getInstallerPackageName(context.packageName)
            }
        }.getOrNull()

        return installerPackage == PLAY_STORE_PACKAGE
    }

    fun request(activity: Activity, onFlowStarted: () -> Unit) {
        val reviewManager = ReviewManagerFactory.create(activity.applicationContext)
        reviewManager.requestReviewFlow().addOnCompleteListener { requestTask ->
            if (!requestTask.isSuccessful || activity.isFinishing || activity.isDestroyed) {
                return@addOnCompleteListener
            }

            onFlowStarted()
            reviewManager.launchReviewFlow(activity, requestTask.result)
        }
    }
}
