package com.sbro.emucorex.core

import android.content.Context
import android.content.Intent
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import androidx.core.content.FileProvider
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL

class RateLimitException(val resetTimestampMs: Long) : IOException("Rate limit exceeded")

data class AppUpdateRelease(
    val tagName: String,
    val name: String,
    val body: String,
    val publishedAt: String,
    val htmlUrl: String,
    val apkAssetName: String?,
    val apkDownloadUrl: String?,
    val apkSizeBytes: Long?,
    val parallelApkAssetName: String? = null,
    val parallelApkDownloadUrl: String? = null,
    val parallelApkSizeBytes: Long? = null
) {
    val displayName: String = name.ifBlank { tagName }
    val hasInstallableApk: Boolean = !apkDownloadUrl.isNullOrBlank()
    val hasParallelApk: Boolean = !parallelApkDownloadUrl.isNullOrBlank()
}

class AppUpdateRepository(private val context: Context) {

    fun checkLatestRelease(force: Boolean = false): AppUpdateRelease? {
        val cacheFile = File(context.cacheDir, "latest_release.json")
        if (!force && isCacheValid(cacheFile)) {
            val cachedJson = runCatching { cacheFile.readText() }.getOrNull()
            if (cachedJson != null) {
                return parseRelease(cachedJson).takeIf { isNewerThanCurrent(it.tagName) }
            }
        }

        if (!hasNetwork()) {
            val cachedJson = runCatching { cacheFile.readText() }.getOrNull()
            if (cachedJson != null) {
                return parseRelease(cachedJson).takeIf { isNewerThanCurrent(it.tagName) }
            }
            return null
        }
        
        val connection = openConnection(LATEST_RELEASE_URL, "application/vnd.github+json,application/json,*/*")
        val etagFile = File(context.cacheDir, "latest_release.etag")
        if (etagFile.exists() && cacheFile.exists()) {
            val etag = runCatching { etagFile.readText() }.getOrNull()
            if (!etag.isNullOrBlank()) {
                connection.setRequestProperty("If-None-Match", etag)
            }
        }
        
        return try {
            ensureSuccess(connection, "latest release", allow304 = true)
            if (connection.responseCode == 304) {
                val json = cacheFile.readText()
                return parseRelease(json).takeIf { isNewerThanCurrent(it.tagName) }
            }
            val json = connection.inputStream.bufferedReader().use { it.readText() }
            cacheFile.writeText(json)
            connection.getHeaderField("ETag")?.let { etagFile.writeText(it) }
            parseRelease(json).takeIf { isNewerThanCurrent(it.tagName) }
        } finally {
            connection.disconnect()
        }
    }

    fun downloadApk(release: AppUpdateRelease, onProgress: (Float) -> Unit): File {
        val downloadUrl = release.apkDownloadUrl ?: throw IOException("Release does not include an APK asset")
        val target = File(context.getExternalFilesDir("updates"), release.safeApkName())
        return downloadApkAsset(
            downloadUrl = downloadUrl,
            target = target,
            expectedSizeBytes = release.apkSizeBytes,
            label = "update APK",
            onProgress = onProgress
        )
    }

    fun loadReleaseHistory(force: Boolean = false): List<AppUpdateRelease> {
        val cacheFile = File(context.cacheDir, "release_history.json")
        if (!force && isCacheValid(cacheFile)) {
            val cachedJson = runCatching { cacheFile.readText() }.getOrNull()
            if (cachedJson != null) {
                return parseReleaseList(cachedJson)
            }
        }

        if (!hasNetwork()) {
            val cachedJson = runCatching { cacheFile.readText() }.getOrNull()
            if (cachedJson != null) {
                return parseReleaseList(cachedJson)
            }
            return emptyList()
        }

        val connection = openConnection(RELEASES_URL, "application/vnd.github+json,application/json,*/*")
        val etagFile = File(context.cacheDir, "release_history.etag")
        if (etagFile.exists() && cacheFile.exists()) {
            val etag = runCatching { etagFile.readText() }.getOrNull()
            if (!etag.isNullOrBlank()) {
                connection.setRequestProperty("If-None-Match", etag)
            }
        }

        return try {
            ensureSuccess(connection, "release history", allow304 = true)
            if (connection.responseCode == 304) {
                val json = cacheFile.readText()
                return parseReleaseList(json)
            }
            val json = connection.inputStream.bufferedReader().use { it.readText() }
            cacheFile.writeText(json)
            connection.getHeaderField("ETag")?.let { etagFile.writeText(it) }
            parseReleaseList(json)
        } finally {
            connection.disconnect()
        }
    }

