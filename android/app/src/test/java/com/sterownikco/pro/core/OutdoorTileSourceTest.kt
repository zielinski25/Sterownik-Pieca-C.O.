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
}
