package com.sbro.emucorex.core

import android.content.Context
import android.os.Build
import android.os.Environment

object StoragePermissionHelper {
    fun hasAllFilesAccess(): Boolean {
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.R || Environment.isExternalStorageManager()
    }

    fun hasGameLibraryAccess(context: Context, rawGamePath: String?): Boolean {
        if (rawGamePath.isNullOrBlank()) return false
        if (rawGamePath.startsWith("content://")) return true
        if (hasAllFilesAccess()) return true
        if (!DocumentPathResolver.isScopedStorageExternalPath(rawGamePath)) return true

        return DocumentPathResolver.findAccessibleTreeUriForRawPath(context, rawGamePath) != null
    }
}
