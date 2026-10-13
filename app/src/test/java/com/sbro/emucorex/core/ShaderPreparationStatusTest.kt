package com.sbro.emucorex.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ShaderPreparationStatusTest {
    @Test
    fun parsesIdleSnapshotAndExactUnsignedWireIdentity() {
        val idle = ShaderPreparationStatus.parseSnapshot(stateSnapshot("idle"))
        assertNotNull(idle)
        assertEquals(ShaderPreparationStatus.State.IDLE, idle?.state)
        assertEquals(ShaderPreparationStatus.Error.NONE, idle?.error)
        assertEquals(0L, idle?.jobId)

        val largeId = "9223372036854775807"
        val running = ShaderPreparationStatus.parseSnapshot(snapshot(jobId = largeId, state = "running"))
        assertEquals(largeId.toLong(), running?.jobId)
    }

    @Test
    fun mapsEveryStableStateAndErrorToken() {
        val states = listOf(
            "idle", "waiting_for_lifecycle", "starting", "running", "cancelling",
            "completed", "cancelled", "failed", "unsupported"
        )
        assertEquals(
            states,
            states.map { token -> ShaderPreparationStatus.parseSnapshot(stateSnapshot(token))?.state?.wireToken }
        )

        val errors = listOf(
            "none", "lifecycle_busy", "unsupported_renderer", "cache_disabled", "missing_source",
            "device_creation_failed", "pipeline_failures", "checkpoint_failed", "internal_error"
        )
        assertEquals(
            errors,
            errors.map { token -> ShaderPreparationStatus.parseSnapshot(errorSnapshot(token))?.error?.wireToken }
        )
    }

    @Test
    fun parsesProgressBeyondBaselineCatalogAndEscapedAdapterName() {
        val adapter = "GPU \"native\"\\folder\nsecond line"
        val raw = snapshot(
            jobId = "42", state = "running", total = 180, completed = 90,
            alreadyAvailable = 20, failed = 2, cancelled = 3, inFlight = 4,
            adapterName = adapter
        )
        val status = ShaderPreparationStatus.parseSnapshot(raw)
        assertNotNull(status)
        assertEquals(180, status?.total)
        assertEquals(115, status?.settled)
        assertEquals(adapter, status?.adapterName)
        assertTrue(status?.inFlight == 4)
    }

    @Test
    fun parsesUnicodeEscapedAdapterNameFromAsciiNativeJson() {
        val expectedAdapter = "\u00e9\u0416\ud83d\udda5"
        val status = ShaderPreparationStatus.parseSnapshot(
            """{"schemaVersion":1,"jobId":"43","attempt":1,"state":"running","error":"none","total":0,"completed":0,"alreadyAvailable":0,"failed":0,"cancelled":0,"inFlight":0,"finished":false,"checkpointSaved":false,"cancelRequested":false,"adapterName":"\u00e9\u0416\ud83d\udda5"}"""
        )
        assertEquals(expectedAdapter, status?.adapterName)
    }

    @Test
    fun rejectsUnknownSchemaOrEnumTokens() {
        assertNull(ShaderPreparationStatus.parseSnapshot(snapshot(schemaVersion = 2)))
        assertNull(ShaderPreparationStatus.parseSnapshot(snapshot(state = "future_state")))
        assertNull(ShaderPreparationStatus.parseSnapshot(snapshot(error = "future_error")))
    }

    @Test
    fun rejectsMissingFieldsMalformedInputAndInvalidJobIds() {
        assertNull(ShaderPreparationStatus.parseSnapshot(null))
        assertNull(ShaderPreparationStatus.parseSnapshot("not json"))
        assertNull(ShaderPreparationStatus.parseSnapshot(snapshot().replace("\"inFlight\":0,", "")))
        assertNull(ShaderPreparationStatus.parseSnapshot(snapshot(jobId = "-1", state = "running")))
        assertNull(ShaderPreparationStatus.parseSnapshot(snapshot(jobId = "9223372036854775808", state = "running")))
        assertNull(ShaderPreparationStatus.parseSnapshot(snapshot(jobId = "0", state = "running")))
    }

    @Test
    fun rejectsImpossibleProgressAndTerminalCheckpointClaims() {
        assertNull(ShaderPreparationStatus.parseSnapshot(snapshot(total = 4, completed = 3, alreadyAvailable = 2)))
        assertNull(ShaderPreparationStatus.parseSnapshot(snapshot(total = 4, completed = 3, inFlight = 2)))
        assertNull(ShaderPreparationStatus.parseSnapshot(snapshot(state = "running", finished = true)))
        assertNull(ShaderPreparationStatus.parseSnapshot(snapshot(state = "completed", finished = false)))
        assertNull(ShaderPreparationStatus.parseSnapshot(snapshot(state = "completed", checkpointSaved = true, failed = 1)))
    }

    private fun snapshot(
        schemaVersion: Int = 1,
        jobId: String = "7",
        attempt: Int = 1,
        state: String = "running",
        error: String = "none",
        total: Int = 164,
        completed: Int = 0,
        alreadyAvailable: Int = 0,
        failed: Int = 0,
        cancelled: Int = 0,
        inFlight: Int = 0,
        finished: Boolean = state in setOf("completed", "cancelled", "failed", "unsupported"),
        checkpointSaved: Boolean = false,
        cancelRequested: Boolean = state == "cancelling",
        adapterName: String = "Adreno 620"
    ): String = """{"schemaVersion":$schemaVersion,"jobId":"$jobId","attempt":$attempt,"state":"$state","error":"$error","total":$total,"completed":$completed,"alreadyAvailable":$alreadyAvailable,"failed":$failed,"cancelled":$cancelled,"inFlight":$inFlight,"finished":$finished,"checkpointSaved":$checkpointSaved,"cancelRequested":$cancelRequested,"adapterName":"${jsonEscape(adapterName)}"}"""

    private fun stateSnapshot(state: String): String = when (state) {
        "idle" -> snapshot(jobId = "0", attempt = 0, state = state, total = 0, adapterName = "")
        "completed" -> snapshot(state = state, completed = 164, finished = true, checkpointSaved = true)
        "cancelled" -> snapshot(state = state, cancelled = 164, finished = true, cancelRequested = true)
        "failed" -> snapshot(state = state, error = "internal_error", failed = 1, cancelled = 163, finished = true)
        "unsupported" -> snapshot(
            state = state, error = "cache_disabled", total = 0, finished = false, adapterName = ""
        )
        else -> snapshot(state = state)
    }

    private fun errorSnapshot(error: String): String = when (error) {
        "none" -> snapshot(error = error)
        "unsupported_renderer", "cache_disabled" -> snapshot(
            state = "unsupported", error = error, total = 0, finished = false, adapterName = ""
        )
        else -> snapshot(
            state = "failed", error = error, failed = 1, cancelled = 163, finished = true
        )
    }

    private fun jsonEscape(value: String): String = buildString {
        value.forEach { char ->
            when (char) {
                '\\' -> append("\\\\")
                '"' -> append("\\\"")
                '\n' -> append("\\n")
                '\r' -> append("\\r")
                '\t' -> append("\\t")
                else -> append(char)
            }
        }
    }
}
