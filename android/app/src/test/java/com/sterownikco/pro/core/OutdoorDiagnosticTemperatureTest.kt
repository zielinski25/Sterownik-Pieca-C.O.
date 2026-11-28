package com.sterownikco.pro.core

import com.sterownikco.pro.ui.chart.buildChartView
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test
import java.nio.file.Files

class OutdoorDiagnosticTemperatureTest {
    private fun row(t2: Double? = null, simulated: Boolean = false) = TelemRow(
        id = "test", seq = 1, ts = 1_700_000_000, mono = 0,
        a = listOf(60.0), k = 0, s = 0, state = 0, q = 0, sim = 0,
        simulated = simulated, stateKnown = true, t_zewn_2 = t2
    )

    @Test fun chartsHaveTwoOutdoorSeriesAndDashboardIsOriginal() {
        val defs = ChartSeries.TEMP.associateBy { it.id }
        assertEquals("Zew 1", defs.getValue("zewn").label)
        assertEquals("Zew 2", defs.getValue("zewn_2").label)
        assertFalse(defs.containsKey("zewn_3"))
        assertEquals("Zewnętrzna", TileDefs.TILES.first { it.id == "zewn" }.title)
        assertFalse(TileDefs.TILES.any { it.id == "zewn_2" || it.id == "zewn_3" })
    }

    @Test fun liveStatusRejectsInvalidAndDoesNotConfuseZeroWithMissing() {
        val s = PiecState()
        s.applyIncomingData(JSONObject("""{"t_zewn":12.5,"t_zewn_2":0,"t_zewn_3":-12.3}"""))
        assertEquals(0.0, s.t_zewn_2, 0.0)
        assertFalse(s.hasData("t_zewn_3")) // previous firmware may still send it
        s.applyIncomingData(JSONObject("""{"t_zewn_2":null}"""))
        assertFalse(s.hasData("t_zewn_2"))
        assertTrue(s.t_zewn_2.isNaN())
    }

    @Test fun historyHandlesOldValidInvalidAndSimulatedSamples() {
        assertNull(ChartSeries.value(row().toPoint(), "t_zewn_2"))
        assertEquals(0.0, ChartSeries.value(row(0.0).toPoint(), "t_zewn_2")!!, 0.0)
        assertNull(row(-40.1).toPoint().t_zewn_2)
        assertNull(row(Double.NaN).toPoint().t_zewn_2)
        assertNull(row(10.0, simulated = true).toPoint().t_zewn_2)
    }

    @Test fun chartBuildsDistinctSelectedLinesWithoutInventingValues() {
        val first = row(0.0).toPoint()
        val second = row(1.0).toPoint().copy(ts = first.ts + 60_000)
        val series = ChartSeries.TEMP.filter { it.id in setOf("zewn", "zewn_2") }.map { it.copy(on = true) }
        val view = buildChartView(listOf(first, second), series, 1f, 1f, "COMMON", false)
        assertEquals(listOf(6.0, 6.0), view.values["zewn"])
        assertEquals(listOf(0.0, 1.0), view.values["zewn_2"])
    }

    @Test fun dayCacheReadsOldAndPreviousThreeSeriesRows() {
        val dir = Files.createTempDirectory("outdoor-chart").toFile()
        try {
            val cache = TelemetryDayCache(dir)
            cache.write("2026-11-15", listOf(row(0.0)))
            assertEquals(0.0, cache.read("2026-11-15")!!.rows.single().t_zewn_2!!, 0.0)
            java.io.File(dir, "2026-11-14.json").writeText(
                """{"v":1,"saved":1,"rows":[["old",1,1700000000,0,[60],0,0,0,0,0,0,1]]}"""
            )
            assertNull(cache.read("2026-11-14")!!.rows.single().t_zewn_2)
            java.io.File(dir, "2026-11-13.json").writeText(
                """{"v":1,"saved":1,"rows":[["previous",1,1700000000,0,[60],0,0,0,0,0,0,1,2.5,-12.3]]}"""
            )
            assertEquals(2.5, cache.read("2026-11-13")!!.rows.single().t_zewn_2!!, 0.0)
        } finally { dir.deleteRecursively() }
    }
    @Test fun bothOutdoorReadingsRemainIndependentWhenDhtIsMissing() {
        val s = PiecState()
        s.applyIncomingData(JSONObject("""{"t_zewn":9.0,"t_zewn_2":0.0}"""))
        assertEquals(9.0, s.valOf("zewn", "t_zewn"), 0.0)
        assertEquals(0.0, s.valOf("zewn_2", "t_zewn_2"), 0.0)
        s.applyIncomingData(JSONObject("""{"t_zewn":8.5,"t_zewn_2":null}"""))
        assertEquals(8.5, s.valOf("zewn", "t_zewn"), 0.0)
        assertTrue(s.valOf("zewn_2", "t_zewn_2").isNaN())
        assertFalse(s.hasData("t_zewn_2"))
    }

}
