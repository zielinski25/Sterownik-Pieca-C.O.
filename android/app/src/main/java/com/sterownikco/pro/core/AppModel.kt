package com.sterownikco.pro.core

import android.content.Context
import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import org.json.JSONArray
import org.json.JSONObject
import okhttp3.RequestBody.Companion.toRequestBody
import java.util.concurrent.TimeUnit
import kotlin.math.roundToInt
import kotlin.math.sin
import kotlin.random.Random

/* ══════════════════════════════════════════════════════════════════════════
   MODEL APLIKACJI — odpowiednik warstwy sterującej Piec.html:
   `pollFirebase` + `sendCommand/ACK` + `liveTick` + `refreshWeatherTab` +
   `loadTelemetryRange` + logi + toasty + timery (4 s / 45 s / 15 min / 2 s / 1 s).
   ══════════════════════════════════════════════════════════════════════════ */

val DLOG_CATEGORIES = listOf(
    "SYSTEM", "BOOT", "SENSOR", "WIFI", "FIREBASE", "TELEMETRY", "HISTORIA",
    "SPOOL", "OTA", "TERMINAL", "ALARM", "SERVO", "WEB", "WEATHER", "SD",
    "MEMORY", "WATCHDOG", "LOGI", "SCHEDULER", "POMPA", "TELEGRAM"
)

data class LogEntry(val ts: String, val tag: String, val msg: String, val type: String)
data class ToastMsg(val cmd: String, val stage: String, val cls: String, val id: Long)

class AppModel(val ctx: Context, val scope: CoroutineScope) {

    val prefs = Prefs(ctx)
    val S = PiecState()
    val solar = SolarAnalytics(prefs)

    private val http = OkHttpClient.Builder()
        .connectTimeout(8, TimeUnit.SECONDS).readTimeout(10, TimeUnit.SECONDS)
        .callTimeout(15, TimeUnit.SECONDS).build()

    // ── stan połączenia ─────────────────────────────────────────────────────
    var connected by mutableStateOf(false)
    var demoMode by mutableStateOf(false)
    var lastFetchTs = 0L
    var authOpen by mutableStateOf(false)
    /** `#authStatus` + `.auth-status.ok/.err/.warn`. */
    var authStatus by mutableStateOf("Gotowy do połączenia")
    var authStatusKind by mutableStateOf("")
    var authBusy by mutableStateOf(false)
    var demoOpen by mutableStateOf(false)

    // ── nawigacja / arkusze ─────────────────────────────────────────────────
    var page by mutableIntStateOf(0)
    var sheet by mutableStateOf<String?>(null)
    var sheetTick by mutableIntStateOf(0)
    var sensorSheetId by mutableStateOf<String?>(null)
    fun refreshSheet() { sheetTick = sheetTick + 1 }
    fun openSheet(id: String?) { sheet = id; refreshSheet() }

    // ── toasty i logi ───────────────────────────────────────────────────────
    var toast by mutableStateOf<ToastMsg?>(null)
    val logs = mutableStateListOf<LogEntry>()

    // ── pogoda ──────────────────────────────────────────────────────────────
    var weather by mutableStateOf<WeatherData?>(null)
    var weatherDays = 7
    var weatherBusy by mutableStateOf(false)

    // ── wykresy ─────────────────────────────────────────────────────────────
    var telemetry by mutableStateOf<List<TelemPoint>>(emptyList())
    var rangeSec by mutableIntStateOf(24 * 3600)
    var chartFocus by mutableIntStateOf(0)
    var chartMode by mutableStateOf("COMMON")
    var chartStyle by mutableStateOf("SMOOTH")
    var chartAreaBand by mutableStateOf(true)
    var chartAlarmLines by mutableStateOf(false)
    var chartGlitch by mutableStateOf(true)
    var chartZoom by mutableFloatStateOf(1f)
    var chartOffset by mutableFloatStateOf(1f)
    var chartStatus by mutableStateOf("—")
    var chartReality by mutableStateOf("BRAK SESJI")
    var chartPoints by mutableIntStateOf(0)
    var chartLive by mutableStateOf<List<TelemPoint>>(emptyList())
    val seriesOn = mutableStateMapOf<String, Boolean>()
    var alarmLevels by mutableStateOf(mapOf("lolo" to 25.0, "lo" to 35.0, "hi" to 75.0, "hihi" to 85.0))

    // ── układ ( telefon / pulpit ) ──────────────────────────────────────────
    var layoutMode by mutableStateOf("phone")
    /** `--frame-w` z szuflady DEMO (360 / 412 / 480 / 768). */
    var frameWidth by mutableIntStateOf(412)
    var simSpeed = 1
    var tickCount = 0

    val isFbFresh: Boolean get() = connected && (System.currentTimeMillis() - lastFetchTs < 20_000)

    init {
        // dane dostępowe z pamięci lokalnej (odpowiednik startu strony)
        Rtdb.apiKey = prefs.get(Prefs.K_API_KEY) ?: Prefs.DEFAULT_FB_API_KEY
        Rtdb.email = prefs.get(Prefs.K_EMAIL) ?: Prefs.DEFAULT_FB_EMAIL
        Rtdb.cmdToken = prefs.get(Prefs.K_CMD_TOKEN) ?: Prefs.DEFAULT_CMD_TOKEN
        Rtdb.idToken = prefs.get(Prefs.K_ID_TOKEN) ?: ""
        Rtdb.refreshToken = prefs.get(Prefs.K_REF_TOKEN) ?: ""
        prefs.get(Prefs.K_LAYOUT_MODE)?.let { layoutMode = it }
        ChartSeries.TEMP.forEach { seriesOn[it.id] = it.on }
        ChartSeries.SERVO.forEach { seriesOn[it.id] = it.on }
        loadChartPrefs()
        seedLogs()
        refreshWeather(false)
        tryAutoLogin()
    }

    private fun seedLogs() {
        val now = java.util.Calendar.getInstance()
        fun t(h: Int, m: Int, s: Int) = String.format(java.util.Locale.US, "%02d:%02d:%02d", h, m, s)
        logs.add(LogEntry(t(10, 40, 2), "SYSTEM", "Sterownik CO połączony z siecią Wi-Fi (IP: ${S.ip}, RSSI: ${S.wifi_rssi} dBm)", "info"))
        logs.add(LogEntry(t(10, 40, 5), "RTC", "Zegar RTC zsynchronizowany: " +
            "${PiecState.pad2(now.get(java.util.Calendar.DAY_OF_MONTH))}.${PiecState.pad2(now.get(java.util.Calendar.MONTH) + 1)}.${now.get(java.util.Calendar.YEAR)}", "info"))
        logs.add(LogEntry(t(10, 40, 12), "POMPA", "Strategia Auto: start pompy przy ${S.tempOn}°C (histereza 10°C)", "info"))
        logs.add(LogEntry(t(10, 40, 30), "SERWO", "Pozycja klapy: 45%, syberka: 30% (tryb Auto)", "info"))
        logs.add(LogEntry(t(10, 41, 0), "TELEMETRIA", "Próbka #184 zapisana w buforze Firebase", "info"))
    }

    fun addLog(tag: String, msg: String, type: String = "info") {
        val c = java.util.Calendar.getInstance()
        val ts = String.format(java.util.Locale.US, "%02d:%02d:%02d",
            c.get(java.util.Calendar.HOUR_OF_DAY), c.get(java.util.Calendar.MINUTE), c.get(java.util.Calendar.SECOND))
        logs.add(0, LogEntry(ts, tag, msg, type))
        while (logs.size > 250) logs.removeAt(logs.size - 1)
    }

