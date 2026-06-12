package com.sbro.emucorex.data

import android.content.Context
import androidx.core.net.toUri
import com.sbro.emucorex.core.DocumentPathResolver
import com.sbro.emucorex.core.NativeApp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLEncoder

data class RetroAchievementEntry(
    val id: Long,
    val title: String,
    val description: String,
    val points: Int,
    val category: Int,
    val type: Int,
    val rarity: Float,
    val rarityHardcore: Float,
    val earnedSoftcore: Boolean,
    val earnedHardcore: Boolean,
    val badgeUrl: String?,
    val badgeLockedUrl: String?
) {
    val isEarned: Boolean
        get() = earnedSoftcore || earnedHardcore
}

data class RetroAchievementGameData(
    val gameId: Long,
    val title: String,
    val gameImageUrl: String?,
    val achievements: List<RetroAchievementEntry>,
    val resolvedOnly: Boolean,
    val earnedCountOverride: Int? = null,
    val totalCountOverride: Int? = null,
    val earnedHardcoreCountOverride: Int? = null,
    val earnedPointsOverride: Int? = null,
    val totalPointsOverride: Int? = null
) {
    val earnedCount: Int
        get() = earnedCountOverride ?: achievements.count { it.isEarned }

    val totalCount: Int
        get() = totalCountOverride ?: achievements.size

    val earnedHardcoreCount: Int
        get() = earnedHardcoreCountOverride ?: achievements.count { it.earnedHardcore }

    val earnedPoints: Int
        get() = earnedPointsOverride ?: achievements.filter { it.isEarned }.sumOf { it.points }

    val totalPoints: Int
        get() = totalPointsOverride ?: achievements.sumOf { it.points }
}

data class RetroAchievementAccountProgress(
    val gameId: Long,
    val title: String,
    val gameImageUrl: String?,
    val earnedAchievements: Int,
    val earnedHardcoreAchievements: Int,
    val totalAchievements: Int
)

data class LibraryUnlockedAchievement(
    val gameTitle: String,
    val gamePath: String,
    val achievement: RetroAchievementEntry
)

data class LibraryAchievementGame(
    val gameTitle: String,
    val gamePath: String,
    val coverArtPath: String?,
    val serial: String?,
    val gameData: RetroAchievementGameData
)

private data class RemoteAccountProgressEntry(
    val gameId: Long,
    val earnedAchievements: Int,
    val earnedHardcoreAchievements: Int,
    val totalAchievements: Int
)

private data class RemoteGameTitle(
    val title: String,
    val imageUrl: String?
)

class RetroAchievementsRepository(private val context: Context) {

    private val preferences = AppPreferences(context)
    private val gameRepository = GameRepository()

    suspend fun loadGameData(gamePath: String): RetroAchievementGameData? {
        if (gamePath.isBlank()) return null

        return withContext(Dispatchers.IO) {
            if (gamePath == ACTIVE_GAME_LOOKUP_KEY) {
                return@withContext loadActiveGameData()
            }

            val activeCachedData = findCachedGameData(gamePath)
            loadAccountProgressGames()
            val accountCachedData = findAccountCachedGameData(gamePath)
            activeCachedData?.takeIf { it.achievements.isNotEmpty() }
                ?: accountCachedData?.let { fetchRemoteGameDataById(it) ?: it }
                ?: activeCachedData
        }
    }

    suspend fun loadActiveGameData(): RetroAchievementGameData? {
        return withContext(Dispatchers.IO) {
            nativeLoadMutex.withLock {
                runCatching {
                    NativeApp.getRetroAchievementGameData(ACTIVE_GAME_LOOKUP_KEY)?.toRetroAchievementGameData()
                }.getOrNull()
            }?.takeIf { it.totalCount > 0 }
                ?.also { data ->
                    if (data.achievements.isNotEmpty()) {
                        lastActiveGameData = data
                    }
                }
        }
    }

