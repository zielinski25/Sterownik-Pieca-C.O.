package com.sterownikco.pro.widget

import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.appwidget.AppWidgetProvider
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.view.View
import android.widget.RemoteViews
import com.sterownikco.pro.MainActivity
import com.sterownikco.pro.R
import com.sterownikco.pro.core.Prefs
import com.sterownikco.pro.service.AlarmMonitorService
import org.json.JSONObject

/* ══════════════════════════════════════════════════════════════════════════
   Widżety pulpitu (APK-only): kompaktowy, szeroki pasek i pełny panel 2×2.
   Wszystkie warianty korzystają z jednego snapshotu rzeczywistych danych.

   • Brak WorkManager/AlarmManager i updatePeriodMillis: launcher nie odpytuje
     widgetu cyklicznie. Odświeżenie następuje po nadejściu danych z aplikacji
     na pierwszym planie lub usługi SSE w tle.
   • Identyczne sygnatury danych są pomijane przez 60 s; zmieniony odczyt lub
     stan alarmu odświeża widget od razu. Bez napływu danych nie ma pushu.
   • Wybór czujników jest zapisany niezależnie dla każdego appWidgetId.
   • Tapnięcie widgetu otwiera aplikację.
   ══════════════════════════════════════════════════════════════════════════ */

class PiecWidget : AppWidgetProvider() {

    override fun onUpdate(ctx: Context, mgr: AppWidgetManager, ids: IntArray) {
        renderAll(ctx)
    }