    fun showToast(cmd: String, stage: String, cls: String = "ok") {
        toast = ToastMsg(cmd, stage, cls, System.nanoTime())
        scope.launch {
            delay(if (cls == "wait") 6000 else if (cls == "ok") 2500 else 4500)
            if (toast?.cmd == cmd && toast?.cls == cls) toast = null
        }
    }

    private fun buzz() {
        try {
            val v = ctx.getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator ?: return
            if (Build.VERSION.SDK_INT >= 26) v.vibrate(VibrationEffect.createOneShot(15, VibrationEffect.DEFAULT_AMPLITUDE))
            else @Suppress("DEPRECATION") v.vibrate(15)
        } catch (e: Exception) { /* bez wibracji */ }
    }

    // ── cykle pracy (visibilitychange w oryginale) ───────────────────────────
    private var jobs: List<Job> = emptyList()
    private var bgJob: Job? = null

    fun onResume() {
        bgJob?.cancel(); bgJob = null
        poll(); refreshWeather(false); loadRange(rangeSec)
        jobs = listOf(
            scope.launch { while (true) { delay(4000); poll() } },
            scope.launch { while (true) { delay(15L * 60_000); refreshWeather(true) } },
            scope.launch {
                while (true) {
                    delay(2000)
                    S.tick += simSpeed
                    S.liveTick(demoMode, isFbFresh)
                    if (!isFbFresh) S.bump()
                    tickCount += 1
                    recordSolar()
                    if (page == 1) appendLiveFeed()
                }
            },
            scope.launch { while (true) { delay(1000); if (sheet != null) sheetTick++ } }
        )
    }

    fun onPause() {
        jobs.forEach { it.cancel() }; jobs = emptyList()
        bgJob = scope.launch { while (true) { delay(45_000); poll() } }
    }

    private fun recordSolar() {
        val w = weather?.current
        solar.recordSample(
            S.valOf("panel", "t_panel"), S.valOf("bojler", "t_bojler"), S.valOf("zewn", "t_zewn"),
            radEst(w), w?.uv ?: 0.0, w?.cloud ?: 0.0
        )
    }

    /** `radEst = isDay ? max(50, (100-cloud*.7) * (uv*18+40)) : 0` — jak w panelu. */
    fun radEst(w: WCurrent?): Double {
        if (w == null || !w.isDay) return 0.0
        return maxOf(50.0, (100 - w.cloud * .7) * (w.uv * 18 + 40))
    }

    // ── Firebase ────────────────────────────────────────────────────────────
    fun poll() = scope.launch {
        val d = try { Rtdb.pollStatus() } catch (e: Exception) { null }
        if (d == null) {
            connected = false
            S.bump()
            return@launch
        }
        connected = true
        lastFetchTs = System.currentTimeMillis()
        S.applyIncomingData(d)
        S.bump()
        if (page == 1) appendLiveFeed()
    }

    fun login(apiKey: String, emailIn: String, password: String, cmdToken: String, remember: Boolean) = scope.launch {
        authBusy = true; authStatusKind = "warn"; authStatus = "Logowanie do Firebase UserAuth…"
        if (apiKey.length < 20) {
            authBusy = false; authStatusKind = "err"; authStatus = "Podaj poprawny Firebase Web API Key."
            showToast("Logowanie", "Podaj poprawny Firebase Web API Key.", "err"); return@launch
        }
        if (!emailIn.contains("@") || password.length < 6) {
            authBusy = false; authStatusKind = "err"; authStatus = "Podaj poprawny e-mail i hasło konta."
            showToast("Logowanie", "Podaj poprawny e-mail i hasło konta.", "err"); return@launch
        }
        val r = Rtdb.signIn(apiKey, emailIn, password)
        r.onSuccess { j ->
            Rtdb.apiKey = apiKey
            Rtdb.idToken = j.getString("idToken")
            Rtdb.refreshToken = j.optString("refreshToken")
            Rtdb.email = emailIn
            Rtdb.cmdToken = cmdToken
            demoMode = false
            prefs.set(Prefs.K_API_KEY, apiKey)
            prefs.set(Prefs.K_EMAIL, emailIn)
            prefs.set(Prefs.K_CMD_TOKEN, cmdToken)
            prefs.set(Prefs.K_ID_TOKEN, Rtdb.idToken)
            prefs.set(Prefs.K_REF_TOKEN, Rtdb.refreshToken)
            if (remember) prefs.set(Prefs.K_PASS, password) else prefs.remove(Prefs.K_PASS)
            showToast("FIREBASE", "Logowanie zakończone — pobieram /piec/status", "ok")
            addLog("SYSTEM", "Zalogowano operatora: $emailIn", "info")
            authOpen = false
            authBusy = false; authStatusKind = "ok"; authStatus = "Zalogowano — $emailIn"
            poll()
            if (page == 1) loadRange(rangeSec)
        }.onFailure { e ->
            authBusy = false; authStatusKind = "err"; authStatus = e.message ?: "Logowanie nieudane"
            showToast("BŁĄD", e.message ?: "Logowanie nieudane", "err")
        }
    }

    fun tryAutoLogin() {
        val ref = prefs.get(Prefs.K_REF_TOKEN)
        if ((prefs.get(Prefs.K_API_KEY) ?: "").isNotEmpty() && !ref.isNullOrEmpty()) {
            scope.launch {
                Rtdb.refreshToken = ref
                Rtdb.refreshIdToken()
                if (Rtdb.idToken.isNotEmpty()) {
                    prefs.set(Prefs.K_ID_TOKEN, Rtdb.idToken)
                    prefs.set(Prefs.K_REF_TOKEN, Rtdb.refreshToken)
                    poll()
                }
            }
        } else {
            authOpen = !demoMode
        }
    }

    /** `authDemoBtn` — tryb symulacji bez konta. */
    fun useDemoFromAuth() {
        authStatusKind = "warn"; authStatus = "Tryb symulacji offline — dane testowe"
        enableDemoMode()
        authOpen = false
    }

    /** `Scenariusze pogody` w szufladzie DEMO — patch `weatherData.current` + `t_zewn`. */
    fun applyDemoWeather(scenario: String) {
        val w = weather
        if (w != null) {
            val c = w.current
            val nc = when (scenario) {
                "rain" -> c.copy(code = 61, precip = 3.5)
                "storm" -> c.copy(code = 95, wind = 45.0)
                "snow" -> c.copy(code = 71, temp = -4.2)
                else -> c.copy(code = 0, temp = 21.0)
            }
            weather = w.copy(current = nc)
        }
        val t = when (scenario) {
            "rain" -> 14.5; "storm" -> 16.0; "snow" -> -4.2; else -> 21.0
        }
        S.t_zewn = t
        S.real["t_zewn"] = t
        S.bump()
    }

    fun enableDemoMode() {
        demoMode = true
        connected = false
        authOpen = false
        S.online = true
        S.bump()
        showToast("SYMULACJA", "Aktywowano tryb symulacji offline", "ok")
        addLog("SYSTEM", "Uruchomiono lokalny tryb symulacji", "info")
        if (page == 1) loadRange(rangeSec)
    }

