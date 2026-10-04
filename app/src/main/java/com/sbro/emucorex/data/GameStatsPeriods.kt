package com.sbro.emucorex.data

import java.time.Instant
import java.time.ZoneOffset
import java.time.temporal.WeekFields
import java.util.Locale

/**
 * Period keys shared with the scheduled Cloud Function (see functions/period.js).
 * Everything is computed in UTC so clients and the server always agree.
 */
object GameStatsPeriods {

    private val weekFields = WeekFields.ISO

    fun weekKey(epochMs: Long): String {
        val date = Instant.ofEpochMilli(epochMs).atZone(ZoneOffset.UTC).toLocalDate()
        val week = date.get(weekFields.weekOfWeekBasedYear())
        val year = date.get(weekFields.weekBasedYear())
        return "W_%d-W%02d".format(Locale.US, year, week)
    }

    fun monthKey(epochMs: Long): String {
        val date = Instant.ofEpochMilli(epochMs).atZone(ZoneOffset.UTC).toLocalDate()
        return "M_%d-%02d".format(Locale.US, date.year, date.monthValue)
    }

    fun weekExpireAtMs(epochMs: Long): Long {
        val date = Instant.ofEpochMilli(epochMs).atZone(ZoneOffset.UTC).toLocalDate()
        val daysUntilNextMonday = 8 - date.get(weekFields.dayOfWeek())
        val nextMonday = date.plusDays(daysUntilNextMonday.toLong())
        return nextMonday.atStartOfDay(ZoneOffset.UTC).toInstant().toEpochMilli() + WEEK_RETENTION_MS
    }

    fun monthExpireAtMs(epochMs: Long): Long {
        val date = Instant.ofEpochMilli(epochMs).atZone(ZoneOffset.UTC).toLocalDate()
        val firstOfNextMonth = date.withDayOfMonth(1).plusMonths(1)
        return firstOfNextMonth.atStartOfDay(ZoneOffset.UTC).toInstant().toEpochMilli() + MONTH_RETENTION_MS
    }

    fun periodStatDocId(gameKey: String, periodKey: String): String = "${gameKey}_$periodKey"

    private const val WEEK_RETENTION_MS = 60L * 24L * 60L * 60L * 1000L
    private const val MONTH_RETENTION_MS = 180L * 24L * 60L * 60L * 1000L
}

internal val GAME_STAT_KEY_REGEX = Regex("^[A-Z0-9_-]{1,32}$")

internal fun isValidGameStatKey(value: String?): Boolean =
    value != null && GAME_STAT_KEY_REGEX.matches(value)
