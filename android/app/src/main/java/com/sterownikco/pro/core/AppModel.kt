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
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.async
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeoutOrNull
import okhttp3.OkHttpClient
import org.json.JSONArray
import org.json.JSONObject
import okhttp3.RequestBody.Companion.toRequestBody
import java.util.concurrent.TimeUnit

/* ══════════════════════════════════════════════════════════════════════════
   MODEL APLIKACJI — odpowiednik warstwy sterującej Piec.html:
   `pollFirebase` + `sendCommand/ACK` + `refreshWeatherTab` + `loadTelemetryRange`
   + logi + toasty + timery (8 s / 45 s / 15 min / 1 s).
   ══════════════════════════════════════════════════════════════════════════ */

private const val CHART_GROUP_TEMPERATURE = 1
private const val CHART_GROUP_SERVO = 2
private const val CHART_GROUP_PUMP = 4
private const val CHART_GROUP_ALL = 7

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
        .callTimeout(15, TimeUnit.SECONDS)
        .followRedirects(false).followSslRedirects(false).retryOnConnectionFailure(false)
        .build()
    private val statusPollGate = StatusPollGate()
    private enum class StatusPollResult { APPLIED, FAILED, SUPERSEDED }
    private val pumpModeCommandMutex = Mutex()
    private var pumpModeRequestId = 0L
    private val mixerModeCommandMutex = Mutex()
    private var mixerModeRequestId = 0L
    private val smokeModeCommandMutex = Mutex()
    private var smokeModeRequestId = 0L
    private val servoModeCommandMutex = Mutex()
    private var servoModeRequestId = 0L
    private val weatherMutex = Mutex()
    private val terminalCommandMutex = Mutex()

    // ── stan połączenia ─────────────────────────────────────────────────────
    var connected by mutableStateOf(false)
    var lastFetchTs = 0L
    /** ACK-only presentations until the slower `/piec/status` snapshot catches up. */
    private var pumpModeAckOverride by mutableStateOf<Int?>(null)
    private var mixerModeAckOverride by mutableStateOf<Int?>(null)
    private var smokeModeAckOverride by mutableStateOf<Int?>(null)
    private var servoModeAckOverride by mutableStateOf<Int?>(null)
    var dashboardWidgets by mutableStateOf(TileDefs.DEFAULT_VISIBLE_IDS)
    var dashboardWidgetOrder by mutableStateOf(TileDefs.TILES.map { it.id })
    var authOpen by mutableStateOf(false)
    /** Okienko alarmu w aplikacji (gdy system nie odpali FSI na pierwszym planie). */
    var alarmPopup by mutableStateOf<AlarmInfo?>(null)
    private val alarmPopupSession = AlarmPopupSession()

    fun showAlarmPopup(info: AlarmInfo) {
        alarmPopupSession.onAlarmEvent()
        alarmPopup = info
    }

    fun restoreActiveAlarmPopup(info: AlarmInfo) {
        if (alarmPopup == null && alarmPopupSession.shouldRestore(info.sig)) alarmPopup = info
    }

    fun dismissAlarmPopup() {
        alarmPopupSession.dismiss(alarmPopup?.sig)
        alarmPopup = null
    }

    fun clearAlarmPopup() {
        alarmPopupSession.clear()
        alarmPopup = null
    }

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
    fun openSheet(id: String?) {
        sheet = id
        refreshSheet()
        if (id == "terminal") startTerminalDlogPolling() else stopTerminalDlogPolling()
    }

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
    /** Bitmask wyboru grup; temperatura pozostaje domyślnie zaznaczona. */
    var chartGroupMask by mutableIntStateOf(CHART_GROUP_TEMPERATURE)
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
        Rtdb.email = prefs.get(Prefs.K_EMAIL) ?: Prefs.DEFAULT_FB_EMAIL
        Rtdb.cmdToken = prefs.get(Prefs.K_CMD_TOKEN) ?: Prefs.DEFAULT_CMD_TOKEN
        Rtdb.onAuthTokensRefreshed = { idToken, refreshToken ->
            prefs.saveRefreshedTokens(Rtdb.apiKey, idToken, refreshToken)
        }
        val initialSession = prefs.firebaseSession()
        Rtdb.setAuthSession(initialSession.apiKey, initialSession.idToken, initialSession.refreshToken)
        ChartSeries.TEMP.forEach { seriesOn[it.id] = it.on }
        ChartSeries.SERVO.forEach { seriesOn[it.id] = it.on }
        ChartSeries.PUMP.forEach { seriesOn[it.id] = it.on }
        loadChartPrefs()
        loadDashboardWidgets()
        loadDashboardWidgetOrder()
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
        if (sheet == "terminal") startTerminalDlogPolling()
        jobs = listOf(
            scope.launch { while (true) { delay(8000); poll() } },
            scope.launch { while (true) { delay(15L * 60_000); refreshWeather(true) } },
            scope.launch {
                while (true) {
                    delay(1000)
                    if (sheet != null) sheetTick++
                    if (!termCategoryBatchBusy && termOpened && termAutoOffAt > 0L && System.currentTimeMillis() >= termAutoOffAt) {
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
        stopTerminalDlogPolling()
        // W tle NIE odpytujemy — oszczędność baterii; alarmy przejmuje
        // AlarmMonitorService z minutowym pollingiem statusu.
        bgJob?.cancel(); bgJob = null
    }

    private fun recordSolar() {
        val w = weather?.current ?: return
        if (!S.hasAllData("t_panel", "t_bojler", "t_zewn")) return
        // Symulowane wartości z centrali są prawdziwie używane przez jej logikę,
        // ale nie trafiają do analityki jako próbki fizyczne.
        if (S.symAktywna("panel") || S.symAktywna("bojler") || S.symAktywna("zewn")) return
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
    fun poll() = scope.launch { pollAwait() }

    /**
     * Routine refreshes are best-effort and coalesce while a poll is active. After an ACK,
     * the caller must wait for the active poll to finish and then fetch a fresh snapshot;
     * dropping that request leaves every state-driven sheet stale until the next 8 s tick.
     */
    private suspend fun pollAwait(waitForInFlight: Boolean = false): Boolean {
        if (Rtdb.refreshToken.isBlank()) {
            connected = false
            termStateKnown = false
            return false
        }
        val result = statusPollGate.withPoll(skipIfBusy = !waitForInFlight) {
            val first = pollStatusLocked()
            if (waitForInFlight && first == StatusPollResult.SUPERSEDED) {
                // A newer alarm-state event can still race this read; fetch once more against
                // the new revision instead of leaving the post-ACK sheet on its old snapshot.
                pollStatusLocked()
            } else first
        }
        return result == StatusPollResult.APPLIED
    }

    /** Runs only while [statusPollGate] is held. */
    private suspend fun pollStatusLocked(): StatusPollResult {
        if (Rtdb.refreshToken.isBlank()) {
            connected = false
            termStateKnown = false
            return StatusPollResult.FAILED
        }

        val startRevision = AlarmSnapshotCoordinator.captureRevision()
        val d = try {
            Rtdb.pollStatus()
        } catch (e: CancellationException) {
            throw e
        } catch (e: Exception) {
            null
        }
        if (d == null) {
            val applied = AlarmSnapshotCoordinator.applyPollIfCurrent(startRevision) {
                connected = false
                termStateKnown = false
                S.bump()
            }
            return if (applied) StatusPollResult.FAILED else StatusPollResult.SUPERSEDED
        }

        val applied = AlarmSnapshotCoordinator.applyPollIfCurrent(startRevision) {
            connected = true
            lastFetchTs = System.currentTimeMillis()
            S.applyIncomingData(d)
            val smokeFlag = S.dym_alarm.takeIf { S.hasData("dym_alarm") }
            val overheatFlag = S.alarm_ogrzewanie.takeIf { S.hasData("alarm_ogrzewanie") }
            AlarmCenter.applyControllerMuteSnapshot(
                p = prefs,
                status = controllerAlarmMuteStatus(d),
                currentSignature = AlarmMuteCommands.signature(
                    smokeFlag, overheatFlag, prefs.get(AlarmCenter.K_LAST_SIG).orEmpty()
                ),
                completeAlarmFlags = smokeFlag != null && overheatFlag != null,
                nowMs = System.currentTimeMillis()
            )
            S.bump()
            if (S.online) {
                recordSolar()
                if (page == 1) appendLiveFeed()
            }
            // Alarmy oceniaj wyłącznie na podstawie flag zwróconych przez sterownik.
            try {
                if (S.hasAllData("dym_alarm", "alarm_ogrzewanie")) {
                    val info = AlarmCenter.evaluate(prefs, S.dym_alarm, S.alarm_ogrzewanie, S.t_ogrz, S.dym)
                    when {
                        info != null -> {
                            AlarmNotify.fire(ctx, info)
                            showAlarmPopup(info)
                        }
                        !S.dym_alarm && !S.alarm_ogrzewanie -> {
                            AlarmNotify.cancel(ctx)
                            clearAlarmPopup()
                        }
                        AlarmCenter.shouldLoopSound(prefs, S.dym_alarm, S.alarm_ogrzewanie) -> {
                            val currentInfo = AlarmCenter.describeActiveFlags(
                                S.dym_alarm, S.alarm_ogrzewanie, S.t_ogrz, S.dym
                            )
                            if (currentInfo != null) restoreActiveAlarmPopup(currentInfo)
                            if (currentInfo != null && !AlarmNotify.postIfMissing(ctx, currentInfo)) {
                                val kind = AlarmCenter.kindFor(S.dym_alarm, S.alarm_ogrzewanie)
                                AlarmMonitorService.startAlarmLoop(ctx, AlarmCenter.sound(prefs, kind))
                            }
                        }
                        else -> {
                            AlarmNotify.cancel(ctx)
                            clearAlarmPopup()
                            AlarmMonitorService.stopAlarmLoop()
                        }
                    }
                }
            } catch (e: Exception) { /* alarm nie może wywalić poll() */ }
            // Widget na pulpit (throttling w środku).
            try { PiecWidget.push(ctx, d) } catch (e: Exception) { /* ignore */ }
        }
        return if (applied) StatusPollResult.APPLIED else StatusPollResult.SUPERSEDED
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
            val idToken = j.getString("idToken")
            val refreshToken = j.optString("refreshToken")
            Rtdb.setAuthSession(apiKey, idToken, refreshToken)
            prefs.saveFirebaseSession(apiKey, idToken, refreshToken)
            Rtdb.email = emailIn
            Rtdb.cmdToken = cmdToken
            prefs.set(Prefs.K_EMAIL, emailIn)
            prefs.set(Prefs.K_CMD_TOKEN, cmdToken)
            prefs.setBool(Prefs.K_REMEMBER_CREDS, remember)
            val passwordSaved = if (remember) prefs.setSecret(Prefs.K_PASS, password)
                else { prefs.remove(Prefs.K_PASS); true }
            if (!passwordSaved) prefs.remove(Prefs.K_PASS)
            if (!passwordSaved) showToast("SESJA", "Zalogowano, ale nie udało się bezpiecznie zapisać hasła.", "err")
            else if (!automatic) showToast("FIREBASE", "Logowanie zakończone — pobieram /piec/status", "ok")
            addLog("SYSTEM", "Zalogowano operatora", "info")
            authOpen = false
            authBusy = false
            authStatusKind = if (passwordSaved) "ok" else "warn"
            authStatus = if (passwordSaved) "Zalogowano — $emailIn"
                else "Zalogowano, ale hasło nie mogło zostać bezpiecznie zapamiętane."
            // Uruchom czuwanie i poproś o zgodę powiadomień tylko przy włączonym monitoringu.
            if (AlarmCenter.isMonitored(prefs)) {
                try { AlarmMonitorService.start(ctx) } catch (e: Exception) { /* ignore */ }
                if (!AlarmNotify.hasPostNotifications(ctx)) askNotifPerm = true
            }
            poll()
            if (sheet == "terminal") startTerminalDlogPolling()
            if (page == 1) loadRange(rangeSec)
        }.onFailure { e ->
            authBusy = false; authStatusKind = "err"; authStatus = e.message ?: "Logowanie nieudane"
            if (automatic) authOpen = true
            else showToast("BŁĄD", e.message ?: "Logowanie nieudane", "err")
        }
    }

    // (Tryb symulacji/DEMO usuniety z APK — tylko w Piec.html.)

    fun logout() {
        stopTerminalDlogPolling()
        // Replace the in-memory session before deleting persisted credentials so a racing refresh cannot restore them.
        Rtdb.setAuthTokens("", "")
        // Persist the DE logout tombstone before touching any other credential storage.
        prefs.clearFirebaseTokens()
        prefs.clearRememberedCredentials()
        try { AlarmMonitorService.stop(ctx) } catch (e: Exception) { /* ignore */ }
        try { PiecWidget.clear(ctx) } catch (e: Exception) { /* ignore */ }
        clearAlarmPopup()
        connected = false
        termStateKnown = false
        S.bump()
        showToast("SESYJA", "Wylogowano operatora", "ok")
        addLog("SYSTEM", "Zamknięto sesję operatora", "warn")
        authOpen = true
    }

    // ── tryby sterownika: szybki widok po ACK + uzgodnienie ze statusem ──
    internal fun pumpModePresentation(): ModeSelectionPresentation = presentModeSelection(
        acknowledgedMode = pumpModeAckOverride,
        reportedMode = S.wybor.takeIf { S.hasData("wybor") },
        allowedModes = 1..3
    )

    internal fun smokeModePresentation(): ModeSelectionPresentation = presentModeSelection(
        acknowledgedMode = smokeModeAckOverride,
        reportedMode = S.dymTrybPracy.takeIf { S.hasData("dymTrybPracy") },
        allowedModes = 0..1
    )

    internal fun mixerModePresentation(): ModeSelectionPresentation = presentMixerModeSelection(
        acknowledgedMode = mixerModeAckOverride,
        reportedOverrideMinutes = S.mieszadlo_override_min.takeIf { S.hasData("mieszadlo_override_min") },
        reportedMixerOn = S.mieszadlo.takeIf { S.hasData("mieszadlo") }
    )

    private fun updatePumpModeAckOverride(mode: Int?) {
        val valid = mode?.takeIf { it in 1..3 }
        if (pumpModeAckOverride == valid) return
        pumpModeAckOverride = valid
        S.bump()
    }

    private fun updateSmokeModeAckOverride(mode: Int?) {
        val valid = mode?.takeIf { it in 0..1 }
        if (smokeModeAckOverride == valid) return
        smokeModeAckOverride = valid
        S.bump()
    }

    private fun updateMixerModeAckOverride(mode: Int?) {
        val valid = mode?.takeIf { it in 0..2 }
        if (mixerModeAckOverride == valid) return
        mixerModeAckOverride = valid
        S.bump()
    }

    /**
     * Updates only presentation after ACK; `PiecState` stays authoritative.
     * Clear the override when a fresh snapshot agrees, or revert to reported data
     * after 30 seconds rather than presenting an unconfirmed mode indefinitely.
     */
    private suspend fun awaitModeStatus(
        label: String,
        mode: Int,
        requestId: Long,
        currentRequestId: () -> Long,
        acknowledgedMode: () -> Int?,
        reportedMode: () -> Int?,
        allowedModes: IntRange,
        clearOverride: () -> Unit
    ) {
        fun awaitingStatus(): Boolean = presentModeSelection(
            acknowledgedMode = acknowledgedMode(),
            reportedMode = reportedMode(),
            allowedModes = allowedModes
        ).awaitingStatus

        if (!awaitingStatus()) {
            clearOverride()
            return
        }
        showToast(label, "Sterownik przyjął tryb — oczekuję na świeży odczyt.", "wait")
        val deadline = System.currentTimeMillis() + 30_000L
        val coroutineContext = currentCoroutineContext()
        while (coroutineContext.isActive && requestId == currentRequestId() && System.currentTimeMillis() < deadline) {
            if (!awaitingStatus()) {
                clearOverride()
                showToast(label, "Status centrali potwierdził nowy tryb.", "ok")
                return
            }
            delay(500L)
        }
        if (requestId == currentRequestId() && acknowledgedMode() == mode) {
            clearOverride()
            showToast(label, "ACK odebrany, ale świeży status nie potwierdził trybu po 30 s. Pokazuję ostatni odczyt centrali.", "warn")
        }
    }

    /** Zmiana strategii pompy jest podświetlana po ACK, bez czekania na wolniejszy status RTDB. */
    fun setPumpMode(mode: Int) {
        if (mode !in 1..3) return
        val requestId = ++pumpModeRequestId
        updatePumpModeAckOverride(null)
        scope.launch {
            val acknowledged = try {
                pumpModeCommandMutex.lock()
                try {
                    if (requestId != pumpModeRequestId) false
                    else sendAwait("ustaw wybor $mode") {
                        if (requestId == pumpModeRequestId) updatePumpModeAckOverride(mode)
                    }
                } finally {
                    pumpModeCommandMutex.unlock()
                }
            } catch (e: CancellationException) {
                throw e
            }
            if (!acknowledged || requestId != pumpModeRequestId) return@launch
            awaitModeStatus(
                label = "POMPA",
                mode = mode,
                requestId = requestId,
                currentRequestId = { pumpModeRequestId },
                acknowledgedMode = { pumpModeAckOverride },
                reportedMode = { S.wybor.takeIf { S.hasData("wybor") } },
                allowedModes = 1..3,
                clearOverride = { updatePumpModeAckOverride(null) }
            )
        }
    }

    /**
     * Tryb mieszadła: 0 = Auto, 1 = wymuś ON, 2 = wymuś OFF. ACK zmienia
     * tylko chwilową prezentację; stan centrali nadal rozstrzyga status RTDB.
     */
    fun setMixerMode(mode: Int) {
        val command = when (mode) {
            0 -> "mieszadlo_auto"
            1 -> "mieszadlo_wl"
            2 -> "mieszadlo_wyl"
            else -> return
        }
        val requestId = ++mixerModeRequestId
        updateMixerModeAckOverride(null)
        scope.launch {
            val acknowledged = try {
                mixerModeCommandMutex.lock()
                try {
                    if (requestId != mixerModeRequestId) false
                    else sendAwait(command) {
                        if (requestId == mixerModeRequestId) updateMixerModeAckOverride(mode)
                    }
                } finally {
                    mixerModeCommandMutex.unlock()
                }
            } catch (e: CancellationException) {
                throw e
            }
            if (!acknowledged || requestId != mixerModeRequestId) return@launch
            awaitModeStatus(
                label = "MIESZADŁO",
                mode = mode,
                requestId = requestId,
                currentRequestId = { mixerModeRequestId },
                acknowledgedMode = { mixerModeAckOverride },
                reportedMode = {
                    presentMixerModeSelection(
                        acknowledgedMode = null,
                        reportedOverrideMinutes = S.mieszadlo_override_min.takeIf { S.hasData("mieszadlo_override_min") },
                        reportedMixerOn = S.mieszadlo.takeIf { S.hasData("mieszadlo") }
                    ).mode
                },
                allowedModes = 0..2,
                clearOverride = { updateMixerModeAckOverride(null) }
            )
        }
    }

    /** Analogiczna synchronizacja wyboru trybu pracy czujnika dymu. */
    fun setSmokeMode(mode: Int) {
        if (mode !in 0..1) return
        val requestId = ++smokeModeRequestId
        updateSmokeModeAckOverride(null)
        scope.launch {
            val acknowledged = try {
                smokeModeCommandMutex.lock()
                try {
                    if (requestId != smokeModeRequestId) false
                    else sendAwait("ustaw dymTrybPracy $mode") {
                        if (requestId == smokeModeRequestId) updateSmokeModeAckOverride(mode)
                    }
                } finally {
                    smokeModeCommandMutex.unlock()
                }
            } catch (e: CancellationException) {
                throw e
            }
            if (!acknowledged || requestId != smokeModeRequestId) return@launch
            awaitModeStatus(
                label = "CZUJNIK DYMU",
                mode = mode,
                requestId = requestId,
                currentRequestId = { smokeModeRequestId },
                acknowledgedMode = { smokeModeAckOverride },
                reportedMode = { S.dymTrybPracy.takeIf { S.hasData("dymTrybPracy") } },
                allowedModes = 0..1,
                clearOverride = { updateSmokeModeAckOverride(null) }
            )
        }
    }

    // ── tryb serwa: szybki widok po ACK + późniejsze uzgodnienie ze statusem ──
    internal fun servoModePresentation(): ServoModePresentation = presentServoMode(
        acknowledgedMode = servoModeAckOverride,
        reportedMode = S.tryb_serwa.takeIf { S.hasData("tryb_serwa") }
    )

    private fun updateServoModeAckOverride(mode: Int?) {
        val valid = mode?.takeIf { it in 1..3 }
        if (servoModeAckOverride == valid) return
        servoModeAckOverride = valid
        // Dashboard tiles and the open sheet subscribe to this revision as well.
        S.bump()
    }

    /**
     * After a positive ESP32 ACK, show the selected mode and manual controls immediately.
     * Firebase status is sent on a slower cadence than command ACKs, so keep this
     * presentation override only until the reported mode catches up (or 30 s elapse).
     */
    fun setServoMode(mode: Int) {
        if (mode !in 1..3) return
        val requestId = ++servoModeRequestId
        updateServoModeAckOverride(null)
        scope.launch {
            servoModeCommandMutex.lock()
            val acknowledged = try {
                if (requestId != servoModeRequestId) false
                else sendAwait("ustaw trybSerwa $mode") {
                    // Apply the UI-only ACK presentation before the post-ACK RTDB refresh,
                    // which can be delayed by a routine poll or the controller's 15 s status cadence.
                    if (requestId == servoModeRequestId) {
                        updateServoModeAckOverride(mode)
                        if (servoModePresentation().awaitingStatus) {
                            showToast("SERWO", "Sterownik przyjął tryb — oczekuję na świeży odczyt.", "wait")
                        }
                    }
                }
            } finally {
                servoModeCommandMutex.unlock()
            }
            if (!acknowledged || requestId != servoModeRequestId) return@launch

            if (!servoModePresentation().awaitingStatus) {
                updateServoModeAckOverride(null)
                return@launch
            }

            showToast("SERWO", "Sterownik przyjął tryb — oczekuję na świeży odczyt.", "wait")
            val deadline = System.currentTimeMillis() + 30_000L
            while (isActive && requestId == servoModeRequestId && System.currentTimeMillis() < deadline) {
                if (!servoModePresentation().awaitingStatus) {
                    updateServoModeAckOverride(null)
                    showToast("SERWO", "Status centrali potwierdził nowy tryb.", "ok")
                    return@launch
                }
                // Routine RTDB polling continues every 8 s; no extra status requests are
                // generated here. This waiter only reacts as soon as one of those snapshots
                // reaches the Compose UI.
                delay(500L)
            }
            if (requestId == servoModeRequestId && servoModeAckOverride == mode) {
                updateServoModeAckOverride(null)
                showToast(
                    "SERWO",
                    "ACK odebrany, ale świeży status nie potwierdził trybu po 30 s. Pokazuję ostatni odczyt centrali.",
                    "warn"
                )
            }
        }
    }

    // ── polecenia; stan urządzenia zmienia się dopiero po ACK / odczycie ─────
    fun send(cmd: String) = scope.launch { sendAwait(cmd) }

    /**
     * Sends a command without mutating reported device state. Optional [onAck] is for a
     * transient UI acknowledgement only; the fresh RTDB snapshot remains authoritative.
     */
    private suspend fun sendAwait(cmd: String, onAck: (() -> Unit)? = null): Boolean {
        val command = cmd.trim()
        if (command.isEmpty()) return false
        val sensitive = CommandRedactor.isSensitive(command)
        val commandLabel = CommandRedactor.forDisplay(command)
        if (!connected) {
            showToast(commandLabel, "Brak połączenia z Firebase — polecenia nie wysłano", "err")
            addLog("CMD_ERR", "Nie wysłano polecenia — brak połączenia z Firebase", "warn")
            return false
        }

        val t0 = System.nanoTime()
        showToast(commandLabel, "Wysyłanie do sterownika…", "wait")
        addLog("CMD", "Wysyłanie: $commandLabel", "cmd")
        val cmdId = Rtdb.makeFireCmdId()
        return try {
            val ack = Rtdb.sendCommand(cmdId, command)
            val latency = ((System.nanoTime() - t0) / 1_000_000).toInt()
            when {
                ack == null -> {
                    showToast(commandLabel, "Brak potwierdzenia sterownika — stan nie został zmieniony lokalnie", "warn")
                    addLog("ACK_WARN", "Brak potwierdzenia sterownika", "warn")
                    poll()
                    false
                }
                ack.ok -> {
                    showToast(commandLabel, "Potwierdzone przez sterownik (ACK, ${latency}ms)", "ok")
                    addLog("ACK", "ACK OK (${latency}ms): $commandLabel", "ack")
                    buzz()
                    onAck?.invoke()
                    // Dociągnij świeży snapshot po ACK. Wymuś oczekiwanie na ewentualny
                    // cykliczny poll, zamiast gubić odświeżenie w jego trakcie.
                    pollAwait(waitForInFlight = true)
                    true
                }
                else -> {
                    val errorText = if (sensitive) "Sterownik odrzucił konfigurację Telegrama."
                        else "Sterownik odrzucił polecenie: ${ack.error}"
                    showToast(commandLabel, errorText, "err")
                    addLog("NACK", if (sensitive) "Odrzucono poufną konfigurację Telegrama"
                        else "Odrzucone (${ack.error}): $commandLabel", "err")
                    false
                }
            }
        } catch (e: CancellationException) {
            throw e
        } catch (e: Exception) {
            val errorText = if (sensitive) "Błąd wysyłki poufnego polecenia"
                else "Błąd wysyłki: ${e.message ?: "brak"}"
            showToast(commandLabel, errorText, "err")
            addLog("CMD_ERR", errorText, "err")
            false
        }
    }

    // ── pogoda ──────────────────────────────────────────────────────────────
    // ── pogoda (TYLKO prawdziwe Open-Meteo — zero symulacji) ────────────────
    fun refreshWeather(force: Boolean, announce: Boolean = false) = scope.launch {
        weatherMutex.withLock {
            // Init + onResume can request the initial forecast back-to-back; coalesce the duplicate.
            if (!force && weather != null) return@withLock
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
        // [v0.31.33 ESP-HISTORY] Równolegle pytamy lokalne archiwum centrali.
        // Chmura bywa niepełna (przerwy wysyłki), a centrala ma te próbki u siebie.
        val espIp = S.ip.trim()
        val espTask = if (espIp.isNotEmpty()) async {
            try { Rtdb.fetchEspHistory(espIp) } catch (e: Exception) { null }
        } else null
        var fb: List<TelemPoint>? = null
        if (connected) {
            try { fb = Rtdb.fetchTelemetry(sec) } catch (e: Exception) { /* fallback poniżej */ }
        }
        // 6 s sufitu: poza domową siecią centrala jest nieosiągalna, a wykresy
        // z chmury mają się pokazać od razu (nie po 18 s timeoutów TCP).
        val esp = espTask?.let {
            try { withTimeoutOrNull(6_000) { it.await() } } catch (e: Exception) { null }
        }
        // Zakres ESP (~24–38 h) ma sens tylko dla 6h/24h; dla 7/30 dni dane
        // z centrali i tak są podzbiorem chmury, więc ich nie mieszamy.
        val useEsp = esp != null && sec <= 24 * 3600 && esp.isNotEmpty()
        if ((fb != null && fb.isNotEmpty()) || useEsp) {
            val merged = combineCloudAndEsp(fb ?: emptyList(), if (useEsp) esp!! else emptyList())
            chartLive = emptyList()
            telemetry = merged
            chartPoints = merged.size
            val fromEspOnly = if (useEsp) merged.count { it.seq < 0L } else 0
            chartStatus = buildString {
                append("Załadowano ${merged.size} próbek")
                when {
                    fromEspOnly > 0 -> append(" · z tego $fromEspOnly tylko z archiwum centrali (brak w chmurze)")
                    useEsp -> append(" · chmura + archiwum centrali zgodne")
                    else -> append(" z RTDB")
                }
                append(" · odcinki symulowane są kreskowane")
            }
            chartReality = when {
                fb == null || fb.isEmpty() -> "ARCHIWUM ESP"
                fromEspOnly > 0 -> "RTDB + ARCHIWUM ESP"
                else -> "FIREBASE RTDB"
            }
            // Historia może kończyć się przed ostatnim odczytem statusu (np. po rozpaleniu).
            // Dołącz świeży pomiar ESP32 od razu, bez czekania na kolejny polling.
            appendLiveFeed()
            return@launch
        }
        telemetry = emptyList()
        chartPoints = 0
        chartStatus = if (connected) "Brak próbek telemetrii dla wybranego zakresu" else "Zaloguj się do Firebase, aby pobrać historię telemetrii"
        chartReality = if (connected) "BRAK DANYCH" else "BRAK SESJI"
        chartLive = emptyList()
    }

    /**
     * [v0.31.33 ESP-HISTORY] Łączy chmurę z lokalnym archiwum centrali.
     * Klucz to znacznik czasu — ta sama próbka z obu źródeł nie dubluje się.
     * Próbki obecne WYŁĄCZNIE w centrali dostają ujemny `seq`, żeby dało się je
     * policzyć i pokazać użytkownikowi, ile danych nie dotarło do chmury.
     */
    private fun combineCloudAndEsp(cloud: List<TelemPoint>, esp: List<TelemPoint>): List<TelemPoint> {
        if (esp.isEmpty()) return cloud
        val byTs = LinkedHashMap<Long, TelemPoint>()
        cloud.forEach { byTs[it.ts] = it }
        var missing = 0L
        esp.forEach { p ->
            if (byTs.containsKey(p.ts)) return@forEach
            byTs[p.ts] = p.copy(seq = -(++missing))
        }
        return byTs.values.sortedBy { it.ts }
    }

    /** Dokleja wyłącznie odczyt z właśnie zakończonego, udanego pollingu Firebase. */
    fun appendLiveFeed() {
        if (page != 1 || !isFbFresh || !S.online || lastFetchTs <= 0L) return
        if (chartLive.lastOrNull()?.ts == lastFetchTs) return
        var simMask = 0L
        fun value(key: String, simKey: String, channel: Int, v: Double): Double? {
            val value = v.takeIf { S.hasData(key) && it.isFinite() } ?: return null
            if (S.symAktywna(simKey)) simMask = simMask or (1L shl channel)
            return value
        }
        fun servoValue(key: String, simKey: String, channel: Int, degrees: Int, maxDegrees: Int): Double? {
            if (!S.hasData(key) || degrees !in 0..maxDegrees) return null
            if (S.symAktywna(simKey)) simMask = simMask or (1L shl channel)
            return degrees * 100.0 / maxDegrees
        }
        val p = TelemPoint(
            ts = lastFetchTs, seq = chartLive.size.toLong() + 1,
            t_zewn = value("t_zewn", "zewn", 0, S.t_zewn),
            t_bojler = value("t_bojler", "bojler", 1, S.t_bojler),
            t_ogrz = value("t_ogrz", "ogrz", 2, S.t_ogrz),
            t_ogrz_sr = value("t_ogrz_sr", "ogrz", 3, S.t_ogrz_sr),
            t_powrot = value("t_powrot", "ogrz_powrot", 4, S.t_powrot),
            t_panel = value("t_panel", "panel", 5, S.t_panel),
            t_pokoj = value("t_pokoj", "pokoj", 6, S.t_pokoj),
            t_trociny = value("t_trociny", "ogrz_trociny", 7, S.t_trociny),
            wilgotnosc = value("wilgotnosc", "wilgotnosc", 8, S.wilgotnosc),
            cisnienie = value("cisnienie", "cisnienie", 9, S.cisnienie),
            dym = value("dym", "dym", 10, S.dym),
            klapa = servoValue("klapa", "klapa", 11, S.klapa, 180),
            syberka = servoValue("syberka", "syberka", 12, S.syberka, 90),
            sim = simMask,
            isLive = true,
            pumpOn = S.pompa.takeIf { S.hasData("pompa") }
        )
        if (p.hasValues()) chartLive = (chartLive + p).takeLast(240)
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
    /** Historia + próbki live w kolejności czasu, jak w wykresie WWW. */
    fun chartDataset(): List<TelemPoint> = mergeChartPoints(telemetry, chartLive)

    private fun chartGroupBit(group: String): Int = when (group) {
        "temp" -> CHART_GROUP_TEMPERATURE
        "servo" -> CHART_GROUP_SERVO
        "pump" -> CHART_GROUP_PUMP
        else -> 0
    }

    fun chartGroupSelected(group: String): Boolean {
        val bit = chartGroupBit(group)
        return bit != 0 && (chartGroupMask and bit) != 0
    }

    /** Grupy są niezależnymi filtrami; zostawiamy przynajmniej jedną zaznaczoną. */
    fun toggleChartGroup(group: String) {
        val bit = chartGroupBit(group)
        if (bit == 0) return
        val next = chartGroupMask xor bit
        if (next == 0) return
        chartGroupMask = next
        saveChartPrefs()
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
    fun chartTitle(): String {
        val groups = mutableListOf<String>()
        if (chartGroupSelected("temp")) groups += "TEMPERATURY"
        if (chartGroupSelected("servo")) groups += "SERWA"
        if (chartGroupSelected("pump")) groups += "POMPA C.O."
        return groups.joinToString(" + ").ifBlank { "WYKRES" }
    }

    fun chartMeta(): String {
        val hasTemp = chartGroupSelected("temp")
        val hasServo = chartGroupSelected("servo")
        val hasPump = chartGroupSelected("pump")
        return when {
            hasTemp && hasServo && hasPump -> "°C + % · POMPA: PRACA / STOP"
            hasTemp && hasServo -> "°C + % · KLAPA + SYBEREK"
            hasTemp && hasPump -> "°C · POMPA: PRACA / STOP"
            hasServo && hasPump -> "% · POMPA: PRACA / STOP"
            hasTemp -> "°C · WSPÓLNA OŚ"
            hasServo -> "% · KLAPA + SYBEREK"
            else -> "STAN · PRACA / STOP"
        }
    }

    fun fsTitle(): String = chartTitle() + " — PEŁNY EKRAN"
    fun fsSubtitle(): String = if (rangeSec <= 24 * 3600) "Podgląd dobowy telemetryczny" else "Analiza wielodniowa"

    /** Bieżący odczyt odpowiada grupie pompy, jeśli jest włączona; brak danych to stan nieznany. */
    fun chartNowStat(): String = when {
        chartGroupSelected("pump") -> when {
            !isFbFresh || !S.hasData("pompa") -> "pompa —"
            S.pompa -> "POMPA: PRACUJE"
            else -> "POMPA: STOI"
        }
        chartGroupSelected("temp") -> if (S.symAktywna("ogrz")) "piec ~${fmt1(S.t_ogrz)}°C · SYM"
            else if (S.hasData("t_ogrz")) "piec " + fmt1(S.t_ogrz) + "°C" else "piec —"
        else -> if (S.hasData("klapa")) "klapa " + Math.round(S.klapa / 1.8) + "%" else "klapa —"
    }

    fun activeCatalog(): List<SeriesDef> = buildList {
        if (chartGroupSelected("temp")) addAll(ChartSeries.TEMP)
        if (chartGroupSelected("servo")) addAll(ChartSeries.SERVO)
        if (chartGroupSelected("pump")) addAll(ChartSeries.PUMP)
    }

    fun saveChartPrefs() {
        try {
            val o = JSONObject()
            o.put("groupMask", chartGroupMask).put("range", rangeSec)
            val t = JSONObject(); ChartSeries.TEMP.forEach { t.put(it.id, seriesOn[it.id] ?: it.on) }
            val s = JSONObject(); ChartSeries.SERVO.forEach { s.put(it.id, seriesOn[it.id] ?: it.on) }
            val p = JSONObject(); ChartSeries.PUMP.forEach { p.put(it.id, seriesOn[it.id] ?: it.on) }
            o.put("tempOn", t).put("servoOn", s).put("pumpOn", p)
            prefs.set(Prefs.K_CHART_PREFS, o.toString())
        } catch (e: Exception) { /* ignore */ }
    }

    private fun loadChartPrefs() {
        try {
            val raw = prefs.get(Prefs.K_CHART_PREFS) ?: return
            val p = JSONObject(raw)
            if (p.has("groupMask")) {
                val savedMask = p.optInt("groupMask", CHART_GROUP_TEMPERATURE) and CHART_GROUP_ALL
                chartGroupMask = if (savedMask != 0) savedMask else CHART_GROUP_TEMPERATURE
            } else if (p.has("focus")) {
                // Migracja starego trybu wyłącznego: indeks 2 był korelacją temperatur i serw.
                chartGroupMask = when (p.optInt("focus", 0)) {
                    1 -> CHART_GROUP_SERVO
                    2 -> CHART_GROUP_TEMPERATURE + CHART_GROUP_SERVO
                    3 -> CHART_GROUP_PUMP
                    else -> CHART_GROUP_TEMPERATURE
                }
            }
            if (p.has("range")) { val r = p.getInt("range"); if (r > 0) rangeSec = r }
            p.optJSONObject("tempOn")?.let { o -> for (k in o.keys()) seriesOn[k] = o.getBoolean(k) }
            p.optJSONObject("servoOn")?.let { o -> for (k in o.keys()) seriesOn[k] = o.getBoolean(k) }
            p.optJSONObject("pumpOn")?.let { o -> for (k in o.keys()) seriesOn[k] = o.getBoolean(k) }
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

    private fun loadDashboardWidgetOrder() {
        val canonical = TileDefs.TILES.map { it.id }
        val raw = prefs.get(Prefs.K_DASH_WIDGET_ORDER)
        if (raw == null) {
            dashboardWidgetOrder = canonical
            return
        }
        try {
            val allowed = canonical.toSet()
            val stored = JSONArray(raw)
            val order = linkedSetOf<String>()
            for (i in 0 until stored.length()) {
                stored.optString(i).takeIf { it in allowed }?.let(order::add)
            }
            canonical.forEach(order::add)
            dashboardWidgetOrder = order.toList()
        } catch (e: Exception) {
            dashboardWidgetOrder = canonical
        }
    }

    fun setDashboardWidgetEnabled(id: String, enabled: Boolean) {
        if (TileDefs.TILES.none { it.id == id }) return
        val next = dashboardWidgets.toMutableSet()
        if (enabled) next.add(id) else next.remove(id)
        dashboardWidgets = next
        prefs.set(Prefs.K_DASH_WIDGETS, JSONArray(next.toList()).toString())
    }

    fun moveDashboardWidget(draggedId: String, targetId: String) {
        val next = reorderTileWithinSection(dashboardWidgetOrder, TileDefs.TILES, draggedId, targetId)
        if (next == dashboardWidgetOrder) return
        dashboardWidgetOrder = next
        prefs.set(Prefs.K_DASH_WIDGET_ORDER, JSONArray(next).toString())
    }

    fun resetDashboardWidgets() {
        dashboardWidgets = TileDefs.DEFAULT_VISIBLE_IDS
        dashboardWidgetOrder = TileDefs.TILES.map { it.id }
        prefs.set(Prefs.K_DASH_WIDGETS, JSONArray(dashboardWidgets.toList()).toString())
        prefs.set(Prefs.K_DASH_WIDGET_ORDER, JSONArray(dashboardWidgetOrder).toString())
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
        tgToken = prefs.getSecret(Prefs.K_TG_TOKEN) ?: ""
        tgChatId = prefs.getSecret(Prefs.K_TG_CHAT) ?: ""
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
        val res = try {
            val (code, body) = Rtdb.postLocalController(S.ip, "/api/telegram/save", payload.toString())
            if (code == -2) false to "Adres centrali nie jest prywatnym adresem IPv4 — połączenie zablokowane."
            else {
                val response = try { JSONObject(body) } catch (e: Exception) { JSONObject() }
                when {
                    code in 200..299 && response.optBoolean("ok") -> true to okMsg
                    code < 0 -> false to "Nie udało się połączyć z centralą."
                    else -> false to "Błąd zapisu konfiguracji Telegrama (HTTP $code)."
                }
            }
        } catch (e: Exception) {
            false to "Nie udało się połączyć z centralą."
        }
        showToast("Telegram", res.second, if (res.first) "ok" else "err")
        if (res.first) {
            val tokenSaved = prefs.setSecret(Prefs.K_TG_TOKEN, payload.optString("token", tgToken))
            val chatSaved = prefs.setSecret(Prefs.K_TG_CHAT, payload.optString("chatId", tgChatId))
            if (!tokenSaved) prefs.remove(Prefs.K_TG_TOKEN)
            if (!chatSaved) prefs.remove(Prefs.K_TG_CHAT)
            if (!tokenSaved || !chatSaved) showToast(
                "Telegram", "Ustawienia zapisano w centrali, ale nie udało się zaszyfrować kopii lokalnej.", "err"
            )
        }
    }

    fun tgSendTest() {
        if (connected) {
            send("tg_test")
            return
        }
        scope.launch {
            val res = try {
                val (code, body) = Rtdb.postLocalController(S.ip, "/api/telegram/send", "")
                if (code == -2) false to "Adres centrali nie jest prywatnym adresem IPv4 — połączenie zablokowane."
                else {
                    val response = try { JSONObject(body) } catch (e: Exception) { JSONObject() }
                    when {
                        code in 200..299 && response.optBoolean("ok") -> true to "Raport wysłany — sprawdź czat operatora"
                        code < 0 -> false to "Nie udało się połączyć z centralą."
                        else -> false to "Nie wysłano raportu (HTTP $code)."
                    }
                }
            } catch (e: Exception) {
                false to "Nie udało się połączyć z centralą."
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
        val localUrlValid = Rtdb.localControllerUrl(ip, "/api/wifi/list") != null
        try {
            val arr = try { if (!localUrlValid) null else Rtdb.wifiList(ip) } catch (e: Exception) { null }
            wifiSavedNote = when {
                ip.isEmpty() -> "Brak adresu IP centrali — nie mogę pobrać listy zapisanych sieci."
                !localUrlValid -> "Nieprawidłowy adres centrali. Dozwolone jest tylko prywatne IPv4 (10/8, 172.16/12, 192.168/16)."
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
        if (Rtdb.localControllerUrl(ip, "/api/wifi/scan/start") == null) {
            wifiBusy = false; wifiScanStatus = "Błędny adres centrali"
            wifiScanNote = "Żądanie zablokowano: centrala musi mieć prywatny adres IPv4 (10/8, 172.16/12, 192.168/16)."
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
        if (Rtdb.localControllerUrl(ipA, "/api/wifi/add") == null) {
            showToast("WiFi", "Nieprawidłowy adres centrali — sieć NIE została wysłana", "err")
            return@launch
        }
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
        if (Rtdb.localControllerUrl(ipD, "/api/wifi/delete") == null) {
            showToast("WiFi", "Nieprawidłowy adres centrali — operacja zablokowana", "err")
            return@launch
        }
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
    var termCategoryBatchBusy by mutableStateOf(false)
    var termAutoOffAt by mutableLongStateOf(0L)
    var termLevel by mutableStateOf("INFO")
    var termStatus by mutableStateOf("Brak odebranych wpisów DLOG. Oczekiwanie na rzeczywiste dane sterownika.")
    var termStatusKind by mutableStateOf("")
    val termCats = mutableStateMapOf<String, Boolean>()
    private var terminalDlogJob: Job? = null
    private var terminalDlogUid = ""
    private var terminalDlogRoot = ""
    private var terminalDlogSession = ""
    private var terminalDlogCursor = -1

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

    fun terminalLogFileName(): String {
        val stamp = java.text.SimpleDateFormat("yyyyMMdd-HHmmss", java.util.Locale.US)
            .format(java.util.Date())
        return "Sterownik-CO-DLOG-$stamp.txt"
    }

    /** Zapisuje odebrane, rzeczywiste wiersze z bufora przez systemowy selektor pliku Androida. */
    fun exportTerminalLogs(uri: android.net.Uri) = scope.launch {
        val lines = terminalRaw.toList()
        if (lines.isEmpty()) {
            termStatus = "Brak odebranych wpisów DLOG do zapisania."
            termStatusKind = "warn"
            return@launch
        }
        val savedAt = java.text.SimpleDateFormat("yyyy-MM-dd HH:mm:ss", java.util.Locale.getDefault())
            .format(java.util.Date())
        val text = buildString {
            append("# Sterownik CO — odebrane wpisy DLOG\n")
            append("# Zapisano: ").append(savedAt).append('\n')
            if (terminalDlogSession.isNotBlank()) append("# Sesja: ").append(terminalDlogSession).append('\n')
            append("# Linie w zapisanym buforze: ").append(lines.size)
                .append("; odebrane chunki: ").append(termChunks)
                .append("; bajty sesji: ").append(termBytes)
                .append("; luki sekwencji: ").append(termGaps).append('\n')
            append("# Eksport zawiera wszystkie odebrane kategorie, także ukryte filtrami widoku.\n")
            append(lines.joinToString("\n"))
            append('\n')
        }
        try {
            withContext(kotlinx.coroutines.Dispatchers.IO) {
                val output = ctx.contentResolver.openOutputStream(uri)
                    ?: throw java.io.IOException("Android nie otworzył pliku do zapisu")
                output.use { it.write(text.toByteArray(Charsets.UTF_8)) }
            }
            termStatus = "Zapisano ${lines.size} odebranych wierszy DLOG do pliku."
            termStatusKind = "ok"
            showToast("DLOG", "Zapisano ${lines.size} wierszy", "ok")
        } catch (e: CancellationException) {
            throw e
        } catch (e: Exception) {
            termStatus = "Nie udało się zapisać DLOG: ${e.message ?: "błąd pliku"}"
            termStatusKind = "err"
            showToast("DLOG", "Zapis pliku nie powiódł się", "err")
        }
    }

    /** Odczyt prawdziwych chunków DLOG tylko wtedy, gdy arkusz terminala jest otwarty. */
    private fun startTerminalDlogPolling() {
        if (terminalDlogJob?.isActive == true) return
        terminalDlogJob = scope.launch {
            termStatus = "Łączenie z rzeczywistymi wpisami DLOG z Firebase…"
            termStatusKind = ""
            while (isActive && sheet == "terminal") {
                try {
                    terminalPollDlogOnce()
                } catch (e: CancellationException) {
                    throw e
                } catch (e: Exception) {
                    termStatus = "Błąd odbioru DLOG: ${e.message ?: "błąd odczytu"}"
                    termStatusKind = "warn"
                }
                delay(5_000)
            }
        }
    }

    private fun stopTerminalDlogPolling() {
        terminalDlogJob?.cancel()
        terminalDlogJob = null
    }

    private suspend fun terminalPollDlogOnce() {
        val uid = Rtdb.currentFirebaseUid()
        if (uid == null) {
            termStatus = "Zaloguj się do Firebase, aby odbierać DLOG."
            termStatusKind = "warn"
            return
        }
        if (uid != terminalDlogUid) {
            terminalDlogUid = uid
            terminalDlogRoot = ""
            terminalDlogSession = ""
            terminalDlogCursor = -1
            terminalResetDlogBuffer()
        }

        // Aktualny firmware zapisuje do prywatnej gałęzi urządzenia; drugi wpis
        // zachowuje zgodność ze starszymi centralami zapisującymi pod /piec/telemetry.
        val candidateRoots = listOf(
            "/piec/devices/$uid/telemetry/diagnostics",
            "/piec/telemetry/diagnostics"
        )
        var selectedRoot = ""
        var session = ""
        var lastReadError: String? = null
        for (root in candidateRoots) {
            val result = Rtdb.get("$root.json", mapOf("shallow" to "true"))
            if (!result.ok) {
                lastReadError = result.error ?: "HTTP error"
                continue
            }
            val sessions = try { JSONObject(result.body) } catch (_: Exception) { null } ?: continue
            val newest = terminalNewestSession(sessions) ?: continue
            selectedRoot = root
            session = newest
            break
        }

        if (selectedRoot.isEmpty()) {
            if (lastReadError != null) {
                termStatus = "Nie udało się odczytać DLOG z Firebase ($lastReadError)."
                termStatusKind = "warn"
            } else {
                termStatus = if (termOpened)
                    "Remote włączony, ale centrala nie wysłała jeszcze wpisów DLOG."
                else "Brak wpisów DLOG. Remote może być wyłączony albo nie ma nowych zdarzeń."
                termStatusKind = "warn"
            }
            return
        }

        if (selectedRoot != terminalDlogRoot) {
            terminalDlogRoot = selectedRoot
            terminalDlogSession = ""
        }
        if (session != terminalDlogSession) terminalSelectDlogSession(session)

        if (terminalDlogCursor < 0) {
            // Przy pierwszym otwarciu pokazujemy tylko najnowsze 50 chunków sesji.
            val result = Rtdb.get(
                "$selectedRoot/$session/chunks.json",
                mapOf("orderBy" to "\"\$key\"", "limitToLast" to "50")
            )
            if (!result.ok) {
                termStatus = "Nie udało się pobrać DLOG (${result.error ?: "HTTP error"})."
                termStatusKind = "warn"
                return
            }
            val chunks = try { JSONObject(result.body) } catch (_: Exception) { null }
            if (chunks != null) terminalAppendDlogChunks(chunks)
            if (terminalDlogCursor < 0) {
                termStatus = if (termOpened)
                    "Remote potwierdzony; oczekiwanie na pierwszy chunk DLOG."
                else "Brak chunków DLOG w tej sesji."
                termStatusKind = "warn"
            } else {
                termStatus = "Wyświetlam ostatnie rzeczywiste wpisy DLOG z sesji $session."
                termStatusKind = "ok"
            }
            return
        }

        // Pojedynczy GET po oczekiwanym numerze ogranicza transfer; pobieramy
        // ograniczoną partię, jeśli aplikacja była chwilowo uśpiona.
        var received = 0
        while (received < 50) {
            val nextSeq = terminalDlogCursor + 1
            val result = Rtdb.get("$selectedRoot/$session/chunks/$nextSeq.json")
            if (!result.ok) {
                termStatus = "Nie udało się pobrać DLOG (${result.error ?: "HTTP error"})."
                termStatusKind = "warn"
                return
            }
            val chunk = try { JSONObject(result.body) } catch (_: Exception) { null } ?: break
            val previous = terminalDlogCursor
            terminalAppendDlogChunk(nextSeq.toString(), chunk)
            if (terminalDlogCursor == previous) break
            received++
        }
        if (received > 0) {
            termStatus = "Odebrano rzeczywiste wpisy DLOG z sesji $session."
            termStatusKind = "ok"
        }
    }

    private fun terminalNewestSession(sessions: JSONObject): String? {
        val keys = mutableListOf<String>()
        val iterator = sessions.keys()
        while (iterator.hasNext()) {
            val key = iterator.next()
            if (key.matches(Regex("[A-Za-z0-9_-]{1,64}"))) keys.add(key)
        }
        return keys.maxWithOrNull(compareBy<String> { it.toLongOrNull() ?: Long.MIN_VALUE }.thenBy { it })
    }

    private fun terminalSelectDlogSession(session: String) {
        terminalDlogSession = session
        terminalDlogCursor = -1
        terminalResetDlogBuffer()
        termStatus = "Odczytuję wpisy DLOG z sesji $session…"
        termStatusKind = ""
    }

    private fun terminalResetDlogBuffer() {
        terminalRaw.clear()
        terminalDisplay.clear()
        termSeq = -1
        termLines = 0
        termBytes = 0L
        termGaps = 0
        termChunks = 0
    }

    private fun terminalAppendDlogChunks(chunks: JSONObject) {
        val entries = mutableListOf<Pair<String, JSONObject>>()
        val iterator = chunks.keys()
        while (iterator.hasNext()) {
            val key = iterator.next()
            val chunk = chunks.optJSONObject(key) ?: continue
            entries.add(key to chunk)
        }
        entries.sortedBy { (key, chunk) -> chunk.optInt("seq", key.toIntOrNull() ?: Int.MAX_VALUE) }
            .forEach { (key, chunk) -> terminalAppendDlogChunk(key, chunk) }
    }

    private fun terminalAppendDlogChunk(key: String, chunk: JSONObject) {
        val seq = chunk.optInt("seq", key.toIntOrNull() ?: -1)
        if (seq < 0 || seq <= terminalDlogCursor) return
        val data = chunk.optString("data", "")
        val bytes = chunk.optLong("bytes", data.toByteArray(Charsets.UTF_8).size.toLong())
            .coerceIn(0L, Int.MAX_VALUE.toLong()).toInt()
        if (data.isNotEmpty()) terminalAppend(data, bytes, seq)
        terminalDlogCursor = seq
    }

    fun terminalRemoteOn() = setTerminalRemote(true)

    fun terminalRemoteOff() = setTerminalRemote(false)

    private suspend fun <T> withTerminalCommandLock(action: suspend () -> T): T {
        terminalCommandMutex.lock()
        try {
            return action()
        } finally {
            terminalCommandMutex.unlock()
        }
    }

    private fun setTerminalRemote(enabled: Boolean) = scope.launch {
        if (termCategoryBatchBusy) {
            termStatus = "Poczekaj na zakończenie zmiany filtrów DLOG."
            termStatusKind = "warn"
            return@launch
        }
        withTerminalCommandLock {
            if (termCategoryBatchBusy) {
                termStatus = "Poczekaj na zakończenie zmiany filtrów DLOG."
                termStatusKind = "warn"
                return@withTerminalCommandLock
            }
            if (!connected) {
                termStateKnown = false
                termStatus = "Brak połączenia z Firebase — zmiana zdalnego odbioru logów nie została wysłana."
                termStatusKind = "err"
                return@withTerminalCommandLock
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
                        termStatus = "ESP32 odrzuciło polecenie: ${ack.error}"
                        termStatusKind = "err"
                    }
                    else -> {
                        termOpened = enabled
                        termStateKnown = true
                        termAutoOffAt = if (enabled) System.currentTimeMillis() + 60L * 60_000L else 0L
                        termStatus = if (enabled)
                            "Zdalny odbiór logów włączony. Oczekiwanie na nowe wpisy."
                        else "Zdalny odbiór logów wyłączony."
                        termStatusKind = "ok"
                        poll()
                    }
                }
            } catch (e: Exception) {
                termStateKnown = false
                termStatus = "Nie udało się zmienić zdalnego odbioru logów: ${e.message ?: "brak odpowiedzi"}"
                termStatusKind = "err"
            }
        }
    }

    private fun terminalSendWithAck(command: String, success: String, onAck: () -> Unit = {}) = scope.launch {
        if (termCategoryBatchBusy) {
            termStatus = "Poczekaj na zakończenie zmiany filtrów DLOG."
            termStatusKind = "warn"
            return@launch
        }
        withTerminalCommandLock {
            if (termCategoryBatchBusy) {
                termStatus = "Poczekaj na zakończenie zmiany filtrów DLOG."
                termStatusKind = "warn"
                return@withTerminalCommandLock
            }
            if (!connected) {
                termStatus = "Brak połączenia Firebase — polecenia DLOG nie wysłano."
                termStatusKind = "err"
                return@withTerminalCommandLock
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
                        termStatus = "ESP32 odrzuciło polecenie: ${ack.error}"
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
    }

    fun terminalSetLevel(lvl: String) {
        if (lvl.isEmpty() || termCategoryBatchBusy) return
        val levelLabel = CommandRedactor.forDisplay("diag remote level $lvl")
        if (!termOpened) {
            termLevel = lvl // lokalny wybór; nie twierdzi, że ESP32 zmieniło poziom
            termStatus = "$levelLabel wybrano lokalnie; zmiany nie wysłano do sterownika."
            termStatusKind = "warn"
            return
        }
        terminalSendWithAck("diag remote level $lvl", "Sterownik potwierdził ustawienie: $levelLabel.") {
            termLevel = lvl
        }
    }

    fun terminalSetCategory(cat: String, on: Boolean) {
        if (termCategoryBatchBusy) return
        termCats[cat] = on
        terminalRender()
        if (termOpened) {
            val state = if (on) "on" else "off"
            scope.launch {
                withTerminalCommandLock {
                    if (!termCategoryBatchBusy) sendAwait("diag remote cat $cat $state")
                }
            }
        }
    }

    /** Firmware accepts one named category per command; send the ALL shortcut serially with ACKs. */
    fun terminalSetAllCategories(on: Boolean) {
        if (termCategoryBatchBusy) return
        DLOG_CATEGORIES.forEach { termCats[it] = on }
        terminalRender()
        if (!termOpened) {
            termStatus = if (on) "Filtry lokalnego bufora włączone." else "Filtry lokalnego bufora wyłączone."
            termStatusKind = ""
            return
        }
        if (!connected) {
            termStatus = "Brak połączenia Firebase — filtrów kategorii nie wysłano."
            termStatusKind = "err"
            return
        }

        val state = if (on) "on" else "off"
        termCategoryBatchBusy = true
        scope.launch {
            var completed = 0
            try {
                termStatus = "Oczekiwanie na poprzednie polecenie terminala…"
                termStatusKind = ""
                withTerminalCommandLock {
                    termStatus = "Ustawianie filtrów logów: 0/${DLOG_CATEGORIES.size}."
                    for (cat in DLOG_CATEGORIES) {
                        val ack = Rtdb.sendCommand(Rtdb.makeFireCmdId(), "diag remote cat $cat $state")
                        when {
                            ack == null -> {
                                termStatus = "Brak potwierdzenia zmiany kategorii logów; wykonano $completed/${DLOG_CATEGORIES.size}."
                                termStatusKind = "warn"
                                return@withTerminalCommandLock
                            }
                            !ack.ok -> {
                                termStatus = "Sterownik odrzucił zmianę kategorii logów: ${ack.error}; wykonano $completed/${DLOG_CATEGORIES.size}."
                                termStatusKind = "err"
                                return@withTerminalCommandLock
                            }
                        }
                        completed++
                        termStatus = "Ustawianie filtrów logów: $completed/${DLOG_CATEGORIES.size}."
                    }
                }
                if (completed < DLOG_CATEGORIES.size) return@launch
                termStatus = "Sterownik potwierdził ${if (on) "włączenie" else "wyłączenie"} wszystkich filtrów logów."
                termStatusKind = "ok"
                poll()
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                termStatus = "Błąd przy ustawianiu kategorii logów po $completed/${DLOG_CATEGORIES.size}: ${e.message ?: "brak odpowiedzi"}."
                termStatusKind = "err"
            } finally {
                termCategoryBatchBusy = false
            }
        }
    }

    /** Polecenie wysyłane do sterownika; konsola nie dopisuje fikcyjnego ACK. */
    fun terminalExec(cmd: String) {
        val command = cmd.trim()
        if (command.isEmpty()) return
        if (termCategoryBatchBusy) {
            termStatus = "Trwa zmiana filtrów DLOG — polecenie nie zostało wysłane."
            termStatusKind = "warn"
            return
        }
        scope.launch {
            withTerminalCommandLock {
                if (termCategoryBatchBusy) {
                    termStatus = "Trwa zmiana filtrów DLOG — polecenie nie zostało wysłane."
                    termStatusKind = "warn"
                    return@withTerminalCommandLock
                }
                sendAwait(command)
            }
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
        val commandLabel = CommandRedactor.forDisplay(cmd)
        otaProgressLabel = "Oczekiwanie na potwierdzenie: $commandLabel ($fileName)…"
        val confirmed = sendAwait(cmd)
        if (confirmed) {
            otaProgressPct = "ACK"
            otaProgressLabel = "Sterownik przyjął zlecenie: $commandLabel. Postęp aktualizacji $targetName nie jest dostępny w panelu — sprawdź logi centrali."
            addLog("OTA", "Sterownik przyjął zlecenie: $commandLabel ($fileName); wynik aktualizacji niezweryfikowany", "ack")
        } else {
            otaProgressPct = "—"
            otaProgressLabel = "Brak potwierdzenia: $commandLabel. Nie można stwierdzić, czy aktualizacja $targetName została rozpoczęta."
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
        return if (S.sym.isNotEmpty()) "$source · symulacja zgłoszona przez sterownik" else source
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
