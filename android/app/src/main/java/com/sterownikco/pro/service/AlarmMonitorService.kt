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
import android.widget.Toast
import androidx.core.content.ContextCompat
import com.sterownikco.pro.core.AlarmCenter
import com.sterownikco.pro.core.AlarmFlags
import com.sterownikco.pro.core.AlarmMuteCommands
import com.sterownikco.pro.core.AlarmMonitorHealth
import com.sterownikco.pro.core.AlarmMonitorPhase
import com.sterownikco.pro.core.AlarmMonitorStartGate
import com.sterownikco.pro.core.AlarmNotify
import com.sterownikco.pro.core.AlarmSnapshotCoordinator
import com.sterownikco.pro.core.AlarmSoundSync
import com.sterownikco.pro.core.Prefs
import com.sterownikco.pro.core.Rtdb
import com.sterownikco.pro.core.controllerAlarmMuteStatus
import com.sterownikco.pro.core.parseAlarmFlag
import com.sterownikco.pro.widget.PiecWidget
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import org.json.JSONObject

/* ══════════════════════════════════════════════════════════════════════════
   Usługa czuwająca nad alarmami dymu i przegrzania (APK-only).

   • Foreground service typu `specialUse` z widocznym powiadomieniem; Android,
     nakładka producenta lub użytkownik nadal mogą zatrzymać usługę.
   • W tle GET /piec/status co 60 s; nie utrzymuje strumienia SSE.
   • Pierwszy odczyt wykonuje się od razu po starcie usługi; błędy ponawia
     zgodnie z minutowym cyklem, bez szybkiej pętli retry.
   • Odświeżenie Firebase ID token po 401 obsługuje klient RTDB.
   • Każdy snapshot: AlarmCenter.evaluate() + odświeżenie widgetu.
   ══════════════════════════════════════════════════════════════════════════ */

