package com.sbro.emucorex.data

import android.app.Activity
import android.content.Context
import com.google.android.gms.tasks.Task
import com.google.firebase.Timestamp
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.auth.GoogleAuthProvider
import com.google.firebase.auth.OAuthProvider
import com.google.firebase.auth.UserProfileChangeRequest
import com.google.firebase.firestore.DocumentSnapshot
import com.google.firebase.firestore.FieldValue
import com.google.firebase.firestore.FirebaseFirestore
import com.google.firebase.firestore.Query
import com.google.firebase.firestore.SetOptions
import kotlinx.coroutines.channels.awaitClose
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.callbackFlow
import kotlinx.coroutines.suspendCancellableCoroutine
import java.io.File
import java.security.MessageDigest
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

data class PlayerAccount(
    val uid: String,
    val email: String?,
    val displayName: String,
    val photoURL: String?
)

data class PlayerProfile(
    val uid: String,
    val email: String?,
    val displayName: String,
    val photoURL: String?,
    val totalPlayTimeMs: Long,
    val gamesPlayed: Int,
    val lastPlayedTitle: String?,
    val lastPlayedAtMs: Long?,
    val games: List<PlayerGamePlayStat>
)

data class PlayerGamePlayStat(
    val gameKey: String,
    val title: String,
    val serial: String?,
    val coverArtPath: String?,
    val totalPlayTimeMs: Long,
    val sessions: Int,
    val lastPlayedAtMs: Long?
)

data class PlayerLeaderboardEntry(
    val rank: Int,
    val uid: String,
    val displayName: String,
    val photoURL: String?,
    val totalPlayTimeMs: Long,
    val gamesPlayed: Int,
    val lastPlayedTitle: String?
)

data class PlayerPlayTimeDelta(
    val gamePath: String?,
    val title: String,
    val serial: String?,
    val coverArtPath: String?,
    val durationMs: Long,
    val sessionCount: Long = 0L,
    val lastPlayedAtMs: Long = System.currentTimeMillis()
)

class PlayerProfileRepository(context: Context) {

    private val appContext = context.applicationContext
    private val auth = FirebaseAuth.getInstance()
    private val firestore = FirebaseFirestore.getInstance()
    private val coverArtRepository = CoverArtRepository(appContext)

    fun observeAuthState(): Flow<PlayerAccount?> = callbackFlow {
        val listener = FirebaseAuth.AuthStateListener { firebaseAuth ->
            trySend(firebaseAuth.currentUser?.toPlayerAccount())
        }
        auth.addAuthStateListener(listener)
        trySend(auth.currentUser?.toPlayerAccount())
        awaitClose { auth.removeAuthStateListener(listener) }
    }

    fun hasSignedInUser(): Boolean = auth.currentUser != null

    fun observeProfile(uid: String): Flow<PlayerProfile?> = callbackFlow {
        val registration = firestore.collection(USERS_COLLECTION)
            .document(uid)
            .addSnapshotListener { snapshot, error ->
                if (error != null) {
                    close(error)
                    return@addSnapshotListener
                }
                trySend(snapshot?.toPlayerProfile())
            }
        awaitClose { registration.remove() }
    }

    fun observeLeaderboard(): Flow<List<PlayerLeaderboardEntry>> = callbackFlow {
        val registration = firestore.collection(LEADERBOARD_COLLECTION)
            .orderBy(FIELD_TOTAL_PLAY_TIME_MS, Query.Direction.DESCENDING)
            .limit(LEADERBOARD_LIMIT)
            .addSnapshotListener { snapshot, error ->
                if (error != null) {
                    close(error)
                    return@addSnapshotListener
                }
                val entries = snapshot?.documents.orEmpty()
                    .mapIndexedNotNull { index, document ->
                        document.toPlayerLeaderboardEntry(index + 1)
                    }
                trySend(entries)
            }
        awaitClose { registration.remove() }
    }

    suspend fun signIn(email: String, password: String) {
        auth.signInWithEmailAndPassword(email.trim(), password).await()
        auth.currentUser?.let { user ->
            ensureUserProfile(user.uid, user.email, user.displayName.cleanDisplayName(user.email), user.bestPhotoUrl())
        }
    }

