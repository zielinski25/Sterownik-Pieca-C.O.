package com.sterownikco.pro.ui.chart

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class ChartScrubIndexTest {
    @Test
    fun mapsFingerPositionToVisibleSampleAndClampsAtPlotEdges() {
        assertEquals(0, chartSampleIndexAtX(x = 20f, padLeft = 20f, plotWidth = 100f, sampleCount = 11))
        assertEquals(5, chartSampleIndexAtX(x = 70f, padLeft = 20f, plotWidth = 100f, sampleCount = 11))
        assertEquals(10, chartSampleIndexAtX(x = 120f, padLeft = 20f, plotWidth = 100f, sampleCount = 11))
        assertEquals(0, chartSampleIndexAtX(x = -30f, padLeft = 20f, plotWidth = 100f, sampleCount = 11))
        assertEquals(10, chartSampleIndexAtX(x = 180f, padLeft = 20f, plotWidth = 100f, sampleCount = 11))
    }

    @Test
    fun scrubbingUsesElapsedTimeWhenSampleSpacingIsUneven() {
        val timestamps = listOf(0L, 100L, 1_000L)
        // At 60% of elapsed time the nearest timestamp is 1,000 ms (index 2),
        // whereas an index-based mapping would incorrectly select index 1.
        assertEquals(2, chartSampleIndexAtX(x = 60f, padLeft = 0f, plotWidth = 100f, sortedTimestamps = timestamps))
    }

    @Test
    fun ignoresEmptyOrInvalidPlot() {
        assertNull(chartSampleIndexAtX(x = 50f, padLeft = 20f, plotWidth = 100f, sampleCount = 1))
        assertNull(chartSampleIndexAtX(x = 50f, padLeft = 20f, plotWidth = 0f, sampleCount = 11))
        assertNull(chartSampleIndexAtX(x = 50f, padLeft = 20f, plotWidth = Float.NaN, sampleCount = 11))
    }
}
