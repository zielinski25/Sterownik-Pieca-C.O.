package com.sterownikco.pro.widget

import org.junit.Assert.*
import org.junit.Test

class WidgetTemperatureBoundsTest {
    @Test fun allFourSensorsAcceptLowReadingsAtTheBoundary() {
        for (key in listOf("t_ogrz", "t_bojler", "t_panel", "t_zewn")) {
            for (simulated in listOf(false, true)) {
                for (v in listOf(-25.0, -10.0, -0.1, 0.0, 5.0)) {
                    assertEquals("$key / $simulated / $v", v, WidgetTemperatureBounds.value(key, v, simulated)!!, 0.0)
                }
                assertNull(WidgetTemperatureBounds.value(key, -25.1, simulated))
                assertNull(WidgetTemperatureBounds.value(key, Double.NaN, simulated))
                assertNull(WidgetTemperatureBounds.value(key, Double.POSITIVE_INFINITY, simulated))
                assertNull(WidgetTemperatureBounds.value(key, null, simulated))
                assertNull(WidgetTemperatureBounds.value(key, "-10", simulated))
            }
        }
    }

    @Test fun upperBoundsRemainUnchanged() {
        val normal = mapOf("t_ogrz" to 99.0, "t_bojler" to 95.0, "t_panel" to 140.0, "t_zewn" to 45.0)
        val simulated = mapOf("t_ogrz" to 160.0, "t_bojler" to 160.0, "t_panel" to 160.0, "t_zewn" to 50.0)
        for ((simulation, bounds) in listOf(false to normal, true to simulated)) {
            for ((key, max) in bounds) {
                assertEquals(max, WidgetTemperatureBounds.value(key, max, simulation)!!, 0.0)
                assertNull(WidgetTemperatureBounds.value(key, max + 0.1, simulation))
            }
        }
        assertNull(WidgetTemperatureBounds.value("unknown", 0.0, false))
    }
    @Test fun secondOutdoorSensorHasIndependentBounds() {
        for (simulation in listOf(false, true)) {
            assertEquals(-25.0, WidgetTemperatureBounds.value("t_zewn_2", -25.0, simulation)!!, 0.0)
            assertEquals(0.0, WidgetTemperatureBounds.value("t_zewn_2", 0.0, simulation)!!, 0.0)
            assertEquals(80.0, WidgetTemperatureBounds.value("t_zewn_2", 80.0, simulation)!!, 0.0)
            assertNull(WidgetTemperatureBounds.value("t_zewn_2", -25.1, simulation))
            assertNull(WidgetTemperatureBounds.value("t_zewn_2", 80.1, simulation))
            assertNull(WidgetTemperatureBounds.value("t_zewn_2", null, simulation))
        }
    }

}
