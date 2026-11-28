package com.sterownikco.pro.core

import org.junit.Assert.*
import org.junit.Test

class WeatherCacheTest {
    private val key = WeatherRequestKey("51.0", "21.0", 7)

    @Test fun cachedDataRetainsTimestampAndExactLocationAndDays() {
        val raw = """{"current":{"temperature_2m":0},"hourly":{"time":[]}}"""
        val saved = WeatherCache.encode(key, raw, 1000L)
        val restored = WeatherCache.decode(saved, key, 2000L)!!
        assertEquals(1000L, restored.savedAtMs)
        assertNotNull(Weather.parseCached(restored.raw, 7))
        assertNull(WeatherCache.decode(saved, key.copy(lat = "52"), 2000L))
        assertNull(WeatherCache.decode(saved, key.copy(lon = "22"), 2000L))
        assertNull(WeatherCache.decode(saved, key.copy(days = 14), 2000L))
    }

    @Test fun corruptOrFutureCacheIsNotTreatedAsFresh() {
        val saved = WeatherCache.encode(key, "{}", 2000L)
        assertNull(WeatherCache.decode(saved, key, 1000L))
        assertNull(WeatherCache.decode("bad json", key, 1000L))
        assertNull(WeatherCache.decode(null, key, 1000L))
        assertNull(WeatherCache.decode("x".repeat(1_000_001), key, 1000L))
    }
}
