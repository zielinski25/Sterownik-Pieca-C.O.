package com.sterownikco.pro.service

import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.media.AudioAttributes
import android.media.MediaPlayer
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.PowerManager
import android.os.SystemClock
import android.provider.Settings
import android.util.Log
import androidx.core.content.ContextCompat
import com.sterownikco.pro.core.AlarmCenter
import com.sterownikco.pro.core.AlarmFlags
import com.sterownikco.pro.core.AlarmMonitorHealth
import com.sterownikco.pro.core.AlarmMonitorPhase
import com.sterownikco.pro.core.AlarmMonitorStartGate
import com.sterownikco.pro.core.AlarmNotify
import com.sterownikco.pro.core.AlarmSnapshotCoordinator
import com.sterownikco.pro.core.Prefs
import com.sterownikco.pro.core.Rtdb
import com.sterownikco.pro.core.parseAlarmFlag
import com.sterownikco.pro.widget.PiecWidget
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeout
import org.json.JSONObject

/* ══════════════════════════════════════════════════════════════════════════
   Usługa czuwająca nad alarmami dymu i przegrzania (APK-only).

   • Foreground service typu `specialUse` z widocznym powiadomieniem; Android,
     nakładka producenta lub użytkownik nadal mogą zatrzymać usługę.
   • Główny odbiór zmian przez SSE z /piec/status.
   • Odczyt awaryjny GET co 30 s: wykrywa połączenia SSE, które pozostały
     otwarte, ale przestały dostarczać aktualizacje przy wygaszonym ekranie.
   • Reconnect z backoffem po zerwaniu + wymuszony restart strumienia co 3 min.
   • `auth_revoked` / 401 → odświeżenie idToken z refresh_token i dalej.
   • Każdy snapshot: AlarmCenter.evaluate() + odświeżenie widgetu.
   ══════════════════════════════════════════════════════════════════════════ */

class AlarmMonitorService : Service() {

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val mainHandler = Handler(Looper.getMainLooper())
    @Volatile private var lastSnap = JSONObject()
    private val snapshotLock = Any()
    @Volatile private var alarmStreamVersion = 0L
    private var alarmPlayer: MediaPlayer? = null
    private var alarmSoundId: String? = null

