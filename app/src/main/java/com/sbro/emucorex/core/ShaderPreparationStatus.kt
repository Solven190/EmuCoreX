package com.sbro.emucorex.core

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.jsonObject

/** Typed snapshot of the native asynchronous Vulkan TFX warmup job. */
data class ShaderPreparationStatus(
    val jobId: Long,
    val attempt: Int,
    val state: State,
    val error: Error,
    val total: Int,
    val completed: Int,
    val alreadyAvailable: Int,
    val failed: Int,
    val cancelled: Int,
    val inFlight: Int,
    val finished: Boolean,
    val checkpointSaved: Boolean,
    val cancelRequested: Boolean,
    val adapterName: String
) {
    val settled: Int
        get() = completed + alreadyAvailable + failed + cancelled

    enum class State(val wireToken: String) {
        IDLE("idle"),
        WAITING_FOR_LIFECYCLE("waiting_for_lifecycle"),
        STARTING("starting"),
        RUNNING("running"),
        CANCELLING("cancelling"),
        COMPLETED("completed"),
        CANCELLED("cancelled"),
        FAILED("failed"),
        UNSUPPORTED("unsupported")
    }

    enum class Error(val wireToken: String) {
        NONE("none"),
        LIFECYCLE_BUSY("lifecycle_busy"),
        UNSUPPORTED_RENDERER("unsupported_renderer"),
        CACHE_DISABLED("cache_disabled"),
        MISSING_SOURCE("missing_source"),
        DEVICE_CREATION_FAILED("device_creation_failed"),
        PIPELINE_FAILURES("pipeline_failures"),
        CHECKPOINT_FAILED("checkpoint_failed"),
        INTERNAL_ERROR("internal_error")
    }

    companion object {
        private val json = Json { isLenient = false }

        /** Parses one schema-versioned native snapshot; malformed/future data safely yields null. */
        fun parseSnapshot(snapshot: String?): ShaderPreparationStatus? {
            if (snapshot.isNullOrBlank())
                return null

            return try {
                val root = json.parseToJsonElement(snapshot).jsonObject
                val schemaVersion = root.requiredInt("schemaVersion") ?: return null
                if (schemaVersion != 1)
                    return null

                val jobIdText = root.requiredString("jobId") ?: return null
                if (jobIdText.isEmpty() || (jobIdText.length > 1 && jobIdText[0] == '0') ||
                    jobIdText.any { !it.isDigit() })
                    return null
                val jobId = jobIdText.toLongOrNull() ?: return null
                val attempt = root.requiredInt("attempt") ?: return null
                val state = State.entries.firstOrNull { it.wireToken == root.requiredString("state") } ?: return null
                val error = Error.entries.firstOrNull { it.wireToken == root.requiredString("error") } ?: return null
                val total = root.requiredInt("total") ?: return null
                val completed = root.requiredInt("completed") ?: return null
                val alreadyAvailable = root.requiredInt("alreadyAvailable") ?: return null
                val failed = root.requiredInt("failed") ?: return null
                val cancelled = root.requiredInt("cancelled") ?: return null
                val inFlight = root.requiredInt("inFlight") ?: return null
                val finished = root.requiredBoolean("finished") ?: return null
                val checkpointSaved = root.requiredBoolean("checkpointSaved") ?: return null
                val cancelRequested = root.requiredBoolean("cancelRequested") ?: return null
                val adapterName = root.requiredString("adapterName") ?: return null

                if (attempt < 0 || total < 0 || completed < 0 || alreadyAvailable < 0 || failed < 0 ||
                    cancelled < 0 || inFlight < 0)
                    return null

                val settled = completed.toLong() + alreadyAvailable + failed + cancelled
                if (settled > total || inFlight.toLong() > total.toLong() - settled)
                    return null
                if (finished && (inFlight != 0 || settled != total.toLong()))
                    return null
                if (checkpointSaved && (!finished || total == 0))
                    return null

                val status = ShaderPreparationStatus(
                    jobId = jobId,
                    attempt = attempt,
                    state = state,
                    error = error,
                    total = total,
                    completed = completed,
                    alreadyAvailable = alreadyAvailable,
                    failed = failed,
                    cancelled = cancelled,
                    inFlight = inFlight,
                    finished = finished,
                    checkpointSaved = checkpointSaved,
                    cancelRequested = cancelRequested,
                    adapterName = adapterName
                )
                if (status.isConsistent()) status else null
            } catch (_: Exception) {
                null
            }
        }

        private fun ShaderPreparationStatus.isConsistent(): Boolean {
            val terminal = state == State.COMPLETED || state == State.CANCELLED ||
                state == State.FAILED || state == State.UNSUPPORTED
            if (finished && state == State.IDLE)
                return false
            if (jobId == 0L) {
                return state == State.IDLE && attempt == 0 && error == Error.NONE && total == 0 &&
                    completed == 0 && alreadyAvailable == 0 && failed == 0 && cancelled == 0 &&
                    inFlight == 0 && !finished && !checkpointSaved && !cancelRequested && adapterName.isEmpty()
            }
            if (jobId < 0 || attempt <= 0 || state == State.IDLE)
                return false
            if ((state == State.CANCELLING || state == State.CANCELLED) != cancelRequested)
                return false
            if (!terminal && error != Error.NONE)
                return false

            return when (state) {
                State.COMPLETED -> error == Error.NONE && finished && checkpointSaved && total > 0 &&
                    failed == 0 && cancelled == 0 && inFlight == 0 && settled.toLong() == total.toLong()
                State.CANCELLED -> error == Error.NONE && inFlight == 0 &&
                    (total == 0 && !finished || total > 0 && finished)
                State.FAILED -> error != Error.NONE && inFlight == 0
                State.UNSUPPORTED -> (error == Error.UNSUPPORTED_RENDERER || error == Error.CACHE_DISABLED) &&
                    total == 0 && completed == 0 && alreadyAvailable == 0 && failed == 0 && cancelled == 0 &&
                    inFlight == 0 && !finished && !checkpointSaved
                else -> true
            }
        }

        private fun JsonObject.requiredString(key: String): String? =
            (this[key] as? JsonPrimitive)?.takeIf { it.isString }?.content

        private fun JsonObject.requiredInt(key: String): Int? =
            (this[key] as? JsonPrimitive)?.takeIf { !it.isString }?.intOrNull

        private fun JsonObject.requiredBoolean(key: String): Boolean? =
            (this[key] as? JsonPrimitive)?.takeIf { !it.isString }?.booleanOrNull
    }
}