    suspend fun signInWithGoogle(activity: Activity) {
        val provider = OAuthProvider.newBuilder(GoogleAuthProvider.PROVIDER_ID)
            .addCustomParameter("prompt", "select_account")
            .build()
        val result = (auth.pendingAuthResult ?: auth.startActivityForSignInWithProvider(activity, provider)).await()
        result.user?.let { user ->
            ensureUserProfile(user.uid, user.email, user.displayName.cleanDisplayName(user.email), user.bestPhotoUrl())
        }
    }

    suspend fun createAccount(email: String, password: String, displayName: String) {
        val result = auth.createUserWithEmailAndPassword(email.trim(), password).await()
        val user = result.user ?: return
        val cleanName = displayName.cleanDisplayName(user.email)
        user.updateProfile(
            UserProfileChangeRequest.Builder()
                .setDisplayName(cleanName)
                .build()
        ).await()
        ensureUserProfile(user.uid, user.email, cleanName, user.bestPhotoUrl())
    }

    suspend fun sendPasswordReset(email: String) {
        auth.sendPasswordResetEmail(email.trim()).await()
    }

    suspend fun updateDisplayName(displayName: String) {
        val user = auth.currentUser ?: return
        val cleanName = displayName.cleanDisplayName(user.email)
        user.updateProfile(
            UserProfileChangeRequest.Builder()
                .setDisplayName(cleanName)
                .build()
        ).await()
        val userPatch = mapOf(
            FIELD_DISPLAY_NAME to cleanName,
            FIELD_UPDATED_AT to FieldValue.serverTimestamp()
        )
        firestore.collection(USERS_COLLECTION).document(user.uid)
            .set(userPatch, SetOptions.merge())
            .await()
        firestore.collection(LEADERBOARD_COLLECTION).document(user.uid)
            .set(userPatch, SetOptions.merge())
            .await()
    }

    fun signOut() {
        auth.signOut()
    }

    suspend fun recordPlayTime(
        gamePath: String?,
        title: String,
        serial: String?,
        coverArtPath: String?,
        durationMs: Long
    ) {
        recordPlayTimeBatch(
            listOf(
                PlayerPlayTimeDelta(
                    gamePath = gamePath,
                    title = title,
                    serial = serial,
                    coverArtPath = coverArtPath,
                    durationMs = durationMs,
                    sessionCount = 1L
                )
            )
        )
    }