    suspend fun loadUnlockedAchievementsFromLibrary(
        onProgress: (processed: Int, total: Int) -> Unit = { _, _ -> }
    ): List<LibraryUnlockedAchievement> {
        val libraryPath = preferences.gamePath.first().orEmpty()
        if (libraryPath.isBlank()) return emptyList()
        val cacheKey = buildUnlockedCacheKey(libraryPath = libraryPath)
        unlockedAchievementsCache[cacheKey]?.let { cached ->
            onProgress(cached.size, cached.size)
            return cached
        }

        val games = if (libraryPath.startsWith("content://")) {
            gameRepository.scanDirectoryFromUri(libraryPath.toUri(), context)
        } else {
            gameRepository.scanDirectory(libraryPath, context)
        }

        val result = mutableListOf<LibraryUnlockedAchievement>()
        games.forEachIndexed { index, game ->
            onProgress(index + 1, games.size)
            val gameData = runCatching { loadGameData(game.path) }.getOrNull()
                ?: lastActiveGameData?.takeIf { game.title.toAchievementTitleKey() == it.title.toAchievementTitleKey() }
                ?: return@forEachIndexed
            if (game.title.toAchievementTitleKey() != gameData.title.toAchievementTitleKey()) {
                return@forEachIndexed
            }
            gameData.achievements
                .asSequence()
                .filter { it.isEarned }
                .mapTo(result) { achievement ->
                    LibraryUnlockedAchievement(
                        gameTitle = game.title,
                        gamePath = game.path,
                        achievement = achievement
                    )
                }
        }

        val sorted = result.sortedWith(
            compareByDescending<LibraryUnlockedAchievement> { it.achievement.earnedHardcore }
                .thenBy { it.gameTitle.lowercase() }
                .thenBy { it.achievement.title.lowercase() }
        )
        unlockedAchievementsCache[cacheKey] = sorted
        return sorted
    }

    suspend fun loadAchievementGamesFromLibrary(
        activeGameTitle: String?,
        activeGameId: Int?
    ): List<LibraryAchievementGame> {
        val libraryPath = preferences.gamePath.first().orEmpty()
        if (libraryPath.isBlank()) return emptyList()

        val accountProgress = loadAccountProgressGames()
        val cachedActiveData = lastActiveGameData?.takeIf { it.totalCount > 0 }
        val cachedActiveTitle = cachedActiveData?.title?.ifBlank { activeGameTitle.orEmpty() }
            ?.takeIf { it.isNotBlank() }
            ?: activeGameTitle.orEmpty()
        val cachedCacheKey = buildAchievementGamesCacheKey(
            libraryPath = libraryPath,
            activeGameTitle = cachedActiveTitle,
            activeGameId = activeGameId ?: cachedActiveData?.gameId?.toInt(),
            accountProgressHash = accountProgress.hashCode()
        )
        achievementGamesCache[cachedCacheKey]?.let { return it }

        val activeData = cachedActiveData ?: runCatching { loadActiveGameData() }.getOrNull()
            ?.takeIf { it.totalCount > 0 }
        val resolvedActiveTitle = activeData?.title?.ifBlank { activeGameTitle.orEmpty() }
            ?.takeIf { it.isNotBlank() }
            ?: activeGameTitle.orEmpty()
        val cacheKey = buildAchievementGamesCacheKey(
            libraryPath = libraryPath,
            activeGameTitle = resolvedActiveTitle,
            activeGameId = activeGameId ?: activeData?.gameId?.toInt(),
            accountProgressHash = accountProgress.hashCode()
        )
        achievementGamesCache[cacheKey]?.let { return it }

        val libraryGames = scanLibraryGames(libraryPath)

        val resultByKey = linkedMapOf<String, LibraryAchievementGame>()
        accountProgress
            .filter { it.totalAchievements > 0 && it.title.isNotBlank() }
            .forEach { progress ->
                val progressKey = progress.title.toAchievementTitleKey()
                val matchedGame = libraryGames.firstOrNull { game -> game.title.toAchievementTitleKey() == progressKey }
                    ?: return@forEach
                resultByKey[matchedGame.path] = LibraryAchievementGame(
                    gameTitle = matchedGame.title,
                    gamePath = matchedGame.path,
                    coverArtPath = matchedGame.coverArtPath,
                    serial = matchedGame.serial,
                    gameData = progress.toSummaryGameData()
                )
            }

        if (activeData != null && resolvedActiveTitle.isNotBlank()) {
            val activeTitleKey = resolvedActiveTitle.toAchievementTitleKey()
            val activeGame = libraryGames.firstOrNull { game -> game.title.toAchievementTitleKey() == activeTitleKey }
            val activeItem = if (activeGame != null) {
                LibraryAchievementGame(
                    gameTitle = activeGame.title,
                    gamePath = activeGame.path,
                    coverArtPath = activeGame.coverArtPath,
                    serial = activeGame.serial,
                    gameData = activeData
                )
            } else {
                LibraryAchievementGame(
                    gameTitle = resolvedActiveTitle,
                    gamePath = "",
                    coverArtPath = null,
                    serial = null,
                    gameData = activeData
                )
            }
            resultByKey[activeItem.gamePath.ifBlank { activeItem.gameData.gameId.toString() }] = activeItem
        }

        val result = resultByKey.values.sortedBy { it.gameTitle.lowercase() }
        lastAccountAchievementGames = result
        lastAccountAchievementGamesLibraryPath = libraryPath

        achievementGamesCache[cacheKey] = result
        return result
    }

