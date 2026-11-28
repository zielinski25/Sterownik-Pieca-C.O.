package com.sterownikco.pro.core

import org.junit.Assert.*
import org.junit.Test

class OutdoorTileSourceTest {
    private fun tile(source: Int, ds: Double = 12.0, dht: Double = 8.5, simulated: Boolean = false, stale: Boolean = false) =
        outdoorTemperatureTile(source, ds, dht, simulated, 5, stale)

    @Test fun eachSourceDisplaysItsOwnReadingAndName() {
        val ds = tile(1)
        val dht = tile(2)
        assertEquals("Zewnętrzna", ds.title)
        assertEquals("12.0", ds.value)
        assertEquals("8.5", dht.value)
        assertTrue(ds.desc.startsWith("Zew 1"))
        assertTrue(dht.desc.startsWith("Zew 2"))
        assertEquals("LIVE", dht.badge)
    }

    @Test fun noFallbackAndZeroIsNotMissing() {
        val missing = tile(2, ds = 12.0, dht = Double.NaN)
        assertEquals("—", missing.value)
        assertEquals("BRAK DANYCH", missing.badge)
        assertEquals("0.0", tile(2, dht = 0.0).value)
        assertEquals("-12.5", tile(2, dht = -12.5).value)
    }

    @Test fun dsSimulationDoesNotMarkDhtSimulated() {
        val ds = tile(1, simulated = true)
        val dht = tile(2, simulated = true)
        assertTrue(ds.sim)
        assertEquals("SYMULACJA 5M", ds.badge)
        assertFalse(dht.sim)
        assertEquals("LIVE", dht.badge)
    }

    @Test fun staleDataAndInvalidSelectionAreHandled() {
        assertEquals("NIEAKTUALNE", tile(2, stale = true).badge)
        assertTrue(tile(2, stale = true).stale)
        assertEquals("12.0", tile(99).value)
    }

    // ── [0.31.102] źródło 3: Pogoda · Open-Meteo ──────────────────────────────
    private val now = 1_700_000_000_000L
    private fun forecastTile(temp: Double, ageMs: Long, stale: Boolean = false, simulated: Boolean = false) =
        outdoorTemperatureTile(
            OutdoorSource.POGODA, 12.0, 8.5, simulated, 5, stale,
            forecast = temp, forecastAtMs = if (ageMs < 0) 0L else now - ageMs, nowMs = now
        )

    @Test fun forecastSourceShowsOpenMeteoTemperatureWithForecastBadge() {
        val t = forecastTile(-3.4, ageMs = 10 * 60_000L)
        assertEquals("Zewnętrzna", t.title)
        assertEquals("-3.4", t.value)
        assertEquals("PROGNOZA", t.badge)
        assertEquals("live", t.badgeCls)
        assertTrue(t.desc.startsWith("Pogoda · Open-Meteo · z "))
        assertEquals("pogoda", t.artKey)
        assertFalse(t.stale)
    }

    @Test fun forecastIgnoresControllerFreshnessAndDsSimulation() {
        // Centrala offline / symulacja DS nie dotyczą prognozy z internetu.
        val t = forecastTile(5.0, ageMs = 60_000L, stale = true, simulated = true)
        assertEquals("PROGNOZA", t.badge)
        assertFalse(t.sim)
        assertFalse(t.stale)
        // ...ale Zew 1 nadal pokazuje symulację jak dotąd.
        assertEquals("SYMULACJA 5M", tile(1, simulated = true).badge)
    }

    @Test fun oldOrMissingForecastIsFlagged() {
        val old = forecastTile(5.0, ageMs = 3 * 60 * 60_000L)
        assertEquals("5.0", old.value)
        assertEquals("PROGNOZA NIEAKTUALNA", old.badge)
        assertTrue(old.stale)
        val missing = forecastTile(Double.NaN, ageMs = -1)
        assertEquals("—", missing.value)
        assertEquals("BRAK DANYCH", missing.badge)
        assertTrue(missing.desc.contains("oczekiwanie na prognozę"))
        assertEquals("0.0", forecastTile(0.0, ageMs = 1_000L).value)
    }

    @Test fun sourceHelpersValidateAndFormat() {
        assertTrue(OutdoorSource.isValid(3))
        assertFalse(OutdoorSource.isValid(4))
        assertEquals(1, OutdoorSource.normalize(0))
        assertEquals(3, OutdoorSource.normalize(3))
        assertFalse(OutdoorSource.isForecastFresh(0L, now))
        assertTrue(OutdoorSource.isForecastFresh(now - 60_000L, now))
        assertFalse(OutdoorSource.isForecastFresh(now + 60_000L, now)) // zegar cofnięty
        assertEquals("—", OutdoorSource.clock(0L))
    }
}
