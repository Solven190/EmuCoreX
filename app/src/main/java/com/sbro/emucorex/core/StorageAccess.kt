package com.sbro.emucorex.core

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.util.Log

object StorageAccess {
    private const val TAG = "StorageAccess"

    fun takePersistableReadPermission(context: Context, uri: Uri): Boolean {
        return takePersistablePermission(context, uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
    }

    fun takePersistableReadWritePermission(context: Context, uri: Uri): Boolean {
        return takePersistablePermission(
            context,
            uri,
            Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
        )
    }

    private fun takePersistablePermission(context: Context, uri: Uri, requestedFlags: Int): Boolean {
        val resolver = context.contentResolver
        val attempts = buildList {
            add(requestedFlags)
            if (requestedFlags and Intent.FLAG_GRANT_READ_URI_PERMISSION != 0) {
                add(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
            if (requestedFlags and Intent.FLAG_GRANT_WRITE_URI_PERMISSION != 0) {
                add(Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
            }
        }.distinct().filter { it != 0 }

        for (flags in attempts) {
            runCatching {
                resolver.takePersistableUriPermission(uri, flags)
            }.onFailure { error ->
                Log.w(TAG, "Persistable URI permission failed for $uri flags=$flags", error)
            }
        }

        return resolver.persistedUriPermissions.any { permission ->
            permission.uri == uri &&
                (requestedFlags and Intent.FLAG_GRANT_READ_URI_PERMISSION == 0 || permission.isReadPermission) &&
                (requestedFlags and Intent.FLAG_GRANT_WRITE_URI_PERMISSION == 0 || permission.isWritePermission)
        }
    }
}