    suspend fun peekCachedAchievementGamesFromLibrary(
        activeGameTitle: String?,
        activeGameId: Int?
    ): List<LibraryAchievementGame>? {
        val libraryPath = preferences.gamePath.first().orEmpty()
        if (libraryPath.isBlank()) return null

        val activeData = lastActiveGameData?.takeIf { it.totalCount > 0 }
        val resolvedActiveTitle = activeData?.title?.ifBlank { activeGameTitle.orEmpty() }
            ?.takeIf { it.isNotBlank() }
            ?: activeGameTitle.orEmpty()
        val cacheKey = buildAchievementGamesCacheKey(
            libraryPath = libraryPath,
            activeGameTitle = resolvedActiveTitle,
            activeGameId = activeGameId ?: activeData?.gameId?.toInt(),
            accountProgressHash = lastAccountProgress.hashCode()
        )
        achievementGamesCache[cacheKey]?.let { return it }

        return lastAccountAchievementGames
            .takeIf { lastAccountAchievementGamesLibraryPath == libraryPath && it.isNotEmpty() }
    }

    suspend fun peekCachedUnlockedAchievementsFromLibrary(): List<LibraryUnlockedAchievement>? {
        val libraryPath = preferences.gamePath.first().orEmpty()
        if (libraryPath.isBlank()) return null
        return unlockedAchievementsCache[buildUnlockedCacheKey(libraryPath)]
    }

    private fun scanLibraryGames(libraryPath: String): List<GameItem> {
        return if (libraryPath.startsWith("content://")) {
            gameRepository.scanDirectoryFromUri(libraryPath.toUri(), context)
        } else {
            gameRepository.scanDirectory(libraryPath, context)
        }
    }

    private fun findCachedGameData(gamePath: String): RetroAchievementGameData? {
        val cached = lastActiveGameData?.takeIf { it.totalCount > 0 } ?: return null
        if (gamePath == ACTIVE_GAME_LOOKUP_KEY) return cached

        val cachedKey = cached.title.toAchievementTitleKey()
        if (cachedKey.isBlank()) return null

        val requestedKeys = buildList {
            add(gamePath.toAchievementTitleKey())
            if (gamePath.startsWith("content://")) {
                runCatching { DocumentPathResolver.getDisplayName(context, gamePath) }.getOrNull()
                    ?.substringBeforeLast('.')
                    ?.takeIf { it.isNotBlank() }
                    ?.let { add(it.toAchievementTitleKey()) }
            } else {
                File(gamePath).nameWithoutExtension
                    .takeIf { it.isNotBlank() }
                    ?.let { add(it.toAchievementTitleKey()) }
            }
        }

        return cached.takeIf { requestedKeys.any { key -> key == cachedKey } }
    }