class AlarmMonitorService : Service() {

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val alarmMuteCommandMutex = Mutex()
    private val mainHandler = Handler(Looper.getMainLooper())
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
        Log.i(TAG, "Usługa monitoringu uruchomiona; polling /piec/status co 60 s")
        scope.launch { monitorLoop() }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACT_STOP) {
            stopLoopingAlarm()
            stopSelf()
            return START_NOT_STICKY
        }
        loadCredsIntoRtdb()
        when (intent?.action) {
            ACT_PLAY_ALARM_LOOP -> {
                val sound = intent.getStringExtra(EXTRA_ALARM_SOUND) ?: "syrena"
                if (requestedAlarmSound == sound) startLoopingAlarm(sound)
            }
            ACT_SYNC_ALARM_MUTE -> {
                val signature = intent.getStringExtra(EXTRA_MUTE_SIGNATURE).orEmpty()
                val secondsExtra = intent.getIntExtra(EXTRA_MUTE_SECONDS, NO_MUTE_SECONDS)
                val seconds = secondsExtra.takeUnless { it == NO_MUTE_SECONDS }
                scope.launch { syncControllerMute(signature, seconds) }
            }
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
            Rtdb.cmdToken = if (android.os.Build.VERSION.SDK_INT >= 24 &&
                (applicationContext.getSystemService(Context.USER_SERVICE) as android.os.UserManager).isUserUnlocked
            ) p.get(Prefs.K_CMD_TOKEN) ?: Prefs.DEFAULT_CMD_TOKEN else Prefs.DEFAULT_CMD_TOKEN
        } catch (e: Exception) { /* prefs niedostępne — pętla i tak poczeka */ }
    }

    private suspend fun syncControllerMute(signature: String, seconds: Int?) {
        alarmMuteCommandMutex.withLock {
            val commands = AlarmMuteCommands.forMute(signature, seconds)
            if (commands.isEmpty()) {
                reportMuteSync("Brak aktywnego alarmu do synchronizacji wyciszenia.", error = true)
                return@withLock
            }
            if (Rtdb.refreshToken.isBlank()) {
                reportMuteSync("Brak sesji Firebase — wyciszenie działa tylko na tym telefonie.", error = true)
                stopIfOnlyStartedForMute()
                return@withLock
            }
            for ((index, command) in commands.withIndex()) {
                val ack = try {
                    Rtdb.sendCommand(Rtdb.makeFireCmdId(), command)
                } catch (e: Exception) {
                    null
                }
                when {
                    ack == null -> {
                        reportMuteSync(
                            "Brak potwierdzenia centrali (${index + 1}/${commands.size}). Wyciszenie globalne nie jest potwierdzone.",
                            error = true
                        )
                        stopIfOnlyStartedForMute()
                        return@withLock
                    }
                    !ack.ok -> {
                        reportMuteSync(
                            "Centrala odrzuciła wyciszenie: ${ack.error}. Wymagany zgodny firmware.",
                            error = true
                        )
                        stopIfOnlyStartedForMute()
                        return@withLock
                    }
                }
            }
            reportMuteSync("Centrala przyjęła wyciszenie; czekam na potwierdzenie stanu wspólnego.")
            stopIfOnlyStartedForMute()
        }
    }

    private fun reportMuteSync(message: String, error: Boolean = false) {
        if (error) Log.w(TAG, message) else Log.i(TAG, message)
        mainHandler.post {
            try { Toast.makeText(applicationContext, message, Toast.LENGTH_LONG).show() }
            catch (e: Exception) { /* ekran powiadomień może nie być dostępny */ }
        }
    }

    private fun stopIfOnlyStartedForMute() {
        val prefs = try { Prefs(applicationContext) } catch (e: Exception) { null }
        if (prefs != null && !AlarmCenter.isMonitored(prefs)) stopSelf()
    }

    /** Background alarm checks use one RTDB read per minute; opening the app has its own faster UI poll. */
    private suspend fun monitorLoop() {
        while (true) {
            val cycleStartedAt = SystemClock.elapsedRealtime()
            try {
                val prefs = Prefs(applicationContext)
                if (AlarmCenter.isMonitored(prefs) && Rtdb.refreshToken.isNotBlank()) {
                    updateMonitorHealth(AlarmMonitorPhase.CONNECTING)
                    val status = Rtdb.pollStatus()
                    if (status == null) {
                        updateMonitorHealth(AlarmMonitorPhase.RETRYING)
                        Log.w(TAG, "GET /piec/status nie powiódł się; ponowię zgodnie z minutowym cyklem")
                    } else {
                        onSnapshot(status)
                        Log.d(TAG, "GET /piec/status OK; dym_alarm=${alarmFlag(status, "dym_alarm")}, " +
                            "alarm_ogrzewanie=${alarmFlag(status, "alarm_ogrzewanie")}")
                    }
                }
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                updateMonitorHealth(AlarmMonitorPhase.RETRYING)
                Log.w(TAG, "Błąd minutowego GET /piec/status", e)
            }

            val elapsedMs = SystemClock.elapsedRealtime() - cycleStartedAt
            val waitMs = (BACKGROUND_STATUS_POLL_MS - elapsedMs).coerceAtLeast(1_000L)
            delay(waitMs)
        }
    }

    private fun onSnapshot(d: JSONObject) {
        AlarmSnapshotCoordinator.applyServiceSnapshot {
            try {
                val smoke = alarmFlag(d, "dym_alarm")
                val overheat = alarmFlag(d, "alarm_ogrzewanie")
                val complete = smoke != null && overheat != null
                val prefs = Prefs(applicationContext)
                AlarmSoundSync.applyStatus(prefs, d, SystemClock.elapsedRealtime())
                val activeSignature = AlarmMuteCommands.signature(
                    smoke, overheat, prefs.get(AlarmCenter.K_LAST_SIG).orEmpty()
                )
                AlarmCenter.applyControllerMuteSnapshot(
                    p = prefs,
                    status = controllerAlarmMuteStatus(d),
                    currentSignature = activeSignature,
                    completeAlarmFlags = complete,
                    nowMs = System.currentTimeMillis()
                )

                // Niepełna migawka nie może potwierdzić ciszy. Jeśli zawiera aktywną
                // flagę, zachowujemy poprzedni aktywny stan drugiej flagi (jeśli był znany).
                if (smoke == true || overheat == true || complete) {
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
                                AlarmNotify.cancel(applicationContext)
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
        const val ACT_SYNC_ALARM_MUTE = "com.sterownikco.pro.SYNC_ALARM_MUTE"
        const val EXTRA_ALARM_SOUND = "alarm_sound"
        const val EXTRA_MUTE_SIGNATURE = "alarm_mute_signature"
        const val EXTRA_MUTE_SECONDS = "alarm_mute_seconds"
        private const val NO_MUTE_SECONDS = Int.MIN_VALUE
        const val BACKGROUND_STATUS_POLL_MS = 60_000L
        private const val ALARM_LOOP_RESTART_MS = 250L
        private const val ALARM_LOOP_RETRY_MS = 5_000L
        private const val TAG = "AlarmMonitorService"
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

        /** Requests a controller-side mute; no furnace or actuator command is sent. */
        fun requestControllerMute(ctx: Context, signature: String, seconds: Int?) {
            val appContext = ctx.applicationContext
            val service = activeInstance
            if (service != null) {
                service.scope.launch {
                    service.loadCredsIntoRtdb()
                    service.syncControllerMute(signature, seconds)
                }
                return
            }
            val session = try { Prefs(appContext).firebaseSession() } catch (e: Exception) { null }
            if (session?.refreshToken.isNullOrBlank()) {
                Handler(Looper.getMainLooper()).post {
                    try {
                        Toast.makeText(
                            appContext,
                            "Brak sesji Firebase — wyciszenie działa tylko na tym telefonie.",
                            Toast.LENGTH_LONG
                        ).show()
                    } catch (ignored: Exception) { /* ignore */ }
                }
                return
            }
            try {
                val intent = Intent(appContext, AlarmMonitorService::class.java)
                    .setAction(ACT_SYNC_ALARM_MUTE)
                    .putExtra(EXTRA_MUTE_SIGNATURE, signature)
                    .putExtra(EXTRA_MUTE_SECONDS, seconds ?: NO_MUTE_SECONDS)
                ContextCompat.startForegroundService(appContext, intent)
            } catch (e: Exception) {
                Log.e(TAG, "Nie udało się uruchomić usługi do synchronizacji wyciszenia", e)
                Handler(Looper.getMainLooper()).post {
                    try {
                        Toast.makeText(
                            appContext,
                            "Nie udało się wysłać wyciszenia do centrali.",
                            Toast.LENGTH_LONG
                        ).show()
                    } catch (ignored: Exception) { /* ignore */ }
                }
            }
        }

        fun stop(ctx: Context) {
            try { ctx.stopService(Intent(ctx, AlarmMonitorService::class.java)) } catch (e: Exception) { /* ignore */ }
            AlarmNotify.cancel(ctx)
        }
    }
}
