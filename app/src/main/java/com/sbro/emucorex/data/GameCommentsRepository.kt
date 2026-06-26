package com.sbro.emucorex.data

import com.google.android.gms.tasks.Task
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.firestore.FieldValue
import com.google.firebase.firestore.FirebaseFirestore
import com.google.firebase.firestore.ListenerRegistration
import com.google.firebase.firestore.Query
import kotlinx.coroutines.suspendCancellableCoroutine
import java.util.Date
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

data class GameComment(
    val id: String,
    val uid: String,
    val displayName: String,
    val photoURL: String = "",
    val text: String,
    val rating: Int,
    val phoneBrand: String = "",
    val phoneId: String = "",
    val phoneModel: String = "",
    val phoneName: String = "",
    val phoneCpu: String = "",
    val phoneRam: String = "",
    val createdAt: Date? = null
) {
    val deviceTitle: String
        get() = phoneName.ifBlank {
            listOf(phoneBrand, phoneModel)
                .filter { it.isNotBlank() }
                .joinToString(" ")
        }

    val deviceSpecs: String
        get() = listOf(phoneCpu, phoneRam)
            .filter { it.isNotBlank() }
            .joinToString(" • ")
}

class GameCommentsRepository(
    private val firestore: FirebaseFirestore = FirebaseFirestore.getInstance(),
    private val auth: FirebaseAuth = FirebaseAuth.getInstance()
) {

    fun observeComments(
        gameId: Long,
        onUpdate: (List<GameComment>) -> Unit,
        onError: (Throwable) -> Unit
    ): ListenerRegistration {
        return firestore.collection("games")
            .document(gameId.toString())
            .collection("comments")
            .orderBy("createdAt", Query.Direction.DESCENDING)
            .limit(60)
            .addSnapshotListener { snapshot, error ->
                if (error != null) {
                    onError(error)
                    return@addSnapshotListener
                }

                val comments = snapshot?.documents?.mapNotNull { document ->
                    val text = document.getString("text")?.trim().orEmpty()
                    if (text.isBlank()) return@mapNotNull null
                    GameComment(
                        id = document.id,
                        uid = document.getString("uid").orEmpty(),
                        displayName = document.getString("displayName")?.trim().orEmpty().ifBlank { "Player" },
                        photoURL = document.getString("photoURL").orEmpty(),
                        text = text,
                        rating = (document.getLong("rating") ?: 0L).toInt().coerceIn(0, 5),
                        phoneBrand = document.getString("phoneBrand").orEmpty(),
                        phoneId = document.getString("phoneId").orEmpty(),
                        phoneModel = document.getString("phoneModel").orEmpty(),
                        phoneName = document.getString("phoneName").orEmpty(),
                        phoneCpu = document.getString("phoneCpu").orEmpty(),
                        phoneRam = document.getString("phoneRam").orEmpty(),
                        createdAt = document.getTimestamp("createdAt")?.toDate()
                    )
                }.orEmpty()

                onUpdate(comments)
            }
    }

    suspend fun addComment(
        gameId: Long,
        rating: Int,
        text: String,
        device: GameCommentDeviceInfo
    ) {
        val user = auth.currentUser ?: error("Sign in required.")
        val cleanText = text.trim()
        require(cleanText.isNotBlank()) { "Comment text is required." }
        require(cleanText.length <= 800) { "Comment is too long." }
        val data = mapOf(
            "gameId" to gameId.toString(),
            "rating" to rating.coerceIn(1, 5),
            "text" to cleanText,
            "uid" to user.uid,
            "displayName" to (user.displayName ?: user.email ?: "Player").take(120),
            "photoURL" to (user.photoUrl?.toString() ?: "").take(500),
            "phoneBrand" to device.phoneBrand.take(80),
            "phoneId" to device.phoneId.take(80),
            "phoneModel" to device.phoneModel.take(160),
            "phoneName" to device.phoneName.take(200),
            "phoneCpu" to device.phoneCpu.take(160),
            "phoneRam" to device.phoneRam.take(80),
            "createdAt" to FieldValue.serverTimestamp()
        )
        firestore.collection("games")
            .document(gameId.toString())
            .collection("comments")
            .add(data)
            .await()
    }

    private suspend fun <T> Task<T>.await(): T = suspendCancellableCoroutine { continuation ->
        addOnSuccessListener { result -> continuation.resume(result) }
        addOnFailureListener { error -> continuation.resumeWithException(error) }
        addOnCanceledListener { continuation.cancel() }
    }
}

data class GameCommentDeviceInfo(
    val phoneBrand: String,
    val phoneId: String,
    val phoneModel: String,
    val phoneName: String,
    val phoneCpu: String,
    val phoneRam: String
)