    private suspend fun loadAccountProgressGames(): List<RetroAchievementAccountProgress> {
        lastAccountProgress.takeIf { it.isNotEmpty() }?.let { return it }

        val nativeProgress = nativeLoadMutex.withLock {
            val nativeJson = runCatching { NativeApp.getRetroAchievementsAccountData() }.getOrNull()
                ?.takeIf { it.isNotBlank() }
            nativeJson?.toRetroAchievementAccountProgress().orEmpty()
        }

        val remoteJson = fetchRemoteAccountProgressJson()
        val remoteProgress = remoteJson.toRetroAchievementAccountProgress()
        if (remoteProgress.isNotEmpty()) {
            preferences.setAchievementsAccountProgressJson(remoteJson)
            lastAccountProgress = remoteProgress
            return remoteProgress
        }

        if (nativeProgress.isNotEmpty()) {
            lastAccountProgress = nativeProgress
            return nativeProgress
        }

        val cachedJson = preferences.getAchievementsAccountProgressJson().orEmpty()
        val cachedProgress = cachedJson.toRetroAchievementAccountProgress()
        if (cachedProgress.isNotEmpty()) {
            lastAccountProgress = cachedProgress
            return cachedProgress
        }
        return emptyList()
    }

    private fun findAccountCachedGameData(gamePath: String): RetroAchievementGameData? {
        val requestedKeys = buildRequestedTitleKeys(gamePath)
        if (requestedKeys.isEmpty()) return null

        lastAccountAchievementGames
            .firstOrNull { game ->
                game.gamePath == gamePath || requestedKeys.any { key ->
                    key == game.gameTitle.toAchievementTitleKey() || key == game.gameData.title.toAchievementTitleKey()
                }
            }
            ?.let { return it.gameData }

        return lastAccountProgress
            .firstOrNull { progress -> requestedKeys.any { key -> key == progress.title.toAchievementTitleKey() } }
            ?.toSummaryGameData()
    }

    private fun buildRequestedTitleKeys(gamePath: String): Set<String> {
        return buildSet {
            add(gamePath.toAchievementTitleKey())
            if (gamePath.startsWith("content://")) {
                runCatching { DocumentPathResolver.getDisplayName(context, gamePath) }.getOrNull()
                    ?.substringBeforeLast('.')
                    ?.takeIf { it.isNotBlank() }
                    ?.let { add(it.toAchievementTitleKey()) }
            } else {
                File(gamePath).nameWithoutExtension
                    .takeIf { it.isNotBlank() }
                    ?.let { add(it.toAchievementTitleKey()) }
            }
        }.filterTo(mutableSetOf()) { it.isNotBlank() }
    }

    private fun fetchRemoteGameDataById(summary: RetroAchievementGameData): RetroAchievementGameData? {
        if (summary.gameId <= 0) return null
        remoteGameDataCache[summary.gameId]?.let { return it }

        val username = preferences.getAchievementsUsernameSync().orEmpty()
        val token = preferences.getAchievementsTokenSync().orEmpty()
        if (username.isBlank() || token.isBlank()) return null

        val patchJson = postRetroAchievementsRequest(
            "r" to "patch",
            "u" to username,
            "t" to token,
            "g" to summary.gameId.toString()
        ) ?: return null
        val softcoreUnlocks = postRetroAchievementsRequest(
            "r" to "unlocks",
            "u" to username,
            "t" to token,
            "g" to summary.gameId.toString(),
            "h" to "0"
        )?.toUnlockIdSet().orEmpty()
        val hardcoreUnlocks = postRetroAchievementsRequest(
            "r" to "unlocks",
            "u" to username,
            "t" to token,
            "g" to summary.gameId.toString(),
            "h" to "1"
        )?.toUnlockIdSet().orEmpty()

        return patchJson.toRetroAchievementPatchGameData(
            fallback = summary,
            softcoreUnlocks = softcoreUnlocks,
            hardcoreUnlocks = hardcoreUnlocks
        )?.also { remoteGameDataCache[summary.gameId] = it }
    }

