package com.sterownikco.pro.widget

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test

class WidgetTemperatureColorsTest {
    @Test
    fun sameTemperatureUsesDifferentScaleForOutdoorAndHeatingCircuit() {
        val outdoor = WidgetTemperatureColors.colorHex("t_zewn", 22.0)
        val heatingCircuit = WidgetTemperatureColors.colorHex("t_ogrz", 22.0)

        assertEquals("#A3E635", outdoor)      // łagodna temperatura na zewnątrz
        assertEquals("#60A5FA", heatingCircuit) // za chłodno dla obiegu C.O.
        assertNotEquals(outdoor, heatingCircuit)
    }

    @Test
    fun operatingTemperaturesAreInterpretedPerSensor() {
        assertEquals("#4ADE80", WidgetTemperatureColors.colorHex("t_ogrz", 69.0))
        assertEquals("#FF5F78", WidgetTemperatureColors.colorHex("t_bojler", 69.0))
        assertEquals("#FBBF24", WidgetTemperatureColors.colorHex("t_panel", 69.0))
    }

    @Test
    fun absentInvalidAndUnknownReadingsUseMutedColor() {
        assertEquals("#7F93A3", WidgetTemperatureColors.colorHex("t_ogrz", null))
        assertEquals("#7F93A3", WidgetTemperatureColors.colorHex("t_zewn", Double.NaN))
        assertEquals("#7F93A3", WidgetTemperatureColors.colorHex("unknown", 42.0))
    }
}
