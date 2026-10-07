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
   Widget na pulpit (APK-only): 4 temperatury + pasek statusu.

   • Brak WorkManager/AlarmManager — widget odświeża się TYLKO gdy przyjdą
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
            val sig = listOf("t_ogrz", "t_bojler", "t_panel", "t_zewn", "dym_alarm", "alarm_ogrzewanie")
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
                val me = ComponentName(app, PiecWidget::class.java)
                val ids = mgr.getAppWidgetIds(me)
                if (ids.isEmpty()) return
                val snap = try {
                    Prefs(app).get(K_SNAP)?.let { JSONObject(it) }
                } catch (e: Exception) { null }
                for (id in ids) mgr.updateAppWidget(id, views(app, snap))
            } catch (e: Exception) { /* widget nie może wywalić wołającego */ }
        }

        private fun views(ctx: Context, d: JSONObject?): RemoteViews {
            val v = RemoteViews(ctx.packageName, R.layout.widget_piec)
            fun deg(k: String): String {
                val x = d?.optDouble(k, Double.NaN) ?: Double.NaN
                return if (x.isFinite()) String.format(java.util.Locale.US, "%.1f°", x) else "—"
            }
            v.setTextViewText(R.id.w_t_ogrz, deg("t_ogrz"))
            v.setTextViewText(R.id.w_t_bojler, deg("t_bojler"))
            v.setTextViewText(R.id.w_t_panel, deg("t_panel"))
            v.setTextViewText(R.id.w_t_zewn, deg("t_zewn"))
            val dym = AlarmMonitorService.jsonBool(d ?: JSONObject(), "dym_alarm")
            val ogrz = AlarmMonitorService.jsonBool(d ?: JSONObject(), "alarm_ogrzewanie")
            val (txt, col) = when {
                d == null -> "○ OCZEKIWANIE NA DANE" to Color.parseColor("#7F93A3")
                dym && ogrz -> "🚨 DYM + PRZEGRZANIE!" to Color.parseColor("#FF5F78")
                dym -> "🚨 ALARM DYMU!" to Color.parseColor("#FF5F78")
                ogrz -> "🚨 PRZEGRZANIE!" to Color.parseColor("#FF5F78")
                else -> "● STEROWNIK CO" to Color.parseColor("#4ADE80")
            }
            v.setTextViewText(R.id.w_status, txt)
            v.setTextColor(R.id.w_status, col)
            val c = java.util.Calendar.getInstance()
            v.setTextViewText(R.id.w_time, String.format(java.util.Locale.US, "%02d:%02d",
                c.get(java.util.Calendar.HOUR_OF_DAY), c.get(java.util.Calendar.MINUTE)))
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
