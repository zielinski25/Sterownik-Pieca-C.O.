package com.sterownikco.pro.core

import org.json.JSONObject

/** Jedna zapisana odpowiedź Open-Meteo z kluczem zapytania i czasem udanego pobrania. */
internal data class CachedWeather(val raw: String, val savedAtMs: Long)

internal object WeatherCache {
    private const val MAX_CHARS = 1_000_000

    fun encode(key: WeatherRequestKey, raw: String, savedAtMs: Long): String = JSONObject()
        .put("v", 1).put("lat", key.lat).put("lon", key.lon).put("days", key.days)
        .put("savedAtMs", savedAtMs).put("response", JSONObject(raw)).toString()

    fun decode(stored: String?, expected: WeatherRequestKey, nowMs: Long): CachedWeather? {
        if (stored == null || stored.length > MAX_CHARS) return null
        return try {
            val o = JSONObject(stored)
            val saved = o.optLong("savedAtMs", 0L)
            if (o.optInt("v", 0) != 1 || o.optString("lat") != expected.lat ||
                o.optString("lon") != expected.lon || o.optInt("days", 0) != expected.days ||
                saved <= 0L || saved > nowMs) return null
            val response = o.optJSONObject("response") ?: return null
            CachedWeather(response.toString(), saved)
        } catch (e: Exception) { null }
    }
}