    private fun fetchRemoteAccountProgressJson(): String {
        val username = preferences.getAchievementsUsernameSync().orEmpty()
        val token = preferences.getAchievementsTokenSync().orEmpty()
        if (username.isBlank() || token.isBlank()) return ""

        val progressJson = postRetroAchievementsRequest(
            "r" to "allprogress",
            "u" to username,
            "t" to token,
            "c" to PLAYSTATION_2_CONSOLE_ID.toString()
        ) ?: return ""

        val progressEntries = progressJson.toRemoteAccountProgressEntries()
        if (progressEntries.isEmpty()) return ""

        val titleMap = fetchRemoteGameTitleMap(progressEntries.keys)
        val games = JSONArray()
        progressEntries.values
            .filter { it.totalAchievements > 0 }
            .sortedBy { entry -> titleMap[entry.gameId]?.title?.lowercase() ?: entry.gameId.toString() }
            .forEach { entry ->
                val info = titleMap[entry.gameId]
                games.put(
                    JSONObject()
                        .put("gameId", entry.gameId)
                        .put("title", info?.title?.takeIf { it.isNotBlank() } ?: "Game ${entry.gameId}")
                        .put("gameImageUrl", normalizeRetroAchievementsImageUrl(info?.imageUrl).orEmpty())
                        .put("earnedAchievements", entry.earnedAchievements)
                        .put("earnedHardcoreAchievements", entry.earnedHardcoreAchievements)
                        .put("totalAchievements", entry.totalAchievements)
                )
            }

        return JSONObject().put("games", games).toString()
    }

    private fun fetchRemoteGameTitleMap(gameIds: Set<Long>): Map<Long, RemoteGameTitle> {
        if (gameIds.isEmpty()) return emptyMap()
        val username = preferences.getAchievementsUsernameSync().orEmpty()
        val token = preferences.getAchievementsTokenSync().orEmpty()

        return buildMap {
            gameIds.chunked(100).forEach { chunk ->
                val titlesJson = postRetroAchievementsRequest(
                    "r" to "gameinfolist",
                    "u" to username,
                    "t" to token,
                    "g" to chunk.joinToString(",")
                ) ?: return@forEach
                putAll(titlesJson.toRemoteGameTitleMap())
            }
        }
    }

    private fun postRetroAchievementsRequest(vararg params: Pair<String, String>): String? {
        var connection: HttpURLConnection? = null
        return runCatching {
            val body = params.joinToString("&") { (key, value) ->
                "${URLEncoder.encode(key, Charsets.UTF_8.name())}=${URLEncoder.encode(value, Charsets.UTF_8.name())}"
            }
            connection = (URL("https://retroachievements.org/dorequest.php").openConnection() as HttpURLConnection).apply {
                requestMethod = "POST"
                connectTimeout = 6_000
                readTimeout = 8_000
                instanceFollowRedirects = true
                doOutput = true
                setRequestProperty("Content-Type", "application/x-www-form-urlencoded")
                setRequestProperty("User-Agent", "EmuCoreX/1.0")
            }
            connection.outputStream.use { output ->
                output.write(body.toByteArray(Charsets.UTF_8))
            }
            val responseCode = connection.responseCode
            val stream = if (responseCode in 200..299) connection.inputStream else connection.errorStream
            stream?.bufferedReader(Charsets.UTF_8)?.use { it.readText() }
                ?.takeIf { responseCode in 200..299 && it.isNotBlank() }
        }.getOrNull().also {
            connection?.disconnect()
        }
    }

