package pl.sterownikco.dev

import android.util.Log
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.net.HttpURLConnection
import java.net.URL
import java.util.concurrent.Executors

/**
 * STEROWNIK CO — Weather service (Open-Meteo API).
 * Provides current weather and forecast without API keys.
 */
object WeatherService {

    @Volatile private var executor: java.util.concurrent.ExecutorService? = null
    private const val TAG = "WeatherService"

    private fun getExecutor(): java.util.concurrent.ExecutorService {
        executor?.let { if (!it.isShutdown) return it }
        val e = Executors.newSingleThreadExecutor()
        executor = e
        return e
    }

    // Fix: Memory leak — allow shutdown of thread pool
    fun shutdown() {
        executor?.shutdownNow()
        executor = null
    }

    data class CurrentWeather(
        val temperature: Double,
        val feelsLike: Double,
        val humidity: Int,
        val windSpeed: Double,
        val weatherCode: Int,
        val isDay: Boolean
    )

    data class HourlyForecast(
        val time: Long,
        val temperature: Double,
        val weatherCode: Int,
        val precipitation: Double
    )

    data class DailyForecast(
        val date: Long,
        val tempMax: Double,
        val tempMin: Double,
        val weatherCode: Int,
        val precipitationSum: Double,
        val windSpeedMax: Double
    )

    data class WeatherData(
        val current: CurrentWeather?,
        val hourly: List<HourlyForecast>,
        val daily: List<DailyForecast>
    )

    interface Callback {
        fun onSuccess(data: WeatherData)
        fun onError(error: String)
    }

    /**
     * Fetch weather for given coordinates.
     * Default: Warsaw (52.2297, 21.0122)
     */
    // Fix: Weather retry with exponential backoff
    private var lastWeather: WeatherData? = null

    fun getCached(): WeatherData? = lastWeather

    fun fetchWeather(
        latitude: Double = 52.2297,
        longitude: Double = 21.0122,
        callback: Callback
    ) {
        fetchWeatherInternal(latitude, longitude, callback, retryCount = 0)
    }

