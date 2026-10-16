package com.sterownikco.pro.ui.chart

import java.util.Calendar
import org.junit.Assert.assertEquals
import org.junit.Test

class ChartXLabelTest {
    @Test
    fun shortRangeShowsDateOnceAndOnlyTimeForSameDayTicks() {
        val first = timestamp(day = 11, month = 10, hour = 3, minute = 46)
        val next = timestamp(day = 11, month = 10, hour = 5, minute = 3)

        assertEquals("11.10 03:46", fmtXLabel(first, 24 * HOUR_MS))
        assertEquals("05:03", fmtXLabel(next, 24 * HOUR_MS, first))
    }

    @Test
    fun dayBoundaryShowsDayWithoutRepeatingMonth() {
        val previous = timestamp(day = 11, month = 10, hour = 23, minute = 50)
        val current = timestamp(day = 12, month = 10, hour = 0, minute = 12)

        assertEquals("12 00:12", fmtXLabel(current, 24 * HOUR_MS, previous))
    }

    @Test
    fun monthBoundaryShowsDayAndMonth() {
        val previous = timestamp(day = 31, month = 10, hour = 23, minute = 50)
        val current = timestamp(day = 1, month = 11, hour = 0, minute = 12)

        assertEquals("01.11 00:12", fmtXLabel(current, 24 * HOUR_MS, previous))
    }

    @Test
    fun longRangeRepeatsMonthOnlyAtMonthBoundary() {
        val first = timestamp(day = 11, month = 10)
        val nextSameMonth = timestamp(day = 18, month = 10)
        val nextMonth = timestamp(day = 1, month = 11)

        assertEquals("11.10", fmtXLabel(first, 30 * DAY_MS))
        assertEquals("18", fmtXLabel(nextSameMonth, 30 * DAY_MS, first))
        assertEquals("01.11", fmtXLabel(nextMonth, 30 * DAY_MS, nextSameMonth))
    }

    @Test
    fun yearBoundaryIncludesYear() {
        val previous = timestamp(day = 31, month = 12, hour = 23, minute = 50)
        val current = timestamp(day = 1, month = 1, hour = 0, minute = 12, year = 2027)

        assertEquals("01.01.2027 00:12", fmtXLabel(current, 24 * HOUR_MS, previous))
    }

    private fun timestamp(
        day: Int,
        month: Int,
        hour: Int = 12,
        minute: Int = 0,
        year: Int = 2026
    ): Long = Calendar.getInstance().apply {
        clear()
        set(year, month - 1, day, hour, minute, 0)
    }.timeInMillis

    private companion object {
        const val HOUR_MS = 60L * 60L * 1000L
        const val DAY_MS = 24L * HOUR_MS
    }
}
