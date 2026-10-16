package com.sterownikco.pro.core

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableDoubleStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import org.json.JSONArray
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.Locale
import kotlin.math.roundToInt

/* ══════════════════════════════════════════════════════════════════════════
   BILANS SOLARNY & DETEKCJA POBORU WODY — `SolarAnalytics` z Piec.html 1:1
   (bramka 25 s, próg poboru rateBojler < -0.30 °C/min, archiwum 5000,
   zapis co 5 próbek do klucza `solar_archive_data`, eksport CSV).
   ══════════════════════════════════════════════════════════════════════════ */
class SolarAnalytics(private val prefs: Prefs) {

    data class Sample(
        val ts: Long, val iso: String, val tPanel: Double, val tBojler: Double, val tZewn: Double,
        val rad: Int, val uv: Double, val cloud: Int, val isDraw: Int, val drop: Double, val gain: Double
    )

    data class DrawEvent(val ts: Long, val drop: Double, val tPrev: Double, val tNow: Double)

    val archive = ArrayList<Sample>()
    val waterDrawEvents = ArrayList<DrawEvent>()
    var lastSampleTs = 0L
    var accumulatedGrossGain by mutableDoubleStateOf(0.0)
    var accumulatedDrawDrop by mutableDoubleStateOf(0.0)
    var forecastGain by mutableDoubleStateOf(Double.NaN)
    var forecastKwh by mutableDoubleStateOf(Double.NaN)
    var deltaT by mutableDoubleStateOf(Double.NaN)
    var drawCount by mutableIntStateOf(0)
    val hasSamples: Boolean get() = archive.isNotEmpty()