    override fun onCreate() {
        super.onCreate()
        activeInstance = this
        updateMonitorHealth(AlarmMonitorPhase.CONNECTING)
        // Promote before SharedPreferences/Keystore reads so Direct Boot startup
        // cannot consume the foreground-service start deadline on credential I/O.
        try {
            val notif = AlarmNotify.serviceNotification(this)
            if (Build.VERSION.SDK_INT >= 29) {
                startForeground(AlarmNotify.SVC_NOTIF_ID, notif, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE)
            } else {
                @Suppress("DEPRECATION")
                startForeground(AlarmNotify.SVC_NOTIF_ID, notif)
            }
        } catch (e: Exception) {
            updateMonitorHealth(AlarmMonitorPhase.START_FAILED)
            if (activeInstance === this) {
                activeInstance = null
                requestedAlarmSound = null
            }
            Log.e(TAG, "Nie udało się przejść do foreground service monitoringu alarmów", e)
            stopSelf()
            return
        }
        loadCredsIntoRtdb()
        Log.i(TAG, "Usługa monitoringu uruchomiona")
        scope.launch { monitorLoop() }
        scope.launch { fallbackPollLoop() }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACT_STOP) {
            stopLoopingAlarm()
            stopSelf()
            return START_NOT_STICKY
        }
        loadCredsIntoRtdb()
        if (intent?.action == ACT_PLAY_ALARM_LOOP) {
            val sound = intent.getStringExtra(EXTRA_ALARM_SOUND) ?: "syrena"
            if (requestedAlarmSound == sound) startLoopingAlarm(sound)
        }
        return START_STICKY
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        stopLoopingAlarm()
        if (activeInstance === this) {
            activeInstance = null
            requestedAlarmSound = null
        }
        if (monitorHealth.value.phase != AlarmMonitorPhase.START_FAILED) {
            updateMonitorHealth(AlarmMonitorPhase.STOPPED)
        }
        scope.cancel()
        super.onDestroy()
    }

    /** Utrzymuj jeden dźwięk alarmowy w pętli; te metody działają na wątku głównym usługi. */
    private fun startLoopingAlarm(sound: String) {
        if (alarmSoundId == sound) {
            try { if (alarmPlayer?.isPlaying == true) return } catch (e: Exception) { /* odtwórz ponownie */ }
        }
        stopLoopingAlarm()
        val res = AlarmNotify.soundRes(sound)
        val attrs = AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_ALARM)
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
            .build()
        val player = try {
            if (res != 0) MediaPlayer.create(applicationContext, res, attrs, 0)
            else MediaPlayer.create(applicationContext, Settings.System.DEFAULT_NOTIFICATION_URI, null, attrs, 0)
        } catch (e: Exception) { null } ?: run {
            Log.e(TAG, "Nie udało się utworzyć MediaPlayer alarmu: $sound")
            scheduleAlarmLoopRestart(sound, ALARM_LOOP_RETRY_MS)
            return
        }
        try {
            player.setWakeMode(applicationContext, PowerManager.PARTIAL_WAKE_LOCK)
            player.isLooping = true
            alarmPlayer = player
            alarmSoundId = sound
            player.setOnCompletionListener { finished ->
                Log.w(TAG, "MediaPlayer alarmu zakończył dźwięk mimo pętli: $sound")
                if (alarmPlayer === finished) {
                    alarmPlayer = null
                    alarmSoundId = null
                    try { finished.release() } catch (e: Exception) { /* ignore */ }
                }
                scheduleAlarmLoopRestart(sound, ALARM_LOOP_RESTART_MS)
            }
            player.setOnErrorListener { failed, what, extra ->
                Log.e(TAG, "Błąd MediaPlayer alarmu: what=$what extra=$extra")
                if (alarmPlayer === failed) {
                    alarmPlayer = null
                    alarmSoundId = null
                }
                try { failed.release() } catch (e: Exception) { /* ignore */ }
                scheduleAlarmLoopRestart(sound, ALARM_LOOP_RETRY_MS)
                true
            }
            player.start()
            Log.i(TAG, "Uruchomiono pętlę dźwięku alarmu: $sound")
        } catch (e: Exception) {
            Log.e(TAG, "Nie udało się uruchomić pętli dźwięku alarmu: $sound", e)
            if (alarmPlayer === player) {
                alarmPlayer = null
                alarmSoundId = null
            }
            try { player.release() } catch (ignored: Exception) { /* ignore */ }
            scheduleAlarmLoopRestart(sound, ALARM_LOOP_RETRY_MS)
        }
    }

    /** MediaPlayer powinien zapętlać sam, ale pilnujemy wznowienia, jeśli ROM go zatrzyma. */
    private fun scheduleAlarmLoopRestart(sound: String, delayMs: Long) {
        mainHandler.postDelayed({
            if (activeInstance === this && requestedAlarmSound == sound &&
                (alarmPlayer == null || alarmSoundId != sound)
            ) {
                startLoopingAlarm(sound)
            }
        }, delayMs)
    }

    private fun stopLoopingAlarm() {
        val player = alarmPlayer
        alarmPlayer = null
        alarmSoundId = null
        if (player != null) {
            Log.i(TAG, "Zatrzymano pętlę dźwięku alarmu")
            try { if (player.isPlaying) player.stop() } catch (e: Exception) { /* ignore */ }
            try { player.release() } catch (e: Exception) { /* ignore */ }
        }
    }

    /** Tokeny z prefs (proces usługi może startować bez AppModel — np. po boocie). */
    private fun loadCredsIntoRtdb() {
        try {
            val p = Prefs(applicationContext)
            Rtdb.onAuthTokensRefreshed = { idToken, refreshToken ->
                p.saveRefreshedTokens(Rtdb.apiKey, idToken, refreshToken)
            }
            val session = p.firebaseSession()
            Rtdb.setAuthSession(session.apiKey, session.idToken, session.refreshToken)
            // Commands are never sent by this service; keep the command credential in CE storage only.
            Rtdb.cmdToken = if (android.os.Build.VERSION.SDK_INT >= 24 &&
                (applicationContext.getSystemService(Context.USER_SERVICE) as android.os.UserManager).isUserUnlocked
            ) p.get(Prefs.K_CMD_TOKEN) ?: Prefs.DEFAULT_CMD_TOKEN else Prefs.DEFAULT_CMD_TOKEN
        } catch (e: Exception) { /* prefs niedostępne — pętla i tak poczeka */ }
    }

    private suspend fun monitorLoop() {
        var backoffMs = 5_000L
        while (true) {
            try {
                val p = Prefs(applicationContext)
                if (!AlarmCenter.isMonitored(p) || Rtdb.refreshToken.isBlank()) {
                    // Wylogowany albo monitoring wyłączony — drzemka bez sieci.
                    delay(60_000); continue
                }
                updateMonitorHealth(AlarmMonitorPhase.CONNECTING)
                // Migawka startowa (przy okazji odświeża idToken po 401) + widget.
                try {
                    val initial = Rtdb.pollStatus()
                    if (initial != null) onSnapshot(initial)
                    else {
                        updateMonitorHealth(AlarmMonitorPhase.RETRYING)
                        Log.w(TAG, "Początkowy odczyt /piec/status nie powiódł się")
                    }
                } catch (e: Exception) {
                    updateMonitorHealth(AlarmMonitorPhase.RETRYING)
                    Log.w(TAG, "Błąd początkowego odczytu /piec/status", e)
                }
                // Strumień SSE z samoleczeniem co 3 min.
                withTimeout(STREAM_MAX_MS) {
                    Rtdb.streamEvents("/piec/status.json") { ev, data, idToken, refreshToken ->
                        onStreamEvent(ev, data, idToken, refreshToken)
                    }
                }
                backoffMs = 5_000L // czyste zerwanie — od razu wznawiamy
            } catch (e: TimeoutCancellationException) {
                // Limit 3 min wymusza reconnect; nie wolno zakończyć przez niego pętli usługi.
                updateMonitorHealth(AlarmMonitorPhase.CONNECTING)
                backoffMs = 5_000L
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                // Sieć padła / Doze / serwer zamknął — backoff 5 s → 15 s → 60 s.
                updateMonitorHealth(AlarmMonitorPhase.RETRYING)
                Log.w(TAG, "Strumień SSE przerwany; ponowienie za ${backoffMs} ms", e)
                try { delay(backoffMs) } catch (ce: CancellationException) { throw ce }
                backoffMs = minOf(backoffMs * 3, 60_000L)
            }
        }
    }

    private fun onStreamEvent(
        ev: String,
        data: String,
        streamIdToken: String,
        streamRefreshToken: String
    ) {
        try {
            when (ev) {
                "put", "patch" -> synchronized(snapshotLock) {
                    val j = JSONObject(data)
                    if (ev == "patch") {
                        val base = j.optString("path", "/").trim('/')
                        val dj = j.optJSONObject("data") ?: return
                        val baseKey = base.substringBefore('/')
                        val touchesAlarm = baseKey in ALARM_STATUS_KEYS ||
                            (base.isEmpty() && ALARM_STATUS_KEYS.any(dj::has))
                        for (k in dj.keys()) applyKey(if (base.isEmpty()) k else "$base/$k", dj.get(k))
                        if (touchesAlarm) alarmStreamVersion++
                    } else {
                        val path = j.optString("path", "/")
                        val payload = if (j.isNull("data")) null else j.get("data")
                        if (path == "/") {
                            if (payload is JSONObject) {
                                lastSnap = payload
                                alarmStreamVersion++
                                onSnapshotLocked(payload)
                                return
                            }
                        } else {
                            // RTDB put-null usuwa klucz; zachowujemy migawkę zgodną ze źródłem.
                            val key = path.trim('/').substringBefore('/')
                            if (key in ALARM_STATUS_KEYS) alarmStreamVersion++
                            applyKey(path.trim('/'), payload)
                        }
                    }
                    onSnapshotLocked(JSONObject(lastSnap.toString()))
                }
                "auth_revoked" -> {
                    // Token wygasł — odświeżamy i wymuszamy reconnect (wyjątek zrywa strumień).
                    scope.launch {
                        try {
                            Rtdb.refreshIdToken(
                                expectedIdToken = streamIdToken,
                                expectedRefreshToken = streamRefreshToken
                            )
                        } catch (e: Exception) { /* pętla ponowi */ }
                    }
                    throw java.io.IOException("auth_revoked — odświeżam token")
                }
                "cancel" -> {
                    updateMonitorHealth(AlarmMonitorPhase.RETRYING)
                    throw java.io.IOException("RTDB anulowało strumień; ponawiam połączenie")
                }
                // keep-alive potwierdza połączenie, ale nie odświeża migawki /piec/status.
            }
        } catch (e: java.io.IOException) {
            throw e
        } catch (e: Exception) {
            // Uszkodzona ramka SSE — ignorujemy, strumień żyje dalej.
        }
    }

    /** Wpina wartość ze strumienia w bieżącą migawkę (obsługa kluczy 1. poziomu). */
    private fun applyKey(slashed: String, v: Any?) {
        try {
            val key = slashed.split("/").firstOrNull()?.takeIf { it.isNotEmpty() } ?: return
            if (v == null || v == JSONObject.NULL) lastSnap.remove(key) else lastSnap.put(key, v)
        } catch (e: Exception) { /* ignore */ }
    }

    private fun onSnapshot(d: JSONObject) {
        synchronized(snapshotLock) { onSnapshotLocked(d) }
    }

    private fun onSnapshotLocked(d: JSONObject) {
        lastSnap = d
        AlarmSnapshotCoordinator.applyServiceSnapshot {
            try {
                val smoke = alarmFlag(d, "dym_alarm")
                val overheat = alarmFlag(d, "alarm_ogrzewanie")
                val complete = smoke != null && overheat != null

                // Niepełna migawka nie może potwierdzić ciszy. Jeśli zawiera aktywną
                // flagę, zachowujemy poprzedni aktywny stan drugiej flagi (jeśli był znany).
                if (smoke == true || overheat == true || complete) {
                    val prefs = Prefs(applicationContext)
                    val flags = if (complete) {
                        AlarmFlags(dym = smoke == true, ogrz = overheat == true)
                    } else {
                        AlarmCenter.activeFlagsForPartialSnapshot(prefs, smoke, overheat)
                    }
                    val hasActiveFlag = flags?.dym == true || flags?.ogrz == true

                    if (flags != null && (complete || hasActiveFlag)) {
                        // `false` below is only the projection of a partial active-set signature;
                        // an incomplete snapshot never reaches the all-clear/cancel branch.
                        val smokeActive = flags.dym == true
                        val overheatActive = flags.ogrz == true
                        val info = AlarmCenter.evaluate(
                            prefs, smokeActive, overheatActive,
                            number(d, "t_ogrz"), number(d, "dym")
                        )
                        when {
                            info != null -> AlarmNotify.fire(applicationContext, info)
                            complete && smoke == false && overheat == false ->
                                AlarmNotify.cancel(applicationContext)
                            AlarmCenter.shouldLoopSound(prefs, smokeActive, overheatActive) -> {
                                val activeInfo = AlarmCenter.describeActiveFlags(
                                    smokeActive, overheatActive,
                                    number(d, "t_ogrz"), number(d, "dym")
                                )
                                if (activeInfo != null && !AlarmNotify.postIfMissing(applicationContext, activeInfo)) {
                                    val kind = AlarmCenter.kindFor(smokeActive, overheatActive)
                                    AlarmMonitorService.startAlarmLoop(applicationContext, AlarmCenter.sound(prefs, kind))
                                }
                            }
                            else -> {
                                AlarmNotify.cancelLegacyNotification(applicationContext)
                                AlarmMonitorService.stopAlarmLoop()
                            }
                        }
                    }
                }
            } catch (e: Exception) { /* alarm nie może wywalić usługi */ }
        }
        try {
            PiecWidget.push(applicationContext, d)
        } catch (e: Exception) { /* widget nie może wywalić usługi */ }
        updateMonitorHealth(AlarmMonitorPhase.STATUS_OK, statusReadSucceeded = true)
    }

    /**
     * Zapasowy, tylko odczytowy GET stanu. Sam proces/usługa może być żywa,
     * mimo że długa odpowiedź SSE przestała napływać; fallback wykrywa alarm
     * najpóźniej po jednym interwale. Odrzucamy wynik, jeśli nowsze SSE zdążyło
     * przyjść w trakcie żądania, aby nie nadpisać świeżego stanu starą migawką.
     */
    private suspend fun fallbackPollLoop() {
        while (true) {
            delay(FALLBACK_POLL_MS)
            val prefs = try { Prefs(applicationContext) } catch (e: Exception) { continue }
            if (!AlarmCenter.isMonitored(prefs)) continue
            if (Rtdb.refreshToken.isBlank()) {
                Log.w(TAG, "Pomijam fallback /status: brak zapisanego refresh tokenu")
                continue
            }
            val versionAtStart = synchronized(snapshotLock) { alarmStreamVersion }
            try {
                val status = Rtdb.pollStatus()
                if (status == null) {
                    updateMonitorHealth(AlarmMonitorPhase.RETRYING)
                    Log.w(TAG, "Fallback GET /piec/status nie powiódł się")
                    continue
                }
                synchronized(snapshotLock) {
                    if (alarmStreamVersion != versionAtStart) {
                        Log.d(TAG, "Odrzucono fallback: nowsze zdarzenie SSE już dotarło")
                    } else {
                        onSnapshotLocked(status)
                        Log.d(TAG, "Fallback GET OK; dym_alarm=${alarmFlag(status, "dym_alarm")}, " +
                            "alarm_ogrzewanie=${alarmFlag(status, "alarm_ogrzewanie")}")
                    }
                }
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                updateMonitorHealth(AlarmMonitorPhase.RETRYING)
                Log.w(TAG, "Błąd fallback GET /piec/status", e)
            }
        }
    }

    private fun alarmFlag(d: JSONObject, key: String): Boolean? {
        if (!d.has(key) || d.isNull(key)) return null
        return parseAlarmFlag(d.opt(key))
    }

    private fun number(d: JSONObject, key: String): Double {
        if (!d.has(key) || d.isNull(key)) return Double.NaN
        return (d.opt(key) as? Number)?.toDouble()?.takeIf { it.isFinite() } ?: Double.NaN
    }

    companion object {
        const val ACT_STOP = "com.sterownikco.pro.STOP_MONITOR"
        const val ACT_PLAY_ALARM_LOOP = "com.sterownikco.pro.PLAY_ALARM_LOOP"
        const val EXTRA_ALARM_SOUND = "alarm_sound"
        const val STREAM_MAX_MS = 3L * 60_000
        const val FALLBACK_POLL_MS = 30_000L
        private const val ALARM_LOOP_RESTART_MS = 250L
        private const val ALARM_LOOP_RETRY_MS = 5_000L
        private const val TAG = "AlarmMonitorService"
        private val ALARM_STATUS_KEYS = setOf("dym_alarm", "alarm_ogrzewanie")
        private val _monitorHealth = MutableStateFlow(AlarmMonitorHealth())
        val monitorHealth: StateFlow<AlarmMonitorHealth> = _monitorHealth.asStateFlow()
        @Volatile private var activeInstance: AlarmMonitorService? = null

        private fun updateMonitorHealth(
            phase: AlarmMonitorPhase,
            statusReadSucceeded: Boolean = false
        ) {
            _monitorHealth.update { current ->
                current.copy(
                    phase = phase,
                    lastStatusElapsedMs = if (statusReadSucceeded) {
                        SystemClock.elapsedRealtime()
                    } else {
                        current.lastStatusElapsedMs
                    }
                )
            }
        }
        @Volatile private var requestedAlarmSound: String? = null

        fun start(ctx: Context) {
            val appContext = ctx.applicationContext
            val prefs = try { Prefs(appContext) } catch (e: Exception) {
                updateMonitorHealth(AlarmMonitorPhase.START_FAILED)
                Log.e(TAG, "Nie można odczytać ustawień monitoringu alarmów", e)
                return
            }
            val session = prefs.firebaseSession()
            if (!AlarmMonitorStartGate.canStart(
                    AlarmCenter.isMonitored(prefs), session.refreshToken
                )
            ) {
                Log.i(TAG, "Pominięto start monitoringu alarmów: monitoring wyłączony lub brak sesji Firebase")
                return
            }
            if (activeInstance == null) updateMonitorHealth(AlarmMonitorPhase.STARTING)
            try {
                ContextCompat.startForegroundService(appContext, Intent(appContext, AlarmMonitorService::class.java))
            } catch (e: Exception) {
                if (activeInstance == null) updateMonitorHealth(AlarmMonitorPhase.START_FAILED)
                Log.e(TAG, "Nie udało się uruchomić foreground service monitoringu alarmów", e)
            }
        }

        /** Wywołuje istniejącą usługę monitorującą; jeśli jej brak, uruchamia ją jako foreground. */
        fun startAlarmLoop(ctx: Context, sound: String) {
            requestedAlarmSound = sound
            val service = activeInstance
            if (service != null) {
                service.mainHandler.post {
                    if (activeInstance === service && requestedAlarmSound == sound) service.startLoopingAlarm(sound)
                }
                return
            }
            if (activeInstance == null) updateMonitorHealth(AlarmMonitorPhase.STARTING)
            try {
                val intent = Intent(ctx, AlarmMonitorService::class.java)
                    .setAction(ACT_PLAY_ALARM_LOOP)
                    .putExtra(EXTRA_ALARM_SOUND, sound)
                ContextCompat.startForegroundService(ctx, intent)
            } catch (e: Exception) {
                if (activeInstance == null) updateMonitorHealth(AlarmMonitorPhase.START_FAILED)
                Log.e(TAG, "Nie udało się uruchomić usługi do pętli alarmowej", e)
            }
        }

        fun stopAlarmLoop() {
            requestedAlarmSound = null
            val service = activeInstance ?: return
            service.mainHandler.post { if (activeInstance === service) service.stopLoopingAlarm() }
        }

        fun stop(ctx: Context) {
            try { ctx.stopService(Intent(ctx, AlarmMonitorService::class.java)) } catch (e: Exception) { /* ignore */ }
            AlarmNotify.cancel(ctx)
        }
    }
}