    override fun onDeleted(ctx: Context, appWidgetIds: IntArray) {
        removeConfigurations(ctx, appWidgetIds)
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
        private const val K_WIDGET_CONFIG_PREFIX = "piec_widget_sensors_"

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

        /** Dane do ekranu konfiguracji właściwego dla istniejącej instancji. */
        internal fun setupForWidget(ctx: Context, appWidgetId: Int): WidgetSetup? {
            val spec = specForWidget(ctx, appWidgetId) ?: return null
            return WidgetSetup(
                title = spec.title,
                maxSensors = spec.slots.valueViews.size,
                sensors = SENSOR_OPTIONS.map { sensor ->
                    WidgetSensorChoice(sensor.key, sensor.label, sensor.description)
                }
            )
        }

        /** Wczytuje wybór tej instancji albo sensowne ustawienie domyślne wariantu. */
        fun selectedSensors(ctx: Context, appWidgetId: Int): List<String> {
            val spec = specForWidget(ctx, appWidgetId) ?: return emptyList()
            return selectedSensors(ctx.applicationContext, appWidgetId, spec)
        }

        /** Zapisuje wybór dla tej instancji; pusty wybór jest celowo odrzucany. */
        fun saveSelectedSensors(ctx: Context, appWidgetId: Int, keys: List<String>) {
            val spec = specForWidget(ctx, appWidgetId) ?: return
            val allowed = SENSOR_OPTIONS.map { it.key }.toSet()
            val normalized = keys.asSequence().filter { it in allowed }.distinct()
                .take(spec.slots.valueViews.size).toList()
            if (normalized.isEmpty()) return
            try {
                Prefs(ctx.applicationContext).set(configKey(appWidgetId), normalized.joinToString(","))
            } catch (e: Exception) { /* ignore */ }
        }

        /** Android nie wysyła APPWIDGET_UPDATE po konfiguracji — zlecamy render tutaj. */
        fun renderWidget(ctx: Context, appWidgetId: Int) {
            try {
                val app = ctx.applicationContext
                val mgr = AppWidgetManager.getInstance(app)
                val spec = specForWidget(app, appWidgetId) ?: return
                val snap = readSnapshot(app)
                mgr.updateAppWidget(appWidgetId, views(app, snap, spec, appWidgetId))
            } catch (e: Exception) { /* widget nie może wywalić wołającego */ }
        }

        fun removeConfigurations(ctx: Context, appWidgetIds: IntArray) {
            try {
                val prefs = Prefs(ctx.applicationContext)
                appWidgetIds.forEach { prefs.remove(configKey(it)) }
            } catch (e: Exception) { /* ignore */ }
        }

        fun renderAll(ctx: Context) {
            try {
                val app = ctx.applicationContext
                val mgr = AppWidgetManager.getInstance(app)
                val snap = readSnapshot(app)
                WIDGETS.forEach { spec ->
                    val ids = mgr.getAppWidgetIds(ComponentName(app, spec.provider))
                    ids.forEach { id -> mgr.updateAppWidget(id, views(app, snap, spec, id)) }
                }
            } catch (e: Exception) { /* widget nie może wywalić wołającego */ }
        }

        private fun readSnapshot(ctx: Context): JSONObject? = try {
            Prefs(ctx).get(K_SNAP)?.let { JSONObject(it) }
        } catch (e: Exception) { null }

        private fun configKey(appWidgetId: Int) = "$K_WIDGET_CONFIG_PREFIX$appWidgetId"

        private fun specForWidget(ctx: Context, appWidgetId: Int): WidgetSpec? = try {
            val providerName = AppWidgetManager.getInstance(ctx.applicationContext)
                .getAppWidgetInfo(appWidgetId)?.provider?.className
            WIDGETS.firstOrNull { it.provider.name == providerName }
        } catch (e: Exception) { null }

        private fun selectedSensors(ctx: Context, appWidgetId: Int, spec: WidgetSpec): List<String> {
            val valid = SENSOR_OPTIONS.map { it.key }.toSet()
            val saved = try {
                Prefs(ctx.applicationContext).get(configKey(appWidgetId))
                    ?.split(",")?.filter { it in valid }?.distinct()
                    ?.take(spec.slots.valueViews.size).orEmpty()
            } catch (e: Exception) { emptyList() }
            return (saved.ifEmpty { spec.defaultSensorKeys })
                .filter { it in valid }.distinct().take(spec.slots.valueViews.size)
                .ifEmpty { spec.defaultSensorKeys.take(spec.slots.valueViews.size) }
        }

        private val SENSOR_OPTIONS = listOf(
            WidgetSensor("t_ogrz", "PIEC C.O.", "Temperatura obiegu grzewczego", "PIEC", 2, 10.0, 99.0),
            WidgetSensor("t_bojler", "BOJLER C.W.U.", "Temperatura zasobnika ciepłej wody", "BOJLER", 1, 10.0, 95.0),
            WidgetSensor("t_panel", "PANEL SŁONECZNY", "Temperatura kolektora słonecznego", "PANEL", 5, 0.0, 140.0, accent = true),
            WidgetSensor("t_zewn", "ZEWNĘTRZNA", "Temperatura zewnętrzna", "ZEWN.", 0, -25.0, 45.0)
        )

        private val WIDGETS = listOf(
            WidgetSpec(
                provider = PiecWidget::class.java,
                layout = R.layout.widget_piec,
                slots = WidgetSlotIds(
                    cellViews = listOf(R.id.w_cell_1, R.id.w_cell_2, R.id.w_cell_3, R.id.w_cell_4),
                    labelViews = listOf(R.id.w_l_1, R.id.w_l_2, R.id.w_l_3, R.id.w_l_4),
                    valueViews = listOf(R.id.w_v_1, R.id.w_v_2, R.id.w_v_3, R.id.w_v_4),
                    rowViews = listOf(R.id.w_row_top, R.id.w_row_bottom)
                ),
                defaultSensorKeys = listOf("t_ogrz", "t_bojler", "t_panel", "t_zewn"),
                useShortLabels = false,
                title = "Pełny · układ 2 × 2"
            ),
            WidgetSpec(
                provider = PiecWidgetCompact::class.java,
                layout = R.layout.widget_piec_compact,
                slots = WidgetSlotIds(
                    cellViews = listOf(R.id.w_cell_1, R.id.w_cell_2),
                    labelViews = listOf(R.id.w_l_1, R.id.w_l_2),
                    valueViews = listOf(R.id.w_v_1, R.id.w_v_2)
                ),
                defaultSensorKeys = listOf("t_bojler", "t_panel"),
                useShortLabels = true,
                title = "Kompaktowy · do 2 odczytów"
            ),
            WidgetSpec(
                provider = PiecWidgetStrip::class.java,
                layout = R.layout.widget_piec_strip,
                slots = WidgetSlotIds(
                    cellViews = listOf(R.id.w_cell_1, R.id.w_cell_2, R.id.w_cell_3, R.id.w_cell_4),
                    labelViews = listOf(R.id.w_l_1, R.id.w_l_2, R.id.w_l_3, R.id.w_l_4),
                    valueViews = listOf(R.id.w_v_1, R.id.w_v_2, R.id.w_v_3, R.id.w_v_4)
                ),
                defaultSensorKeys = listOf("t_ogrz", "t_bojler", "t_panel", "t_zewn"),
                useShortLabels = true,
                title = "Szeroki · do 4 odczytów"
            )
        )

        private fun views(ctx: Context, d: JSONObject?, spec: WidgetSpec, appWidgetId: Int): RemoteViews {
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
            val simMaskInvalid = d?.has("sim") == true && simRaw !is Boolean && simMask == null
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

            fun degrees(sensor: WidgetSensor): String {
                val x = numeric(sensor.key)?.takeIf { it in sensor.min..sensor.max } ?: return "—"
                val value = "${Math.round(x)}°"
                return if (simulated(sensor.channel)) "~$value" else value
            }

            val selected = selectedSensors(ctx, appWidgetId, spec)
                .mapNotNull { key -> SENSOR_OPTIONS.firstOrNull { it.key == key } }
            spec.slots.cellViews.forEachIndexed { index, cellId ->
                val sensor = selected.getOrNull(index)
                v.setViewVisibility(cellId, if (sensor == null) View.GONE else View.VISIBLE)
                if (sensor != null) {
                    val label = if (spec.useShortLabels) sensor.shortLabel else sensor.label
                    val labelColor = if (sensor.accent) Color.parseColor("#FFD32A") else Color.parseColor("#8EA6BA")
                    val valueColor = if (sensor.accent) Color.parseColor("#FFD32A") else Color.WHITE
                    v.setTextViewText(spec.slots.labelViews[index], label)
                    v.setTextViewText(spec.slots.valueViews[index], degrees(sensor))
                    v.setTextColor(spec.slots.labelViews[index], labelColor)
                    v.setTextColor(spec.slots.valueViews[index], valueColor)
                }
            }
            spec.slots.rowViews.forEachIndexed { rowIndex, rowId ->
                val hasVisibleCell = when (rowIndex) {
                    0 -> selected.isNotEmpty()
                    1 -> selected.size > 2
                    else -> selected.isNotEmpty()
                }
                v.setViewVisibility(rowId, if (hasVisibleCell) View.VISIBLE else View.GONE)
            }

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
            val hasAnyReading = SENSOR_OPTIONS.any { sensor ->
                numeric(sensor.key)?.let { it in sensor.min..sensor.max } == true
            }
            val hasRealReading = SENSOR_OPTIONS.any { sensor ->
                numeric(sensor.key)?.let { it in sensor.min..sensor.max } == true && !simulated(sensor.channel)
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
            v.setTextViewText(R.id.w_status, statusText)
            v.setTextColor(R.id.w_status, statusColor)
            val updatedAt = if (lastPush > 0L) java.util.Calendar.getInstance().apply { timeInMillis = lastPush } else null
            val updatedLabel = updatedAt?.let { "od " + String.format(java.util.Locale.US, "%02d:%02d",
                it.get(java.util.Calendar.HOUR_OF_DAY), it.get(java.util.Calendar.MINUTE)) } ?: "—"
            v.setTextViewText(R.id.w_time, updatedLabel)

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

/** Compact launcher provider; boiler + solar are the default configurable sensors. */
class PiecWidgetCompact : AppWidgetProvider() {
    override fun onUpdate(ctx: Context, mgr: AppWidgetManager, ids: IntArray) {
        PiecWidget.renderAll(ctx)
    }

    override fun onDeleted(ctx: Context, appWidgetIds: IntArray) {
        PiecWidget.removeConfigurations(ctx, appWidgetIds)
    }
}

/** Wide one-row launcher provider; up to four temperature sensors are configurable. */
class PiecWidgetStrip : AppWidgetProvider() {
    override fun onUpdate(ctx: Context, mgr: AppWidgetManager, ids: IntArray) {
        PiecWidget.renderAll(ctx)
    }

    override fun onDeleted(ctx: Context, appWidgetIds: IntArray) {
        PiecWidget.removeConfigurations(ctx, appWidgetIds)
    }
}

internal data class WidgetSetup(
    val title: String,
    val maxSensors: Int,
    val sensors: List<WidgetSensorChoice>
)

internal data class WidgetSensorChoice(
    val key: String,
    val label: String,
    val description: String
)

private data class WidgetSensor(
    val key: String,
    val label: String,
    val description: String,
    val shortLabel: String,
    val channel: Int,
    val min: Double,
    val max: Double,
    val accent: Boolean = false
)

private data class WidgetSlotIds(
    val cellViews: List<Int>,
    val labelViews: List<Int>,
    val valueViews: List<Int>,
    val rowViews: List<Int> = emptyList()
)

private data class WidgetSpec(
    val provider: Class<out AppWidgetProvider>,
    val layout: Int,
    val slots: WidgetSlotIds,
    val defaultSensorKeys: List<String>,
    val useShortLabels: Boolean,
    val title: String
)
