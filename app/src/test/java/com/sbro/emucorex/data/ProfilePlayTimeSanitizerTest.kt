package com.sbro.emucorex.data

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ProfilePlayTimeSanitizerTest {

    @Test
    fun `all legacy autotest games are removed and their exact time is returned`() {
        val affectedGames = listOf(
            "postopt_vu1_fmac" to 300_000L,
            "clamp_vu1_fmac" to 300_000L,
            "postopt_vu_random" to 240_000L,
            "mask_vu_random" to 300_000L,
            "vu0_acc_prebatch" to 600_000L,
            "final_barrier_vu_fdiv" to 1_080_000L,
            "final_productsum_vu_efu" to 120_000L,
            "post_vudouble_vu1_fmac" to 1_380_000L,
            "latest_clamp_vu_fdiv" to 60_000L,
            "baseline_loads_cop2_maddmsub" to 900_000L,
            "preopt_vu1_fmac" to 60_000L,
            "final_acc_random" to 60_000L,
            "final_vu1_madd_edge" to 120_000L,
            "accopt_vu_random" to 240_000L,
            "vu0_acc_batch" to 60_000L
        ).mapIndexed { index, (title, durationMs) ->
            "P_test_$index" to mapOf<String, Any?>(
                "t" to title,
                "ms" to durationMs,
                "n" to 0L,
                "lp" to (1_784_000_000_000L + index)
            )
        }
        val realGames = (0 until 47).associate { index ->
            "SLES-${index.toString().padStart(5, '0')}" to mapOf<String, Any?>(
                "t" to "Real game $index",
                "s" to "SLES-${index.toString().padStart(5, '0')}",
                "ms" to 60_000L,
                "n" to 1L
            )
        }

        val result = sanitizeLegacyAutotestGames(realGames + affectedGames)

        assertEquals(15, result.removedGames.size)
        assertEquals(5_820_000L, result.removedPlayTimeMs)
        assertEquals(47, result.remainingGames.size)
        assertTrue(result.remainingGames.keys.all { it.startsWith("SLES-") })

        val retry = sanitizeLegacyAutotestGames(result.remainingGames)
        assertTrue(retry.removedGames.isEmpty())
        assertEquals(0L, retry.removedPlayTimeMs)
        assertEquals(result.remainingGames, retry.remainingGames)
    }

    @Test
    fun `real serial games and normal homebrew sessions are preserved`() {
        val games = mapOf(
            "SLES-52541" to mapOf<String, Any?>("s" to "SLES-52541", "ms" to 600_000L, "n" to 0L),
            "P_homebrew" to mapOf<String, Any?>("ms" to 120_000L, "n" to 1L),
            "P_empty" to mapOf<String, Any?>("ms" to 0L, "n" to 0L)
        )

        val result = sanitizeLegacyAutotestGames(games)

        assertTrue(result.removedGames.isEmpty())
        assertEquals(games, result.remainingGames)
        assertEquals(0L, result.removedPlayTimeMs)
    }

    @Test
    fun `autotest directory is recognized on Android and Windows paths only`() {
        assertTrue(isAutotestPlayTimeEntry(delta("/data/user/0/com.sbro.emucorex/files/autotest-elf/vu.elf")))
        assertTrue(isAutotestPlayTimeEntry(delta("C:\\work\\autotest-elf\\vu.elf")))
        assertFalse(isAutotestPlayTimeEntry(delta("/games/homebrew/vu.elf")))
        assertFalse(isAutotestPlayTimeEntry(delta("content://games/Tekken%205.iso")))
    }

    private fun delta(path: String) = PlayerPlayTimeDelta(
        gamePath = path,
        title = "test",
        serial = null,
        coverArtPath = null,
        durationMs = 60_000L
    )
}
