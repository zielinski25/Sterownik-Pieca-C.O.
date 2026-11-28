package com.sterownikco.pro.core

/** Used on the model's main coroutine dispatcher, protected during fetch by weatherMutex. */
internal data class WeatherRequestKey(val lat: String, val lon: String, val days: Int)

internal class WeatherRefreshGate {
    var completedCount: Long = 0
        private set
    private var lastKey: WeatherRequestKey? = null
    var lastSuccess: Boolean = false
        private set

    fun completedForSince(key: WeatherRequestKey, observedCount: Long): Boolean =
        completedCount != observedCount && lastKey == key

    fun complete(key: WeatherRequestKey, success: Boolean) {
        lastKey = key
        lastSuccess = success
        completedCount++
    }
}

/** Czas od ostatniego udanego pobrania, nie od próby ani zegara ściennego. */
internal object WeatherFreshness {
    const val MAX_AGE_MS = 30L * 60_000L

    fun shouldRefresh(force: Boolean, hasData: Boolean, sameKey: Boolean, lastSuccessMs: Long?, nowMs: Long): Boolean {
        if (force || !hasData || !sameKey || lastSuccessMs == null) return true
        val age = nowMs - lastSuccessMs
        return age < 0L || age >= MAX_AGE_MS
    }
}
