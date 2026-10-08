package com.sterownikco.pro.service

import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import androidx.core.content.ContextCompat
import com.sterownikco.pro.core.AlarmCenter
import com.sterownikco.pro.core.AlarmNotify
import com.sterownikco.pro.core.Prefs
import com.sterownikco.pro.core.Rtdb
import com.sterownikco.pro.widget.PiecWidget
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeout
import org.json.JSONObject

/* ══════════════════════════════════════════════════════════════════════════
   Usługa czuwająca nad alarmami dymu i przegrzania (APK-only).

   • Foreground service typu `specialUse` — system go nie ubija po cichu,
     a użytkownik widzi stałą ikonkę „Monitoring alarmów".
   • JEDEN strumień SSE z /piec/status zamiast odpytań — Firebase sam pcha
     zmiany (keep-alive ~30 s), więc radio śpi, a alarm wpada w ~1 s.
   • Reconnect z backoffem po każdym zerwaniu + wymuszony restart strumienia
     co 3 min (samoleczenie „na wpół otwartych" połączeń po Doze/tunelach).
   • `auth_revoked` / 401 → odświeżenie idToken z refresh_token i dalej.
   • Każdy snapshot: AlarmCenter.evaluate() + odświeżenie widgetu.
   ══════════════════════════════════════════════════════════════════════════ */

class AlarmMonitorService : Service() {

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    @Volatile private var lastSnap = JSONObject()

