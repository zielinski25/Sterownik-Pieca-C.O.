package com.sterownikco.pro.ui.chart

import com.sterownikco.pro.core.TelemPoint
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

/**
 * [v0.31.33 TIMELINE] Oś X liczona z CZASU, nie z numeru próbki.
 *
 * Dlaczego to ma znaczenie: przy osi indeksowej 10-minutowa przerwa w transmisji
 * zajmowała tyle samo miejsca co minuta, więc wykres „sklejał" godziny i etykiety
 * kłamały (zrzut 11:27 pokazywał prawą krawędź 10:34, choć dane sięgały 11:26).
 */
class ChartTimeAxisTest {

    private fun point(ts: Long, seq: Long = 0L) = TelemPoint(
        ts = ts, seq = seq,
        t_zewn = null, t_bojler = null, t_ogrz = null, t_ogrz_sr = null,
        t_powrot = null, t_panel = null, t_pokoj = null, t_trociny = null,
        wilgotnosc = null, cisnienie = null, dym = null, klapa = null, syberka = null
    )

    @Test
    fun placesSamplesProportionallyToElapsedTime() {
        // 0, 4, 6, 12 minut — oś liczona z czasu, nie z numeru próbki.
        val axis = ChartTimeAxis(t0 = 0L, t1 = 12 * 60_000L, padLeft = 20f, plotWidth = 240f)
        assertEquals(20f, axis.xOf(0L), 0.01f)
        // 4 z 12 minut = 1/3 szerokości: 20 + 240/3 = 100 (wcześniej stało tu 60,
        // czyli wartość dla 2 minut - test był błędny, nie wykres).
        assertEquals(100f, axis.xOf(4 * 60_000L), 0.01f)
        assertEquals(140f, axis.xOf(6 * 60_000L), 0.01f)  // połowa
        assertEquals(260f, axis.xOf(12 * 60_000L), 0.01f) // prawa krawędź
    }

    @Test
    fun clampsOutsideTheWindow() {
        val axis = ChartTimeAxis(t0 = 1_000L, t1 = 2_000L, padLeft = 10f, plotWidth = 100f)
        assertEquals(10f, axis.xOf(0L), 0.01f)
        assertEquals(110f, axis.xOf(9_999L), 0.01f)
        assertEquals(1_000L, axis.tsAtX(0f).toLong())
        assertEquals(2_000L, axis.tsAtX(999f).toLong())
    }

    @Test
    fun degenerateWindowDoesNotDivideByZero() {
        val axis = ChartTimeAxis(t0 = 5_000L, t1 = 5_000L, padLeft = 10f, plotWidth = 100f)
        assertEquals(1L, axis.spanMs.toLong())          // span zawsze >= 1 ms
        assertEquals(10f, axis.xOf(5_000L), 0.01f)
        // Jedna próbka/ten sam czas = wracamy do starego mapowania indeksowego.
        assertEquals(0, chartSampleIndexAtTimeX(10f, 10f, 100f, listOf(point(5_000L), point(5_000L))))
    }

    @Test
    fun picksNearestSampleEvenAfterAGap() {
        // 0, 1, 2 min … potem 12 min (dziura 10 min) … 13, 14 min.
        val rows = listOf(
            point(0L), point(60_000L), point(120_000L),
            point(12 * 60_000L), point(13 * 60_000L), point(14 * 60_000L)
        )
        val padLeft = 0f
        val plotW = 840f  // 1 minuta = 60 px przy 14 minutach
        // Palec na 2,5 min: blisko próbki z 2 min (indeks 2), nie „środka tabeli".
        assertEquals(2, chartSampleIndexAtTimeX(2.5f * 60f, padLeft, plotW, rows))
        // Palec na 11 min: najbliższa jest próbka z 12 min (indeks 3).
        assertEquals(3, chartSampleIndexAtTimeX(11f * 60f, padLeft, plotW, rows))
        // Palec na 12,6 min: próbka z 13 min (indeks 4).
        assertEquals(4, chartSampleIndexAtTimeX(12.6f * 60f, padLeft, plotW, rows))
    }

    @Test
    fun ignoresInvalidPlot() {
        val rows = listOf(point(0L), point(60_000L))
        assertNull(chartSampleIndexAtTimeX(10f, 0f, 0f, rows))
        assertNull(chartSampleIndexAtTimeX(10f, 0f, Float.NaN, rows))
        assertNull(chartSampleIndexAtTimeX(10f, 0f, 100f, listOf(point(0L))))
    }

    @Test
    fun labelTimesFollowTheClockNotTheRowNumber() {
        val axis = ChartTimeAxis(t0 = 0L, t1 = 3_600_000L, padLeft = 0f, plotWidth = 100f)
        // 4 podziałki co 15 minut — równe odstępy w czasie, nie w liczbie próbek.
        assertEquals(0L, axis.tsAtFraction(0.0).toLong())
        assertEquals(900_000L, axis.tsAtFraction(0.25).toLong())
        assertEquals(1_800_000L, axis.tsAtFraction(0.5).toLong())
        assertEquals(3_600_000L, axis.tsAtFraction(1.0).toLong())
    }
}
