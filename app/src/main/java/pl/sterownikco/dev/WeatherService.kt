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
 * Coordinates match the ESP32 firmware (51.066389, 21.509167).
 */
object WeatherService {

    @Volatile private var executor: java.util.concurrent.ExecutorService? = null
    private const val TAG = "WeatherService"

    // Default location — matches the firmware (DESZCZ_LAT/DESZCZ_LON)
    const val DEFAULT_LAT = 51.066389
    const val DEFAULT_LON = 21.509167

    private fun getExecutor(): java.util.concurrent.ExecutorService {
        executor?.let { if (!it.isShutdown) return it }
        val e = Executors.newSingleThreadExecutor()
        executor = e
        return e
    }

    fun shutdown() {
        executor?.shutdownNow()
        executor = null
    }

    data class CurrentWeather(
        val temperature: Double,
        val feelsLike: Double,
        val humidity: Int,
        val windSpeed: Double,
        val windGusts: Double,
        val windDirection: Double,
        val weatherCode: Int,
        val isDay: Boolean,
        val cloudCover: Double,
        val precipitation: Double,
        val uvIndex: Double,
        val pressure: Double
    )

    data class HourlyForecast(
        val time: Long,
        val temperature: Double,
        val weatherCode: Int,
        val precipitation: Double,
        val precipitationProb: Int,
        val cloudCover: Double,
        val windSpeed: Double
    )

    data class DailyForecast(
        val date: Long,
        val tempMax: Double,
        val tempMin: Double,
        val weatherCode: Int,
        val precipitationSum: Double,
        val windSpeedMax: Double,
        val windGustsMax: Double,
        val sunrise: Long,
        val sunset: Long,
        val uvIndexMax: Double,
        val shortwaveRadiation: Double
    )

    data class WeatherData(
        val current: CurrentWeather?,
        val hourly: List<HourlyForecast>,
        val daily: List<DailyForecast>
    )

    @Volatile private var lastWeather: WeatherData? = null

    fun getCached(): WeatherData? = lastWeather

    fun fetchWeather(
        latitude: Double = DEFAULT_LAT,
        longitude: Double = DEFAULT_LON,
        days: Int = 7,
        onError: ((String) -> Unit)? = null,
        onSuccess: (WeatherData) -> Unit
    ) {
        fetchWeatherInternal(latitude, longitude, days, object : Callback {
            override fun onSuccess(data: WeatherData) = onSuccess(data)
            override fun onError(error: String) {
                onError?.invoke(error)
            }
        }, retryCount = 0)
    }

    interface Callback {
        fun onSuccess(data: WeatherData)
        fun onError(error: String)
    }

