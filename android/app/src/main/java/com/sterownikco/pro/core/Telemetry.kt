package com.sterownikco.pro.core

import kotlin.math.abs
import kotlin.math.roundToInt

/* ══════════════════════════════════════════════════════════════════════════
   SZEREGI WYKRESÓW + FILTR ANOMALII — przeniesione 1:1 z Piec.html
   (`historyParseRecord`, `TEMP_SERIES`, `filterSeriesGlitches` v5).
   ══════════════════════════════════════════════════════════════════════════ */

/** Surowy rekord ze shardu `/piec/telemetry/1m/v1/RRRR/MM/DD`. */
data class TelemRow(
    val id: String, val seq: Long, val ts: Long, val mono: Long,
    val a: List<Double>, val k: Long, val s: Long, val state: Long, val q: Long,
    /** Bit i oznacza, że kanał i tej próbki pochodził z symulacji. */
    val sim: Long,
    /** Jawny znacznik całego rekordu (`is_sim` / `simulated`). */
    val simulated: Boolean = false
)

/** Rekord po mapowaniu kanałów; bitmapa zachowuje odczyty z aktywnych symulacji. */
data class TelemPoint(
    val ts: Long, val seq: Long,
    val t_zewn: Double?, val t_bojler: Double?, val t_ogrz: Double?, val t_ogrz_sr: Double?,
    val t_powrot: Double?, val t_panel: Double?, val t_pokoj: Double?, val t_trociny: Double?,
    val wilgotnosc: Double?, val cisnienie: Double?, val dym: Double?,
    val klapa: Double?, val syberka: Double?,
    /** Bit i oznacza, że kanał i tej próbki pochodził z symulacji sterownika. */
    val sim: Long = 0L,
    /** Jawny znacznik całego rekordu — takie rekordy demonstracyjne nadal pomijamy. */
    val simulated: Boolean = false
) {
    fun hasValues(): Boolean = listOf(
        t_zewn, t_bojler, t_ogrz, t_ogrz_sr, t_powrot, t_panel, t_pokoj, t_trociny,
        wilgotnosc, cisnienie, dym, klapa, syberka
    ).any { it != null && it.isFinite() }

    fun isSimulated(key: String): Boolean {
        if (simulated) return true
        val channel = SIM_CHANNELS[key] ?: return false
        return channel in 0..62 && (sim and (1L shl channel)) != 0L
    }
}

private val SIM_CHANNELS = mapOf(
    "t_zewn" to 0, "t_bojler" to 1, "t_ogrz" to 2, "t_ogrz_sr" to 3,
    "t_powrot" to 4, "t_panel" to 5, "t_pokoj" to 6, "t_trociny" to 7,
    "wilgotnosc" to 8, "cisnienie" to 9, "dym" to 10, "klapa" to 11, "syberka" to 12
)

private val SIM_BOUNDS_BY_CHANNEL = mapOf(
    0 to (-30.0 to 50.0), 1 to (0.0 to 160.0), 2 to (0.0 to 160.0), 3 to (0.0 to 160.0),
    4 to (-30.0 to 120.0), 5 to (-30.0 to 160.0), 6 to (-50.0 to 50.0), 7 to (-30.0 to 120.0),
    8 to (0.0 to 100.0), 9 to (970.0 to 1040.0), 10 to (0.0 to 4095.0)
)