    companion object {
        private val unlockedAchievementsCache = mutableMapOf<String, List<LibraryUnlockedAchievement>>()
        private val achievementGamesCache = mutableMapOf<String, List<LibraryAchievementGame>>()
        private val nativeLoadMutex = Mutex()
        private var lastActiveGameData: RetroAchievementGameData? = null
        private var lastAccountProgress: List<RetroAchievementAccountProgress> = emptyList()
        private var lastAccountAchievementGames: List<LibraryAchievementGame> = emptyList()
        private var lastAccountAchievementGamesLibraryPath: String? = null
        private val remoteGameDataCache = mutableMapOf<Long, RetroAchievementGameData>()
        private const val PLAYSTATION_2_CONSOLE_ID = 21
        private const val ACTIVE_GAME_LOOKUP_KEY = "__active_retroachievements_game__"

        fun invalidateUnlockedAchievementsCache() {
            unlockedAchievementsCache.clear()
            achievementGamesCache.clear()
            lastAccountProgress = emptyList()
            lastAccountAchievementGames = emptyList()
            lastAccountAchievementGamesLibraryPath = null
            remoteGameDataCache.clear()
        }

        private fun buildUnlockedCacheKey(libraryPath: String): String {
            return libraryPath.trim()
        }

        private fun buildAchievementGamesCacheKey(
            libraryPath: String,
            activeGameTitle: String,
            activeGameId: Int?,
            accountProgressHash: Int
        ): String {
            return "${libraryPath.trim()}|${activeGameTitle.toAchievementTitleKey()}|${activeGameId ?: 0}|$accountProgressHash"
        }
    }
}

private fun String.toAchievementTitleKey(): String {
    return lowercase()
        .substringBeforeLast('.')
        .replace(Regex("[^a-z0-9]+"), "")
}

private fun String.toRetroAchievementGameData(): RetroAchievementGameData? {
    return runCatching {
        val root = JSONObject(this)
        val achievements = root.optJSONArray("achievements").toRetroAchievementEntries()
        RetroAchievementGameData(
            gameId = root.optLong("gameId"),
            title = root.optString("title"),
            gameImageUrl = normalizeRetroAchievementsImageUrl(root.optString("gameImageUrl")),
            achievements = achievements,
            resolvedOnly = root.optBoolean("resolvedOnly")
        )
    }.getOrNull()
}

private fun String.toRetroAchievementAccountProgress(): List<RetroAchievementAccountProgress> {
    return runCatching {
        val games = JSONObject(this).optJSONArray("games") ?: return@runCatching emptyList()
        buildList {
            for (index in 0 until games.length()) {
                val item = games.optJSONObject(index) ?: continue
                add(
                    RetroAchievementAccountProgress(
                        gameId = item.optLong("gameId"),
                        title = item.optString("title"),
                        gameImageUrl = normalizeRetroAchievementsImageUrl(item.optString("gameImageUrl")),
                        earnedAchievements = item.optInt("earnedAchievements"),
                        earnedHardcoreAchievements = item.optInt("earnedHardcoreAchievements"),
                        totalAchievements = item.optInt("totalAchievements")
                    )
                )
            }
        }
    }.getOrDefault(emptyList())
}

private fun String.toRemoteAccountProgressEntries(): Map<Long, RemoteAccountProgressEntry> {
    return runCatching {
        val root = JSONObject(this)
        if (!root.optBoolean("Success", true)) return@runCatching emptyMap()
        val response = root.optJSONObject("Response") ?: return@runCatching emptyMap()
        buildMap {
            response.keys().forEach { gameIdKey ->
                val gameId = gameIdKey.toLongOrNull() ?: return@forEach
                val item = response.optJSONObject(gameIdKey) ?: return@forEach
                put(
                    gameId,
                    RemoteAccountProgressEntry(
                        gameId = gameId,
                        earnedAchievements = item.optInt("Unlocked"),
                        earnedHardcoreAchievements = item.optInt("UnlockedHardcore"),
                        totalAchievements = item.optInt("Achievements")
                    )
                )
            }
        }
    }.getOrDefault(emptyMap())
}

