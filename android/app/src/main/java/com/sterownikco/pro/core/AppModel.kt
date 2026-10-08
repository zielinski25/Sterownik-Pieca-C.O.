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
import com.sterownikco.pro.service.AlarmMonitorService
import com.sterownikco.pro.widget.PiecWidget
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

/* ══════════════════════════════════════════════════════════════════════════
   MODEL APLIKACJI — odpowiednik warstwy sterującej Piec.html:
   `pollFirebase` + `sendCommand/ACK` + `liveTick` + `refreshWeatherTab` +
   `loadTelemetryRange` + logi + toasty + timery (4 s / 45 s / 15 min / 2 s / 1 s).
   ══════════════════════════════════════════════════════════════════════════ */

private const val SMOKE_ALARM_HYSTERESIS_ADC = 30

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
    var lastFetchTs = 0L
    var dashboardWidgets by mutableStateOf(TileDefs.DEFAULT_VISIBLE_IDS)
    var authOpen by mutableStateOf(false)
    /** Okienko alarmu w aplikacji (gdy system nie odpali FSI na pierwszym planie). */
    var alarmPopup by mutableStateOf<AlarmInfo?>(null)
    /** Krawędź lokalnego testu dymu; nie zmienia alarmowych flag sterownika ani prefs AlarmCenter. */
    private var localSmokeAlarmLatched = false
    /** Jednorazowa prośba o zgodę na powiadomienia po zalogowaniu (Android 13+). */
    var askNotifPerm by mutableStateOf(false)
    /** Licznik odświeżeń arkusza Alarmy (stan uprawnień zmienia się w tle). */
    var alarmUiTick by mutableIntStateOf(0)
    /** `#authStatus` + `.auth-status.ok/.err/.warn`. */
    var authStatus by mutableStateOf("Gotowy do połączenia")
    var authStatusKind by mutableStateOf("")
    var authBusy by mutableStateOf(false)

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
    var weatherDays by mutableStateOf(7)
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

    /** Szerokość ramki w trybie szerokim (tablet / poziomo). */
    // (Przełącznik telefon/PC usunięty z APK — potrzebny tylko w Piec.html.)
    var frameWidth by mutableIntStateOf(412)

    val isFbFresh: Boolean get() = connected && (System.currentTimeMillis() - lastFetchTs < 20_000)

    init {
        // dane dostępowe z pamięci lokalnej (odpowiednik startu strony)
        Rtdb.apiKey = prefs.get(Prefs.K_API_KEY) ?: Prefs.DEFAULT_FB_API_KEY
        Rtdb.email = prefs.get(Prefs.K_EMAIL) ?: Prefs.DEFAULT_FB_EMAIL
        Rtdb.cmdToken = prefs.get(Prefs.K_CMD_TOKEN) ?: Prefs.DEFAULT_CMD_TOKEN
        Rtdb.idToken = prefs.get(Prefs.K_ID_TOKEN) ?: ""
        Rtdb.refreshToken = prefs.get(Prefs.K_REF_TOKEN) ?: ""
        ChartSeries.TEMP.forEach { seriesOn[it.id] = it.on }
        ChartSeries.SERVO.forEach { seriesOn[it.id] = it.on }
        loadChartPrefs()
        loadDashboardWidgets()
        refreshWeather(false)

        // Przywróć konto po udanym wcześniejszym logowaniu; hasło jest w Android Keystore.
        val savedPassword = prefs.getSecret(Prefs.K_PASS)
        val rememberPref = prefs.get(Prefs.K_REMEMBER_CREDS)
        val shouldRemember = if (rememberPref != null) prefs.getBool(Prefs.K_REMEMBER_CREDS, false)
            else savedPassword != null
        if (rememberPref == null && shouldRemember) prefs.setBool(Prefs.K_REMEMBER_CREDS, true)
        if (!shouldRemember) prefs.remove(Prefs.K_PASS)
        if (shouldRemember && !savedPassword.isNullOrBlank() && Rtdb.email.isNotBlank()) {
            authOpen = false
            login(Rtdb.email, savedPassword, remember = true, automatic = true)
        } else {
            authOpen = true
        }
    }

    fun addLog(tag: String, msg: String, type: String = "info") {
        val c = java.util.Calendar.getInstance()
        val ts = String.format(java.util.Locale.US, "%02d:%02d:%02d",
            c.get(java.util.Calendar.HOUR_OF_DAY), c.get(java.util.Calendar.MINUTE), c.get(java.util.Calendar.SECOND))
        logs.add(0, LogEntry(ts, tag, msg, type))
        while (logs.size > 250) logs.removeAt(logs.size - 1)
    }

    fun showToast(cmd: String, stage: String, cls: String = "ok") {
        val message = ToastMsg(cmd, stage, cls, System.nanoTime())
        toast = message
        // Tak jak w HTML, spinner „Wysyłanie…” zostaje aż do ACK/NACK/timeoutu.
        if (cls == "wait") return
        scope.launch {
            delay(if (cls == "ok") 2500 else 4500)
            if (toast?.id == message.id) toast = null
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
                    if (S.liveTick()) {
                        S.bump(); refreshSheet()
                        try { syncLocalSmokeAlarmSimulation() } catch (e: Exception) { /* test lokalny nie może wywalić timera */ }
                    }
                }
            },
            scope.launch {
                while (true) {
                    delay(1000)
                    if (sheet != null) sheetTick++
                    if (termOpened && termAutoOffAt > 0L && System.currentTimeMillis() >= termAutoOffAt) {
                        termAutoOffAt = 0L
                        termStatus = "Minął lokalny limit czasu — wysyłam Remote OFF i czekam na ACK."
                        termStatusKind = "warn"
                        terminalRemoteOff()
                    }
                }
            }
        )
    }

    fun onPause() {
        jobs.forEach { it.cancel() }; jobs = emptyList()
        // W tle NIE odpytujemy (oszczędność baterii) — czuwanie przejmuje
        // AlarmMonitorService na strumieniu SSE (natychmiast + bez pollingu).
        bgJob?.cancel(); bgJob = null
    }

    private fun recordSolar() {
        val w = weather?.current ?: return
        if (!S.hasAllData("t_panel", "t_bojler", "t_zewn")) return
        val panel = S.t_panel
        val boiler = S.t_bojler
        val outside = S.t_zewn
        if (!panel.isFinite() || !boiler.isFinite() || !outside.isFinite()) return
        solar.recordSample(panel, boiler, outside, radEst(w), w.uv, w.cloud)
    }

    /** Promieniowanie krótkofalowe Open-Meteo z bieżącej godziny; nie estymujemy go z UV. */
    fun radEst(w: WCurrent?): Double {
        if (w == null) return Double.NaN
        val hourStart = Weather.startOfHour()
        return weather?.hourly?.firstOrNull { it.time == hourStart }?.shortwaveRadiation
            ?.takeIf { it.isFinite() && it >= 0.0 } ?: Double.NaN
    }

    // ── Firebase ────────────────────────────────────────────────────────────
    fun poll() = scope.launch {
        val d = try { Rtdb.pollStatus() } catch (e: Exception) { null }
        if (d == null) {
            connected = false
            termStateKnown = false
            S.bump()
            return@launch
        }
        connected = true
        lastFetchTs = System.currentTimeMillis()
        S.applyIncomingData(d)
        S.bump()
        if (S.online) {
            recordSolar()
            if (page == 1) appendLiveFeed()
        }
        // Alarmy oceniaj tylko wtedy, gdy centrala zwróciła rzeczywiste flagi.
        try {
            if (S.hasAllData("dym_alarm", "alarm_ogrzewanie")) {
                val info = AlarmCenter.evaluate(prefs, S.dym_alarm, S.alarm_ogrzewanie, S.t_ogrz, S.dym)
                if (info != null) {
                    AlarmNotify.fire(ctx, info)
                    alarmPopup = info
                } else if (!S.dym_alarm && !S.alarm_ogrzewanie) {
                    AlarmNotify.cancel(ctx)
                }
            }
        } catch (e: Exception) { /* alarm nie może wywalić poll() */ }
        // Jawna symulacja dymu testuje lokalny tor alarmu, ale nie zmienia flag sterownika.
        try { syncLocalSmokeAlarmSimulation() } catch (e: Exception) { /* test lokalny nie może wywalić poll() */ }
        // Widget na pulpit (throttling w środku).
        try { PiecWidget.push(ctx, d) } catch (e: Exception) { /* ignore */ }
    }

    /** Logowanie kontem e-mail + hasło; zapamiętanie hasła jest opcjonalne i szyfrowane. */
    fun login(emailIn: String, password: String, remember: Boolean, automatic: Boolean = false) = scope.launch {
        authBusy = true; authStatusKind = "warn"
        authStatus = if (automatic) "Przywracanie zapisanej sesji…" else "Logowanie do Firebase UserAuth…"
        val apiKey = prefs.get(Prefs.K_API_KEY) ?: Prefs.DEFAULT_FB_API_KEY
        val cmdToken = prefs.get(Prefs.K_CMD_TOKEN) ?: Prefs.DEFAULT_CMD_TOKEN
        if (apiKey.length < 20) {
            authBusy = false; authStatusKind = "err"; authStatus = "Brak klucza API."
            if (automatic) authOpen = true
            else showToast("Logowanie", "Brak klucza API.", "err")
            return@launch
        }
        if (!emailIn.contains("@") || password.length < 6) {
            authBusy = false; authStatusKind = "err"; authStatus = "Podaj poprawny e-mail i hasło konta."
            if (automatic) authOpen = true
            else showToast("Logowanie", "Podaj poprawny e-mail i hasło konta.", "err")
            return@launch
        }
        val r = Rtdb.signIn(apiKey, emailIn, password)
        r.onSuccess { j ->
            Rtdb.apiKey = apiKey
            Rtdb.idToken = j.getString("idToken")
            Rtdb.refreshToken = j.optString("refreshToken")
            Rtdb.email = emailIn
            Rtdb.cmdToken = cmdToken
            prefs.set(Prefs.K_API_KEY, apiKey)
            prefs.set(Prefs.K_EMAIL, emailIn)
            prefs.set(Prefs.K_CMD_TOKEN, cmdToken)
            prefs.set(Prefs.K_ID_TOKEN, Rtdb.idToken)
            prefs.set(Prefs.K_REF_TOKEN, Rtdb.refreshToken)
            prefs.setBool(Prefs.K_REMEMBER_CREDS, remember)
            val passwordSaved = if (remember) prefs.setSecret(Prefs.K_PASS, password)
                else { prefs.remove(Prefs.K_PASS); true }
            if (!passwordSaved) prefs.remove(Prefs.K_PASS)
            if (!passwordSaved) showToast("SESJA", "Zalogowano, ale nie udało się bezpiecznie zapisać hasła.", "err")
            else if (!automatic) showToast("FIREBASE", "Logowanie zakończone — pobieram /piec/status", "ok")
            addLog("SYSTEM", "Zalogowano operatora: $emailIn", "info")
            authOpen = false
            authBusy = false
            authStatusKind = if (passwordSaved) "ok" else "warn"
            authStatus = if (passwordSaved) "Zalogowano — $emailIn"
                else "Zalogowano, ale hasło nie mogło zostać bezpiecznie zapamiętane."
            // Start czuwania w tle + prośba o zgodę na powiadomienia (Android 13+).
            try { AlarmMonitorService.start(ctx) } catch (e: Exception) { /* ignore */ }
            if (!AlarmNotify.hasPostNotifications(ctx)) askNotifPerm = true
            poll()
            if (page == 1) loadRange(rangeSec)
        }.onFailure { e ->
            authBusy = false; authStatusKind = "err"; authStatus = e.message ?: "Logowanie nieudane"
            if (automatic) authOpen = true
            else showToast("BŁĄD", e.message ?: "Logowanie nieudane", "err")
        }
    }

    // (Tryb symulacji/DEMO usuniety z APK — tylko w Piec.html.)

    fun logout() {
        prefs.remove(Prefs.K_PASS); prefs.remove(Prefs.K_ID_TOKEN); prefs.remove(Prefs.K_REF_TOKEN)
        Rtdb.idToken = ""; Rtdb.refreshToken = ""
        try { AlarmMonitorService.stop(ctx) } catch (e: Exception) { /* ignore */ }
        try { PiecWidget.clear(ctx) } catch (e: Exception) { /* ignore */ }
        try { AlarmNotify.cancelSimulation(ctx) } catch (e: Exception) { /* ignore */ }
        localSmokeAlarmLatched = false
        alarmPopup = null
        connected = false
        termStateKnown = false
        S.bump()
        showToast("SESYJA", "Wylogowano operatora", "ok")
        addLog("SYSTEM", "Zamknięto sesję operatora", "warn")
        authOpen = true
    }

    // ── polecenia; stan urządzenia zmienia się dopiero po ACK / odczycie ─────
    fun send(cmd: String) = scope.launch { sendAwait(cmd) }

    /** Wysyła komendę bez lokalnej mutacji stanu i zwraca true wyłącznie po ACK. */
    private suspend fun sendAwait(cmd: String): Boolean {
        val command = cmd.trim()
        if (command.isEmpty()) return false
        val verb = command.substringBefore(' ').lowercase()
        val isSimulation = verb == "symuluj" || verb == "symuluj_stop"

        if (isSimulation) {
            val result = S.applySimulationCommand(command)
            if (result != true) {
                showToast(command, result as? String ?: "Nieprawidłowe polecenie symulacji", "err")
                return false
            }
            S.bump(); refreshSheet()
            val smokeStatus = try { syncLocalSmokeAlarmSimulation() } catch (e: Exception) { null }
            addLog("SYM", if (verb == "symuluj") "Operator włączył lokalną symulację czujnika" else "Operator wyłączył lokalną symulację czujnika", "warn")
            val simField = command.split(Regex("\\s+")).getOrNull(1)
            val stage = when {
                verb == "symuluj_stop" -> "Lokalna symulacja wyłączona"
                verb == "symuluj" && simField == "dym" -> smokeStatus ?: "SYM aktywne lokalnie — nie wysłano do sterownika"
                else -> "SYM aktywne lokalnie — nie wysłano do sterownika"
            }
            showToast(command, stage, "warn")
            return true
        }
        if (!connected) {
            showToast(command, "Brak połączenia z Firebase — polecenia nie wysłano", "err")
            addLog("CMD_ERR", "Nie wysłano polecenia — brak połączenia z Firebase", "warn")
            return false
        }

        val t0 = System.nanoTime()
        showToast(command, "Wysyłanie do sterownika…", "wait")
        addLog("CMD", "Wysyłanie: $command", "cmd")
        val cmdId = Rtdb.makeFireCmdId()
        return try {
            val ack = Rtdb.sendCommand(cmdId, command)
            val latency = ((System.nanoTime() - t0) / 1_000_000).toInt()
            when {
                ack == null -> {
                    showToast(command, "Brak potwierdzenia sterownika — stan nie został zmieniony lokalnie", "warn")
                    addLog("ACK_WARN", "Brak potwierdzenia sterownika", "warn")
                    poll()
                    false
                }
                ack.ok -> {
                    showToast(command, "Potwierdzone przez sterownik (ACK, ${latency}ms)", "ok")
                    addLog("ACK", "ACK OK (${latency}ms): $command", "ack")
                    buzz()
                    poll()
                    true
                }
                else -> {
                    showToast(command, "Sterownik odrzucił polecenie: ${ack.error}", "err")
                    addLog("NACK", "Odrzucone (${ack.error}): $command", "err")
                    false
                }
            }
        } catch (e: Exception) {
            showToast(command, "Błąd wysyłki: ${e.message ?: "brak"}", "err")
            addLog("CMD_ERR", "Błąd wysyłki: " + (e.message ?: "?"), "err")
            false
        }
    }

    /**
     * Lokalny test alarmu dla ręcznie symulowanego ADC dymu. Nie modyfikuje
     * S.dym_alarm ani sygnatur AlarmCenter, więc nie udaje ACK/telemetrii ESP.
     */
    private fun syncLocalSmokeAlarmSimulation(): String? {
        val candidate = S.sym["dym"]
        val simulated = candidate?.takeUnless {
            it.expiresAtNanos <= android.os.SystemClock.elapsedRealtimeNanos()
        }
        if (candidate != null && simulated == null) S.sym.remove("dym")
        if (simulated == null) {
            clearLocalSmokeAlarmSimulation()
            return null
        }

        val threshold = if (S.hasData("progAlarmDym"))
            S.progAlarmDym.takeIf { it > SMOKE_ALARM_HYSTERESIS_ADC } else null
        if (threshold == null) {
            return if (localSmokeAlarmLatched) "Lokalny test alarmu dymu pozostaje aktywny"
            else "SYM lokalna — próg alarmu dymu nieodebrany lub nieprawidłowy; test pominięty"
        }
        // Firmware alarmuje przy ADC > próg i kasuje dopiero poniżej progu minus histereza 30 ADC.
        val releaseThreshold = (threshold - SMOKE_ALARM_HYSTERESIS_ADC).toDouble()
        if (localSmokeAlarmLatched && simulated.wartosc < releaseThreshold) {
            clearLocalSmokeAlarmSimulation()
            return "SYM lokalna — odczyt spadł poniżej progu zwolnienia alarmu"
        }
        val thresholdExceeded = simulated.wartosc > threshold.toDouble()
        if (!localSmokeAlarmLatched && !thresholdExceeded) {
            return "SYM lokalna — odczyt nie przekracza progu $threshold ADC"
        }
        val realAlarmActive = (S.hasData("dym_alarm") && S.dym_alarm) ||
            (S.hasData("alarm_ogrzewanie") && S.alarm_ogrzewanie)
        if (realAlarmActive) {
            localSmokeAlarmLatched = true
            if (alarmPopup?.simulated == true) alarmPopup = null
            try { AlarmNotify.cancelSimulation(ctx) } catch (e: Exception) { /* ignore */ }
            return "Próg przekroczony — alarm sterownika jest już aktywny"
        }
        if (localSmokeAlarmLatched) return "Lokalny test alarmu dymu aktywny"
        if (!AlarmCenter.isMonitored(prefs)) return "Próg przekroczony — monitoring alarmów jest wyłączony"

        localSmokeAlarmLatched = true
        val value = simulated.wartosc.roundToInt()
        val info = AlarmInfo(
            kind = "dym",
            title = "TEST ALARMU DYMU · SYMULACJA LOKALNA",
            msg = "Symulowany odczyt: $value ADC (próg: $threshold ADC). Test lokalny — nie pochodzi ze sterownika.",
            sig = "SYM-DYM",
            simulated = true
        )
        alarmPopup = info
        try { AlarmNotify.fire(ctx, info) } catch (e: Exception) { /* popup pozostaje widoczny */ }
        addLog("ALARM", "Lokalny test alarmu dymu: $value ADC (próg $threshold ADC)", "warn")
        return "Próg $threshold ADC przekroczony — uruchomiono lokalny test alarmu"
    }

    private fun clearLocalSmokeAlarmSimulation() {
        if (!localSmokeAlarmLatched) return
        localSmokeAlarmLatched = false
        if (alarmPopup?.simulated == true) alarmPopup = null
        try { AlarmNotify.cancelSimulation(ctx) } catch (e: Exception) { /* ignore */ }
        addLog("ALARM", "Zakończono lokalny test alarmu dymu", "info")
    }

    // ── pogoda ──────────────────────────────────────────────────────────────
    // ── pogoda (TYLKO prawdziwe Open-Meteo — zero symulacji) ────────────────
    fun refreshWeather(force: Boolean, announce: Boolean = false) = scope.launch {
        if (weather == null || force) weatherBusy = true
        val lat = prefs.get(Prefs.K_LAT) ?: "51.066389"
        val lon = prefs.get(Prefs.K_LON) ?: "21.509167"
        val real = Weather.fetch(http, lat, lon, weatherDays)
        if (real != null) {
            weather = real; S.bump()
            if (announce) showToast("Pogoda", "Dane pogodowe zaktualizowane", "ok")
        } else if (weather == null) {
            // Pierwsze pobranie padlo: uczciwy brak danych, zadnych wymyslonych.
            if (announce) showToast("Pogoda", "Brak połączenia z Open-Meteo", "err")
        } else if (announce) {
            showToast("Pogoda", "Brak połączenia — pokazuję ostatnie dane", "err")
        }
        weatherBusy = false
        solar.estimateForecastGain(weather?.hourly ?: emptyList())
    }

    /** Zakres studia pogody (W_RANGES w HTML): zmiana dni = refetch + przerys. */
    @JvmName("hmiSetWeatherDays")
    fun setWeatherDays(d: Int) {
        if (weatherDays == d) return
        weatherDays = d
        refreshWeather(true)
    }

    /** Eksport archiwum solarnego do CSV (Pobrane) — jak `SolarAnalytics.exportCsv`. */
    fun exportSolarCsv() = scope.launch {
        if (!solar.hasSamples) {
            showToast("Solar", "Brak rzeczywistych próbek do eksportu", "warn")
            return@launch
        }
        try {
            val csv = solar.csv()
            val name = solar.fileName()
            val where = withContext(kotlinx.coroutines.Dispatchers.IO) {
                if (android.os.Build.VERSION.SDK_INT >= 29) {
                    val cv = android.content.ContentValues().apply {
                        put(android.provider.MediaStore.Downloads.DISPLAY_NAME, name)
                        put(android.provider.MediaStore.Downloads.MIME_TYPE, "text/csv")
                    }
                    val u = ctx.contentResolver.insert(
                        android.provider.MediaStore.Downloads.EXTERNAL_CONTENT_URI, cv
                    ) ?: throw java.io.IOException("MediaStore odmówił")
                    ctx.contentResolver.openOutputStream(u)?.use { it.write(csv.toByteArray(Charsets.UTF_8)) }
                        ?: throw java.io.IOException("zapis niemożliwy")
                    "Pobrane/$name"
                } else {
                    @Suppress("DEPRECATION")
                    val dir = ctx.getExternalFilesDir(android.os.Environment.DIRECTORY_DOCUMENTS)
                        ?: throw java.io.IOException("brak katalogu")
                    java.io.File(dir, name).writeText(csv, Charsets.UTF_8)
                    "Dokumenty/$name"
                }
            }
            showToast("Solar", "Zapisano $where", "ok")
            addLog("SOLAR", "Eksport CSV: $where", "info")
        } catch (e: Exception) {
            showToast("Solar", "Eksport nieudany: ${e.message}", "err")
        }
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
                    chartLive = emptyList()
                    telemetry = fb
                    chartPoints = fb.size
                    chartStatus = "Załadowano ${fb.size} próbek z bazy Firebase ($rangeName) · tylko rzeczywiste wartości"
                    chartReality = "FIREBASE RTDB"
                    return@launch
                }
            } catch (e: Exception) { /* fallback poniżej, jak w panelu */ }
        }
        telemetry = emptyList()
        chartPoints = 0
        chartStatus = if (connected) "Brak rzeczywistych próbek dla wybranego zakresu" else "Zaloguj się do Firebase, aby pobrać historię telemetrii"
        chartReality = if (connected) "BRAK DANYCH" else "BRAK SESJI"
        chartLive = emptyList()
    }

    /** Dokleja wyłącznie odczyt z właśnie zakończonego, udanego pollingu Firebase. */
    fun appendLiveFeed() {
        if (page != 1 || !isFbFresh || !S.online || lastFetchTs <= 0L) return
        if (chartLive.lastOrNull()?.ts == lastFetchTs) return
        fun value(key: String, simKey: String, v: Double) =
            v.takeIf { S.hasData(key) && !S.symAktywna(simKey) && it.isFinite() }
        val p = TelemPoint(
            ts = lastFetchTs, seq = chartLive.size.toLong() + 1,
            t_zewn = value("t_zewn", "zewn", S.t_zewn),
            t_bojler = value("t_bojler", "bojler", S.t_bojler),
            t_ogrz = value("t_ogrz", "ogrz", S.t_ogrz),
            t_ogrz_sr = value("t_ogrz_sr", "ogrz", S.t_ogrz_sr),
            t_powrot = value("t_powrot", "ogrz_powrot", S.t_powrot),
            t_panel = value("t_panel", "panel", S.t_panel),
            t_pokoj = value("t_pokoj", "pokoj", S.t_pokoj),
            t_trociny = value("t_trociny", "ogrz_trociny", S.t_trociny),
            wilgotnosc = value("wilgotnosc", "wilgotnosc", S.wilgotnosc),
            cisnienie = value("cisnienie", "cisnienie", S.cisnienie),
            dym = value("dym", "dym", S.dym),
            klapa = S.klapa.takeIf { S.hasData("klapa") && it in 0..180 }?.let { it * 100.0 / 180.0 },
            syberka = S.syberka.takeIf { S.hasData("syberka") && it in 0..90 }?.let { it * 100.0 / 90.0 }
        )
        if (p.hasRealValues()) chartLive = (chartLive + p).takeLast(240)
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
    fun catalogOn(): List<SeriesDef> = activeCatalog()
        .map { it.copy(on = seriesOn[it.id] ?: it.on) }
        .filter { it.on }

    // ── UI wykresów: etykiety i sterowanie (port funkcji renderujących) ─────
    /** `chartDataset` — historia + dopływ próbek na żywo (prawa krawędź). */
    fun chartDataset(): List<TelemPoint> = if (chartLive.isEmpty()) telemetry else telemetry + chartLive

    // Wlasciwosc o tej samej nazwie generuje JVM-owy setChartFocus — stqd "Platform declaration clash".
    @JvmName("hmiSetChartFocus")
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

    // Wlasciwosc o tej samej nazwie generuje JVM-owy setChartMode — stqd "Platform declaration clash".
    @JvmName("hmiSetChartMode")
    fun setChartMode(v: String) { chartMode = v; saveChartPrefs() }
    // Wlasciwosc o tej samej nazwie generuje JVM-owy setChartStyle — stqd "Platform declaration clash".
    @JvmName("hmiSetChartStyle")
    fun setChartStyle(v: String) { chartStyle = v; saveChartPrefs() }
    // Wlasciwosc o tej samej nazwie generuje JVM-owy setChartGlitch — stqd "Platform declaration clash".
    @JvmName("hmiSetChartGlitch")
    fun setChartGlitch(v: Boolean) { chartGlitch = v; saveChartPrefs() }
    // Wlasciwosc o tej samej nazwie generuje JVM-owy setChartAreaBand — stqd "Platform declaration clash".
    @JvmName("hmiSetChartAreaBand")
    fun setChartAreaBand(v: Boolean) { chartAreaBand = v; saveChartPrefs() }
    // Wlasciwosc o tej samej nazwie generuje JVM-owy setChartAlarmLines — stqd "Platform declaration clash".
    @JvmName("hmiSetChartAlarmLines")
    fun setChartAlarmLines(v: Boolean) { chartAlarmLines = v; saveChartPrefs() }
    fun setAlarmLevel(k: String, v: Double) { alarmLevels = alarmLevels + (k to v); saveChartPrefs() }

    /** `chartZoom` 1…16 (zoom +/-1.3× w arkuszu narzędzi). */
    fun zoomChart(f: Float) { chartZoom = (chartZoom * f).coerceIn(1f, 16f) }
    fun resetChartView() { chartZoom = 1f; chartOffset = 1f }

    /** Rozmiar datasetu bez kosztownej konkatenacji list (gesty wywoluja to co klatke). */
    fun chartTotal(): Int = telemetry.size + chartLive.size

    private fun windowGeom(total: Int, zoom: Float): Pair<Int, Int> {
        val vis = maxOf(4, Math.round(total / maxOf(zoom, 0.01f)))
        return vis to maxOf(0, total - vis)
    }

    /** Biezace okno [start, end) + total — do paska pozycji pod wykresem. */
    fun chartWindow(): Triple<Int, Int, Int> {
        val total = chartTotal()
        if (total <= 0) return Triple(0, 0, 0)
        val (vis, maxStart) = windowGeom(total, chartZoom)
        val start = maxOf(0, minOf(maxStart, Math.round(maxStart * (1 - chartOffset))))
        return Triple(start, minOf(total, start + vis), total)
    }

    /** Zoom szczypaniem z kotwica w punkcie focusFrac (0..1 plotu). Okno 1:1
     * z buildChartView: start = maxStart·(1−offset) — jak w Piec.html. */
    fun pinchChart(factor: Float, focusFrac: Float) {
        val total = chartTotal()
        if (total < 5 || !factor.isFinite() || factor <= 0f) return
        val f = focusFrac.coerceIn(0f, 1f)
        val (vis, maxStart) = windowGeom(total, chartZoom)
        val start = maxOf(0, minOf(maxStart, Math.round(maxStart * (1 - chartOffset))))
        val anchor = start + f * maxOf(1, vis - 1)
        val z2 = (chartZoom * factor).coerceIn(1f, 16f)
        val (vis2, maxStart2) = windowGeom(total, z2)
        val start2 = maxOf(0, minOf(maxStart2, Math.round(anchor - f * maxOf(1, vis2 - 1))))
        chartZoom = z2
        chartOffset = if (maxStart2 > 0) 1f - start2.toFloat() / maxStart2 else 1f
    }

    /** Pan: dxFrac = ulamek szerokosci plotu (dx>0 w prawo — jak drag w HTML:
     * offset rosnie, tresc idzie za palcem). */
    fun panChart(dxFrac: Float) {
        if (!dxFrac.isFinite()) return
        chartOffset = (chartOffset + dxFrac / maxOf(chartZoom, 1f)).coerceIn(0f, 1f)
    }

    /** Pasek pod wykresem: przesuniecie okna o ulamek calego datasetu
     * (dx>0 = w strone nowszych, kciuk idzie za palcem). */
    fun seekChartBy(dStartFrac: Float) {
        val (start, _, total) = chartWindow()
        if (total < 5 || !dStartFrac.isFinite()) return
        val (_, maxStart) = windowGeom(total, chartZoom)
        if (maxStart <= 0) return
        chartOffset = 1f - (start + dStartFrac * total).coerceIn(0f, maxStart.toFloat()) / maxStart
    }

    /** Pasek pod wykresem: tap — srodek okna w miejsce tapa. */
    fun seekChartCentered(frac: Float) {
        val (_, _, total) = chartWindow()
        if (total < 5) return
        val (vis, maxStart) = windowGeom(total, chartZoom)
        if (maxStart <= 0) return
        chartOffset = 1f - (frac.coerceIn(0f, 1f) * total - vis / 2f).coerceIn(0f, maxStart.toFloat()) / maxStart
    }
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
    fun chartNowStat(): String = if (chartFocus == 0) {
        if (S.symAktywna("ogrz")) "SYM wyłączona z wykresów"
        else if (S.hasData("t_ogrz")) "piec " + fmt1(S.t_ogrz) + "°C" else "piec —"
    } else {
        if (S.hasData("klapa")) "klapa " + Math.round(S.klapa / 1.8) + "%" else "klapa —"
    }

    fun chartPhase(): String {
        if (!S.hasData("t_ogrz")) return "Brak rzeczywistego odczytu temperatury pieca"
        return when {
            S.t_ogrz > 65 -> "🔥 FAZA GRZANIA: INTENSYWNA (" + fmt1(S.t_ogrz) + "°C)"
            S.t_ogrz > 45 -> "🔥 FAZA GRZANIA: STABILNA (" + fmt1(S.t_ogrz) + "°C)"
            else -> "⏸ FAZA POSTOJU / WYGASZANIE (" + fmt1(S.t_ogrz) + "°C)"
        }
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

    private fun loadDashboardWidgets() {
        val raw = prefs.get(Prefs.K_DASH_WIDGETS)
        if (raw == null) {
            dashboardWidgets = TileDefs.DEFAULT_VISIBLE_IDS
            return
        }
        try {
            val allowed = TileDefs.TILES.map { it.id }.toSet()
            val stored = JSONArray(raw)
            val selected = linkedSetOf<String>()
            for (i in 0 until stored.length()) {
                stored.optString(i).takeIf { it in allowed }?.let(selected::add)
            }
            dashboardWidgets = selected
        } catch (e: Exception) {
            dashboardWidgets = TileDefs.DEFAULT_VISIBLE_IDS
        }
    }

    fun setDashboardWidgetEnabled(id: String, enabled: Boolean) {
        if (TileDefs.TILES.none { it.id == id }) return
        val next = dashboardWidgets.toMutableSet()
        if (enabled) next.add(id) else next.remove(id)
        dashboardWidgets = next
        prefs.set(Prefs.K_DASH_WIDGETS, JSONArray(next.toList()).toString())
    }

    fun resetDashboardWidgets() {
        dashboardWidgets = TileDefs.DEFAULT_VISIBLE_IDS
        prefs.set(Prefs.K_DASH_WIDGETS, JSONArray(dashboardWidgets.toList()).toString())
    }

    // (setLayoutMode usunięty z APK — przełącznik telefon/PC tylko w Piec.html.)

    // ── Telegram (przez centralę / Firebase — jak w panelu) ─────────────────
    var tgEnabled by mutableStateOf(false)
    var tgToken by mutableStateOf("")
    var tgChatId by mutableStateOf("")
    var wifiSaved by mutableStateOf<List<WifiNet>>(emptyList())
    var wifiLoading by mutableStateOf(false)
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

    /** Raport Telegram nie wypełnia brakujących pól wartościami domyślnymi. */
    fun telegramReport(): String {
        fun deg(key: String, value: Double) = if (S.hasData(key) && value.isFinite()) fmt1(value) + "°C" else "—"
        val pump = if (S.hasData("pompa")) if (S.pompa) "🟢 PRACA" else "⚪ STOP" else "—"
        val pumpDesc = when {
            !S.hasData("wybor") -> "ustawienia niedostępne"
            S.wybor == 1 && S.hasAllData("czasOn", "czasOff") -> "Trociniak (${S.czasOn}m/${S.czasOff}m)"
            S.wybor == 2 && S.hasAllData("tempOn", "tempOff") -> "Kopciuch (ON: ${S.tempOn}°C, OFF: ${S.tempOff}°C)"
            S.wybor == 3 && S.hasData("tempOn") -> "Auto (start: ${S.tempOn}°C)"
            else -> "ustawienia niedostępne"
        }
        val servoMode = if (S.hasData("tryb_serwa")) PiecState.TRYB_NAZWA[S.tryb_serwa] ?: "—" else "—"
        val servo = if (S.hasAllData("tryb_serwa", "klapa", "syberka"))
            "Klapa ${Math.round(S.klapa / 1.8)}% | Syberek ${Math.round(S.syberka / 0.9)}% (Tryb: $servoMode)" else "—"
        val alarm = if (!S.hasAllData("dym_alarm", "alarm_ogrzewanie", "alarm_panel")) "BRAK DANYCH"
        else when {
            S.dym_alarm -> "🚨 ALARM DYMU"
            S.alarm_ogrzewanie -> "🚨 ALARM PRZEGRZANIA"
            S.alarm_panel -> "🚨 ALARM PANELU"
            else -> "BRAK (stan odczytany)"
        }
        val wifi = if (S.hasData("wifi_rssi")) "${S.wifi_rssi} dBm" else "—"
        return "🔥 *Sterownik C.O.* — Raport stanu\n" +
            "• *Piec:* ${deg("t_ogrz", S.t_ogrz)} | *Bojler:* ${deg("t_bojler", S.t_bojler)} | *Panel:* ${deg("t_panel", S.t_panel)} | *Zewn:* ${deg("t_zewn", S.t_zewn)}\n" +
            "• *Pompa:* $pump — $pumpDesc\n" +
            "• *Serwa:* $servo\n" +
            "• *Alerty:* $alarm\n" +
            "• *WiFi:* $wifi · IP: " + (if (S.hasData("ip")) S.ip else "—")
    }

    private fun fmt1(v: Double): String = if (v.isFinite()) String.format(java.util.Locale.US, "%.1f", v) else "—"

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
        wifiLoading = true
        wifiSavedNote = null
        val ip = S.ip.trim()
        try {
            val arr = try { if (ip.isEmpty()) null else Rtdb.wifiList(ip) } catch (e: Exception) { null }
            wifiSavedNote = when {
                ip.isEmpty() -> "Brak adresu IP centrali — nie mogę pobrać listy zapisanych sieci."
                arr == null -> "Nie udało się pobrać listy z http://$ip/api/wifi/list. Sprawdź połączenie telefonu z siecią centrali i dostępność endpointu w firmware."
                else -> null
            }
            if (arr != null) {
                wifiSaved = (0 until arr.length()).map { i ->
                    val o = arr.getJSONObject(i)
                    WifiNet(o.optString("ssid"), o.optBoolean("active"), o.optInt("rssi"), o.optBoolean("hasPass"))
                }
            }
        } finally {
            wifiLoading = false
        }
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
            wifiScanNote = "Nie udało się uruchomić skanowania pod http://" + ip +
                "/api/wifi/scan/start. Sprawdź połączenie telefonu z siecią centrali i dostępność endpointu w firmware."
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
            cur != null && cur.ssid.isNotBlank() -> cur.ssid
            S.hasData("wifi_ssid") && fromState.isNotEmpty() -> fromState
            else -> "SSID nieodebrane z centrali"
        }
    }

    // ─────────────────────────  TERMINAL DIAGNOSTYCZNY (21 kategorii DLOG)  ─────────────────────────
    /** `_terminalState` z Piec.html:6060. */
    val terminalRaw = ArrayList<String>()
    val terminalDisplay = mutableStateListOf<String>()
    var termSeq by mutableIntStateOf(-1)
    var termLines by mutableIntStateOf(0)
    var termBytes by mutableLongStateOf(0L)
    var termGaps by mutableIntStateOf(0)
    var termChunks by mutableIntStateOf(0)
    var termPaused by mutableStateOf(false)
    var termOpened by mutableStateOf(false)
    var termStateKnown by mutableStateOf(false)
    var termAutoOffAt by mutableLongStateOf(0L)
    var termLevel by mutableStateOf("INFO")
    var termStatus by mutableStateOf("Brak odebranych wpisów DLOG. Oczekiwanie na rzeczywiste dane sterownika.")
    var termStatusKind by mutableStateOf("")
    val termCats = mutableStateMapOf<String, Boolean>()

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
        terminalRaw.clear()
        termSeq = -1; termLines = 0; termBytes = 0L; termGaps = 0; termChunks = 0
        terminalRender()
        termStatus = "Wyczyszczono lokalny bufor DLOG."
        termStatusKind = ""
    }

    fun terminalTogglePause() {
        termPaused = !termPaused
        termStatus = if (termPaused) "Wstrzymano wyświetlanie lokalnego bufora." else "Wyświetlanie bufora wznowione."
        termStatusKind = ""
        if (!termPaused) terminalRender()
    }

    fun terminalRemoteOn() = setTerminalRemote(true)

    fun terminalRemoteOff() = setTerminalRemote(false)

    private fun setTerminalRemote(enabled: Boolean) = scope.launch {
        if (!connected) {
            termStateKnown = false
            termStatus = "Brak połączenia z Firebase — polecenie Remote nie zostało wysłane."
            termStatusKind = "err"
            return@launch
        }
        val command = if (enabled) "diag remote on" else "diag remote off"
        if (!enabled) termAutoOffAt = 0L
        termStatus = "Oczekiwanie na potwierdzenie sterownika…"
        termStatusKind = ""
        try {
            val ack = Rtdb.sendCommand(Rtdb.makeFireCmdId(), command)
            when {
                ack == null -> {
                    termStateKnown = false
                    termStatus = "Brak potwierdzenia ESP32; stan Remote jest nieznany."
                    termStatusKind = "warn"
                }
                !ack.ok -> {
                    termStatus = "ESP32 odrzucił polecenie: ${ack.error}"
                    termStatusKind = "err"
                }
                else -> {
                    termOpened = enabled
                    termStateKnown = true
                    termAutoOffAt = if (enabled) System.currentTimeMillis() + 60L * 60_000L else 0L
                    termStatus = if (enabled)
                        "ESP32 potwierdził Remote ON. Wersja Android nie odbiera strumienia DLOG."
                    else "ESP32 potwierdził Remote OFF."
                    termStatusKind = "ok"
                    poll()
                }
            }
        } catch (e: Exception) {
            termStateKnown = false
            termStatus = "Błąd polecenia Remote: ${e.message ?: "brak odpowiedzi"}"
            termStatusKind = "err"
        }
    }

    private fun terminalSendWithAck(command: String, success: String, onAck: () -> Unit = {}) = scope.launch {
        if (!connected) {
            termStatus = "Brak połączenia Firebase — polecenia DLOG nie wysłano."
            termStatusKind = "err"
            return@launch
        }
        termStatus = "Oczekiwanie na potwierdzenie sterownika…"
        termStatusKind = ""
        try {
            val ack = Rtdb.sendCommand(Rtdb.makeFireCmdId(), command)
            when {
                ack == null -> {
                    termStatus = "Brak ACK ESP32; stan tej opcji jest nieznany."
                    termStatusKind = "warn"
                }
                !ack.ok -> {
                    termStatus = "ESP32 odrzucił polecenie: ${ack.error}"
                    termStatusKind = "err"
                }
                else -> {
                    onAck()
                    termStatus = success
                    termStatusKind = "ok"
                    poll()
                }
            }
        } catch (e: Exception) {
            termStatus = "Błąd polecenia DLOG: ${e.message ?: "brak odpowiedzi"}"
            termStatusKind = "err"
        }
    }

    fun terminalSetLevel(lvl: String) {
        if (lvl.isEmpty()) return
        if (!termOpened) {
            termLevel = lvl // lokalny wybór; nie twierdzi, że ESP32 zmieniło poziom
            termStatus = "Poziom $lvl wybrano lokalnie; nie wysłano go do sterownika."
            termStatusKind = "warn"
            return
        }
        terminalSendWithAck("diag remote level $lvl", "ESP32 potwierdził poziom DLOG $lvl.") {
            termLevel = lvl
        }
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
            val state = if (on) "on" else "off"
            terminalSendWithAck("diag remote cat ALL $state", "ESP32 potwierdził filtry kategorii DLOG: $state.")
        } else {
            termStatus = if (on) "Filtry lokalnego bufora włączone." else "Filtry lokalnego bufora wyłączone."
            termStatusKind = ""
        }
    }

    /** Polecenie wysyłane do sterownika; konsola nie dopisuje fikcyjnego ACK. */
    fun terminalExec(cmd: String) {
        val command = cmd.trim()
        if (command.isEmpty()) return
        send(command)
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

    fun checkGithubRelease(onDone: ((Boolean) -> Unit)? = null) = scope.launch {
        ghChecking = true
        val d = try { Rtdb.latestRelease() } catch (e: Exception) { null }
        val tag = d?.optString("tag_name", "")?.trim().orEmpty()
        ghRelease = if (d != null && tag.isNotEmpty()) JSONObject()
            .put("tag", tag)
            .put("name", d.optString("name", ""))
            .put("publishedAt", d.optString("published_at").split("T").firstOrNull().orEmpty())
            .put("body", d.optString("body", ""))
            .put("assets", d.optJSONArray("assets") ?: JSONArray())
        else null
        ghChecking = false
        onDone?.invoke(ghRelease != null)
    }

    /** ACK potwierdza tylko przyjęcie komendy, nie postęp ani wynik flashowania. */
    fun runOtaProcedure(targetName: String, fileName: String, cmd: String) = scope.launch {
        otaProgressShown = true
        otaProgressPct = "…"
        otaProgressLabel = "Oczekiwanie na potwierdzenie komendy \"$cmd\" ($fileName)…"
        val confirmed = sendAwait(cmd)
        if (confirmed) {
            otaProgressPct = "ACK"
            otaProgressLabel = "Sterownik potwierdził przyjęcie \"$cmd\". Postęp OTA $targetName nie jest dostępny w panelu — sprawdź logi centrali."
            addLog("OTA", "Sterownik potwierdził przyjęcie $cmd ($fileName); wynik flashowania niezweryfikowany", "ack")
        } else {
            otaProgressPct = "—"
            otaProgressLabel = "Brak potwierdzenia \"$cmd\". Nie można stwierdzić, czy OTA $targetName zostało rozpoczęte."
        }
    }

    fun rtcSync() {
        val c = java.util.Calendar.getInstance()
        if (connected) {
            send("rtc_sync ${c.get(java.util.Calendar.YEAR)} ${c.get(java.util.Calendar.MONTH) + 1} ${c.get(java.util.Calendar.DAY_OF_MONTH)} " +
                "${c.get(java.util.Calendar.HOUR_OF_DAY)} ${c.get(java.util.Calendar.MINUTE)} ${c.get(java.util.Calendar.SECOND)}")
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
            val response = j?.optString("resp")?.takeIf { it.isNotBlank() }
            terminalLines.add(response ?: "⚠ sterownik nie zwrócił odpowiedzi")
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
        if (weather == null) refreshWeather(true) else refreshWeather(false)
    }

    // ─────────────────────────  mapowanie UI (PULPIT → arkusze) ─────────────────────────
    /** `#sysSub` z renderDashboard (Piec.html:4330-4345). */
    fun sysStripSub(): String {
        val source = when {
            isFbFresh && S.online -> "telemetria ESP32 · IP: " + (if (S.hasData("ip")) S.ip else "—")
            connected -> "Firebase połączone · oczekiwanie na rzeczywiste dane centrali"
            lastFetchTs > 0L -> "Ostatnie dane sterownika są nieaktualne"
            else -> "oczekiwanie na połączenie z Firebase"
        }
        return if (S.sym.isNotEmpty()) "$source · symulacja lokalna" else source
    }

    /** kafelki bez pozycji w `MENUS` nie otwierają arkusza (openMenu = () => {}). */
    fun menuForTile(id: String): String? = when (id) {
        "zewn", "bojler", "pokoj", "cisnienie", "wilgotnosc", "ogrz_powrot", "ogrz_trociny", "dym",
        "czujniki", "ogrz", "panel", "pompa", "serwo", "mieszadlo", "czas", "alarmy" -> id
        "powrot" -> "ogrz_powrot"
        "trociny" -> "ogrz_trociny"
        "ogrz_sr" -> "ogrz"
        else -> null
    }

    /** `numInput(...)` → `ustaw <key> <v>` + walidacja zakresu (`NumRow` err). */
    fun commitNum(name: String, key: String, min: Int, max: Int, v: Int) {
        if (v < min || v > max) {
            showToast("Ustawienia", "$name: zakres $min–$max", "err")
            return
        }
        send("ustaw $key $v")
    }

    /** `checkbox(...)` → `ustaw <key> 1|0`; widok zmienia się po rzeczywistym odczycie. */
    fun commitBool(name: String, key: String, v: Boolean) {
        send("ustaw $key ${if (v) 1 else 0}")
    }
}