fun TelemRow.toPoint(): TelemPoint {
    fun isSimulatedChannel(channel: Int): Boolean =
        channel in 0..62 && (sim and (1L shl channel)) != 0L

    fun getVal(idx: Int, channel: Int, scale: Double, minB: Double?, maxB: Double?): Double? {
        if (simulated || idx !in a.indices) return null
        val qVal = ((q shr (channel * 2)) and 3).toInt()
        val raw = a[idx]
        if (!raw.isFinite() || qVal >= 2) return null
        if (raw <= -1200 || raw >= 15000) return null // -1270 = odłączony DS18B20
        val v = raw * scale
        val simBounds = if (isSimulatedChannel(channel)) SIM_BOUNDS_BY_CHANNEL[channel] else null
        val lo = simBounds?.first ?: minB
        val hi = simBounds?.second ?: maxB
        if (lo != null && v < lo) return null
        if (hi != null && v > hi) return null
        return (v * 10).roundToInt() / 10.0
    }

    fun getServoVal(deg: Long, maxDeg: Double, channel: Int): Double? {
        if (simulated || deg < 0L || deg.toDouble() > maxDeg) return null
        return (deg / maxDeg * 100).roundToInt().toDouble()
    }

    return TelemPoint(
        ts = ts * 1000, seq = seq,
        t_zewn = getVal(0, 0, .1, -25.0, 45.0),
        t_bojler = getVal(1, 1, .1, 10.0, 95.0),
        t_ogrz = getVal(2, 2, .1, 10.0, 99.0),
        t_ogrz_sr = getVal(3, 3, .1, 10.0, 99.0),
        t_powrot = getVal(4, 4, .1, 10.0, 95.0),
        t_panel = getVal(5, 5, .1, 0.0, 140.0),
        t_pokoj = getVal(6, 6, .1, 8.0, 45.0),
        t_trociny = getVal(7, 7, .1, 0.0, 90.0),
        wilgotnosc = getVal(8, 8, .1, 10.0, 100.0),
        cisnienie = getVal(9, 9, .1, 900.0, 1100.0),
        dym = getVal(10, 10, 1.0, 0.0, 4095.0),
        klapa = getServoVal(k, 180.0, 11),
        syberka = getServoVal(s, 90.0, 12),
        sim = sim, simulated = simulated
    )
}

/** Opis serii wykresu — pola identyczne jak TEMP_SERIES / SERVO_SERIES. */
data class SeriesDef(
    val id: String, val label: String, val ch: String, val idx: Int, val qCh: Int,
    val unit: String, val group: String, val accent: String, val rgb: String,
    var on: Boolean, val servo: Boolean = false
)

object ChartSeries {
    val TEMP = listOf(
        SeriesDef("ogrz", "Ogrz.", "t_ogrz", 2, 2, "°C", "temp", "#ff5f78", "255,95,120", true),
        SeriesDef("ogrzsr", "Ogrz. śr.", "t_ogrz_sr", 3, 3, "°C", "temp", "#ffd166", "255,209,102", true),
        SeriesDef("bojler", "Bojler", "t_bojler", 1, 1, "°C", "temp", "#ffb04a", "255,176,74", true),
        SeriesDef("zewn", "Zewn.", "t_zewn", 0, 0, "°C", "temp", "#55d7ff", "85,215,255", true),
        SeriesDef("powrot", "Powrót", "t_powrot", 4, 4, "°C", "temp", "#a879ff", "168,121,255", false),
        SeriesDef("panel", "Panel", "t_panel", 5, 5, "°C", "temp", "#ffd32a", "255,211,42", false),
        SeriesDef("pokoj", "Pokój", "t_pokoj", 6, 6, "°C", "temp", "#c084fc", "192,132,252", false),
        SeriesDef("trociny", "Trociny", "t_trociny", 7, 7, "°C", "temp", "#fb7185", "251,113,133", false),
        SeriesDef("wilgotnosc", "Wilgotność", "wilgotnosc", 8, 8, "%", "rh", "#00d4f5", "0,212,245", false),
        SeriesDef("cisnienie", "Ciśnienie", "cisnienie", 9, 9, "hPa", "pressure", "#45d98b", "69,217,139", false),
        SeriesDef("dym", "Dym ADC", "dym", 10, 10, "ADC", "smoke", "#f06292", "240,98,146", false)
    )
    val SERVO = listOf(
        SeriesDef("servo_flap", "Klapa", "klapa", -1, -1, "%", "position", "#ff6fc7", "255,111,199", true, servo = true),
        SeriesDef("servo_damper", "Syberek", "syberka", -1, -1, "%", "position", "#45d98b", "69,217,139", true, servo = true)
    )
    val ALL: Map<String, SeriesDef> = (TEMP + SERVO).associateBy { it.ch }

    val BOUNDS = mapOf(
        "t_ogrz" to (10.0 to 99.0), "t_ogrz_sr" to (10.0 to 99.0), "t_bojler" to (10.0 to 95.0),
        "t_powrot" to (10.0 to 95.0), "t_panel" to (0.0 to 140.0), "t_trociny" to (0.0 to 90.0),
        "t_pokoj" to (8.0 to 45.0), "t_zewn" to (-25.0 to 45.0), "wilgotnosc" to (10.0 to 100.0),
        "cisnienie" to (900.0 to 1100.0), "dym" to (0.0 to 4095.0), "klapa" to (0.0 to 100.0),
        "syberka" to (0.0 to 100.0)
    )

