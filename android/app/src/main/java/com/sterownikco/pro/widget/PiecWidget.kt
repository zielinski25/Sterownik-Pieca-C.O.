package com.sterownikco.pro.widget

import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.appwidget.AppWidgetProvider
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.widget.RemoteViews
import com.sterownikco.pro.MainActivity
import com.sterownikco.pro.R
import com.sterownikco.pro.core.Prefs
import com.sterownikco.pro.service.AlarmMonitorService
import org.json.JSONObject

/* ══════════════════════════════════════════════════════════════════════════
   Widżety pulpitu (APK-only): kompaktowy, szeroki pasek i pełny panel 2×2.
   Wszystkie warianty korzystają z jednego snapshotu rzeczywistych danych.

   • Brak WorkManager/AlarmManager — widżety odświeżają się TYLKO gdy przyjdą
     świeże dane (usługa SSE w tle lub poll() na pierwszym planie), więc nie
     budzi telefonu na próżno. Ostatnia migawka siedzi w prefs, więc widget
     renderuje się poprawnie także po reboocie / ubiciu procesu.
   • Throttling: binder-update max co 60 s albo natychmiast przy zmianie
     wartości lub stanu alarmu.
   • Tapnięcie widgetu otwiera aplikację.
   ══════════════════════════════════════════════════════════════════════════ */

class PiecWidget : AppWidgetProvider() {

    override fun onUpdate(ctx: Context, mgr: AppWidgetManager, ids: IntArray) {
        renderAll(ctx)
    }

    override fun onReceive(ctx: Context, intent: Intent) {
        super.onReceive(ctx, intent)
        if (intent.action == ACT_REFRESH) {
            // Tap w odświeżanie: upewniamy się, że usługa żyje + przerys.
            try { AlarmMonitorService.start(ctx.applicationContext) } catch (e: Exception) { /* ignore */ }
            renderAll(ctx)
        }
    }

