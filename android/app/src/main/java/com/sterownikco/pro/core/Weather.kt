package com.sterownikco.pro.core

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.Request
import org.json.JSONObject
import java.util.Calendar
import java.util.Locale
import java.util.TimeZone
import kotlin.math.max
import kotlin.math.min

/* ══════════════════════════════════════════════════════════════════════════
   POGODA — wylacznie prawdziwe `fetchRealOpenMeteo` + `getWeatherDesc/Emoji`
   1:1 z Piec.html. ZERO symulacji: brak sieci = uczciwy brak danych.
   ══════════════════════════════════════════════════════════════════════════ */

data class WHour(
    val time: Long, val temp: Double, val wind: Double, val cloud: Double,
    val precip: Double, val precipProb: Double, val code: Int, val isDay: Boolean,
    val shortwaveRadiation: Double? = null
)

data class WDay(
    val date: Long, val minT: Double, val maxT: Double, val rainSum: Double, val code: Int,
    val sunrise: String?, val sunset: String?
)

data class WCurrent(
    val temp: Double, val feelsLike: Double, val humidity: Double, val wind: Double,
    val windGusts: Double, val windDir: String, val pressure: Int, val cloud: Double,
    val precip: Double, val uv: Double, val code: Int, val isDay: Boolean
)

data class WeatherData(val current: WCurrent, val hourly: List<WHour>, val daily: List<WDay>)

object Weather {

    fun degToDir(deg: Double): String {
        val v = kotlin.math.floor(deg / 22.5 + 0.5).toInt()
        val arr = arrayOf("N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW")
        return arr[v.mod(16)]
    }

    fun desc(code: Int): String = when {
        code == 0 -> "Bezchmurnie"
        code == 1 || code == 2 -> "Częściowe zachmurzenie"
        code == 3 -> "Pochmurno"
        code == 45 || code == 48 -> "Mgła"
        code in 51..57 -> "Mżawka"
        code in 61..67 -> "Opady deszczu"
        code in 71..77 -> "Opady śniegu"
        code in 80..86 -> "Przelotny deszcz"
        code >= 95 -> "Burza z piorunami"
        else -> "Umiarkowanie"
    }

    fun emoji(code: Int, isDay: Boolean): String = when {
        code == 0 -> if (isDay) "☀️" else "🌙"
        code == 1 || code == 2 -> if (isDay) "⛅" else "☁️"
        code == 3 -> "☁️"
        code == 45 || code == 48 -> "🌫️"
        code in 51..57 -> "🌦️"
        code in 61..67 -> "🌧️"
        code in 71..77 -> "🌨️"
        code >= 95 -> "⛈️"
        else -> "🌤️"
    }

    /** Początek bieżącej godziny w lokalnej strefie urządzenia. */
    fun startOfHour(nowMillis: Long = System.currentTimeMillis()): Long =
        Calendar.getInstance().apply {
            timeInMillis = nowMillis
            set(Calendar.MINUTE, 0)
            set(Calendar.SECOND, 0)
            set(Calendar.MILLISECOND, 0)
        }.timeInMillis

    /** Lista kafelków prognozy zaczyna się od bieżącej godziny, nie od północy. */
    fun forecastFromHour(hourly: List<WHour>, startMillis: Long, limit: Int = 48): List<WHour> =
        hourly.filter { it.time >= startMillis }.take(max(0, limit))

    /** `fetchRealOpenMeteo(days)` — te same parametry zapytania co w panelu. */
    suspend fun fetch(client: okhttp3.OkHttpClient, lat: String, lon: String, days: Int): WeatherData? =
        withContext(Dispatchers.IO) {
            try {
                val q = "latitude=" + enc(lat) + "&longitude=" + enc(lon) +
                    "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,precipitation,weather_code,cloud_cover,surface_pressure,wind_speed_10m,wind_direction_10m,wind_gusts_10m,uv_index" +
                    "&hourly=temperature_2m,relative_humidity_2m,apparent_temperature,precipitation_probability,precipitation,weather_code,surface_pressure,cloud_cover,wind_speed_10m,wind_direction_10m,wind_gusts_10m,uv_index,is_day,shortwave_radiation" +
                    "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset,precipitation_sum" +
                    "&timezone=auto&forecast_days=" + enc(max(1, min(14, days)).toString())
                val req = Request.Builder().url("https://api.open-meteo.com/v1/forecast?" + q)
                    .header("Cache-Control", "no-store").build()
                val body = client.newCall(req).execute().use { r -> if (r.isSuccessful) r.body?.string() else null } ?: return@withContext null
                parse(JSONObject(body), days)
            } catch (e: Exception) { null }
        }

    private fun enc(s: String) = java.net.URLEncoder.encode(s, "UTF-8")

