package com.sbro.emucorex.core

import android.content.Context

object StoragePermissionHelper {
    fun hasGameLibraryAccess(context: Context, rawGamePath: String?): Boolean {
        if (rawGamePath.isNullOrBlank()) return false
        if (rawGamePath.startsWith("content://")) return true
        if (!DocumentPathResolver.isScopedStorageExternalPath(rawGamePath)) return true

        return DocumentPathResolver.findAccessibleTreeUriForRawPath(context, rawGamePath) != null
    }
}