    companion object {
        const val ACT_REFRESH = "com.sterownikco.pro.WIDGET_REFRESH"
        const val K_SNAP = "piec_widget_snap"
        const val K_PUSH_TS = "piec_widget_push_ts"
        const val K_PUSH_SIG = "piec_widget_push_sig"
        const val PUSH_MIN_MS = 60_000L

        /** Wepchnięcie migawki z usługi / aplikacji (z throttlingiem). */
        fun push(ctx: Context, d: JSONObject) {
            val app = ctx.applicationContext
            val p = try { Prefs(app) } catch (e: Exception) { return }
            val sig = listOf("t_ogrz", "t_bojler", "t_panel", "t_zewn", "dym_alarm", "alarm_ogrzewanie", "alarm_panel", "online", "sim", "is_sim", "simulated")
                .joinToString("|") { k -> if (d.isNull(k)) "" else d.opt(k).toString() }
            val now = System.currentTimeMillis()
            val same = sig == (p.get(K_PUSH_SIG) ?: "")
            if (same && now - p.getLong(K_PUSH_TS, 0L) < PUSH_MIN_MS) return
            p.set(K_SNAP, d.toString())
            p.set(K_PUSH_SIG, sig)
            p.setLong(K_PUSH_TS, now)
            renderAll(app)
        }

        fun clear(ctx: Context) {
            try {
                val p = Prefs(ctx.applicationContext)
                p.remove(K_SNAP); p.remove(K_PUSH_SIG); p.setLong(K_PUSH_TS, 0L)
            } catch (e: Exception) { /* ignore */ }
            renderAll(ctx.applicationContext)
        }

        fun renderAll(ctx: Context) {
            try {
                val app = ctx.applicationContext
                val mgr = AppWidgetManager.getInstance(app)
                val snap = try {
                    Prefs(app).get(K_SNAP)?.let { JSONObject(it) }
                } catch (e: Exception) { null }
                WIDGETS.forEach { spec ->
                    val ids = mgr.getAppWidgetIds(ComponentName(app, spec.provider))
                    ids.forEach { id -> mgr.updateAppWidget(id, views(app, snap, spec)) }
                }
            } catch (e: Exception) { /* widget nie może wywalić wołającego */ }
        }

        private val WIDGETS = listOf(
            WidgetSpec(
                PiecWidget::class.java, R.layout.widget_piec,
                WidgetIds(R.id.w_t_ogrz, R.id.w_t_bojler, R.id.w_t_panel, R.id.w_t_zewn, R.id.w_status, R.id.w_time)
            ),
            WidgetSpec(
                PiecWidgetCompact::class.java, R.layout.widget_piec_compact,
                WidgetIds(boiler = R.id.w_t_bojler, panel = R.id.w_t_panel, status = R.id.w_status, time = R.id.w_time)
            ),
            WidgetSpec(
                PiecWidgetStrip::class.java, R.layout.widget_piec_strip,
                WidgetIds(R.id.w_t_ogrz, R.id.w_t_bojler, R.id.w_t_panel, R.id.w_t_zewn, R.id.w_status, R.id.w_time)
            )
        )

        private fun views(ctx: Context, d: JSONObject?, spec: WidgetSpec): RemoteViews {
            val v = RemoteViews(ctx.packageName, spec.layout)

            val simRaw = d?.opt("sim")
            val simMask = when (simRaw) {
                is Number -> simRaw.toDouble().takeIf {
                    it.isFinite() && it >= 0.0 && it % 1.0 == 0.0 && it < Long.MAX_VALUE.toDouble()
                }?.toLong()
                is String -> simRaw.trim().toLongOrNull()?.takeIf { it >= 0L }
                else -> null
            }
            val simFlagInvalid = listOf("is_sim", "simulated").any { key ->
                d != null && d.has(key) && d.opt(key) !is Boolean
            }
            val simMaskInvalid = d?.has("sim") == true &&
                simRaw !is Boolean && simMask == null
            val metadataInvalid = simFlagInvalid || simMaskInvalid
            val explicitSim = d?.optBoolean("is_sim", false) == true ||
                d?.optBoolean("simulated", false) == true || simRaw == true
            val simMaskValue = simMask ?: 0L
            val sim = metadataInvalid || explicitSim || simMaskValue != 0L

            fun simulated(channel: Int): Boolean = metadataInvalid || explicitSim ||
                (channel in 0..62 && simMaskValue and (1L shl channel) != 0L)

            fun numeric(key: String): Double? {
                val raw = if (d == null || !d.has(key) || d.isNull(key)) null else d.opt(key)
                return (raw as? Number)?.toDouble()?.takeIf { it.isFinite() }
            }

            fun deg(key: String, channel: Int, min: Double, max: Double): String {
                val x = numeric(key)?.takeIf { it in min..max } ?: return "—"
                val value = String.format(java.util.Locale.US, "%.1f°", x)
                return if (simulated(channel)) "~$value" else value
            }

            val sensors = listOf(
                Triple("t_ogrz", 2, 10.0..99.0),
                Triple("t_bojler", 1, 10.0..95.0),
                Triple("t_panel", 5, 0.0..140.0),
                Triple("t_zewn", 0, -25.0..45.0)
            )
            fun display(sensor: Triple<String, Int, ClosedFloatingPointRange<Double>>) =
                deg(sensor.first, sensor.second, sensor.third.start, sensor.third.endInclusive)

            fun setText(viewId: Int?, text: String) {
                if (viewId != null) v.setTextViewText(viewId, text)
            }
            setText(spec.ids.ogrz, display(sensors[0]))
            setText(spec.ids.boiler, display(sensors[1]))
            setText(spec.ids.panel, display(sensors[2]))
            setText(spec.ids.outside, display(sensors[3]))

            fun flag(key: String): Boolean? {
                if (d == null || !d.has(key) || d.isNull(key)) return null
                return when (val raw = d.opt(key)) {
                    is Boolean -> raw
                    is Number -> raw.toDouble().takeIf { it.isFinite() && (it == 0.0 || it == 1.0) }
                        ?.let { it == 1.0 }
                    else -> null
                }
            }

            // Bool `sim` oznacza cały snapshot symulowany; w takim przypadku
            // także flagi alarmowe nie są potwierdzeniem stanu sterownika.
            val alarmTrusted = !metadataInvalid && !explicitSim
            val dym = if (alarmTrusted) flag("dym_alarm") else null
            val ogrz = if (alarmTrusted) flag("alarm_ogrzewanie") else null
            val panel = if (alarmTrusted) flag("alarm_panel") else null
            val alarmKnown = dym != null && ogrz != null && panel != null
            val alarmActive = dym == true || ogrz == true || panel == true
            val hasAnyReading = sensors.any { sensor -> numeric(sensor.first)?.let { value -> value in sensor.third } == true }
            val hasRealReading = sensors.any { sensor ->
                numeric(sensor.first)?.let { it in sensor.third } == true && !simulated(sensor.second)
            }
            val online = flag("online")
            val controllerOffline = online == false
            val lastPush = try { Prefs(ctx.applicationContext).getLong(K_PUSH_TS, 0L) } catch (e: Exception) { 0L }
            val stale = lastPush <= 0L || System.currentTimeMillis() - lastPush > 5L * 60_000L
            val oldAlarm = controllerOffline || stale
            val (statusText, statusColor) = when {
                alarmActive && oldAlarm -> "⚠ STARY ALARM" to Color.parseColor("#FBBF24")
                alarmActive -> "🚨 ALARM" to Color.parseColor("#FF5F78")
                !hasRealReading && hasAnyReading -> "⚠ SYMULACJA" to Color.parseColor("#FBBF24")
                !hasRealReading -> "○ BRAK DANYCH" to Color.parseColor("#7F93A3")
                sim && !alarmKnown -> "⚠ SYM · ALARM?" to Color.parseColor("#FBBF24")
                sim -> "⚠ SYMULACJA" to Color.parseColor("#FBBF24")
                !alarmKnown -> "⚠ ALARM?" to Color.parseColor("#FBBF24")
                controllerOffline || stale -> "○ NIEAKTUALNE" to Color.parseColor("#7F93A3")
                online != true -> "○ STATUS?" to Color.parseColor("#7F93A3")
                else -> "● LIVE" to Color.parseColor("#4ADE80")
            }
            setText(spec.ids.status, statusText)
            if (spec.ids.status != null) v.setTextColor(spec.ids.status, statusColor)
            val updatedAt = if (lastPush > 0L) java.util.Calendar.getInstance().apply { timeInMillis = lastPush } else null
            val updatedLabel = updatedAt?.let { "od " + String.format(java.util.Locale.US, "%02d:%02d",
                it.get(java.util.Calendar.HOUR_OF_DAY), it.get(java.util.Calendar.MINUTE)) } ?: "—"
            setText(spec.ids.time, updatedLabel)

            val open = PendingIntent.getActivity(
                ctx, 31, Intent(ctx, MainActivity::class.java),
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
            )
            // Cały widget klikalny: tap otwiera aplikację.
            v.setOnClickPendingIntent(R.id.w_root, open)
            return v
        }
    }
}

/** Compact launcher entry: boiler + solar collector at a glance. */
class PiecWidgetCompact : AppWidgetProvider() {
    override fun onUpdate(ctx: Context, mgr: AppWidgetManager, ids: IntArray) {
        PiecWidget.renderAll(ctx)
    }
}

/** Wide one-row launcher entry: four real temperature readings. */
class PiecWidgetStrip : AppWidgetProvider() {
    override fun onUpdate(ctx: Context, mgr: AppWidgetManager, ids: IntArray) {
        PiecWidget.renderAll(ctx)
    }
}

private data class WidgetIds(
    val ogrz: Int? = null,
    val boiler: Int? = null,
    val panel: Int? = null,
    val outside: Int? = null,
    val status: Int? = null,
    val time: Int? = null
)

private data class WidgetSpec(
    val provider: Class<out AppWidgetProvider>,
    val layout: Int,
    val ids: WidgetIds
)