    fun value(p: TelemPoint, key: String): Double? = when (key) {
        "t_zewn" -> p.t_zewn; "t_bojler" -> p.t_bojler; "t_ogrz" -> p.t_ogrz; "t_ogrz_sr" -> p.t_ogrz_sr
        "t_powrot" -> p.t_powrot; "t_panel" -> p.t_panel; "t_pokoj" -> p.t_pokoj; "t_trociny" -> p.t_trociny
        "wilgotnosc" -> p.wilgotnosc; "cisnienie" -> p.cisnienie; "dym" -> p.dym
        "klapa" -> p.klapa; "syberka" -> p.syberka; else -> null
    }

    /** Granice walidacji aplikacji dla próbek jawnie oznaczonych jako symulowane. */
    val SIM_BOUNDS = mapOf(
        "t_ogrz" to (0.0 to 160.0), "t_ogrz_sr" to (0.0 to 160.0), "t_bojler" to (0.0 to 160.0),
        "t_powrot" to (-30.0 to 120.0), "t_panel" to (-30.0 to 160.0), "t_trociny" to (-30.0 to 120.0),
        "t_pokoj" to (-50.0 to 50.0), "t_zewn" to (-30.0 to 50.0), "wilgotnosc" to (0.0 to 100.0),
        "cisnienie" to (970.0 to 1040.0), "dym" to (0.0 to 4095.0)
    )

    /** `filterSeriesGlitches(rows, key, group)` — granice fizyczne + Hampel. */
    fun filterGlitches(rows: List<TelemPoint>, key: String, group: String, enabled: Boolean = true): List<Double?> {
        val rawAll = rows.map { value(it, key) }
        if (!enabled || rows.size < 3) return rawAll
        val b = BOUNDS[key]
        val jump = when (group) {
            "temp" -> 12.0; "rh" -> 20.0; "pressure" -> 4.0; "position" -> 15.0; else -> 400.0
        }
        val simulated = rows.map { it.isSimulated(key) }
        val cleaned = rawAll.mapIndexed { i, v ->
            if (v == null || !v.isFinite()) null
            else if (simulated[i]) v.takeIf { SIM_BOUNDS[key]?.let { (lo, hi) -> v >= lo && v <= hi } ?: true }
            else v.takeIf { b == null || (v >= b.first && v <= b.second) }
        }
        val n = rows.size
        val out = arrayOfNulls<Double>(n)
        for (i in 0 until n) {
            val v = cleaned[i] ?: continue
            // Zachowaj przebieg symulacji dokładnie; nie filtruj skoku wejścia/wyjścia
            // Hampel-em i nie porównuj go z sąsiednią serią fizyczną.
            if (simulated[i]) { out[i] = v; continue }
            val past = ArrayList<Double>(4)
            var k = i - 1
            while (k >= 0 && past.size < 4) {
                if (simulated[k]) break
                val c = cleaned[k]
                if (c != null) {
                    if (rows[i].ts - rows[k].ts > 3_600_000L) break
                    past.add(c)
                }
                k--
            }
            val future = ArrayList<Double>(4)
            k = i + 1
            while (k < n && future.size < 4) {
                if (simulated[k]) break
                val c = cleaned[k]
                if (c != null) {
                    if (rows[k].ts - rows[i].ts > 3_600_000L) break
                    future.add(c)
                }
                k++
            }
            if (past.isEmpty() && future.isEmpty()) { out[i] = v; continue }
            if (past.isEmpty() && future.isNotEmpty() && abs(v - future[0]) <= jump) { out[i] = v; continue }
            if (future.isEmpty() && past.isNotEmpty() && abs(v - past[0]) <= jump) { out[i] = v; continue }
            var matchesPast = false
            if (past.isNotEmpty()) {
                val pMed = past.sorted()[past.size / 2]
                if (abs(v - pMed) <= jump) matchesPast = true
            }
            var matchesFuture = false
            if (future.isNotEmpty()) {
                val fMed = future.sorted()[future.size / 2]
                if (abs(v - fMed) <= jump && (future.size >= 2 || past.isEmpty())) matchesFuture = true
            }
            out[i] = if (matchesPast || matchesFuture) v else null
        }
        return out.toList()
    }
}
