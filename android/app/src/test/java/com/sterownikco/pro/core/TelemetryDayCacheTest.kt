package com.sterownikco.pro.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

/**
 * Testy dziennego cache telemetrii (v0.31.20).
 *
 * Nie dotykają Androida ani sieci: `TelemetryDayCache(rootDir)` przyjmuje zwykły
 * katalog tymczasowy, a `org.json` jest dostępny w testach jednostkowych.
 */
class TelemetryDayCacheTest {

    private fun tempRoot(): File =
        java.nio.file.Files.createTempDirectory("telemetry-cache-test").toFile()

    private fun row(
        id: String,
        ts: Long,
        values: List<Double> = listOf(21.5, Double.NaN, -1270.0, 0.0)
    ) = TelemRow(
        id = id, seq = 7L, ts = ts, mono = 1_234L, a = values,
        k = 3L, s = -1L, state = 1L, q = 2L, sim = 5L,
        simulated = false, stateKnown = true
    )

    @Test
    fun utcDayKeySplitsOnMidnightUtc() {
        // 2026-10-22 00:00:00 UTC
        assertEquals("2026-10-22", TelemetryDayCache.utcDayKey(1_792_627_200L))
        assertEquals("2026-10-21", TelemetryDayCache.utcDayKey(1_792_627_199L))
        // 2026-10-22 23:59:59 UTC
        assertEquals("2026-10-22", TelemetryDayCache.utcDayKey(1_792_713_599L))
        assertEquals("2026-10-23", TelemetryDayCache.utcDayKey(1_792_713_600L))
    }

    @Test
    fun writeAndReadRoundTripKeepsChannelGapsAndFlags() {
        val root = tempRoot()
        val cache = TelemetryDayCache(root)
        cache.write("2026-10-22", listOf(row("r1", 1_000L), row("r2", 1_060L)))

        val day = cache.read("2026-10-22")
        assertNotNull(day)
        assertEquals(2, day!!.rows.size)
        assertEquals(1_060L, day.maxTs)

        val first = day.rows[0]
        assertEquals("r1", first.id)
        assertEquals(7L, first.seq)
        assertEquals(1_234L, first.mono)
        assertEquals(3L, first.k)
        assertEquals(-1L, first.s)
        assertEquals(1L, first.state)
        assertEquals(2L, first.q)
        assertEquals(5L, first.sim)
        assertEquals(true, first.stateKnown)
        assertEquals(false, first.simulated)
        // Przerwy w kanałach muszą przetrwać zapis: NaN nie istnieje w JSON.
        assertEquals(4, first.a.size)
        assertEquals(21.5, first.a[0], 0.0)
        assertTrue(first.a[1].isNaN())
        assertEquals(-1270.0, first.a[2], 0.0)
    }

    @Test
    fun emptyWriteCreatesNoFile() {
        val root = tempRoot()
        val cache = TelemetryDayCache(root)
        cache.write("2026-10-22", emptyList())
        assertNull(cache.read("2026-10-22"))
        assertEquals(0, cache.countDays())
    }

    @Test
    fun corruptFileIsDroppedInsteadOfCrashing() {
        val root = tempRoot()
        val cache = TelemetryDayCache(root)
        root.mkdirs()
        val broken = File(root, "2026-10-22.json")
        broken.writeText("{to nie jest json", Charsets.UTF_8)

        assertNull(cache.read("2026-10-22"))
        assertEquals(false, broken.exists())
    }

    @Test
    fun wrongCacheVersionIsIgnored() {
        val root = tempRoot()
        val cache = TelemetryDayCache(root)
        root.mkdirs()
        File(root, "2026-10-22.json").writeText(
            "{\"v\":99,\"day\":\"2026-10-22\",\"saved\":1,\"rows\":[]}", Charsets.UTF_8
        )
        assertNull(cache.read("2026-10-22"))
    }

    @Test
    fun pruneKeepsTheWholeWindowAndDropsOlderDays() {
        val root = tempRoot()
        val cache = TelemetryDayCache(root)
        cache.write("2026-10-22", listOf(row("a", 1L)))
        cache.write("2026-09-07", listOf(row("b", 2L)))   // dokładnie 45 dni wstecz
        cache.write("2026-09-06", listOf(row("c", 3L)))   // 46 dni — poza oknem

        cache.prune("2026-10-22", keepDays = 45)

        assertNotNull(cache.read("2026-10-22"))
        assertNotNull(cache.read("2026-09-07"))
        assertNull(cache.read("2026-09-06"))
        assertEquals(2, cache.countDays())
    }

    @Test
    fun deleteAndClearRemoveFiles() {
        val root = tempRoot()
        val cache = TelemetryDayCache(root)
        cache.write("2026-10-22", listOf(row("a", 1L)))
        cache.write("2026-10-21", listOf(row("b", 2L)))
        assertEquals(2, cache.countDays())
        assertTrue(cache.sizeBytes() > 0L)

        cache.delete("2026-10-21")
        assertEquals(1, cache.countDays())

        cache.clear()
        assertEquals(0, cache.countDays())
        assertEquals(0L, cache.sizeBytes())
    }

    @Test
    fun missingDayReturnsNull() {
        val cache = TelemetryDayCache(tempRoot())
        assertNull(cache.read("2026-01-01"))
        assertEquals(0, cache.countDays())
    }
}