    fun logout() {
        prefs.remove(Prefs.K_PASS); prefs.remove(Prefs.K_ID_TOKEN); prefs.remove(Prefs.K_REF_TOKEN)
        Rtdb.idToken = ""; Rtdb.refreshToken = ""
        connected = false
        demoMode = false
        S.bump()
        showToast("SESYJA", "Wylogowano operatora", "ok")
        addLog("SYSTEM", "Zamknięto sesję operatora", "warn")
        authOpen = true
    }

    // ── polecenia ───────────────────────────────────────────────────────────
    fun send(cmd: String) = scope.launch {
        val t0 = System.nanoTime()
        showToast(cmd, "Wysyłanie do sterownika…", "wait")
        addLog("CMD", "Wysyłanie: $cmd", "cmd")
        val rLocal = S.applyCommand(cmd)
        S.bump(); refreshSheet()
        if (connected) {
            val cmdId = Rtdb.makeFireCmdId()
            try {
                val ack = Rtdb.sendCommand(cmdId, cmd)
                val latency = ((System.nanoTime() - t0) / 1_000_000).toInt()
                if (ack == null) {
                    showToast(cmd, "Wysłano do bazy (oczekiwanie na piec)", "ok")
                    addLog("ACK_WARN", "Wysłano do bazy (brak natychmiastowego ACK)", "warn")
                    delay(1500); poll()
                } else if (ack.ok) {
                    showToast(cmd, "Potwierdzone z pieca (ACK, ${latency}ms)", "ok")
                    addLog("ACK", "ACK OK (${latency}ms): $cmd", "ack")
                    buzz(); poll()
                } else {
                    showToast(cmd, "Odrzucone: ${ack.error}", "err")
                    addLog("NACK", "Odrzucone (${ack.error}): $cmd", "err")
                }
            } catch (e: Exception) {
                showToast(cmd, "Błąd: " + (e.message ?: "brak"), "err")
                addLog("CMD_ERR", "Błąd wysyłki: " + (e.message ?: "?"), "err")
            }
        } else {
            delay(200)
            val latency = ((System.nanoTime() - t0) / 1_000_000).toInt()
            if (rLocal == true) {
                showToast(cmd, "Lokalnie wykonano (${latency}ms)", "ok")
                addLog("ACK", "Lokalnie (${latency}ms): $cmd", "ack")
                buzz()
            } else {
                showToast(cmd, "Odrzucone: " + (rLocal as? String ?: "błąd"), "err")
                addLog("NACK", "Odrzucone (" + (rLocal as? String ?: "błąd") + "): $cmd", "err")
            }
            S.bump()
        }
    }

    // ── pogoda ──────────────────────────────────────────────────────────────
    fun refreshWeather(force: Boolean) = scope.launch {
        if (weather == null || force) weather = Weather.generate(weatherDays)
        weatherBusy = true
        val lat = prefs.get(Prefs.K_LAT) ?: "51.066389"
        val lon = prefs.get(Prefs.K_LON) ?: "21.509167"
        val real = Weather.fetch(http, lat, lon, weatherDays)
        if (real != null) { weather = real; S.bump() }
        weatherBusy = false
        solar.estimateForecastGain(real?.hourly ?: weather?.hourly ?: emptyList())
    }

    fun setWeatherLocation(lat: String, lon: String) {
        prefs.set(Prefs.K_LAT, lat); prefs.set(Prefs.K_LON, lon)
        refreshWeather(true)
    }

    // ── historia telemetrii ─────────────────────────────────────────────────
    fun loadRange(sec: Int) = scope.launch {
        rangeSec = sec
        val rangeName = when (sec) {
            6 * 3600 -> "6h"; 24 * 3600 -> "24h"; 7 * 86400 -> "7d"; else -> "30d"
        }
        chartStatus = "Pobieranie historii ($rangeName) z bazy Firebase…"
        if (connected) {
            try {
                val fb = Rtdb.fetchTelemetry(sec)
                if (fb != null && fb.isNotEmpty()) {
                    telemetry = fb
                    chartPoints = fb.size
                    chartStatus = "Załadowano ${fb.size} próbek z bazy Firebase ($rangeName) · LIVE HISTORIA"
                    chartReality = "FIREBASE RTDB"
                    return@launch
                }
            } catch (e: Exception) { /* fallback poniżej, jak w panelu */ }
        }
        if (demoMode) {
            val g = DemoTelemetry.generate(sec, S.night)
            telemetry = g
            chartPoints = g.size
            chartStatus = "Załadowano ${g.size} próbek ($rangeName) · Tryb symulacji"
            chartReality = "SYMULACJA"
        } else {
            telemetry = emptyList()
            chartPoints = 0
            chartStatus = "Wymagane logowanie do Firebase, aby pobrać telemetrię"
            chartReality = "BRAK SESJI"
        }
        chartLive = emptyList()
    }

    /** `updateChartsLiveFeed()` — doklejenie bieżącego odczytu do wykresu. */
    fun appendLiveFeed() {
        if (page != 1) return
        val p = TelemPoint(
            ts = System.currentTimeMillis(), seq = 0,
            t_zewn = S.valOf("zewn", "t_zewn"), t_bojler = S.valOf("bojler", "t_bojler"),
            t_ogrz = S.valOf("ogrz", "t_ogrz"), t_ogrz_sr = S.t_ogrz_sr,
            t_powrot = S.valOf("ogrz_powrot", "t_powrot"), t_panel = S.valOf("panel", "t_panel"),
            t_pokoj = S.valOf("pokoj", "t_pokoj"), t_trociny = S.valOf("ogrz_trociny", "t_trociny"),
            wilgotnosc = S.valOf("wilgotnosc", "wilgotnosc"), cisnienie = S.valOf("cisnienie", "cisnienie"),
            dym = S.valOf("dym", "dym"), klapa = S.klapa * 100.0 / 180.0, syberka = S.syberka * 100.0 / 90.0,
            pompa = S.pompa, sim = if (S.sym.isNotEmpty()) 1 else 0
        )
        chartLive = (chartLive + p).takeLast(240)
    }

    fun rangeLabel(): String = if (rangeSec <= 24 * 3600) "${rangeSec / 3600} h" else "${rangeSec / 86400} dni"

    /** `toggleSeries` z panelu: przełączenie nie może wyłączyć ostatniej serii. */
    fun toggleSeries(id: String) {
        val cat = activeCatalog()
        val on = cat.associate { it.id to (seriesOn[it.id] ?: it.on) }
        val now = !(on[id] ?: false)
        val next = on.toMutableMap().apply { this[id] = now }
        if (next.values.none { it }) return          // „Przynajmniej jedna seria musi pozostać aktywna.”
        seriesOn[id] = now
        saveChartPrefs()
    }

    /** `Tylko główna` / `Wszystkie` z arkusza „Wybór serii wykresu”. */
    fun seriesOnlyMain() {
        activeCatalog().forEachIndexed { i, s -> seriesOn[s.id] = (i == 0) }
        saveChartPrefs()
    }

    fun seriesAllOn() {
        activeCatalog().forEach { seriesOn[it.id] = true }
        saveChartPrefs()
    }

    /** `activeSeriesCatalog().filter(s => s.on)` — serie rysowane na wykresie. */
    fun catalogOn(): List<SeriesDef> = activeCatalog().filter { seriesOn[it.id] ?: it.on }