    suspend fun recordPlayTimeBatch(entries: List<PlayerPlayTimeDelta>) {
        val user = auth.currentUser ?: return
        val validEntries = entries
            .filter { entry ->
                !entry.gamePath.isNullOrBlank() &&
                    entry.durationMs > 0L &&
                    !entry.title.equals(BIOS_TITLE, ignoreCase = true)
            }
            .groupBy { entry -> buildGameKey(entry.serial, entry.gamePath.orEmpty()) }
            .map { (gameKey, gameEntries) ->
                val latestEntry = gameEntries.maxBy { it.lastPlayedAtMs }
                gameKey to latestEntry.copy(
                    durationMs = gameEntries.sumOf { it.durationMs },
                    sessionCount = gameEntries.sumOf { it.sessionCount }
                )
            }
        if (validEntries.isEmpty()) return

        val latestEntry = validEntries.maxBy { it.second.lastPlayedAtMs }.second
        val nowMs = System.currentTimeMillis()
        val latestTitle = latestEntry.title.ifBlank { latestEntry.gamePath.orEmpty().substringAfterLast('/').substringBeforeLast('.') }
        val cleanName = user.displayName.cleanDisplayName(user.email)
        val photoURL = user.bestPhotoUrl().orEmpty()
        val userRef = firestore.collection(USERS_COLLECTION).document(user.uid)
        val leaderboardRef = firestore.collection(LEADERBOARD_COLLECTION).document(user.uid)

        firestore.runTransaction { transaction ->
            val snapshot = transaction.get(userRef)
            val existingGames = snapshot.getGamesMap().toMutableMap()
            validEntries.forEach { (gameKey, entry) ->
                val cleanTitle = entry.title.ifBlank { entry.gamePath.orEmpty().substringAfterLast('/').substringBeforeLast('.') }
                val existingGame = existingGames[gameKey].toGameMap()
                val nextGame = buildMap<String, Any> {
                    put(GAME_TITLE, cleanTitle.take(MAX_GAME_TITLE_LENGTH))
                    put(GAME_TOTAL_MS, existingGame.longValue(GAME_TOTAL_MS) + entry.durationMs)
                    put(GAME_SESSIONS, existingGame.longValue(GAME_SESSIONS) + entry.sessionCount)
                    put(GAME_LAST_PLAYED_AT_MS, entry.lastPlayedAtMs)
                    entry.serial?.takeIf { it.isNotBlank() }?.let { put(GAME_SERIAL, it.take(MAX_SERIAL_LENGTH)) }
                    buildShareableCoverPath(entry.coverArtPath, entry.serial)?.let { put(GAME_COVER_ART_PATH, it.take(MAX_COVER_PATH_LENGTH)) }
                }
                existingGames[gameKey] = nextGame
            }

            val totalPlayTimeMs = (snapshot.getLong(FIELD_TOTAL_PLAY_TIME_MS) ?: 0L) + validEntries.sumOf { it.second.durationMs }
            val gamesPlayed = existingGames.size.toLong()
            val userData = mapOf(
                FIELD_UID to user.uid,
                FIELD_EMAIL to (user.email ?: ""),
                FIELD_DISPLAY_NAME to cleanName,
                FIELD_PHOTO_URL to photoURL,
                FIELD_TOTAL_PLAY_TIME_MS to totalPlayTimeMs,
                FIELD_GAMES_PLAYED to gamesPlayed,
                FIELD_LAST_PLAYED_TITLE to latestTitle.take(MAX_GAME_TITLE_LENGTH),
                FIELD_LAST_PLAYED_AT_MS to latestEntry.lastPlayedAtMs,
                FIELD_GAMES to existingGames,
                FIELD_UPDATED_AT to FieldValue.serverTimestamp(),
                FIELD_CREATED_AT to (snapshot.getTimestamp(FIELD_CREATED_AT) ?: FieldValue.serverTimestamp())
            )
            val leaderboardData = mapOf(
                FIELD_UID to user.uid,
                FIELD_DISPLAY_NAME to cleanName,
                FIELD_PHOTO_URL to photoURL,
                FIELD_TOTAL_PLAY_TIME_MS to totalPlayTimeMs,
                FIELD_GAMES_PLAYED to gamesPlayed,
                FIELD_LAST_PLAYED_TITLE to latestTitle.take(MAX_GAME_TITLE_LENGTH),
                FIELD_UPDATED_AT to FieldValue.serverTimestamp()
            )

            transaction.set(userRef, userData, SetOptions.merge())
            transaction.set(leaderboardRef, leaderboardData, SetOptions.merge())
        }.await()
    }

    private suspend fun ensureUserProfile(uid: String, email: String?, displayName: String, photoURL: String?) {
        val userRef = firestore.collection(USERS_COLLECTION).document(uid)
        val leaderboardRef = firestore.collection(LEADERBOARD_COLLECTION).document(uid)
        firestore.runTransaction { transaction ->
            val snapshot = transaction.get(userRef)
            val existingGames = snapshot.getGamesMap()
            val totalPlayTimeMs = snapshot.getLong(FIELD_TOTAL_PLAY_TIME_MS) ?: 0L
            val gamesPlayed = if (snapshot.exists()) {
                snapshot.getLong(FIELD_GAMES_PLAYED) ?: existingGames.size.toLong()
            } else {
                0L
            }
            val userData = mutableMapOf<String, Any?>(
                FIELD_UID to uid,
                FIELD_EMAIL to (email ?: ""),
                FIELD_DISPLAY_NAME to displayName,
                FIELD_PHOTO_URL to photoURL.orEmpty(),
                FIELD_TOTAL_PLAY_TIME_MS to totalPlayTimeMs,
                FIELD_GAMES_PLAYED to gamesPlayed,
                FIELD_GAMES to existingGames,
                FIELD_UPDATED_AT to FieldValue.serverTimestamp()
            )
            if (!snapshot.exists()) {
                userData[FIELD_CREATED_AT] = FieldValue.serverTimestamp()
            }
            val leaderboardData = mapOf(
                FIELD_UID to uid,
                FIELD_DISPLAY_NAME to displayName,
                FIELD_PHOTO_URL to photoURL.orEmpty(),
                FIELD_TOTAL_PLAY_TIME_MS to totalPlayTimeMs,
                FIELD_GAMES_PLAYED to gamesPlayed,
                FIELD_LAST_PLAYED_TITLE to snapshot.getString(FIELD_LAST_PLAYED_TITLE).orEmpty(),
                FIELD_UPDATED_AT to FieldValue.serverTimestamp()
            )
            transaction.set(userRef, userData, SetOptions.merge())
            transaction.set(leaderboardRef, leaderboardData, SetOptions.merge())
        }.await()
    }

