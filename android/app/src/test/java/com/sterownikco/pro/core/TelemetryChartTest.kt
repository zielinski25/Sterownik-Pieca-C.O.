package com.sterownikco.pro.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class TelemetryChartTest {
    private fun point(ts: Long, temp: Double, live: Boolean = false) = TelemPoint(
        ts = ts, seq = ts,
        t_zewn = null, t_bojler = null, t_ogrz = temp, t_ogrz_sr = null,
        t_powrot = null, t_panel = null, t_pokoj = null, t_trociny = null,
        wilgotnosc = null, cisnienie = null, dym = null, klapa = null, syberka = null,
        isLive = live
    )

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
}