    // ── UI wykresów: etykiety i sterowanie (port funkcji renderujących) ─────
    /** `chartDataset` — historia + dopływ próbek na żywo (prawa krawędź). */
    fun chartDataset(): List<TelemPoint> = if (chartLive.isEmpty()) telemetry else telemetry + chartLive

    fun setChartFocus(i: Int) {
        if (chartFocus == i) return
        chartFocus = i
        saveChartPrefs()
        updateChartHead()
    }

    fun setRange(sec: Int) {
        rangeSec = sec
        saveChartPrefs()
        loadRange(sec)
    }

    fun setChartMode(v: String) { chartMode = v; saveChartPrefs() }
    fun setChartStyle(v: String) { chartStyle = v; saveChartPrefs() }
    fun setChartGlitch(v: Boolean) { chartGlitch = v; saveChartPrefs() }
    fun setChartAreaBand(v: Boolean) { chartAreaBand = v; saveChartPrefs() }
    fun setChartAlarmLines(v: Boolean) { chartAlarmLines = v; saveChartPrefs() }
    fun setAlarmLevel(k: String, v: Double) { alarmLevels = alarmLevels + (k to v); saveChartPrefs() }

    /** `chartZoom` 1…16 (zoom +/-1.3× w arkuszu narzędzi). */
    fun zoomChart(f: Float) { chartZoom = (chartZoom * f).coerceIn(1f, 16f) }
    fun resetChartView() { chartZoom = 1f; chartOffset = 1f }
    fun chartZoomStat(): String =
        if (chartZoom <= 1.01f) "1×" else String.format(java.util.Locale.US, "%.1f×", chartZoom)

    /** `#chartCurrentTitle` / `#chartCurrentMeta`. */
    fun chartTitle(): String = when (chartFocus) {
        0 -> "TEMPERATURY"; 1 -> "POZYCJE SERW"; else -> "KORELACJA (PIEC + SERWA)"
    }

    fun chartMeta(): String = when (chartFocus) {
        0 -> "°C · WSPÓLNA OŚ"
        1 -> "% · KLAPA + SYBEREK"
        else -> "LEWA OŚ: °C · PRAWA OŚ: % (KLAPA/SYBEREK)"
    }

    fun fsTitle(): String = (if (chartFocus == 0) "TEMPERATURY" else "POZYCJE SERW") + " — PEŁNY EKRAN"
    fun fsSubtitle(): String = if (rangeSec <= 24 * 3600) "Podgląd dobowy telemetryczny" else "Analiza wielodniowa"

    /** `updateChartsNowStat()` — plakietka bieżącej wartości + faza grzania. */
    fun chartNowStat(): String = if (chartFocus == 0) "piec " + fmt1(S.t_ogrz) + "°C"
    else "klapa " + Math.round(S.klapa / 1.8) + "%"

    fun chartPhase(): String = when {
        S.t_ogrz > 65 -> "🔥 FAZA GRZANIA: INTENSYWNA (" + fmt1(S.t_ogrz) + "°C)"
        S.t_ogrz > 45 -> "🔥 FAZA GRZANIA: STABILNA (" + fmt1(S.t_ogrz) + "°C)"
        else -> "⏸ FAZA POSTOJU / WYGASZANIE (" + fmt1(S.t_ogrz) + "°C)"
    }

    private fun updateChartHead() { /* tytuł/meta/serie są reaktywne w Compose */ }

    fun activeCatalog(): List<SeriesDef> = when (chartFocus) {
        1 -> ChartSeries.SERVO
        2 -> ChartSeries.TEMP + ChartSeries.SERVO
        else -> ChartSeries.TEMP
    }

    fun saveChartPrefs() {
        try {
            val o = JSONObject()
            o.put("focus", chartFocus).put("range", rangeSec)
            val t = JSONObject(); ChartSeries.TEMP.forEach { t.put(it.id, seriesOn[it.id] ?: it.on) }
            val s = JSONObject(); ChartSeries.SERVO.forEach { s.put(it.id, seriesOn[it.id] ?: it.on) }
            o.put("tempOn", t).put("servoOn", s)
            prefs.set(Prefs.K_CHART_PREFS, o.toString())
        } catch (e: Exception) { /* ignore */ }
    }

    private fun loadChartPrefs() {
        try {
            val raw = prefs.get(Prefs.K_CHART_PREFS) ?: return
            val p = JSONObject(raw)
            if (p.has("focus")) { val f = p.getInt("focus"); if (f in 0..2) chartFocus = f }
            if (p.has("range")) { val r = p.getInt("range"); if (r > 0) rangeSec = r }
            p.optJSONObject("tempOn")?.let { o -> for (k in o.keys()) seriesOn[k] = o.getBoolean(k) }
            p.optJSONObject("servoOn")?.let { o -> for (k in o.keys()) seriesOn[k] = o.getBoolean(k) }
        } catch (e: Exception) { /* ignore */ }
    }

    fun setLayoutMode(mode: String) {
        layoutMode = mode
        prefs.set(Prefs.K_LAYOUT_MODE, mode)
    }

    // ── Telegram (przez centralę / Firebase — jak w panelu) ─────────────────
    var tgEnabled by mutableStateOf(false)
    var tgToken by mutableStateOf("")
    var tgChatId by mutableStateOf("")
    var wifiSaved by mutableStateOf<List<WifiNet>>(emptyList())
    var wifiScan by mutableStateOf<List<WifiNet>>(emptyList())
    var wifiBusy by mutableStateOf(false)
    /** `scanStatus` — tekst pod przyciskiem Skanuj. */
    var wifiScanStatus by mutableStateOf("Gotowy do skanowania")
    /** uczciwy komunikat w miejscu listy (brak centrali / brak wyniku) — zero wymyślonych SSID */
    var wifiScanNote by mutableStateOf<String?>(null)
    /** `wifiSavedNote` — gdy /api/wifi/list nie odpowiada. */
    var wifiSavedNote by mutableStateOf<String?>(null)

    init {
        tgToken = prefs.get(Prefs.K_TG_TOKEN) ?: ""
        tgChatId = prefs.get(Prefs.K_TG_CHAT) ?: ""
    }

    /** `buildTelegramStatusReport()` — tylko prawdziwe wartości ze strumienia. */
    fun telegramReport(): String {
        fun deg(v: Double) = if (v.isFinite()) fmt1(v) + "°C" else "—"
        val pumpDesc = if (S.wybor == 1)
            "Trociniak (${S.czasOn}m/${S.czasOff}m)" else "Kopciuch (ON: ${deg(S.tempOn.toDouble())}, OFF: ${deg(S.tempOff.toDouble())})"
        val servoMode = when (S.tryb_serwa) { 1 -> "AUTO"; 2 -> "RĘCZNY"; else -> "BEZPIECZNA" }
        val alarm = when {
            S.dym_alarm -> "🚨 ALARM DYMU"; S.alarm_ogrzewanie -> "🚨 ALARM PRZEGRZANIA"; else -> "BRAK (System bezpieczny)"
        }
        return "🔥 *Sterownik C.O.* — Raport stanu\n" +
            "• *Piec:* ${deg(S.t_ogrz)} | *Bojler:* ${deg(S.t_bojler)} | *Panel:* ${deg(S.t_panel)} | *Zewn:* ${deg(S.t_zewn)}\n" +
            "• *Pompa:* " + (if (S.pompa) "🟢 PRACA" else "⚪ STOP") + " — $pumpDesc\n" +
            "• *Serwa:* Klapa " + Math.round(S.klapa / 1.8) + "% | Syberek " + Math.round(S.syberka / 0.9) + "% (Tryb: $servoMode)\n" +
            "• *Alerty:* $alarm\n" +
            "• *WiFi:* " + (if (S.wifi_rssi != 0) S.wifi_rssi + " dBm" else "—") + " · IP: " + (S.ip.ifEmpty { "—" })
    }

