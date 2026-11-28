package com.sterownikco.pro.core

import org.junit.Assert.*
import org.junit.Test

class HumidityTileTest {
    private val now = 1_700_000_000_000L
    private fun tile(hu: Double, stale: Boolean = false, simulated: Boolean = false, rh: Double = 78.4,
                     ageMs: Long = 60_000L, enabled: Boolean = true, dhtAlive: Boolean = true) =
        humidityTile(hu, simulated, 7, stale, forecastRh = rh,
            forecastAtMs = if (ageMs < 0) 0L else now - ageMs, nowMs = now, netFallback = enabled, dhtAlive = dhtAlive)

    @Test fun sensorReadingWins() {
        val t = tile(55.0)
        assertEquals("55", t.value); assertEquals("LIVE", t.badge); assertFalse(t.net); assertEquals("w normie", t.desc)
        assertEquals("podwyższona", tile(80.0).desc)
    }

    @Test fun missingOrStaleSensorUsesInternetHumidity() {
        val missing = tile(Double.NaN)
        assertEquals("78", missing.value); assertEquals("PROGNOZA", missing.badge); assertTrue(missing.net); assertFalse(missing.stale)
        assertTrue(missing.desc.startsWith("DHT22 brak danych · internet z "))
        val stale = tile(55.0, stale = true)
        assertEquals("78", stale.value); assertTrue(stale.net)
        assertTrue(stale.desc.startsWith("DHT22 nieaktualne · internet"))
    }

    @Test fun noFallbackWhenDisabledSimulatedOrForecastUnusable() {
        val off = tile(Double.NaN, enabled = false)
        assertEquals("—", off.value); assertEquals("BRAK DANYCH", off.badge); assertFalse(off.net)
        val sim = tile(40.0, stale = true, simulated = true)
        assertEquals("SYMULACJA 7M", sim.badge); assertEquals("40", sim.value); assertFalse(sim.net)
        assertEquals("NIEAKTUALNE", tile(55.0, stale = true, ageMs = 3 * 60 * 60_000L).badge)
        assertEquals("BRAK DANYCH", tile(Double.NaN, rh = Double.NaN).badge)
    }

    /** [0.31.105] Centrala trzyma ostatnią wilgotność po padzie DHT22 (81 % LIVE bez końca) — t_zewn_2 = null zdradza pad. */
    @Test fun heldHumidityAfterDhtDeathUsesInternet() {
        val t = tile(81.0, dhtAlive = false)
        assertEquals("78", t.value); assertEquals("PROGNOZA", t.badge); assertTrue(t.net)
        assertTrue(t.desc.startsWith("DHT22 nie odpowiada · internet z "))
        // auto wyłączone → pokazujemy ostatnią wartość, ale uczciwie jako NIEAKTUALNE
        val off = tile(81.0, dhtAlive = false, enabled = false)
        assertEquals("81", off.value); assertEquals("NIEAKTUALNE", off.badge); assertTrue(off.stale); assertFalse(off.net)
        assertEquals("DHT22 nie odpowiada · ostatnia wartość", off.desc)
        // symulacja wilgotności nie zależy od DHT22
        assertEquals("SYMULACJA 7M", tile(40.0, simulated = true, dhtAlive = false).badge)
    }
}
