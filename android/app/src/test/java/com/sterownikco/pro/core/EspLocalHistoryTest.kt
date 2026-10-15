package com.sterownikco.pro.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class EspLocalHistoryTest {
    private val t0 = 1_792_000_800L // pełna minuta

    // Format historiaLiniaCsv(): etykieta, 7 temperatur, pompa, ciśnienie, wilgotność, klapa°, syberka°, czas_unix.
    private fun shortLine(ts: Long, ogrz: String = "61.3", pompa: String = "1", klapa: String = "90", syberka: String = "45") =
        "12:00,8.5,47.2,$ogrz,60.1,44.0,21.0,20.4,$pompa,1012.3,55,$klapa,$syberka,$ts"

    // 35 kolumn (trójki średnia/min/max) + opcjonalna 36. kolumna pompy (v3.44.0+).
    private fun longLine(ts: Long, ogrz: Double, pompa: String? = null): String {
        val triples = listOf(8.5, 47.2, ogrz, 60.1, 44.0, 21.0, 20.4, 1012.3, 55.0, 120.0, 30.0)
            .flatMap { listOf(it, it - 1, it + 1) }
            .joinToString(",") { if (it % 1.0 == 0.0) it.toLong().toString() else it.toString() }
        return "14.10 12:00,$triples,$ts" + (pompa?.let { ",$it" } ?: "")
    }

    @Test
    fun shortHistoryMapsColumnsUnitsAndPump() {
        val points = EspLocalHistory.parseShort(shortLine(t0) + "\n" + shortLine(t0 + 60, pompa = "0") + "\n")
        assertEquals(2, points.size)
        val p = points[0]
        assertEquals(t0 * 1000, p.ts)
        assertEquals(8.5, p.t_zewn!!, 0.0)
        assertEquals(47.2, p.t_bojler!!, 0.0)
        assertEquals(61.3, p.t_ogrz!!, 0.0)
        assertEquals(1012.3, p.cisnienie!!, 0.0)
        assertEquals(55.0, p.wilgotnosc!!, 0.0)
        assertEquals(50.0, p.klapa!!, 0.0)   // 90° z 180°
        assertEquals(50.0, p.syberka!!, 0.0) // 45° z 90°
        assertEquals(true, p.pumpOn)
        assertEquals(false, points[1].pumpOn)
        assertNull(p.t_trociny)
        assertNull(p.dym)
    }

    @Test
    fun invalidReadingsAndUnknownTimeAreDropped() {
        val csv = listOf(
            shortLine(t0, ogrz = "-127.0"),        // odłączony DS18B20
            shortLine(t0 + 60, ogrz = "nan"),      // brak odczytu
            shortLine(0L),                         // centrala bez wiarygodnego czasu
            "12:03,1,2,3",                         // ucięta linia
            shortLine(t0 + 180, klapa = "200")     // kąt poza zakresem
        ).joinToString("\n")
        val points = EspLocalHistory.parseShort(csv)
        assertEquals(listOf(t0 * 1000, (t0 + 60) * 1000, (t0 + 180) * 1000), points.map { it.ts })
        assertNull(points[0].t_ogrz)
        assertNull(points[1].t_ogrz)
        assertNull(points[2].klapa)
        assertEquals(47.2, points[0].t_bojler!!, 0.0) // pozostałe kanały zostają
    }

    @Test
    fun longHistoryUsesAveragesAndOptionalPumpColumn() {
        val withPump = EspLocalHistory.parseLong(longLine(t0, 70.0, pompa = "1"))
        val legacy = EspLocalHistory.parseLong(longLine(t0, 70.0))
        assertEquals(1, withPump.size)
        assertEquals(70.0, withPump[0].t_ogrz!!, 0.0)
        assertEquals(1012.3, withPump[0].cisnienie!!, 0.0)
        assertEquals(67.0, withPump[0].klapa!!, 0.0) // 120° z 180°
        assertEquals(true, withPump[0].pumpOn)
        assertNull(legacy[0].pumpOn)
    }

    @Test
    fun combineUsesLongArchiveBeforeLast24hAndMinuteBufferAfter() {
        val endMs = (t0 + 3 * 86_400L) * 1000
        val longPts = EspLocalHistory.parseLong((0 until 72).joinToString("\n") { longLine(t0 + it * 3600L, 60.0) })
        val shortPts = EspLocalHistory.parseShort((0 until 1440).joinToString("\n") { shortLine(t0 + 2 * 86_400L + it * 60L) })
        val out = EspLocalHistory.combine(shortPts, longPts, endMs - 7 * 86_400_000L, endMs)
        val cutoff = endMs - 86_400_000L
        assertTrue(out.filter { it.ts < cutoff }.all { p -> longPts.any { it.ts == p.ts } })
        assertTrue(out.filter { it.ts >= cutoff }.all { p -> shortPts.any { it.ts == p.ts } })
        assertEquals(out.sortedBy { it.ts }, out)
    }

    @Test
    fun localTailOnlyExtendsPhoneCopy() {
        val cached = EspLocalHistory.parseShort((0 until 10).joinToString("\n") { shortLine(t0 + it * 60L) })
        val local = EspLocalHistory.parseShort((5 until 20).joinToString("\n") { shortLine(t0 + it * 60L, ogrz = "70.0") })
        val merged = EspLocalHistory.appendAfter(cached, local)
        assertEquals(20, merged.size)
        assertEquals(cached, merged.take(10))                       // kopia z telefonu ma pierwszeństwo
        assertTrue(merged.drop(10).all { it.t_ogrz == 70.0 })      // dalej wyłącznie archiwum centrali
        assertEquals(local, EspLocalHistory.appendAfter(emptyList(), local))
        assertFalse(EspLocalHistory.appendAfter(cached, emptyList()).isEmpty())
    }
}