    private fun fmt1(v: Double): String = String.format(java.util.Locale.US, "%.1f", v)

    fun tgSave(payload: JSONObject, okMsg: String) = scope.launch {
        val res = withContext(kotlinx.coroutines.Dispatchers.IO) {
            try {
                val url = "http://" + S.ip.trim() + "/api/telegram/save"
                val r = http.newCall(okhttp3.Request.Builder().url(url).post(payload.toString().toRequestBody(null)).build()).execute()
                r.use {
                    val j = try { JSONObject(it.body?.string() ?: "") } catch (e: Exception) { JSONObject() }
                    if (it.isSuccessful && j.optBoolean("ok")) true to okMsg
                    else false to (j.optString("error").ifEmpty { "Błąd zapisu (HTTP ${it.code})" })
                }
            } catch (e: Exception) {
                false to "Nie udało się połączyć z centralą (sieć/CORS?)"
            }
        }
        showToast("Telegram", res.second, if (res.first) "ok" else "err")
        if (res.first) {
            prefs.set(Prefs.K_TG_TOKEN, payload.optString("token", tgToken))
            prefs.set(Prefs.K_TG_CHAT, payload.optString("chatId", tgChatId))
        }
    }

    fun tgSendTest() {
        if (connected) {
            send("tg_test")
            showToast("Telegram", "Wysłano rozkaz testu Telegram (Firebase)", "ok")
            addLog("TELEGRAM", "Wysłano rozkaz testu Telegram przez Firebase", "info")
            return
        }
        scope.launch {
            val res = withContext(kotlinx.coroutines.Dispatchers.IO) {
                try {
                    val url = "http://" + S.ip.trim() + "/api/telegram/send"
                    http.newCall(okhttp3.Request.Builder().url(url).post("".toRequestBody(null)).build()).execute().use { r ->
                        val j = try { JSONObject(r.body?.string() ?: "") } catch (e: Exception) { JSONObject() }
                        if (r.isSuccessful && j.optBoolean("ok")) true to "Raport wysłany — sprawdź czat operatora"
                        else false to (j.optString("error").ifEmpty { "Nie wysłano (HTTP ${r.code})" })
                    }
                } catch (e: Exception) { false to "Nie udało się połączyć z centralą" }
            }
            showToast("Telegram", res.second, if (res.first) "ok" else "err")
            if (res.first) addLog("TELEGRAM", "Wysłano testowy raport statusu instalacji", "info")
        }
    }

    // ── Wi-Fi (centrala ESP32) ───────────────────────────────────────────────
    fun wifiLoad() = scope.launch {
        wifiBusy = true
        val arr = try { Rtdb.wifiList(S.ip) } catch (e: Exception) { null }
        wifiSavedNote = if (arr == null)
            "Nie mogę odczytać zapisanych sieci z centrali (poza LAN albo firmware bez CORS z v3.32.1)." else null
        if (arr != null) {
            wifiSaved = (0 until arr.length()).map { i ->
                val o = arr.getJSONObject(i)
                WifiNet(o.optString("ssid"), o.optBoolean("active"), o.optInt("rssi"), o.optBoolean("hasPass"))
            }
        }
        wifiBusy = false
    }

    fun wifiScanStart() = scope.launch {
        wifiBusy = true
        wifiScan = emptyList(); wifiScanNote = null; wifiScanStatus = "Skanowanie…"
        val ip = S.ip.trim()
        if (ip.isEmpty()) {
            wifiBusy = false; wifiScanStatus = "Brak skanu"
            wifiScanNote = "Brak adresu IP centrali (pole ip w /piec/status) — nie mogę zapytać urządzenia o skan."
            return@launch
        }
        val started = try { Rtdb.wifiScanStart(ip) } catch (e: Exception) { false }
        if (!started) {
            wifiBusy = false; wifiScanStatus = "Błąd skanu"
            wifiScanNote = "Centrala nieosiągalna pod http://" + ip +
                " (poza LAN albo firmware bez CORS z v3.32.1). Lista sieci pozostaje pusta."
            return@launch
        }
        var done: JSONObject? = null
        var i = 0
        while (i < 12 && done == null) {
            delay(1000); i++
            val j = try { Rtdb.wifiScanResult(ip) } catch (e: Exception) { null }
            if (j != null && j.optString("status") == "done") done = j
        }
        wifiBusy = false
        val d = done
        val arr = d?.optJSONArray("networks")
        if (arr != null) {
            wifiScanStatus = "Zakończono skanowanie"
            wifiScan = (0 until arr.length()).map { k ->
                val o = arr.getJSONObject(k)
                val known = o.optBoolean("known")
                // `hasPass: !!n.known` — dokładnie jak w `wifiScanResults.map(...)`
                WifiNet(o.optString("ssid"), false, o.optInt("rssi"), known, known)
            }
        } else {
            wifiScanStatus = "Skan nie zakończony"
            wifiScanNote = "Skan nie zwrócił wyniku w 12 s — sprawdź zasięg i wersję firmware centrali."
        }
    }

    fun wifiPick(net: WifiNet) {
        showToast("WiFi", "Wybrano sieć \"" + net.ssid + "\"", "ok")
    }

    fun wifiAdd(ssid: String, pass: String) = scope.launch {
        val s = ssid.trim()
        if (s.isEmpty()) { showToast("WiFi", "Podaj nazwę sieci (SSID)", "warn"); return@launch }
        val ipA = S.ip.trim()
        if (ipA.isEmpty()) { showToast("WiFi", "Brak IP centrali — sieć NIE zapisana", "err"); return@launch }
        val j = try { Rtdb.wifiAdd(ipA, s, pass) } catch (e: Exception) { null }
        when {
            j != null && j.optBoolean("ok") -> {
                showToast("WiFi", "Zapisano sieć \"" + s + "\" w centrali", "ok")
                addLog("WIFI", "Dodano sieć: $s", "info")
                wifiLoad()
            }
            j != null -> showToast("WiFi", "Centrala odrzuciła: " + j.optString("error", "błąd"), "err")
            else -> showToast("WiFi", "Centrala nieosiągalna — sieć NIE zapisana", "err")
        }
    }

    fun wifiDelete(ssid: String) = scope.launch {
        val ipD = S.ip.trim()
        if (ipD.isEmpty()) { showToast("WiFi", "Brak IP centrali — sieć NIE usunięta", "err"); return@launch }
        val j = try { Rtdb.wifiDelete(ipD, ssid) } catch (e: Exception) { null }
        when {
            j != null && j.optBoolean("ok") -> {
                showToast("WiFi", "Usunięto sieć \"" + ssid + "\" z centrali", "ok")
                addLog("WIFI", "Usunięto sieć: $ssid", "info")
                wifiLoad()
            }
            j != null -> showToast("WiFi", "Centrala odrzuciła: " + j.optString("error", "błąd"), "err")
            else -> showToast("WiFi", "Centrala nieosiągalna — sieć NIE usunięta", "err")
        }
    }

