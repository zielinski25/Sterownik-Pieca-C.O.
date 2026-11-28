package com.sterownikco.pro.core

import org.junit.Assert.*
import org.junit.Test

class WeatherFreshnessTest {
    @Test fun automaticRefreshWaitsUntilThirtyMinutesAfterSuccess() {
        val t = 1000L
        assertFalse(WeatherFreshness.shouldRefresh(false, true, true, t, t + 1))
        assertFalse(WeatherFreshness.shouldRefresh(false, true, true, t, t + 30L * 60_000 - 1))
        assertTrue(WeatherFreshness.shouldRefresh(false, true, true, t, t + 30L * 60_000))
        assertTrue(WeatherFreshness.shouldRefresh(false, true, true, t, t + 31L * 60_000))
    }

    @Test fun missingDataChangedParametersAndManualRefreshStillRequest() {
        val t = 1000L
        assertTrue(WeatherFreshness.shouldRefresh(false, false, true, t, t + 1))
        assertTrue(WeatherFreshness.shouldRefresh(false, true, false, t, t + 1))
        assertTrue(WeatherFreshness.shouldRefresh(false, true, true, null, t + 1))
        assertTrue(WeatherFreshness.shouldRefresh(true, true, true, t, t + 1))
        assertTrue(WeatherFreshness.shouldRefresh(false, true, true, t, t - 1))
    }
}