    private fun parse(raw: JSONObject, days: Int): WeatherData? {
        val hh = raw.optJSONObject("hourly") ?: return null
        val times = hh.optJSONArray("time") ?: return null
        val hourly = ArrayList<WHour>(times.length())
        for (i in 0 until times.length()) {
            hourly.add(
                WHour(
                    time = parseIso(times.optString(i)),
                    temp = arr(hh, "temperature_2m", i, 15.0),
                    wind = arr(hh, "wind_speed_10m", i, 10.0),
                    cloud = arr(hh, "cloud_cover", i, 20.0),
                    precip = arr(hh, "precipitation", i, 0.0),
                    precipProb = arr(hh, "precipitation_probability", i, 0.0),
                    code = arr(hh, "weather_code", i, 0.0).toInt(),
                    isDay = if (hh.has("is_day")) arr(hh, "is_day", i, 1.0) == 1.0 else true,
                    shortwaveRadiation = arrOrNull(hh, "shortwave_radiation", i)
                )
            )
        }
        val daily = ArrayList<WDay>()
        raw.optJSONObject("daily")?.let { dd ->
            val dt = dd.optJSONArray("time") ?: return@let
            for (i in 0 until dt.length()) {
                daily.add(
                    WDay(
                        date = parseIso(dt.optString(i)),
                        minT = arr(dd, "temperature_2m_min", i, 10.0),
                        maxT = arr(dd, "temperature_2m_max", i, 20.0),
                        rainSum = arr(dd, "precipitation_sum", i, 0.0),
                        code = arr(dd, "weather_code", i, 0.0).toInt(),
                        sunrise = str(dd, "sunrise", i), sunset = str(dd, "sunset", i)
                    )
                )
            }
        }
        val curRaw = raw.optJSONObject("current") ?: JSONObject()
        val c0 = hourly.firstOrNull()
        val cur = WCurrent(
            temp = if (curRaw.has("temperature_2m")) curRaw.getDouble("temperature_2m") else (c0?.temp ?: 15.0),
            feelsLike = if (curRaw.has("apparent_temperature")) curRaw.getDouble("apparent_temperature") else ((c0?.temp ?: 15.0) - 1),
            humidity = if (curRaw.has("relative_humidity_2m")) curRaw.getDouble("relative_humidity_2m") else 60.0,
            wind = if (curRaw.has("wind_speed_10m")) curRaw.getDouble("wind_speed_10m") else (c0?.wind ?: 10.0),
            windGusts = if (curRaw.has("wind_gusts_10m")) curRaw.getDouble("wind_gusts_10m") else 15.0,
            windDir = degToDir(if (curRaw.has("wind_direction_10m")) curRaw.getDouble("wind_direction_10m") else 180.0),
            pressure = if (curRaw.has("surface_pressure")) curRaw.getDouble("surface_pressure").toInt() else 1013,
            cloud = if (curRaw.has("cloud_cover")) curRaw.getDouble("cloud_cover") else 30.0,
            precip = if (curRaw.has("precipitation")) curRaw.getDouble("precipitation") else 0.0,
            uv = if (curRaw.has("uv_index")) curRaw.getDouble("uv_index") else 3.0,
            code = if (curRaw.has("weather_code")) curRaw.getInt("weather_code") else (c0?.code ?: 0),
            isDay = if (curRaw.has("is_day")) curRaw.getInt("is_day") == 1 else (c0?.isDay ?: true)
        )
        return WeatherData(cur, hourly, daily)
    }

    private fun arr(o: JSONObject, key: String, i: Int, def: Double): Double {
        val a = o.optJSONArray(key) ?: return def
        return if (i < a.length() && !a.isNull(i)) a.optDouble(i, def) else def
    }

    private fun arrOrNull(o: JSONObject, key: String, i: Int): Double? {
        val a = o.optJSONArray(key) ?: return null
        if (i !in 0 until a.length() || a.isNull(i)) return null
        return a.optDouble(i, Double.NaN).takeIf { it.isFinite() && it >= 0.0 }
    }

    private fun str(o: JSONObject, key: String, i: Int): String? {
        val a = o.optJSONArray(key) ?: return null
        return if (i < a.length() && !a.isNull(i)) a.optString(i) else null
    }

    /** Open-Meteo zwraca czasy lokalne bez strefy — traktujemy jak lokalne. */
    fun parseIso(s: String): Long = try {
        val s2 = s.replace("T", " ").trim()
        // Wzorzec MUSI pasowac do dlugosci: "yyyy-MM-dd HH:mm:ss" na "2026-10-07 14:00"
        // rzuca ParseException -> lapalismy 0L -> wszystkie karty "01:00" i dni "Czw 1.1".
        val pat = when {
            s2.length >= 19 -> "yyyy-MM-dd HH:mm:ss"
            s2.length >= 16 -> "yyyy-MM-dd HH:mm"
            s2.length >= 13 -> "yyyy-MM-dd HH"
            else -> "yyyy-MM-dd"
        }
        val f = java.text.SimpleDateFormat(pat, Locale.US)
        f.isLenient = false
        f.parse(s2.take(pat.length))?.time ?: 0L
    } catch (e: Exception) { 0L }
}