    fun downloadParallelApk(release: AppUpdateRelease, onProgress: (Float) -> Unit): File {
        val downloadUrl = release.parallelApkDownloadUrl ?: throw IOException("Release does not include a parallel APK asset")
        val target = File(context.getExternalFilesDir("updates"), release.safeParallelApkName())
        return downloadApkAsset(
            downloadUrl = downloadUrl,
            target = target,
            expectedSizeBytes = release.parallelApkSizeBytes,
            label = "parallel APK",
            onProgress = onProgress
        )
    }

    private fun downloadApkAsset(
        downloadUrl: String,
        target: File,
        expectedSizeBytes: Long?,
        label: String,
        onProgress: (Float) -> Unit
    ): File {
        target.parentFile?.mkdirs()
        if (target.exists()) {
            target.delete()
        }

        val connection = openConnection(downloadUrl, "application/vnd.android.package-archive,application/octet-stream,*/*").apply {
            readTimeout = 90_000
        }
        try {
            ensureSuccess(connection, label)
            val total = connection.contentLengthLong.takeIf { it > 0L } ?: expectedSizeBytes ?: -1L
            var copied = 0L
            connection.inputStream.use { input ->
                target.outputStream().use { output ->
                    val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
                    while (true) {
                        val read = input.read(buffer)
                        if (read <= 0) break
                        output.write(buffer, 0, read)
                        copied += read
                        if (total > 0L) {
                            onProgress((copied.toFloat() / total.toFloat()).coerceIn(0f, 1f))
                        }
                    }
                }
            }
        } finally {
            connection.disconnect()
        }
        onProgress(1f)
        return target
    }

    fun launchInstaller(apkFile: File) {
        val apkUri = FileProvider.getUriForFile(context, "${context.packageName}.fileprovider", apkFile)
        val intent = Intent(Intent.ACTION_VIEW).apply {
            setDataAndType(apkUri, "application/vnd.android.package-archive")
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        }
        context.startActivity(intent)
    }

    private fun parseRelease(json: String): AppUpdateRelease {
        val root = JSONObject(json)
        return parseRelease(root)
    }

    private fun parseReleaseList(json: String): List<AppUpdateRelease> {
        val items = JSONArray(json)
        return buildList {
            for (index in 0 until items.length()) {
                val item = items.optJSONObject(index) ?: continue
                add(parseRelease(item))
            }
        }
    }

    private fun parseRelease(root: JSONObject): AppUpdateRelease {
        val assets = root.optJSONArray("assets")
        var apkAssetName: String? = null
        var apkDownloadUrl: String? = null
        var apkSizeBytes: Long? = null
        var parallelApkAssetName: String? = null
        var parallelApkDownloadUrl: String? = null
        var parallelApkSizeBytes: Long? = null
        if (assets != null) {
            for (index in 0 until assets.length()) {
                val asset = assets.getJSONObject(index)
                val name = asset.optString("name")
                if (!name.endsWith(".apk", ignoreCase = true)) continue
                val downloadUrl = asset.optString("browser_download_url").takeIf { it.isNotBlank() }
                val sizeBytes = asset.optLong("size").takeIf { it > 0L }
                if (name.contains("parallel", ignoreCase = true)) {
                    if (parallelApkDownloadUrl == null) {
                        parallelApkAssetName = name
                        parallelApkDownloadUrl = downloadUrl
                        parallelApkSizeBytes = sizeBytes
                    }
                } else if (apkDownloadUrl == null) {
                    apkAssetName = name
                    apkDownloadUrl = downloadUrl
                    apkSizeBytes = sizeBytes
                }
            }
        }
        return AppUpdateRelease(
            tagName = root.optString("tag_name"),
            name = root.optString("name"),
            body = root.optString("body"),
            publishedAt = root.optString("published_at"),
            htmlUrl = root.optString("html_url"),
            apkAssetName = apkAssetName,
            apkDownloadUrl = apkDownloadUrl,
            apkSizeBytes = apkSizeBytes,
            parallelApkAssetName = parallelApkAssetName,
            parallelApkDownloadUrl = parallelApkDownloadUrl,
            parallelApkSizeBytes = parallelApkSizeBytes
        )
    }

