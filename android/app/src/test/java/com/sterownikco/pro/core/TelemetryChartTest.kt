package com.sterownikco.pro.core

import com.sterownikco.pro.ui.chart.buildChartView
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class TelemetryChartTest {
    private fun point(ts: Long, temp: Double, live: Boolean = false) = TelemPoint(
        ts = ts, seq = ts,
        t_zewn = null, t_bojler = null, t_ogrz = temp, t_ogrz_sr = null,
        t_powrot = null, t_panel = null, t_pokoj = null, t_trociny = null,
        wilgotnosc = null, cisnienie = null, dym = null, klapa = null, syberka = null,
        isLive = live
    )

    private fun pumpPoint(state: Long, stateKnown: Boolean = true) = TelemRow(
        id = "session-1", seq = 1, ts = 1_000, mono = 0,
        a = emptyList(), k = -1, s = -1, state = state, q = 0, sim = 0,
        stateKnown = stateKnown
    ).toPoint()

    @Test
    fun pumpSeriesMapsStateBitToOnAndOffAndKeepsMissingStateUnknown() {
        val on = pumpPoint(state = PUMP_ON_STATE_BIT)
        val off = pumpPoint(state = 0L)
        val unknown = pumpPoint(state = 0L, stateKnown = false)
        val simulatedRecord = TelemRow(
            id = "session-1", seq = 2, ts = 2_000, mono = 0,
            a = emptyList(), k = -1, s = -1, state = PUMP_ON_STATE_BIT, q = 0, sim = 0,
            simulated = true, stateKnown = true
        ).toPoint()

        assertEquals(true, on.pumpOn)
        assertEquals(100.0, ChartSeries.value(on, "pompa")!!, 0.001)
        assertEquals(false, off.pumpOn)
        assertEquals(0.0, ChartSeries.value(off, "pompa")!!, 0.001)
        assertNull(unknown.pumpOn)
        assertNull(ChartSeries.value(unknown, "pompa"))
        assertEquals(true, simulatedRecord.pumpOn)
        assertTrue(on.hasValues())
    }

    @Test
    fun missingOrInvalidTelemetryStateRemainsUnknown() {
        assertNull(parseTelemetryState(null))
        assertNull(parseTelemetryState("0"))
        assertNull(parseTelemetryState(1.5))
        assertEquals(0L, parseTelemetryState(0))
        assertEquals(PUMP_ON_STATE_BIT, parseTelemetryState(1L))
    }

    @Test
    fun pumpChartUsesFixedCategoricalScale() {
        val rows = listOf(
            point(1_000, 20.0).copy(pumpOn = false),
            point(61_000, 21.0).copy(pumpOn = true)
        )

        val view = buildChartView(
            dataset = rows, catalog = ChartSeries.PUMP, zoom = 1f, offset = 1f,
            focus = 3, mode = "COMMON", glitch = true
        )

        assertEquals(0.0, view.min, 0.001)
        assertEquals(100.0, view.max, 0.001)
        assertEquals("", view.axisUnit)
    }

    @Test
    fun pumpTransitionsRemainBinaryAndAreNotSmoothedAway() {
        val rows = listOf(
            point(1_000, 20.0).copy(pumpOn = false),
            point(61_000, 20.0).copy(pumpOn = true),
            point(121_000, 21.0).copy(pumpOn = false)
        )

        val filtered = ChartSeries.filterGlitches(rows, "pompa", "pump")

        assertEquals(listOf(0.0, 100.0, 0.0), filtered)
    }

    @Test
    fun freshLiveEndpointIsNotHiddenByStepFilter() {
        val rows = listOf(point(1_000, 76.0), point(61_000, 76.0), point(121_000, 53.5, live = true))

        val filtered = ChartSeries.filterGlitches(rows, "t_ogrz", "temp")

        assertEquals(53.5, filtered.last()!!, 0.001)
    }

    @Test
    fun historicalEndpointOutlierStillGetsFiltered() {
        val rows = listOf(point(1_000, 76.0), point(61_000, 76.0), point(121_000, 53.5))

        val filtered = ChartSeries.filterGlitches(rows, "t_ogrz", "temp")

        assertNull(filtered.last())
    }

    @Test
    fun liveEndpointStillMustPassPhysicalSensorBounds() {
        val rows = listOf(point(1_000, 76.0), point(61_000, 76.0), point(121_000, 150.0, live = true))

        val filtered = ChartSeries.filterGlitches(rows, "t_ogrz", "temp")

        assertNull(filtered.last())
    }

    @Test
    fun historyAndLivePointsAreMergedChronologically() {
        val history = listOf(point(1_000, 30.0), point(3_000, 32.0))
        val live = listOf(point(4_000, 34.0, live = true), point(2_000, 31.0, live = true))

        val merged = mergeChartPoints(history, live)

        assertEquals(listOf(1_000L, 2_000L, 3_000L, 4_000L), merged.map { it.ts })
    }

    @Test
    fun liveObservationAtSameTimeFollowsArchiveObservation() {
        val history = listOf(point(1_000, 30.0), point(2_000, 32.0))
        val live = listOf(point(2_000, 35.0, live = true))

        val merged = mergeChartPoints(history, live)

        assertEquals(listOf(1_000L, 2_000L, 2_000L), merged.map { it.ts })
        assertEquals(32.0, merged[1].t_ogrz!!, 0.001)
        assertEquals(35.0, merged[2].t_ogrz!!, 0.001)
    }

    @Test
    fun duplicateLiveTimestampKeepsNewestSnapshot() {
        val live = listOf(point(2_000, 31.0, live = true), point(2_000, 33.0, live = true))

        val merged = mergeChartPoints(emptyList(), live)

        assertEquals(1, merged.size)
        assertEquals(33.0, merged.single().t_ogrz!!, 0.001)
    }
}
