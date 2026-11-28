package com.sterownikco.pro.core

import org.junit.Assert.*
import org.junit.Test

class WeatherRefreshGateTest {
    private val a = WeatherRequestKey("51", "21", 7)
    private val otherLocation = WeatherRequestKey("52", "21", 7)
    private val otherDays = WeatherRequestKey("51", "21", 14)

    @Test fun pendingRefreshesShareOnlyTheSameCompletedRequest() {
        val gate = WeatherRefreshGate()
        val resume = gate.completedCount
        val tabEntry = gate.completedCount
        assertFalse(gate.completedForSince(a, resume))
        gate.complete(a, true)
        assertTrue(gate.completedForSince(a, resume))
        assertTrue(gate.completedForSince(a, tabEntry))
        assertTrue(gate.lastSuccess)
        assertFalse(gate.completedForSince(otherLocation, tabEntry))
        assertFalse(gate.completedForSince(otherDays, tabEntry))
        // A later resume/entry requests again even with previously cached data.
        assertFalse(gate.completedForSince(a, gate.completedCount))
    }

    @Test fun failureCanBeSharedButLaterEntryRetries() {
        val gate = WeatherRefreshGate()
        val whileLoading = gate.completedCount
        gate.complete(a, false)
        assertFalse(gate.lastSuccess)
        assertTrue(gate.completedForSince(a, whileLoading))
        assertFalse(gate.completedForSince(a, gate.completedCount))
        gate.complete(a, true)
        assertTrue(gate.lastSuccess)
    }

    @Test fun changedLocationOrRangeStartsFreshRequest() {
        val gate = WeatherRefreshGate()
        val waiting = gate.completedCount
        gate.complete(a, true)
        assertFalse(gate.completedForSince(otherLocation, waiting))
        assertFalse(gate.completedForSince(otherDays, waiting))
        gate.complete(otherLocation, true)
        assertTrue(gate.completedForSince(otherLocation, waiting))
        assertFalse(gate.completedForSince(a, waiting))
    }
}