    private fun fetchWeatherInternal(
        latitude: Double,
        longitude: Double,
        days: Int,
        callback: Callback,
        retryCount: Int
    ) {
        getExecutor().execute {
            try {
                val daysClamped = days.coerceIn(1, 16)
                val currentUrl = "https://api.open-meteo.com/v1/forecast" +
                    "?latitude=$latitude&longitude=$longitude" +
                    "&current=temperature_2m,relative_humidity_2m,apparent_temperature," +
                    "precipitation,cloud_cover,wind_speed_10m,wind_gusts_10m," +
                    "wind_direction_10m,uv_index,weather_code,is_day,pressure_msl" +
                    "&hourly=temperature_2m,precipitation_probability,precipitation," +
                    "cloud_cover,wind_speed_10m,weather_code" +
                    "&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset," +
                    "uv_index_max,precipitation_sum,wind_speed_10m_max,wind_gusts_10m_max," +
                    "shortwave_radiation_sum,weather_code" +
                    "&forecast_days=$daysClamped&timezone=Europe%2FWarsaw"

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
                        windGusts = currentJson.optDouble("wind_gusts_10m", 0.0),
                        windDirection = currentJson.optDouble("wind_direction_10m", 0.0),
                        weatherCode = currentJson.optInt("weather_code", 0),
                        isDay = currentJson.optInt("is_day", 1) == 1,
                        cloudCover = currentJson.optDouble("cloud_cover", Double.NaN),
                        precipitation = currentJson.optDouble("precipitation", 0.0),
                        uvIndex = currentJson.optDouble("uv_index", 0.0),
                        pressure = currentJson.optDouble("pressure_msl", Double.NaN)
                    )
                } else null

                // Parse hourly forecast — all hours of the requested forecast_days
                // (the chart trims to the selected range; the hourly strip takes the first 48).
                val hourlyJson = json.optJSONObject("hourly")
                val hourly = mutableListOf<HourlyForecast>()
                if (hourlyJson != null) {
                    val times = hourlyJson.optJSONArray("time")
                    val temps = hourlyJson.optJSONArray("temperature_2m")
                    val codes = hourlyJson.optJSONArray("weather_code")
                    val precip = hourlyJson.optJSONArray("precipitation")
                    val precipProb = hourlyJson.optJSONArray("precipitation_probability")
                    val clouds = hourlyJson.optJSONArray("cloud_cover")
                    val winds = hourlyJson.optJSONArray("wind_speed_10m")

                    if (times != null && temps != null) {
                        val maxItems = times.length()
                        for (i in 0 until maxItems) {
                            val timeStr = times.optString(i, "")
                            val time = parseTime(timeStr)
                            hourly.add(
                                HourlyForecast(
                                    time = time,
                                    temperature = temps.optDouble(i, Double.NaN),
                                    weatherCode = codes?.optInt(i, 0) ?: 0,
                                    precipitation = precip?.optDouble(i, 0.0) ?: 0.0,
                                    precipitationProb = precipProb?.optInt(i, 0) ?: 0,
                                    cloudCover = clouds?.optDouble(i, Double.NaN) ?: Double.NaN,
                                    windSpeed = winds?.optDouble(i, 0.0) ?: 0.0
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
                    val maxT = dailyJson.optJSONArray("temperature_2m_max")
                    val minT = dailyJson.optJSONArray("temperature_2m_min")
                    val codes = dailyJson.optJSONArray("weather_code")
                    val precipSum = dailyJson.optJSONArray("precipitation_sum")
                    val windMax = dailyJson.optJSONArray("wind_speed_10m_max")
                    val gustMax = dailyJson.optJSONArray("wind_gusts_10m_max")
                    val sunrises = dailyJson.optJSONArray("sunrise")
                    val sunsets = dailyJson.optJSONArray("sunset")
                    val uvMax = dailyJson.optJSONArray("uv_index_max")
                    val radiation = dailyJson.optJSONArray("shortwave_radiation_sum")

                    if (times != null && maxT != null && minT != null) {
                        for (i in 0 until times.length()) {
                            val timeStr = times.optString(i, "")
                            val time = parseTime(timeStr)
                            daily.add(
                                DailyForecast(
                                    date = time,
                                    tempMax = maxT.optDouble(i, Double.NaN),
                                    tempMin = minT.optDouble(i, Double.NaN),
                                    weatherCode = codes?.optInt(i, 0) ?: 0,
                                    precipitationSum = precipSum?.optDouble(i, 0.0) ?: 0.0,
                                    windSpeedMax = windMax?.optDouble(i, 0.0) ?: 0.0,
                                    windGustsMax = gustMax?.optDouble(i, 0.0) ?: 0.0,
                                    sunrise = parseTime(sunrises?.optString(i, "") ?: ""),
                                    sunset = parseTime(sunsets?.optString(i, "") ?: ""),
                                    uvIndexMax = uvMax?.optDouble(i, 0.0) ?: 0.0,
                                    shortwaveRadiation = radiation?.optDouble(i, 0.0) ?: 0.0
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
                if (retryCount < 2) {
                    val delay = (1L shl (retryCount + 1)) * 1000L
                    Thread.sleep(delay)
                    fetchWeatherInternal(latitude, longitude, days, callback, retryCount + 1)
                } else {
                    val cached = lastWeather
                    android.os.Handler(android.os.Looper.getMainLooper()).post {
                        if (cached != null) {
                            callback.onSuccess(cached)
                        } else {
                            callback.onError(e.message ?: "Unknown error")
                        }
                    }
                }
            }
        }
    }

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

    fun getWeatherIcon(code: Int, isDay: Boolean = true): String {
        return when (code) {
            0 -> if (isDay) "☀️" else "🌙"
            1, 2 -> if (isDay) "⛅" else "🌙"
            3 -> "☁️"
            45, 48 -> "🌫️"
            51, 53, 55 -> "🌦️"
            61, 63, 65 -> "🌧️"
            71, 73, 75 -> "🌨️"
            95, 96, 99 -> "⛈️"
            else -> "🌤️"
        }
    }

    fun getWindDirection(deg: Double): String {
        val dirs = arrayOf("N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
            "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW")
        val idx = ((deg + 11.25) / 22.5).toInt() % 16
        return dirs[idx]
    }

    private fun parseTime(timeStr: String): Long {
        if (timeStr.isBlank()) return 0L
        return try {
            val formats = listOf(
                "yyyy-MM-dd'T'HH:mm",
                "yyyy-MM-dd'T'HH:mm:ss",
                "yyyy-MM-dd"
            )
            for (fmt in formats) {
                try {
                    val sdf = java.text.SimpleDateFormat(fmt, java.util.Locale.US)
                    sdf.timeZone = java.util.TimeZone.getTimeZone("Europe/Warsaw")
                    val result = sdf.parse(timeStr)
                    if (result != null) return result.time
                } catch (_: Exception) { }
            }
            0L
        } catch (e: Exception) {
            0L
        }
    }
}