private fun String.toRemoteGameTitleMap(): Map<Long, RemoteGameTitle> {
    return runCatching {
        val root = JSONObject(this)
        if (!root.optBoolean("Success", true)) return@runCatching emptyMap()
        buildMap {
            root.optJSONArray("Response")?.let { response ->
                for (index in 0 until response.length()) {
                    val item = response.optJSONObject(index) ?: continue
                    putRemoteGameTitle(item.optLong("ID"), item)
                }
            } ?: root.optJSONObject("Response")?.let { response ->
                response.keys().forEach { gameIdKey ->
                    val item = response.optJSONObject(gameIdKey) ?: return@forEach
                    putRemoteGameTitle(gameIdKey.toLongOrNull() ?: item.optLong("ID"), item)
                }
            }
        }
    }.getOrDefault(emptyMap())
}

private fun MutableMap<Long, RemoteGameTitle>.putRemoteGameTitle(gameId: Long, item: JSONObject) {
    if (gameId <= 0) return
    put(
        gameId,
        RemoteGameTitle(
            title = item.optString("Title"),
            imageUrl = normalizeRetroAchievementsImageUrl(
                item.optString("ImageUrl").takeIf { it.isNotBlank() }
                    ?: item.optString("ImageIcon").takeIf { it.isNotBlank() }
                    ?: item.optString("ImageIconURL").takeIf { it.isNotBlank() }
            )
        )
    )
}

private fun String.toRetroAchievementPatchGameData(
    fallback: RetroAchievementGameData,
    softcoreUnlocks: Set<Long>,
    hardcoreUnlocks: Set<Long>
): RetroAchievementGameData? {
    return runCatching {
        val root = JSONObject(this)
        if (!root.optBoolean("Success", true)) return@runCatching null
        val patchData = root.optJSONObject("PatchData") ?: return@runCatching null
        val achievements = patchData.optJSONArray("Achievements").toPatchAchievementEntries(
            softcoreUnlocks = softcoreUnlocks,
            hardcoreUnlocks = hardcoreUnlocks
        )
        if (achievements.isEmpty() && fallback.totalCount > 0) return@runCatching null
        RetroAchievementGameData(
            gameId = patchData.optLong("ID", fallback.gameId),
            title = patchData.optString("Title").takeIf { it.isNotBlank() } ?: fallback.title,
            gameImageUrl = normalizeRetroAchievementsImageUrl(patchData.optString("ImageIconURL")) ?: fallback.gameImageUrl,
            achievements = achievements,
            resolvedOnly = false
        )
    }.getOrNull()
}

private fun String.toUnlockIdSet(): Set<Long> {
    return runCatching {
        val root = JSONObject(this)
        if (!root.optBoolean("Success", true)) return@runCatching emptySet()
        val ids = root.optJSONArray("UserUnlocks") ?: return@runCatching emptySet()
        buildSet {
            for (index in 0 until ids.length()) {
                val id = ids.optLong(index)
                if (id > 0) add(id)
            }
        }
    }.getOrDefault(emptySet())
}

