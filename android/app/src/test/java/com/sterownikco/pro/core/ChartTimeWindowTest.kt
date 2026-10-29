package com.sterownikco.pro.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ChartTimeWindowTest {
    @Test
    fun elapsedTimestampControlsXPositionInsteadOfListIndex() {
        val window = ChartTimeWindow.calculate(0L, 1_000L, zoom = 1f, offset = 1f)
        val timestamps = listOf(0L, 100L, 1_000L)

        assertEquals(0f, chartTimestampFraction(timestamps[0], window.startMs, window.endMs), 0.0001f)
        assertEquals(0.1f, chartTimestampFraction(timestamps[1], window.startMs, window.endMs), 0.0001f)
        assertEquals(1f, chartTimestampFraction(timestamps[2], window.startMs, window.endMs), 0.0001f)
    }

    @Test
    fun zoomAndOffsetSelectTimeIntervalsAtBothEnds() {
        val oldest = ChartTimeWindow.calculate(1_000L, 2_000L, zoom = 2f, offset = 1f)
        val newest = ChartTimeWindow.calculate(1_000L, 2_000L, zoom = 2f, offset = 0f)

        assertEquals(1_000L, oldest.startMs)
        assertEquals(1_500L, oldest.endMs)
        assertEquals(1_500L, newest.startMs)
        assertEquals(2_000L, newest.endMs)
    }

    @Test
    fun crosshairChoosesNearestTimestamp() {
        val index = nearestChartTimestampIndex(listOf(0L, 100L, 1_000L), 750L)
        assertEquals(2, index)
        assertEquals(1, nearestChartTimestampIndex(listOf(0L, 100L, 1_000L), 550L))
        assertEquals(null, nearestChartTimestampIndex(emptyList(), 0L))
    }

    @Test
    fun overlappingArchiveReadsDeduplicateByRtdbIdAndClipToRange() {
        val merged = ChartArchive.merge(
            existing = listOf(point("keep", 200L, 2L), point("old", 100L, 1L)),
            incoming = listOf(point("keep", 250L, 3L), point("new", 300L, 4L), point("future", 1_001L, 5L)),
            startMs = 0L,
            endMs = 1_000L
        )

        assertEquals(listOf("old", "keep", "new"), merged.map { it.sourceId })
        assertEquals(listOf(100L, 250L, 300L), merged.map { it.ts })
    }

    @Test
    fun rtdbServoMappingKeepsTheLocalChartsFractionalPercent() {
        val point = TelemRow(
            id = "servo-1", seq = 1L, ts = 1_800_000_000L, mono = 0L,
            a = listOf(100.0, 500.0, 550.0, 540.0, 400.0, 20.0, 200.0, 200.0, 500.0, 10_000.0, 123.0),
            k = 1L, s = 1L, state = 0L, q = 0L, sim = 0L
        ).toPoint()

        assertNotNull(point.t_panel)
        assertEquals(2.0, point.t_panel!!, 0.0001)
        assertEquals(100.0 / 180.0, point.klapa!!, 0.0001)
        assertEquals(100.0 / 90.0, point.syberka!!, 0.0001)
        assertEquals(1_800_000_000_000L, point.ts)
        assertEquals("servo-1", point.sourceId)
    }

    @Test
    fun pumpSeriesUsesKnownStateBitAndKeepsDiscreteTransitions() {
        fun row(id: String, state: Long, known: Boolean) = TelemRow(
            id = id, seq = 1L, ts = 1_800_000_000L, mono = 0L,
            a = emptyList(), k = -1L, s = -1L, state = state, q = 0L, sim = 0L,
            stateKnown = known
        ).toPoint()

        val stop = row("pump-stop", 0L, true)
        val run = row("pump-run", 1L, true)
        val unknown = row("pump-unknown", 0L, false)

        assertEquals(false, stop.pumpOn)
        assertEquals(true, run.pumpOn)
        assertNull(unknown.pumpOn)
        assertEquals(0.0, ChartSeries.value(stop, "pompa")!!, 0.0)
        assertEquals(100.0, ChartSeries.value(run, "pompa")!!, 0.0)
        assertTrue(stop.hasValues())
        assertEquals(listOf(0.0, 100.0), ChartSeries.filterGlitches(
            listOf(stop, run), "pompa", "pump", enabled = true
        ))
    }

    private fun point(id: String, timestamp: Long, seq: Long) = TelemPoint(
        ts = timestamp, seq = seq,
        t_zewn = null, t_bojler = null, t_ogrz = null, t_ogrz_sr = null,
        t_powrot = null, t_panel = null, t_pokoj = null, t_trociny = null,
        wilgotnosc = null, cisnienie = null, dym = null, klapa = null, syberka = null,
        sourceId = id
    )
}