    private fun AppUpdateRelease.safeApkName(): String {
        val rawName = apkAssetName?.ifBlank { null } ?: "EmuCoreX-${tagName.ifBlank { "update" }}.apk"
        return rawName.replace(Regex("[^A-Za-z0-9._-]"), "_")
    }

    private fun AppUpdateRelease.safeParallelApkName(): String {
        val rawName = parallelApkAssetName?.ifBlank { null } ?: "EmuCoreX-${tagName.ifBlank { "parallel" }}-parallel.apk"
        return rawName.replace(Regex("[^A-Za-z0-9._-]"), "_")
    }

    private fun hasNetwork(): Boolean {
        val manager = context.getSystemService(ConnectivityManager::class.java) ?: return false
        val network = manager.activeNetwork ?: return false
        val capabilities = manager.getNetworkCapabilities(network) ?: return false
        return capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
    }

    private fun isNewerThanCurrent(remoteTag: String): Boolean {
        val remote = parseVersion(remoteTag)
        val currentVersion = currentVersionName()
        val current = parseVersion(currentVersion)
        return if (remote != null && current != null) {
            compareVersions(remote, current) > 0
        } else {
            remoteTag.trim().removePrefix("v").isNotBlank() &&
                !remoteTag.trim().removePrefix("v").equals(currentVersion.trim().removePrefix("v"), ignoreCase = true)
        }
    }

    private fun currentVersionName(): String {
        return runCatching {
            context.packageManager.getPackageInfo(context.packageName, 0).versionName
        }.getOrNull()?.takeIf { it.isNotBlank() } ?: "0.0.0"
    }

    private fun parseVersion(value: String): List<Int>? {
        val parts = value.trim()
            .removePrefix("v")
            .substringBefore('-')
            .split('.')
            .mapNotNull { part -> part.takeWhile(Char::isDigit).toIntOrNull() }
        return parts.takeIf { it.isNotEmpty() }
    }

    private fun compareVersions(left: List<Int>, right: List<Int>): Int {
        val maxSize = maxOf(left.size, right.size)
        for (index in 0 until maxSize) {
            val l = left.getOrNull(index) ?: 0
            val r = right.getOrNull(index) ?: 0
            if (l != r) return l.compareTo(r)
        }
        return 0
    }

    private fun openConnection(url: String, accept: String): HttpURLConnection {
        return (URL(url).openConnection() as HttpURLConnection).apply {
            connectTimeout = 10_000
            readTimeout = 20_000
            instanceFollowRedirects = true
            setRequestProperty("Accept", accept)
            setRequestProperty("X-GitHub-Api-Version", "2022-11-28")
            setRequestProperty("User-Agent", "EmuCoreX/${currentVersionName()}")
        }
    }

    private fun isCacheValid(file: File): Boolean {
        if (!file.exists()) return false
        val age = System.currentTimeMillis() - file.lastModified()
        return age < 4 * 60 * 60 * 1000L // 4 hours
    }

    private fun ensureSuccess(connection: HttpURLConnection, label: String, allow304: Boolean = false) {
        val remaining = connection.getHeaderField("x-ratelimit-remaining")?.toIntOrNull() ?: -1
        val reset = connection.getHeaderField("x-ratelimit-reset")?.toLongOrNull() ?: 0L
        val responseCode = connection.responseCode

        if (responseCode == 403 && remaining == 0) {
            throw RateLimitException(reset * 1000L)
        }

        if (responseCode == 304 && allow304) {
            return
        }

        if (responseCode !in 200..299) {
            val responseMessage = connection.responseMessage.orEmpty().ifBlank { "HTTP $responseCode" }
            throw IOException("Could not load $label: $responseMessage")
        }
    }

    companion object {
        private const val LATEST_RELEASE_URL = "https://api.github.com/repos/sashkinbro/EmuCoreX/releases/latest"
        private const val RELEASES_URL = "https://api.github.com/repos/sashkinbro/EmuCoreX/releases?per_page=100"
    }
}
