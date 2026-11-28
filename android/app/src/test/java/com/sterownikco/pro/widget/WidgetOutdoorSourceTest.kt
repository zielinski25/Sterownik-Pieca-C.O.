package com.sterownikco.pro.widget

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class WidgetOutdoorSourceTest {
    @Test fun outdoorSlotFollowsTheSharedChoice() {
        assertEquals("t_zewn", WidgetOutdoorSource.key("t_zewn", 1))
        assertEquals("t_zewn_2", WidgetOutdoorSource.key("t_zewn", 2))
        assertEquals("pogoda_temp", WidgetOutdoorSource.key("t_zewn", 3))
        assertEquals("t_zewn", WidgetOutdoorSource.key("t_zewn", -1))
        assertEquals("t_zewn", WidgetOutdoorSource.key("t_zewn", 4))
        assertEquals("t_bojler", WidgetOutdoorSource.key("t_bojler", 2))
        assertEquals("t_bojler", WidgetOutdoorSource.key("t_bojler", 3))
    }

    private val now = 1_700_000_000_000L

    @Test fun forecastIsUsedOnlyWhenFreshAndSane() {
        assertEquals(-2.5, WidgetForecast.read("-2.5", now - 5 * 60_000L, now)!!, 1e-9)
        assertEquals(7.0, WidgetForecast.read("7,0", now - 5 * 60_000L, now)!!, 1e-9)
        assertEquals(0.0, WidgetForecast.read("0.0", now, now)!!, 1e-9)
        assertNull(WidgetForecast.read("5.0", now - 3 * 60 * 60_000L, now))   // za stara
        assertNull(WidgetForecast.read("5.0", 0L, now))                        // brak czasu
        assertNull(WidgetForecast.read("5.0", now + 60_000L, now))             // zegar cofnięty
        assertNull(WidgetForecast.read(null, now, now))
        assertNull(WidgetForecast.read("abc", now, now))
        assertNull(WidgetForecast.read("99", now, now))
    }

    @Test fun forecastFormatsAndColorsLikeOutdoorSensor() {
        assertEquals("-2,5°", WidgetTemperatureFormat.degrees("pogoda_temp", -2.5, false))
        assertEquals("—", WidgetTemperatureFormat.degrees("pogoda_temp", null, false))
        assertEquals(
            WidgetTemperatureColors.colorHex("t_zewn", -2.5),
            WidgetTemperatureColors.colorHex("pogoda_temp", -2.5)
        )
    }

    @Test fun widgetFallsBackToForecastOnlyForOutdoorAndOnlyWhenNeeded() {
        assertEquals(4.2 to true, WidgetNetFallback.pick("t_zewn_2", null, false, false, 4.2, true))
        assertEquals(4.2 to true, WidgetNetFallback.pick("t_zewn", 12.0, true, false, 4.2, true))
        assertEquals(12.0 to false, WidgetNetFallback.pick("t_zewn", 12.0, false, false, 4.2, true))
        assertEquals(null to false, WidgetNetFallback.pick("t_zewn_2", null, false, false, 4.2, false))
        assertEquals(null to false, WidgetNetFallback.pick("t_zewn_2", null, false, false, null, true))
        assertEquals(12.0 to false, WidgetNetFallback.pick("t_zewn", 12.0, true, true, 4.2, true))
        assertEquals(null to false, WidgetNetFallback.pick("t_bojler", null, true, false, 4.2, true))
        assertEquals(4.2 to true, WidgetNetFallback.pick("pogoda_temp", 4.2, true, false, 4.2, true))
        assertEquals(null to false, WidgetNetFallback.pick("pogoda_temp", null, false, false, null, true))
    }
}