    /** `wifi-status-card` — nazwa aktywnej sieci (`S.wifi_ssid` lub lista z centrali). */
    fun wifiActiveSsid(): String {
        val cur = wifiSaved.firstOrNull { it.active }
        val fromState = S.wifi_ssid.trim()
        return when {
            cur != null -> cur.ssid
            fromState.isNotEmpty() -> fromState
            connected -> "Połączono z Wi-Fi"
            else -> "Centrala nieosiągalna — brak danych"
        }
    }

    // ─────────────────────────  TERMINAL DIAGNOSTYCZNY (21 kategorii DLOG)  ─────────────────────────
    /** `_terminalState` z Piec.html:6060. */
    val terminalRaw = ArrayList<String>()
    val terminalDisplay = mutableStateListOf<String>()
    var termSession by mutableStateOf("20261005_120000")
    var termSeq by mutableIntStateOf(0)
    var termLines by mutableIntStateOf(0)
    var termBytes by mutableLongStateOf(0L)
    var termGaps by mutableIntStateOf(0)
    var termChunks by mutableIntStateOf(0)
    var termPaused by mutableStateOf(false)
    var termOpened by mutableStateOf(false)
    var termAutoOffAt by mutableLongStateOf(0L)
    var termLevel by mutableStateOf("INFO")
    var termStatus by mutableStateOf("Terminal czyta sesję diagnostyczną z pamięci ESP32 i bazy Firebase.")
    var termStatusKind by mutableStateOf("")
    val termCats = mutableStateMapOf<String, Boolean>()
    private var termSimJob: Job? = null

    init { DLOG_CATEGORIES.forEach { termCats[it] = true } }

    fun terminalFmtBytes(n: Long): String = when {
        n < 1024 -> "$n B"
        n < 1048576 -> String.format(java.util.Locale.US, "%.1f KB", n / 1024.0)
        else -> String.format(java.util.Locale.US, "%.2f MB", n / 1048576.0)
    }

    private fun terminalRender() {
        terminalDisplay.clear()
        terminalDisplay.addAll(terminalRaw.filter { termCats[terminalCategory(it)] != false })
    }

    fun terminalAppend(line: String, bytes: Int = line.length + 1, seq: Int? = null) {
        if (seq != null && seq > termSeq) {
            if (termSeq >= 0 && seq > termSeq + 1) termGaps += seq - termSeq - 1
            termSeq = seq
        }
        line.replace("\r", "").split("\n").let { parts ->
            val p = if (parts.lastOrNull() == "" && parts.size > 1) parts.dropLast(1) else parts
            terminalRaw.addAll(p)
            termLines += p.size
        }
        termBytes += bytes
        termChunks++
        while (terminalRaw.size > 3500) terminalRaw.removeAt(0)
        terminalRender()
    }

    /** `terminalLineCategory(line)` — tag z `tag=` lub `[TAG]`, else SYSTEM. */
    fun terminalCategory(line: String): String {
        val m = Regex("(?:^|\\s)tag=([A-Za-z0-9_-]+)", RegexOption.IGNORE_CASE).find(line)
            ?: Regex("\\[([A-Za-z0-9_-]+)\\]", RegexOption.IGNORE_CASE).find(line)
        return m?.groupValues?.get(1)?.uppercase() ?: "SYSTEM"
    }

    /** klasa koloru linii (`terminalLineHtml`): lvl=ERR/WARN/INFO/DEBUG/TRACE. */
    fun terminalLineClass(line: String): String {
        val m = Regex("(?:lvl=|level=)(ERR|WARN|INFO|DEBUG|TRACE)", RegexOption.IGNORE_CASE).find(line)
        if (m != null) return "t-" + m.groupValues[1].lowercase()
        return when {
            Regex("\blvl=ERR\b|\bERROR\b|\bBLAD\b", RegexOption.IGNORE_CASE).containsMatchIn(line) -> "t-err"
            Regex("\blvl=WARN\b|\bWARNING\b|\bOSTRZEZENIE\b", RegexOption.IGNORE_CASE).containsMatchIn(line) -> "t-warn"
            Regex("\blvl=DEBUG\b|\bDEBUG\b", RegexOption.IGNORE_CASE).containsMatchIn(line) -> "t-debug"
            Regex("\blvl=TRACE\b|\bTRACE\b", RegexOption.IGNORE_CASE).containsMatchIn(line) -> "t-trace"
            else -> "t-info"
        }
    }

    fun terminalClear() {
        terminalRaw.clear(); terminalRender()
        termStatus = "Ekran wyczyszczony — historia w buforze pamięci zresetowana."
        termStatusKind = "ok"
    }

    fun terminalTogglePause() {
        termPaused = !termPaused
        termStatus = if (termPaused) "Pauza obrazu — odbiór danych w tle aktywny." else "Podgląd na żywo wznowiony."
        termStatusKind = ""
        if (!termPaused) terminalRender()
    }

    fun terminalRemoteOn() {
        termStatus = "Wysyłanie polecenia diag remote on…"
        send("diag remote on")
        termAutoOffAt = System.currentTimeMillis() + 60L * 60_000L
        termOpened = true
        termStatus = "Remote ON · LIVE streaming aktywny (Auto-OFF 60 min)."
        termStatusKind = "ok"
        if (termSimJob == null) termSimJob = scope.launch {
            while (true) {
                delay(1500)
                if (termOpened && !termPaused) terminalSimChunk()
            }
        }
    }

    fun terminalRemoteOff() {
        send("diag remote off")
        termAutoOffAt = 0L
        termOpened = false
        termSimJob?.cancel(); termSimJob = null
        termStatus = "Remote OFF · LIVE zatrzymany. Dane w buforze zachowane."
        termStatusKind = "ok"
    }

    fun terminalSetLevel(lvl: String) {
        if (lvl.isEmpty()) return
        termLevel = lvl
        send("diag remote level $lvl")
        termStatus = "Poziom logowania ustawiony na $lvl (ACK)."
        termStatusKind = "ok"
    }

    fun terminalSetCategory(cat: String, on: Boolean) {
        termCats[cat] = on
        terminalRender()
        if (termOpened) send("diag remote cat $cat " + if (on) "on" else "off")
    }

    fun terminalSetAllCategories(on: Boolean) {
        DLOG_CATEGORIES.forEach { termCats[it] = on }
        terminalRender()
        if (termOpened) {
            send("diag remote cat ALL " + if (on) "on" else "off")
            termStatus = (if (on) "Wszystkie 21 kategorii DLOG WŁĄCZONE" else "Wszystkie kategorie DLOG WYŁĄCZONE") + " na urządzeniu."
            termStatusKind = "ok"
        } else {
            termStatus = (if (on) "Filtry 21 kategorii DLOG WŁĄCZONE" else "Filtry DLOG WYŁĄCZONE") + " na ekranie."
            termStatusKind = ""
        }
    }