    private fun fetchWeatherInternal(
        latitude: Double,
        longitude: Double,
        callback: Callback,
        retryCount: Int
    ) {
        getExecutor().execute {
            try {
                val currentUrl = "https://api.open-meteo.com/v1/forecast?latitude=$latitude&longitude=$longitude&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,is_day,wind_speed_10m&hourly=temperature_2m,weather_code,precipitation&daily=temperature_2m_max,temperature_2m_min,weather_code,precipitation_sum,wind_speed_10m_max&timezone=Europe%2FWarsaw&forecast_days=7"

                val url = URL(currentUrl)
                val connection = url.openConnection() as HttpURLConnection
                connection.requestMethod = "GET"
                connection.connectTimeout = 10000
                connection.readTimeout = 15000

                val responseCode = connection.responseCode
                if (responseCode !in 200..299) {
                    connection.disconnect()
                    throw Exception("HTTP $responseCode")
                }

                val response = BufferedReader(InputStreamReader(connection.inputStream, Charsets.UTF_8))
                    .use { it.readText() }
                connection.disconnect()

                val json = JSONObject(response)

                // Parse current weather
                val currentJson = json.optJSONObject("current")
                val current = if (currentJson != null) {
                    CurrentWeather(
                        temperature = currentJson.optDouble("temperature_2m", Double.NaN),
                        feelsLike = currentJson.optDouble("apparent_temperature", Double.NaN),
                        humidity = currentJson.optInt("relative_humidity_2m", 0),
                        windSpeed = currentJson.optDouble("wind_speed_10m", 0.0),
                        weatherCode = currentJson.optInt("weather_code", 0),
                        isDay = currentJson.optInt("is_day", 1) == 1
                    )
                } else null

                // Parse hourly forecast (next 48 hours)
                val hourlyJson = json.optJSONObject("hourly")
                val hourly = mutableListOf<HourlyForecast>()
                if (hourlyJson != null) {
                    val times = hourlyJson.optJSONArray("time")
                    val temps = hourlyJson.optJSONArray("temperature_2m")
                    val codes = hourlyJson.optJSONArray("weather_code")
                    val precip = hourlyJson.optJSONArray("precipitation")

                    if (times != null && temps != null && codes != null && precip != null) {
                        val maxItems = minOf(times.length(), 48)
                        for (i in 0 until maxItems) {
                            val timeStr = times.optString(i, "")
                            val time = parseTime(timeStr)
                            hourly.add(
                                HourlyForecast(
                                    time = time,
                                    temperature = temps.optDouble(i, Double.NaN),
                                    weatherCode = codes.optInt(i, 0),
                                    precipitation = precip.optDouble(i, 0.0)
                                )
                            )
                        }
                    }
                }

                // Parse daily forecast (7 days)
                val dailyJson = json.optJSONObject("daily")
                val daily = mutableListOf<DailyForecast>()
                if (dailyJson != null) {
                    val times = dailyJson.optJSONArray("time")
                    val max = dailyJson.optJSONArray("temperature_2m_max")
                    val min = dailyJson.optJSONArray("temperature_2m_min")
                    val codes = dailyJson.optJSONArray("weather_code")
                    val precip = dailyJson.optJSONArray("precipitation_sum")
                    val wind = dailyJson.optJSONArray("wind_speed_10m_max")

                    if (times != null && max != null && min != null && codes != null && precip != null && wind != null) {
                        for (i in 0 until times.length()) {
                            val timeStr = times.optString(i, "")
                            val time = parseTime(timeStr)
                            daily.add(
                                DailyForecast(
                                    date = time,
                                    tempMax = max.optDouble(i, Double.NaN),
                                    tempMin = min.optDouble(i, Double.NaN),
                                    weatherCode = codes.optInt(i, 0),
                                    precipitationSum = precip.optDouble(i, 0.0),
                                    windSpeedMax = wind.optDouble(i, 0.0)
                                )
                            )
                        }
                    }
                }

                val data = WeatherData(current, hourly, daily)
                lastWeather = data
                android.os.Handler(android.os.Looper.getMainLooper()).post {
                    callback.onSuccess(data)
                }
            } catch (e: Exception) {
                Log.e(TAG, "Weather fetch failed (attempt ${retryCount + 1}/3)", e)
                // Fix: Exponential backoff retry (3 attempts: 2s, 4s, 8s)
                if (retryCount < 2) {
                    val delay = (1L shl (retryCount + 1)) * 1000 // 2s, 4s
                    Thread.sleep(delay)
                    fetchWeatherInternal(latitude, longitude, callback, retryCount + 1)
                } else {
                    // Final error — return cached data if available
                    val cached = lastWeather
                    android.os.Handler(android.os.Looper.getMainLooper()).post {
                        if (cached != null) {
                            callback.onSuccess(cached) // Fallback to cache
                        } else {
                            callback.onError(e.message ?: "Unknown error")
                        }
                    }
                }
            }
        }
    }

    /**
     * Get weather description from Open-Meteo weather code.
     */
    fun getWeatherDescription(code: Int): String {
        return when (code) {
            0 -> "Bezchmurnie"
            1 -> "Prawie bezchmurnie"
            2 -> "Częściowe zachmurzenie"
            3 -> "Pochmurno"
            45, 48 -> "Mgła"
            51, 53, 55 -> "Mżawka"
            56, 57 -> "Mżawka lodowa"
            61, 63, 65 -> "Deszcz"
            66, 67 -> "Deszcz lodowy"
            71, 73, 75 -> "Śnieg"
            77 -> "Ziarna śniegu"
            80, 81, 82 -> "Przelotny deszcz"
            85, 86 -> "Przelotny śnieg"
            95 -> "Burza"
            96, 99 -> "Burza z gradem"
            else -> "Nieznane"
        }
    }

    /**
     * Get weather icon based on code and day/night.
     */
    fun getWeatherIcon(code: Int, isDay: Boolean = true): String {
        return when (code) {
            0 -> if (isDay) "☀️" else "🌙"
            1, 2 -> if (isDay) "⛅" else "️"
            3 -> "☁️"
            45, 48 -> "🌫️"
            51, 53, 55 -> "🌦️"
            61, 63, 65 -> "🌧️"
            71, 73, 75 -> "🌨️"
            95, 96, 99 -> "⛈️"
            else -> ""
        }
    }

    private fun parseTime(timeStr: String): Long {
        return try {
            val sdf = java.text.SimpleDateFormat("yyyy-MM-dd'T'HH:mm", java.util.Locale.US)
            sdf.timeZone = java.util.TimeZone.getTimeZone("Europe/Warsaw")
            sdf.parse(timeStr)?.time ?: 0L
        } catch (e: Exception) {
            0L
        }
    }
}