    override fun onCreate() {
        super.onCreate()
        loadCredsIntoRtdb()
        val notif = AlarmNotify.serviceNotification(this)
        if (Build.VERSION.SDK_INT >= 29) {
            startForeground(AlarmNotify.SVC_NOTIF_ID, notif, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE)
        } else {
            @Suppress("DEPRECATION")
            startForeground(AlarmNotify.SVC_NOTIF_ID, notif)
        }
        scope.launch { monitorLoop() }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACT_STOP) {
            stopSelf()
            return START_NOT_STICKY
        }
        loadCredsIntoRtdb()
        return START_STICKY
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        scope.cancel()
        super.onDestroy()
    }

    /** Tokeny z prefs (proces usługi może startować bez AppModel — np. po boocie). */
    private fun loadCredsIntoRtdb() {
        try {
            val p = Prefs(applicationContext)
            (p.get(Prefs.K_API_KEY) ?: Prefs.DEFAULT_FB_API_KEY).let {
                if (it.isNotEmpty()) Rtdb.apiKey = it
            }
            Rtdb.idToken = p.get(Prefs.K_ID_TOKEN) ?: ""
            Rtdb.refreshToken = p.get(Prefs.K_REF_TOKEN) ?: ""
            Rtdb.cmdToken = p.get(Prefs.K_CMD_TOKEN) ?: Prefs.DEFAULT_CMD_TOKEN
        } catch (e: Exception) { /* prefs niedostępne — pętla i tak poczeka */ }
    }

    private suspend fun monitorLoop() {
        var backoffMs = 5_000L
        while (true) {
            try {
                val p = Prefs(applicationContext)
                if (!AlarmCenter.isMonitored(p) || (p.get(Prefs.K_REF_TOKEN) ?: "").isEmpty()) {
                    // Wylogowany albo monitoring wyłączony — drzemka bez sieci.
                    delay(60_000); continue
                }
                // Migawka startowa (przy okazji odświeża idToken po 401) + widget.
                try {
                    Rtdb.pollStatus()?.let { onSnapshot(it) }
                } catch (e: Exception) { /* strumień i tak spróbuje */ }
                // Strumień SSE z samoleczeniem co 3 min.
                withTimeout(STREAM_MAX_MS) {
                    Rtdb.streamEvents("/piec/status.json") { ev, data -> onStreamEvent(ev, data) }
                }
                backoffMs = 5_000L // czyste zerwanie po timeout — od razu wznawiamy
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                // Sieć padła / Doze / serwer zamknął — backoff 5 s → 15 s → 60 s.
                try { delay(backoffMs) } catch (ce: CancellationException) { throw ce }
                backoffMs = minOf(backoffMs * 3, 60_000L)
            }
        }
    }

    private fun onStreamEvent(ev: String, data: String) {
        try {
            when (ev) {
                "put", "patch" -> {
                    val j = JSONObject(data)
                    if (ev == "patch") {
                        val base = j.optString("path", "/").trim('/')
                        val dj = j.optJSONObject("data") ?: return
                        for (k in dj.keys()) applyKey(if (base.isEmpty()) k else "$base/$k", dj.get(k))
                    } else {
                        val path = j.optString("path", "/")
                        val payload = if (j.isNull("data")) null else j.get("data")
                        if (path == "/") {
                            if (payload is JSONObject) { lastSnap = payload; onSnapshot(payload); return }
                        } else if (payload != null) {
                            applyKey(path.trim('/'), payload)
                        }
                    }
                    onSnapshot(JSONObject(lastSnap.toString()))
                }
                "auth_revoked" -> {
                    // Token wygasł — odświeżamy i wymuszamy reconnect (wyjątek zrywa strumień).
                    scope.launch {
                        try {
                            Rtdb.refreshIdToken()
                            val p = Prefs(applicationContext)
                            if (Rtdb.idToken.isNotEmpty()) {
                                p.set(Prefs.K_ID_TOKEN, Rtdb.idToken)
                                p.set(Prefs.K_REF_TOKEN, Rtdb.refreshToken)
                            }
                        } catch (e: Exception) { /* pętla ponowi */ }
                    }
                    throw java.io.IOException("auth_revoked — odświeżam token")
                }
                // keep-alive / cancel: ignorujemy (cancel = reguły; reconnect i tak nastąpi)
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
        lastSnap = d
        try {
            val smoke = alarmFlag(d, "dym_alarm")
            val overheat = alarmFlag(d, "alarm_ogrzewanie")
            // Brak, null lub błędny typ nie jest potwierdzeniem stanu "bez alarmu".
            // Alarm aktywny można obsłużyć od razu; ciszę zatwierdzamy dopiero,
            // gdy obie flagi są jawnie znane i wyłączone.
            if (smoke == true || overheat == true || (smoke == false && overheat == false)) {
                val info = AlarmCenter.evaluate(
                    Prefs(applicationContext), smoke == true, overheat == true,
                    number(d, "t_ogrz"), number(d, "dym")
                )
                if (info != null) AlarmNotify.fire(applicationContext, info)
                else if (smoke == false && overheat == false) AlarmNotify.cancel(applicationContext)
            }
        } catch (e: Exception) { /* alarm nie może wywalić usługi */ }
        try {
            PiecWidget.push(applicationContext, d)
        } catch (e: Exception) { /* widget nie może wywalić usługi */ }
    }

    private fun alarmFlag(d: JSONObject, key: String): Boolean? {
        if (!d.has(key) || d.isNull(key)) return null
        return when (val raw = d.opt(key)) {
            is Boolean -> raw
            is Number -> raw.toDouble().takeIf { it.isFinite() && (it == 0.0 || it == 1.0) }?.let { it == 1.0 }
            is String -> when (raw.trim().lowercase()) {
                "1", "true" -> true
                "0", "false" -> false
                else -> null
            }
            else -> null
        }
    }

    private fun number(d: JSONObject, key: String): Double {
        if (!d.has(key) || d.isNull(key)) return Double.NaN
        return (d.opt(key) as? Number)?.toDouble()?.takeIf { it.isFinite() } ?: Double.NaN
    }

    companion object {
        const val ACT_STOP = "com.sterownikco.pro.STOP_MONITOR"
        const val STREAM_MAX_MS = 3L * 60_000

        fun start(ctx: Context) {
            try {
                ContextCompat.startForegroundService(ctx, Intent(ctx, AlarmMonitorService::class.java))
            } catch (e: Exception) { /* np. start z tła na Androidzie 12+ */ }
        }

        fun stop(ctx: Context) {
            try { ctx.stopService(Intent(ctx, AlarmMonitorService::class.java)) } catch (e: Exception) { /* ignore */ }
            AlarmNotify.cancel(ctx)
        }
    }
}