    private fun DocumentSnapshot.toPlayerProfile(): PlayerProfile? {
        if (!exists()) return null
        val uid = getString(FIELD_UID) ?: id
        val games = getGamesMap()
            .mapNotNull { (key, value) -> value.toPlayerGamePlayStat(key) }
            .sortedByDescending { it.totalPlayTimeMs }
        return PlayerProfile(
            uid = uid,
            email = getString(FIELD_EMAIL),
            displayName = getString(FIELD_DISPLAY_NAME).cleanDisplayName(getString(FIELD_EMAIL)),
            photoURL = getString(FIELD_PHOTO_URL),
            totalPlayTimeMs = getLong(FIELD_TOTAL_PLAY_TIME_MS) ?: games.sumOf { it.totalPlayTimeMs },
            gamesPlayed = (getLong(FIELD_GAMES_PLAYED) ?: games.size.toLong()).toInt(),
            lastPlayedTitle = getString(FIELD_LAST_PLAYED_TITLE),
            lastPlayedAtMs = getLong(FIELD_LAST_PLAYED_AT_MS) ?: getTimestamp(FIELD_LAST_PLAYED_AT)?.toMillisCompat(),
            games = games
        )
    }

    private fun DocumentSnapshot.toPlayerLeaderboardEntry(rank: Int): PlayerLeaderboardEntry? {
        if (!exists()) return null
        val uid = getString(FIELD_UID) ?: id
        return PlayerLeaderboardEntry(
            rank = rank,
            uid = uid,
            displayName = getString(FIELD_DISPLAY_NAME).cleanDisplayName(null),
            photoURL = getString(FIELD_PHOTO_URL),
            totalPlayTimeMs = getLong(FIELD_TOTAL_PLAY_TIME_MS) ?: 0L,
            gamesPlayed = (getLong(FIELD_GAMES_PLAYED) ?: 0L).toInt(),
            lastPlayedTitle = getString(FIELD_LAST_PLAYED_TITLE)
        )
    }

    private fun Map.Entry<String, Any?>.toPlayerGamePlayStat(): PlayerGamePlayStat? {
        return value.toPlayerGamePlayStat(key)
    }

    private fun Any?.toPlayerGamePlayStat(gameKey: String): PlayerGamePlayStat? {
        val map = toGameMap()
        val title = map.stringValue(GAME_TITLE).takeIf { it.isNotBlank() } ?: return null
        val serial = map.stringValue(GAME_SERIAL).takeIf { it.isNotBlank() }
        return PlayerGamePlayStat(
            gameKey = gameKey,
            title = title,
            serial = serial,
            coverArtPath = resolveReadableCoverPath(map.stringValue(GAME_COVER_ART_PATH), serial),
            totalPlayTimeMs = map.longValue(GAME_TOTAL_MS),
            sessions = map.longValue(GAME_SESSIONS).toInt(),
            lastPlayedAtMs = map.longValue(GAME_LAST_PLAYED_AT_MS).takeIf { it > 0L }
        )
    }

    private fun DocumentSnapshot.getGamesMap(): Map<String, Any?> {
        @Suppress("UNCHECKED_CAST")
        return (get(FIELD_GAMES) as? Map<String, Any?>).orEmpty()
    }

    private fun Any?.toGameMap(): Map<String, Any?> {
        @Suppress("UNCHECKED_CAST")
        return this as? Map<String, Any?> ?: emptyMap()
    }

    private fun Map<String, Any?>.stringValue(key: String): String {
        return this[key] as? String ?: ""
    }

    private fun Map<String, Any?>.longValue(key: String): Long {
        return when (val value = this[key]) {
            is Long -> value
            is Int -> value.toLong()
            is Double -> value.toLong()
            is Number -> value.toLong()
            else -> 0L
        }
    }