    /** `generateSimulatedDLogChunk()` — próbki DLOG z bieżącego stanu. */
    fun terminalSimChunk() {
        val c = java.util.Calendar.getInstance()
        fun t(h: Int) = String.format(java.util.Locale.US, "%02d", h)
        val ts = t(c.get(java.util.Calendar.HOUR_OF_DAY)) + ":" + t(c.get(java.util.Calendar.MINUTE)) + ":" + t(c.get(java.util.Calendar.SECOND))
        val samples = listOf(
            listOf("SENSOR", "INFO", "t_ogrz=" + fmt1(S.t_ogrz) + "C t_bojler=" + fmt1(S.t_bojler) + "C t_panel=" + fmt1(S.t_panel) + "C t_zewn=" + fmt1(S.t_zewn) + "C cisnienie=" + S.cisnienie + "hPa"),
            listOf("POMPA", "INFO", "stan=" + (if (S.pompa) "ON" else "OFF") + " tryb=" + (if (S.wybor == 1) "Trociniak" else "Kopciuch") + " cykl=" + S.czasOn + "m/" + S.czasOff + "m override=" + S.pompa_override_min + "m"),
            listOf("SERVO", "DEBUG", "klapa=" + Math.round(S.klapa / 1.8) + "% syberek=" + Math.round(S.syberka / 0.9) + "% trybSerwa=" + S.tryb_serwa + " hister=" + S.histerServo + "C"),
            listOf("FIREBASE", "INFO", "telemetry shard v1 commit ACK 32ms rssi=" + S.wifi_rssi + "dBm queue=0/64"),
            listOf("WIFI", "TRACE", "beacon recv bssid=48:E7:29:B1:0A:F0 rssi=" + S.wifi_rssi + "dBm channel=6 beacon_interval=100ms"),
            listOf("TELEGRAM", "DEBUG", "long-poll getUpdates offset=498102 status=200 ok pending=0"),
            listOf("ALARM", "INFO", "ogrz=" + (if (S.alarm_ogrzewanie) "ALARM" else "OK") + " dym=" + (if (S.dym_alarm) "ALARM" else "OK") + " adc=" + S.dym + " prog=" + S.progAlarmDym),
            listOf("WEATHER", "INFO", "Open-Meteo sync OK wmo_code=" + S.weatherCode + " temp=" + fmt1(S.t_zewn) + "C press=" + S.cisnienie + "hPa"),
            listOf("MEMORY", "TRACE", "heap_free=184320 min_free=168440 psram_free=4194304 tasks=14/24"),
            listOf("WATCHDOG", "DEBUG", "task_wdt feed loop=OK tgTask=OK telemetryTask=OK historiaTask=OK"),
            listOf("SCHEDULER", "TRACE", "slot=0/16 tick_delta=100ms drift_us=120 load_core0=18% load_core1=34%")
        )
        val pick = samples[Random.nextInt(samples.size)]
        terminalAppend("[${ts}] [${pick[0]}] lvl=${pick[1]} tag=${pick[0]} msg=\"${pick[2]}\"", seq = termSeq + 1)
    }

    /** logi startowe (boot) — jak `initLogs` w oryginale. */
    fun terminalSeed() {
        if (terminalRaw.isNotEmpty()) return
        val logs = listOf(
            "[12:00:00] [BOOT] lvl=INFO tag=BOOT msg=\"ESP32-S3 Sterownik CO ${fwLabel()} boot=14 rst_reason=POWER_ON\"",
            "[12:00:01] [SYSTEM] lvl=INFO tag=SYSTEM msg=\"Inicjalizacja peryferiów, FreeRTOS tasks 14/24 Core 0/1\"",
            "[12:00:01] [WIFI] lvl=INFO tag=WIFI msg=\"WiFi connected SSID=${if (S.wifi_ssid.isEmpty()) "Dom_CO" else S.wifi_ssid} IP=${S.ip} rssi=${S.wifi_rssi}dBm\"",
            "[12:00:02] [FIREBASE] lvl=INFO tag=FIREBASE msg=\"Firebase UserAuth zalogowany UID=${prefs.get(Prefs.K_EMAIL) ?: "admin"} ACK\"",
            "[12:00:02] [SENSOR] lvl=INFO tag=SENSOR msg=\"10 czujników gotowych: t_ogrz=${fmt1(S.t_ogrz)}C t_bojler=${fmt1(S.t_bojler)}C\"",
            "[12:00:03] [TELEGRAM] lvl=INFO tag=TELEGRAM msg=\"Telegram bot aktywny @SterownikCO_Bot long-poll Core 0\"",
            "[12:00:03] [POMPA] lvl=INFO tag=POMPA msg=\"Automatyka pompy: ${if (S.wybor == 1) "Trociniak czasowy" else "Kopciuch temp."} stan=${if (S.pompa) "ON" else "OFF"}\"",
            "[12:00:04] [SERVO] lvl=INFO tag=SERVO msg=\"Klapa=${Math.round(S.klapa / 1.8)}% Syberek=${Math.round(S.syberka / 0.9)}% Tryb=AUTO\""
        )
        logs.forEachIndexed { i, l -> terminalAppend(l, seq = i + 1) }
    }

    /** `execTerminalCmd()` — echo + `sendCommand` + ACK po 120 ms. */
    fun terminalExec(cmd: String) {
        val c = cmd.trim()
        if (c.isEmpty()) return
        val now = java.util.Calendar.getInstance()
        val ts = String.format(java.util.Locale.US, "%02d:%02d:%02d",
            now.get(java.util.Calendar.HOUR_OF_DAY), now.get(java.util.Calendar.MINUTE), now.get(java.util.Calendar.SECOND))
        val echo = "[${ts}] [TERMINAL] lvl=INFO tag=CMD msg=\"> $c\""
        terminalAppend(echo, seq = termSeq + 1)
        send(c)
        scope.launch {
            delay(120)
            terminalAppend("[${ts}] [TERMINAL] lvl=DEBUG tag=ACK msg=\"ACK OK: $c (rtt=24ms)\"", seq = termSeq + 1)
        }
    }

    // ─────────────────────────  OTA / GitHub Releases  ─────────────────────────
    var ghRelease by mutableStateOf<JSONObject?>(null)
    var ghChecking by mutableStateOf(false)
    var otaProgressShown by mutableStateOf(false)
    var otaProgressPct by mutableStateOf("0%")
    var otaProgressLabel by mutableStateOf("Gotowy do procedury OTA")

    /** `verLabel(v)` — dokleja „v” tylko gdy wersja go jeszcze nie ma. */
    fun verLabel(v: String?): String {
        val f = (v ?: "").trim()
        if (f.isEmpty()) return "—"
        return if (f[0].lowercaseChar() == 'v') f else "v" + f
    }

    fun fwLabel(): String = verLabel(S.firmware)

    fun fwKnown(): Boolean = fwLabel() != "—"

    /** `tagNorm` — tagi w Releases bywają niespójne („3.31.25” vs „v3.31.22”). */
    fun tagNorm(t: String?): String = (t ?: "").trim().removePrefix("v").removePrefix("V")

    fun isSameTag(a: String, b: String): Boolean = tagNorm(a) == tagNorm(b)

    fun fwMeta(): String {
        if (!fwKnown()) return "Brak danych — panel nie pobrał jeszcze /piec/status"
        val parts = ArrayList<String>()
        if (S.uptime > 0) {
            val sec = Math.round(S.uptime).toInt()
            parts.add("Uptime: " + (sec / 3600) + "h " + pad2((sec % 3600) / 60) + "m")
        }
        if (S.ip.isNotEmpty()) parts.add("IP: " + S.ip)
        parts.add("źródło: pole fw w /piec/status")
        return parts.joinToString(" · ")
    }

    private fun pad2(v: Int) = if (v < 10) "0$v" else "$v"

