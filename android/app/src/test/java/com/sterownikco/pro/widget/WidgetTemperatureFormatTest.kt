package com.sterownikco.pro.widget

import org.junit.Assert.assertEquals
import org.junit.Test

class WidgetTemperatureFormatTest {
    @Test fun bothOutdoorSensorsHaveOneDecimalAndPolishComma() {
        for (key in listOf("t_zewn", "t_zewn_2")) {
            assertEquals("5,9°", WidgetTemperatureFormat.degrees(key, 5.9, false))
            assertEquals("-10,5°", WidgetTemperatureFormat.degrees(key, -10.5, false))
            assertEquals("0,0°", WidgetTemperatureFormat.degrees(key, 0.0, false))
            assertEquals("~9,5°", WidgetTemperatureFormat.degrees(key, 9.5, true))
            assertEquals("—", WidgetTemperatureFormat.degrees(key, null, false))
            assertEquals("—", WidgetTemperatureFormat.degrees(key, Double.NaN, true))
        }
    }

    @Test fun otherSensorsKeepIntegerFormattingAndSimulationMarker() {
        assertEquals("8°", WidgetTemperatureFormat.degrees("t_ogrz", 7.5, false))
        assertEquals("~21°", WidgetTemperatureFormat.degrees("t_bojler", 21.2, true))
    }
}