private fun JSONArray?.toPatchAchievementEntries(
    softcoreUnlocks: Set<Long>,
    hardcoreUnlocks: Set<Long>
): List<RetroAchievementEntry> {
    if (this == null) return emptyList()
    return buildList {
        for (index in 0 until length()) {
            val item = optJSONObject(index) ?: continue
            val id = item.optLong("ID")
            val title = item.optString("Title")
            val description = item.optString("Description")
            if (isRetroAchievementsUnsupportedEmulatorWarning(title, description)) {
                continue
            }
            val type = when (item.optString("Type")) {
                "missable" -> 1
                "progression" -> 2
                "win_condition" -> 3
                else -> 0
            }
            add(
                RetroAchievementEntry(
                    id = id,
                    title = title,
                    description = description,
                    points = item.optInt("Points"),
                    category = item.optInt("Flags"),
                    type = type,
                    rarity = item.optDouble("Rarity", 100.0).toFloat(),
                    rarityHardcore = item.optDouble("RarityHardcore", 100.0).toFloat(),
                    earnedSoftcore = id in softcoreUnlocks || id in hardcoreUnlocks,
                    earnedHardcore = id in hardcoreUnlocks,
                    badgeUrl = normalizeRetroAchievementsImageUrl(item.optString("BadgeURL"))
                        ?: item.optString("BadgeName").takeIf { it.isNotBlank() }?.let { "https://media.retroachievements.org/Badge/$it.png" },
                    badgeLockedUrl = normalizeRetroAchievementsImageUrl(item.optString("BadgeLockedURL"))
                        ?: item.optString("BadgeName").takeIf { it.isNotBlank() }?.let { "https://media.retroachievements.org/Badge/${it}_lock.png" }
                )
            )
        }
    }
}

private fun RetroAchievementAccountProgress.toSummaryGameData(): RetroAchievementGameData {
    return RetroAchievementGameData(
        gameId = gameId,
        title = title,
        gameImageUrl = gameImageUrl,
        achievements = emptyList(),
        resolvedOnly = true,
        earnedCountOverride = earnedAchievements,
        totalCountOverride = totalAchievements,
        earnedHardcoreCountOverride = earnedHardcoreAchievements,
        earnedPointsOverride = 0,
        totalPointsOverride = 0
    )
}

private fun JSONArray?.toRetroAchievementEntries(): List<RetroAchievementEntry> {
    if (this == null) return emptyList()
    return buildList {
        for (index in 0 until length()) {
            val item = optJSONObject(index) ?: continue
            val title = item.optString("title")
            val description = item.optString("description")
            if (isRetroAchievementsUnsupportedEmulatorWarning(title, description)) {
                continue
            }
            add(
                RetroAchievementEntry(
                    id = item.optLong("id"),
                    title = title,
                    description = description,
                    points = item.optInt("points"),
                    category = item.optInt("category"),
                    type = item.optInt("type"),
                    rarity = item.optDouble("rarity").toFloat(),
                    rarityHardcore = item.optDouble("rarityHardcore").toFloat(),
                    earnedSoftcore = item.optBoolean("earnedSoftcore"),
                    earnedHardcore = item.optBoolean("earnedHardcore"),
                    badgeUrl = normalizeRetroAchievementsImageUrl(item.optString("badgeUrl")),
                    badgeLockedUrl = normalizeRetroAchievementsImageUrl(item.optString("badgeLockedUrl"))
                )
            )
        }
    }
}

private fun isRetroAchievementsUnsupportedEmulatorWarning(title: String, description: String): Boolean {
    return title.equals("Warning: Unknown Emulator", ignoreCase = true) ||
        description.contains("Hardcore unlocks cannot be earned using this emulator", ignoreCase = true)
}

private fun normalizeRetroAchievementsImageUrl(value: String?): String? {
    val clean = value?.trim().orEmpty()
    if (clean.isBlank()) return null

    return when {
        clean.startsWith("http://", ignoreCase = true) ||
            clean.startsWith("https://", ignoreCase = true) ||
            clean.startsWith("content://", ignoreCase = true) -> clean

        clean.startsWith("/Images/", ignoreCase = true) ||
            clean.startsWith("/Badge/", ignoreCase = true) -> "https://media.retroachievements.org$clean"

        clean.startsWith("Images/", ignoreCase = true) ||
            clean.startsWith("Badge/", ignoreCase = true) -> "https://media.retroachievements.org/$clean"

        else -> clean
    }
}