    private fun com.google.firebase.auth.FirebaseUser.toPlayerAccount(): PlayerAccount {
        return PlayerAccount(
            uid = uid,
            email = email,
            displayName = displayName.cleanDisplayName(email),
            photoURL = bestPhotoUrl()
        )
    }

    private fun com.google.firebase.auth.FirebaseUser.bestPhotoUrl(): String? {
        return photoUrl?.toString()
            ?: providerData.firstNotNullOfOrNull { it.photoUrl?.toString() }
    }

    private fun String?.cleanDisplayName(email: String?): String {
        val fallback = email?.substringBefore('@')?.takeIf { it.isNotBlank() } ?: DEFAULT_DISPLAY_NAME
        return this?.trim()?.takeIf { it.isNotBlank() }?.take(MAX_DISPLAY_NAME_LENGTH) ?: fallback
    }

    private fun buildGameKey(serial: String?, gamePath: String): String {
        val serialKey = serial
            ?.trim()
            ?.uppercase()
            ?.replace(Regex("[^A-Z0-9_-]"), "_")
            ?.takeIf { it.isNotBlank() }
        return serialKey ?: "P_${gamePath.sha256().take(32)}"
    }

    private fun buildShareableCoverPath(coverArtPath: String?, serial: String?): String? {
        val cover = coverArtPath?.trim().orEmpty()
        return when {
            cover.startsWith("http://") || cover.startsWith("https://") -> cover
            else -> coverArtRepository.buildPublicCoverUrl(serial)
        }
    }

    private fun resolveReadableCoverPath(coverArtPath: String, serial: String?): String? {
        val cover = coverArtPath.trim()
        return when {
            cover.isBlank() -> coverArtRepository.buildPublicCoverUrl(serial)
            cover.startsWith("http://") || cover.startsWith("https://") -> cover
            cover.startsWith("content://") -> cover
            File(cover).exists() -> cover
            else -> coverArtRepository.buildPublicCoverUrl(serial)
        }
    }

    private fun String.sha256(): String {
        val digest = MessageDigest.getInstance("SHA-256")
        return digest.digest(toByteArray()).joinToString("") { "%02x".format(it) }
    }

    private fun Timestamp.toMillisCompat(): Long = seconds * 1000L + nanoseconds / 1_000_000L

    private suspend fun <T> Task<T>.await(): T = suspendCancellableCoroutine { continuation ->
        addOnSuccessListener { result ->
            continuation.resume(result)
        }
        addOnFailureListener { error ->
            continuation.resumeWithException(error)
        }
        addOnCanceledListener {
            continuation.cancel()
        }
    }

    companion object {
        private const val USERS_COLLECTION = "users"
        private const val LEADERBOARD_COLLECTION = "leaderboardPlayTime"
        private const val LEADERBOARD_LIMIT = 100L
        private const val MAX_DISPLAY_NAME_LENGTH = 32
        private const val MAX_GAME_TITLE_LENGTH = 120
        private const val MAX_SERIAL_LENGTH = 32
        private const val MAX_COVER_PATH_LENGTH = 500
        private const val DEFAULT_DISPLAY_NAME = "Player"
        private const val BIOS_TITLE = "PlayStation 2 BIOS"

        private const val FIELD_UID = "uid"
        private const val FIELD_EMAIL = "email"
        private const val FIELD_DISPLAY_NAME = "displayName"
        private const val FIELD_PHOTO_URL = "photoURL"
        private const val FIELD_TOTAL_PLAY_TIME_MS = "totalPlayTimeMs"
        private const val FIELD_GAMES_PLAYED = "gamesPlayed"
        private const val FIELD_LAST_PLAYED_TITLE = "lastPlayedTitle"
        private const val FIELD_LAST_PLAYED_AT = "lastPlayedAt"
        private const val FIELD_LAST_PLAYED_AT_MS = "lastPlayedAtMs"
        private const val FIELD_UPDATED_AT = "updatedAt"
        private const val FIELD_CREATED_AT = "createdAt"
        private const val FIELD_GAMES = "games"

        private const val GAME_TITLE = "t"
        private const val GAME_TOTAL_MS = "ms"
        private const val GAME_SESSIONS = "n"
        private const val GAME_LAST_PLAYED_AT_MS = "lp"
        private const val GAME_SERIAL = "s"
        private const val GAME_COVER_ART_PATH = "c"
    }
}
