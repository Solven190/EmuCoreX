package com.sbro.emucorex.data

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.time.LocalDate
import java.time.LocalTime
import java.time.ZoneOffset

class GameStatsPeriodsTest {

    @Test
    fun `iso week keys match the cloud function`() {
        assertEquals("W_2026-W01", GameStatsPeriods.weekKey(utc(2025, 12, 29, 12)))
        assertEquals("W_2026-W01", GameStatsPeriods.weekKey(utc(2026, 1, 1, 0)))
        assertEquals("W_2026-W01", GameStatsPeriods.weekKey(utc(2026, 1, 4, 23)))
        assertEquals("W_2026-W02", GameStatsPeriods.weekKey(utc(2026, 1, 5, 0)))
        assertEquals("W_2026-W40", GameStatsPeriods.weekKey(utc(2026, 10, 3, 0)))
        assertEquals("W_2026-W53", GameStatsPeriods.weekKey(utc(2026, 12, 31, 0)))
        assertEquals("W_2026-W53", GameStatsPeriods.weekKey(utc(2027, 1, 1, 0)))
    }

    @Test
    fun `month keys are zero padded`() {
        assertEquals("M_2026-01", GameStatsPeriods.monthKey(utc(2026, 1, 31, 23)))
        assertEquals("M_2026-10", GameStatsPeriods.monthKey(utc(2026, 10, 3, 0)))
        assertEquals("M_2026-12", GameStatsPeriods.monthKey(utc(2026, 12, 1, 0)))
    }

    @Test
    fun `expire timestamps are after the period end`() {
        val weekExpire = GameStatsPeriods.weekExpireAtMs(utc(2026, 10, 3, 0))
        assertTrue(weekExpire > utc(2026, 10, 5, 0))
        val monthExpire = GameStatsPeriods.monthExpireAtMs(utc(2026, 10, 3, 0))
        assertTrue(monthExpire > utc(2026, 11, 1, 0))
    }

    @Test
    fun `period document id uses the counter key`() {
        assertEquals(
            "SLUS-20946_W_2026-W40",
            GameStatsPeriods.periodStatDocId("SLUS-20946", "W_2026-W40")
        )
    }

    private fun utc(year: Int, month: Int, day: Int, hour: Int): Long {
        return LocalDate.of(year, month, day)
            .atTime(LocalTime.of(hour, 0))
            .toInstant(ZoneOffset.UTC)
            .toEpochMilli()
    }
}

class GameStatsKeyTest {

    @Test
    fun `accepts normalized serial keys`() {
        assertTrue(isValidGameStatKey("SLUS-20946"))
        assertTrue(isValidGameStatKey("SCUS_97399"))
        assertTrue(isValidGameStatKey("SLES-52541"))
    }

    @Test
    fun `rejects malformed keys`() {
        assertFalse(isValidGameStatKey(null))
        assertFalse(isValidGameStatKey(""))
        assertFalse(isValidGameStatKey("slus-20946"))
        assertFalse(isValidGameStatKey("SLUS 20946"))
        assertFalse(isValidGameStatKey("SLUS.20946"))
        assertFalse(isValidGameStatKey("A".repeat(33)))
    }
}

class GameTopGroupingTest {

    @Test
    fun `merges regional serials of the same game`() {
        val us = TopGameStat(
            key = "SLUS-20946",
            serial = "SLUS-20946",
            title = "Grand Theft Auto: San Andreas",
            totalPlayTimeMs = 1_000L,
            sessions = 2,
            players = 1,
            lastPlayedAtMs = 100L,
            igdbId = 42L
        )
        val eu = us.copy(
            key = "SLES-52541",
            serial = "SLES-52541",
            totalPlayTimeMs = 500L,
            sessions = 1,
            players = 1,
            lastPlayedAtMs = 200L
        )

        val grouped = groupTopGames(listOf(us, eu))

        assertEquals(1, grouped.size)
        assertEquals("SLUS-20946", grouped.first().key)
        assertEquals(1_500L, grouped.first().totalPlayTimeMs)
        assertEquals(3, grouped.first().sessions)
        assertEquals(2, grouped.first().players)
        assertEquals(200L, grouped.first().lastPlayedAtMs)
    }

    @Test
    fun `keeps unresolved games separate and sorts by time`() {
        val unresolved = TopGameStat(
            key = "SLPM-65788",
            serial = "SLPM-65788",
            title = "Unknown",
            totalPlayTimeMs = 900L,
            sessions = 1,
            players = 1,
            lastPlayedAtMs = null,
            igdbId = null
        )
        val resolved = TopGameStat(
            key = "SLUS-20946",
            serial = "SLUS-20946",
            title = "GTA SA",
            totalPlayTimeMs = 1_000L,
            sessions = 1,
            players = 1,
            lastPlayedAtMs = 1L,
            igdbId = 42L
        )

        val grouped = groupTopGames(listOf(unresolved, resolved))

        assertEquals(2, grouped.size)
        assertEquals("SLUS-20946", grouped[0].key)
        assertEquals("SLPM-65788", grouped[1].key)
    }
}