    private val isoFmt = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'", Locale.US).apply {
        timeZone = java.util.TimeZone.getTimeZone("UTC")
    }

    init {
        try {
            // Only the versioned archive written by this real-data path is trusted.
            // Older app versions could persist local demo/simulated values under K_SOLAR_ARCHIVE.
            prefs.get(Prefs.K_SOLAR_REAL_ARCHIVE)?.let { raw ->
                val arr = JSONArray(raw)
                for (i in 0 until arr.length()) {
                    val o = arr.optJSONObject(i) ?: continue
                    val sample = Sample(
                        o.optLong("ts"), o.optString("iso"), o.optDouble("t_panel", Double.NaN),
                        o.optDouble("t_bojler", Double.NaN), o.optDouble("t_zewn", Double.NaN),
                        o.optInt("rad"), o.optDouble("uv", Double.NaN), o.optInt("cloud"),
                        o.optInt("isDraw"), o.optDouble("drop"), o.optDouble("gain")
                    )
                    if (sample.ts > 0L && sample.tPanel.isFinite() && sample.tBojler.isFinite() && sample.tZewn.isFinite()) {
                        archive.add(sample)
                    }
                }
            }
        } catch (e: Exception) { /* brak archiwum zweryfikowanego */ }
        lastSampleTs = archive.lastOrNull()?.ts ?: 0L
        accumulatedGrossGain = archive.filter { it.tPanel > it.tZewn && it.rad > 30 }.sumOf { it.gain }
        val draws = archive.filter { it.isDraw != 0 }.takeLast(100)
        draws.forEach { waterDrawEvents.add(DrawEvent(it.ts, it.drop, it.tBojler + it.drop, it.tBojler)) }
        accumulatedDrawDrop = archive.filter { it.isDraw != 0 }.sumOf { it.drop }
        drawCount = draws.size
        archive.lastOrNull()?.let { deltaT = it.tPanel - it.tZewn }
    }

    private fun save() {
        try {
            val arr = JSONArray()
            archive.takeLast(500).forEach {
                arr.put(
                    JSONObject()
                        .put("ts", it.ts).put("iso", it.iso).put("t_panel", it.tPanel)
                        .put("t_bojler", it.tBojler).put("t_zewn", it.tZewn).put("rad", it.rad)
                        .put("uv", it.uv).put("cloud", it.cloud).put("isDraw", it.isDraw)
                        .put("drop", it.drop).put("gain", it.gain)
                )
            }
            prefs.set(Prefs.K_SOLAR_REAL_ARCHIVE, arr.toString())
        } catch (e: Exception) { /* ignore */ }
    }

    fun recordSample(tPanel: Double, tBojler: Double, tZewn: Double, rad: Double, uv: Double, cloud: Double) {
        if (!listOf(tPanel, tBojler, tZewn, rad, uv, cloud).all { it.isFinite() }) return
        if (tPanel !in 0.0..140.0 || tBojler !in 10.0..95.0 || tZewn !in -25.0..45.0 || rad < 0.0) return
        val now = System.currentTimeMillis()
        if (lastSampleTs > 0 && now - lastSampleTs < 25_000) return
        lastSampleTs = now
        var isDraw = false
        var drawDrop = 0.0
        var gain = 0.0
        if (archive.isNotEmpty()) {
            val prev = archive.last()
            val dtMin = maxOf(.5, (now - prev.ts) / 60000.0)
            val rateBojler = (tBojler - prev.tBojler) / dtMin
            val ratePanel = (tPanel - prev.tPanel) / dtMin
            if (rateBojler < -0.30 || (ratePanel < -0.60 && tPanel > tZewn + 5)) {
                isDraw = true
                drawDrop = maxOf(0.0, prev.tBojler - tBojler)
                accumulatedDrawDrop += drawDrop
                waterDrawEvents.add(DrawEvent(now, drawDrop, prev.tBojler, tBojler))
                while (waterDrawEvents.size > 100) waterDrawEvents.removeAt(0)
            }
            val deltaP = maxOf(0.0, tPanel - prev.tPanel)
            val deltaB = maxOf(0.0, tBojler - prev.tBojler)
            gain = maxOf(deltaP, deltaB) + (if (isDraw) drawDrop else 0.0)
            if (tPanel > tZewn && rad > 30) accumulatedGrossGain += gain
        }
        archive.add(
            Sample(
                now, isoFmt.format(now), (tPanel * 10).roundToInt() / 10.0,
                (tBojler * 10).roundToInt() / 10.0, (tZewn * 10).roundToInt() / 10.0,
                rad.roundToInt(), (uv * 10).roundToInt() / 10.0, cloud.roundToInt(),
                if (isDraw) 1 else 0, (drawDrop * 10).roundToInt() / 10.0, (gain * 10).roundToInt() / 10.0
            )
        )
        while (archive.size > 5000) archive.removeAt(0)
        if (archive.size % 5 == 0) save()
        updateUi(tPanel, tZewn)
    }

    /** `estimateForecastGain(hourly)` — 1:1 (estymata radiacji z zachmurzenia). */
    fun estimateForecastGain(hourly: List<com.sterownikco.pro.core.WHour>) {
        if (hourly.isEmpty()) { forecastGain = Double.NaN; forecastKwh = Double.NaN; return }
        var sumRad = 0.0
        hourly.take(24).forEach { h ->
            val estRad = (100 - h.cloud) * 8.5 * (if (h.isDay) 1 else 0)
            sumRad += maxOf(0.0, estRad)
        }
        val totalKwhM2 = sumRad / 1000.0
        forecastGain = (totalKwhM2 * 4.2 * 10).roundToInt() / 10.0
        forecastKwh = (totalKwhM2 * 2.0 * .65 * 10).roundToInt() / 10.0
    }

    private fun updateUi(tPanel: Double, tZewn: Double) {
        deltaT = tPanel - tZewn
        drawCount = waterDrawEvents.size
    }

    fun grossGain(): Double = accumulatedGrossGain
    fun estKwh(): Double = if (hasSamples) (accumulatedGrossGain * 200 * 4.186) / 3600.0 else Double.NaN

    /** `exportCsv()` — zawartość pliku CSV (nazwa: archiwum_solarno_pogodowe_RRRR-MM-DD.csv). */
    fun csv(): String {
        val sb = StringBuilder("timestamp,iso_time,t_panel,t_bojler,t_zewn,promieniowanie_W_m2,uv_index,zachmurzenie_proc,pobor_wody,spadek_poboru_C,zysk_brutto_C\n")
        archive.forEach {
            sb.append("${it.ts},${it.iso},${it.tPanel},${it.tBojler},${it.tZewn},${it.rad},${it.uv},${it.cloud},${it.isDraw},${it.drop},${it.gain}\n")
        }
        return sb.toString()
    }

    fun fileName(): String = "archiwum_solarno_pogodowe_" + SimpleDateFormat("yyyy-MM-dd", Locale.US).format(System.currentTimeMillis()) + ".csv"
}