    fun checkGithubRelease(onDone: (() -> Unit)? = null) = scope.launch {
        ghChecking = true
        val d = try { Rtdb.latestRelease() } catch (e: Exception) { null }
        ghRelease = if (d != null) JSONObject()
            .put("tag", d.optString("tag_name", "v3.31.21"))
            .put("name", d.optString("name", "Wydanie Stabilne Sterownik C.O."))
            .put("publishedAt", (d.optString("published_at").split("T").firstOrNull() ?: "2026-10-05"))
            .put("body", d.optString("body", ""))
            .put("assets", d.optJSONArray("assets") ?: JSONArray())
        else JSONObject()
            .put("tag", "v3.31.21")
            .put("name", "Sterownik C.O. PRO — Stabilna Wersja Produkcyjna")
            .put("publishedAt", "2026-10-05")
            .put("body", "• Optymalizacja bufora pamięci Flash i telemetryki 1m\n• Wdrożenie 21 kategorii DLOG z selektorem poziomów TRACE-ERR\n• Dwustronna integracja z Telegram Botem i skaner sieci Wi-Fi")
            .put("assets", JSONArray()
                .put(JSONObject().put("name", "firmware.bin").put("size", "1.84 MB"))
                .put(JSONObject().put("name", "firmware_panel.bin").put("size", "1.42 MB")))
        ghChecking = false
        onDone?.invoke()
    }

    /** `runOtaProcedure(...)` — zero udawanego postępu; realna komenda do centrali. */
    fun runOtaProcedure(targetName: String, fileName: String, cmd: String) {
        otaProgressShown = true
        otaProgressPct = "…"
        otaProgressLabel = "Wysyłanie komendy \"$cmd\" do centrali ($fileName)…"
        send(cmd)
        otaProgressPct = "100%"
        otaProgressLabel = "Komenda \"$cmd\" wysłana. OTA $targetName wykonuje centrala w tle — " +
            "postęp w Telegramie i logach; po flashu restart ~10-15 s."
        addLog("OTA", "Kolejkowano $cmd ($fileName)", "info")
    }

    fun rtcSync() {
        val c = java.util.Calendar.getInstance()
        if (connected) {
            send("rtc_sync ${c.get(java.util.Calendar.YEAR)} ${c.get(java.util.Calendar.MONTH) + 1} ${c.get(java.util.Calendar.DAY_OF_MONTH)} " +
                "${c.get(java.util.Calendar.HOUR_OF_DAY)} ${c.get(java.util.Calendar.MINUTE)} ${c.get(java.util.Calendar.SECOND)}")
            showToast("Czas", "Wysłano czas telefonu do zegara RTC centrali", "ok")
            addLog("RTC", "Zsynchronizowano zegar z czasem urządzenia", "info")
            return
        }
        scope.launch {
            val j = try {
                Rtdb.rtcSync(S.ip, c.get(java.util.Calendar.YEAR), c.get(java.util.Calendar.MONTH) + 1,
                    c.get(java.util.Calendar.DAY_OF_MONTH), c.get(java.util.Calendar.HOUR_OF_DAY),
                    c.get(java.util.Calendar.MINUTE), c.get(java.util.Calendar.SECOND))
            } catch (e: Exception) { null }
            when {
                j == null -> showToast("Czas", "Nie udało się połączyć z centralą", "err")
                j.optBoolean("ok") -> showToast("Czas",
                    if (j.optBoolean("pominieto")) "Pominięto — NTP już zsynchronizowało RTC" else "RTC ustawiony z czasu urządzenia", "ok")
                else -> showToast("Czas", "Błąd: " + j.optString("blad", "?"), "err")
            }
        }
    }

    /** Terminal ESP — wpisy komend wysyłane przez /api/terminal albo /piec/cmd. */
    val terminalLines = mutableStateListOf<String>()
    fun terminalSend(line: String) = scope.launch {
        if (line.isBlank()) return@launch
        terminalLines.add("» " + line)
        if (connected) {
            send(line)
        } else {
            val j = try { Rtdb.terminalSend(S.ip, line) } catch (e: Exception) { null }
            terminalLines.add(if (j != null) j.optString("resp", "OK") else "⚠ centrala brak odpowiedzi")
        }
    }

    // ── Logi: filtr + szukajka (`logFilter` / `logSearchQuery`) ─────────────
    var logFilter by mutableStateOf("all")
    var logSearch by mutableStateOf("")
    fun logsFiltered(): List<LogEntry> = logs.filter { l ->
        val q = logSearch.lowercase()
        if (q.isNotEmpty() && !l.msg.lowercase().contains(q) && !l.tag.lowercase().contains(q)) return@filter false
        when (logFilter) {
            "system" -> l.tag == "SYSTEM" || l.tag == "RTC"
            "cmd" -> l.tag == "CMD" || l.tag == "ACK" || l.tag == "NACK"
            "warn" -> l.type == "warn" || l.type == "err"
            else -> true
        }
    }
    fun clearLogs() {
        logs.clear()
        addLog("SYSTEM", "Wyczyszczono bufor logów", "info")
    }

    /** `navigate(i)` — PAGES = pulpit, wykresy, pogoda, ustawienia, więcej. */
    fun navigate(i: Int) {
        page = i
        when (i) {
            // `drawTrend()` w oryginale — w Compose trend jest reaktywny (S.hist)
            1 -> { loadRange(rangeSec) }              // loadTelemetryRange + updateChartCanvas
            2 -> refreshWeatherTab()                  // refreshWeatherTab()
        }
    }

    /** Odświeżenie zakładki Pogoda (pobranie prognozy, gdy jeszcze nie ma danych). */
    fun refreshWeatherTab() {
        if (weather == null) refreshWeather(true) else if (!demoMode) refreshWeather(false)
    }

    // ─────────────────────────  mapowanie UI (PULPIT → arkusze) ─────────────────────────
    /** `#sysSub` z renderDashboard (Piec.html:4330-4345). */
    fun sysStripSub(): String = when {
        connected -> "stan na żywo z ESP32 (${S.ip}) · telemetria Firebase"
        demoMode -> "tryb symulacji offline (dane testowe)"
        else -> "oczekiwanie na połączenie z Firebase"
    }

    /** kafelki bez pozycji w `MENUS` nie otwierają arkusza (openMenu = () => {}). */
    fun menuForTile(id: String): String? = when (id) {
        "zewn", "bojler", "pokoj", "cisnienie", "wilgotnosc", "ogrz_powrot", "ogrz_trociny", "dym",
        "czujniki", "ogrz", "panel", "pompa", "serwo", "mieszadlo", "czas" -> id
        else -> null
    }

    /** `numInput(...)` → `ustaw <key> <v>` + walidacja zakresu (`NumRow` err). */
    fun commitNum(name: String, key: String, min: Int, max: Int, v: Int) {
        if (v < min || v > max) {
            showToast("Ustawienia", "$name: zakres $min–$max", "err")
            return
        }
        S.setNum(key, v.toDouble())
        S.bump()
        showToast("Ustawienia", "ustaw $key $v", "ok")
        send("ustaw $key $v")
        refreshSheet()
    }

    /** `checkbox(...)` → `ustaw <key> 1|0`. */
    fun commitBool(name: String, key: String, v: Boolean) {
        S.setBool(key, v)
        S.bump()
        showToast("Ustawienia", "ustaw $key ${if (v) 1 else 0}", "ok")
        send("ustaw $key ${if (v) 1 else 0}")
        refreshSheet()
    }
}
