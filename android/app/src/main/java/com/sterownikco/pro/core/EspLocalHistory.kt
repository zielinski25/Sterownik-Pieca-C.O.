package com.sterownikco.pro.core

import kotlin.math.roundToInt

/* ══════════════════════════════════════════════════════════════════════════
   LOKALNA HISTORIA CENTRALI → PUNKTY WYKRESU (tryb LAN, także bez internetu)

   Ten sam układ kolumn co parseEspShortHistory()/parseEspLongHistory() w Piec.html
   i historiaLiniaCsv()/historiaDlugaZbudujCsv() w firmware:
   - GET /api/history (bufor minutowy ~24 h, 14 kolumn): etykieta, t_zewn, t_bojler,
     t_ogrz, t_ogrz_sr, t_powrot, t_panel, t_pokoj, pompa (0/1), cisnienie,
     wilgotnosc, klapa [°], syberka [°], czas_unix;
   - GET /api/history/long (zdarzeniowa, wiele dni): trójki średnia/min/max —
     średnie w kolumnach 1, 4, …, 31, czas_unix w 34, pompa w 35 (v3.44.0+).
   Wartości w jednostkach statusu (°C, hPa, %); serwa w stopniach (klapa 0–180,
   syberek 0–90) → procent jak w TelemRow.toPoint(). Te same granice czujników
   (odrzuca np. -127 °C odłączonego DS18B20 i „nan”). Lokalne archiwum nie ma
   t_trociny ani dym — te serie zostają puste.
   ══════════════════════════════════════════════════════════════════════════ */

internal object EspLocalHistory {
    fun parseShort(csv: String): List<TelemPoint> = parse(csv, minColumns = 14) { c, seq ->
        point(
            tsSec = c.long(13), seq = seq,
            tZewn = c.num(1), tBojler = c.num(2), tOgrz = c.num(3), tOgrzSr = c.num(4),
            tPowrot = c.num(5), tPanel = c.num(6), tPokoj = c.num(7), pompa = c.num(8),
            cisnienie = c.num(9), wilgotnosc = c.num(10), klapaDeg = c.num(11), syberkaDeg = c.num(12)
        )
    }

    fun parseLong(csv: String): List<TelemPoint> = parse(csv, minColumns = 35) { c, seq ->
        point(
            tsSec = c.long(34), seq = seq,
            tZewn = c.num(1), tBojler = c.num(4), tOgrz = c.num(7), tOgrzSr = c.num(10),
            tPowrot = c.num(13), tPanel = c.num(16), tPokoj = c.num(19), pompa = c.num(35),
            cisnienie = c.num(22), wilgotnosc = c.num(25), klapaDeg = c.num(28), syberkaDeg = c.num(31)
        )
    }

    /**
     * Zakres z archiwum centrali: długie (zdarzeniowe) do granicy ostatnich 24 h,
     * dalej bufor minutowy — tak samo jak lokalny wykres w Piec.html.
     */
    fun combine(shortPoints: List<TelemPoint>, longPoints: List<TelemPoint>, startMs: Long, endMs: Long): List<TelemPoint> {
        val recentCutoff = endMs - 24 * 3600_000L
        val older = longPoints.filter { it.ts >= startMs && it.ts < recentCutoff }
        val recent = shortPoints.filter { it.ts >= maxOf(startMs, recentCutoff) && it.ts <= endMs }
        return (older + recent).sortedWith(compareBy({ it.ts }, { it.seq }))
    }

    /**
     * Kopia z telefonu (RTDB, próbki minutowe) ma pierwszeństwo; lokalna historia
     * centrali uzupełnia wyłącznie czas po ostatniej próbce z kopii.
     */
    fun appendAfter(cached: List<TelemPoint>, local: List<TelemPoint>): List<TelemPoint> {
        val lastCached = cached.maxOfOrNull { it.ts } ?: Long.MIN_VALUE
        val tail = local.filter { it.ts > lastCached }.sortedWith(compareBy({ it.ts }, { it.seq }))
        return if (tail.isEmpty()) cached else cached + tail
    }

    private class Columns(private val c: List<String>) {
        fun num(i: Int): Double? = c.getOrNull(i)?.trim()?.toDoubleOrNull()?.takeIf { it.isFinite() }
        fun long(i: Int): Long? = c.getOrNull(i)?.trim()?.toLongOrNull()
    }

    private inline fun parse(csv: String, minColumns: Int, build: (Columns, Long) -> TelemPoint?): List<TelemPoint> {
        val out = ArrayList<TelemPoint>()
        var seq = 0L
        for (line in csv.lineSequence()) {
            if (line.isBlank()) continue
            val columns = line.split(',')
            seq++
            if (columns.size < minColumns) continue
            build(Columns(columns), seq)?.let { if (it.hasValues()) out.add(it) }
        }
        return out
    }

    private fun bounded(v: Double?, lo: Double, hi: Double): Double? =
        v?.takeIf { it in lo..hi }?.let { (it * 10).roundToInt() / 10.0 }

    private fun servoPercent(deg: Double?, maxDeg: Double): Double? =
        deg?.takeIf { it in 0.0..maxDeg }?.let { (it / maxDeg * 100).roundToInt().toDouble() }

    private fun point(
        tsSec: Long?, seq: Long,
        tZewn: Double?, tBojler: Double?, tOgrz: Double?, tOgrzSr: Double?, tPowrot: Double?,
        tPanel: Double?, tPokoj: Double?, pompa: Double?, cisnienie: Double?, wilgotnosc: Double?,
        klapaDeg: Double?, syberkaDeg: Double?
    ): TelemPoint? {
        // czas_unix = 0: centrala nie miała wiarygodnego czasu (brak RTC/NTP) — próbki nie da się umieścić.
        if (tsSec == null || tsSec <= 0L) return null
        return TelemPoint(
            ts = tsSec * 1000, seq = seq,
            t_zewn = bounded(tZewn, -25.0, 45.0),
            t_bojler = bounded(tBojler, 10.0, 95.0),
            t_ogrz = bounded(tOgrz, 10.0, 99.0),
            t_ogrz_sr = bounded(tOgrzSr, 10.0, 99.0),
            t_powrot = bounded(tPowrot, 10.0, 95.0),
            t_panel = bounded(tPanel, 0.0, 140.0),
            t_pokoj = bounded(tPokoj, 8.0, 45.0),
            t_trociny = null,
            wilgotnosc = bounded(wilgotnosc, 10.0, 100.0),
            cisnienie = bounded(cisnienie, 900.0, 1100.0),
            dym = null,
            klapa = servoPercent(klapaDeg, 180.0),
            syberka = servoPercent(syberkaDeg, 90.0),
            pumpOn = when (pompa) { 0.0 -> false; 1.0 -> true; else -> null }
        )
    }
}
