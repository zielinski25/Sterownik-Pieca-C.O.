package com.sterownikco.pro.core

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.io.IOException
import java.io.StringReader
import java.nio.file.Files
import java.time.LocalDate

class TelemetryDayStoreTest {
    private val today = TelemDay(LocalDate.of(2026, 10, 15).toEpochDay() * TelemDay.DAY_SEC)
    private val dayMinus1 = TelemDay(today.startTs - TelemDay.DAY_SEC)
    private val dayMinus2 = TelemDay(today.startTs - 2 * TelemDay.DAY_SEC)
    private val dayMinus3 = TelemDay(today.startTs - 3 * TelemDay.DAY_SEC)

    private fun row(ts: Long, id: String = "a1b2c3d4e5f60718-" + ts.toString(16).padStart(8, '0')) = TelemRow(
        id = id, seq = ts, ts = ts, mono = ts * 1000,
        a = listOf(85.0, 452.0, 615.0, 600.0, 400.0, 350.0, 215.0, 300.0, 550.0, 10132.0, 37.0),
        k = 90, s = 45, state = 1, q = 0, sim = 0, stateKnown = true
    )

    /** Udawana baza: zapytanie o dobę zwraca rekordy z `ts` w `[fromTs, koniec doby]`, jak `orderBy="ts"`. */
    private class FakeDb {
        val rows = ArrayList<TelemRow>()
        val calls = ArrayList<Pair<Long, Long>>()
        val failDays = HashSet<Long>()

        suspend fun fetch(day: TelemDay, fromTs: Long): List<TelemRow> {
            calls.add(day.startTs to fromTs)
            if (day.startTs in failDays) throw IOException("brak sieci")
            return rows.filter { it.ts in fromTs..day.endTs }.shuffled()
        }
    }

    private fun every10Min(fromTs: Long, toTs: Long): List<TelemRow> {
        val out = ArrayList<TelemRow>()
        var ts = fromTs
        while (ts <= toTs) { out.add(row(ts)); ts += 600 }
        return out
    }

    private fun store(dir: File? = null) = TelemetryDayStore(dir, io = Dispatchers.Unconfined)

    private fun tempDir(): File = Files.createTempDirectory("telday-test").toFile()

    private fun TelemetryDayStore.loadAt(db: FakeDb, now: Long, rangeSec: Long, progress: ((Int, Int) -> Unit)? = null) =
        runBlocking { load(now - rangeSec, now, progress) { day, from -> db.fetch(day, from) } }

    @Test
    fun dayHelpersFollowFirmwareShardLayout() {
        assertEquals("/piec/telemetry/1m/v1/2026/10/15", TelemDay.of(today.startTs + 86_399).shardPath)
        assertEquals("/piec/telemetry/1m/v1/2026/10/16", TelemDay.of(today.startTs + 86_400).shardPath)
        assertEquals("/piec/telemetry/1m/v1/2026/01/05",
            TelemDay(LocalDate.of(2026, 1, 5).toEpochDay() * TelemDay.DAY_SEC).shardPath)
        assertEquals("2026-10-15.tday.gz", today.fileName)
        assertEquals(today, TelemDay.fromFileName("2026-10-15.tday.gz"))
        assertNull(TelemDay.fromFileName("smieci.tday.gz"))
        assertNull(TelemDay.fromFileName("2026-10-15.txt"))

        val now = today.startTs + 6 * 3600
        val days = TelemDay.covering(now - 30 * 86_400L, now)
        assertEquals(31, days.size)
        assertEquals(TelemDay(today.startTs - 30 * TelemDay.DAY_SEC), days.first())
        assertEquals(today, days.last())
    }

    @Test
    fun firstLoadFetchesWholeDaysNewestFirstAndClosesOlderDays() {
        val db = FakeDb()
        val now = today.startTs + 6 * 3600
        db.rows.addAll(every10Min(dayMinus2.startTs, now))
        val s = store()
        val progress = ArrayList<Pair<Int, Int>>()

        val first = s.loadAt(db, now, 2 * 86_400L) { done, total -> progress.add(done to total) }

        assertEquals(listOf(today.startTs to today.startTs, dayMinus1.startTs to dayMinus1.startTs,
            dayMinus2.startTs to dayMinus2.startTs), db.calls)
        assertEquals(listOf(1 to 3, 2 to 3, 3 to 3), progress)
        assertEquals(0, first.cachedDays)
        assertEquals(3, first.refreshedDays)
        assertEquals(0, first.failedDays)
        assertEquals(db.rows.size, first.newRows)
        // Wykres dostaje tylko zakres; cała pierwsza doba i tak zostaje w pamięci.
        val expected = db.rows.filter { it.ts in (now - 2 * 86_400L)..now }.map { it.ts * 1000 }
        assertEquals(expected, first.points.map { it.ts })

        db.calls.clear()
        val second = s.loadAt(db, now, 2 * 86_400L)
        assertEquals(listOf(today.startTs to now), db.calls)
        assertEquals(2, second.cachedDays)
        assertEquals(1, second.refreshedDays)
        assertEquals(0, second.newRows)
        assertEquals(first.points, second.points)
    }

    @Test
    fun laterLoadsOnlyAskForNewRecordsAndDoNotDuplicateTheBoundaryRecord() {
        val db = FakeDb()
        var now = today.startTs + 6 * 3600
        db.rows.addAll(every10Min(dayMinus1.startTs, now))
        val s = store()
        s.loadAt(db, now, 86_400L)

        val newer = every10Min(now + 600, now + 3600)
        db.rows.addAll(newer)
        val lastKnown = now
        now += 3600
        db.calls.clear()
        val r = s.loadAt(db, now, 86_400L)

        assertEquals(listOf(today.startTs to lastKnown), db.calls)
        assertEquals(newer.size, r.newRows)
        val tss = r.points.map { it.ts }
        assertEquals(tss.distinct(), tss)
        assertEquals(now * 1000, tss.last())
        assertEquals(db.rows.filter { it.ts in (now - 86_400L)..now }.size, r.points.size)
    }

    @Test
    fun dayStaysOpenUntilNewerDataWasSeenInTheDatabase() {
        val db = FakeDb()
        val now = today.startTs + 3600
        db.rows.addAll(every10Min(dayMinus1.startTs, dayMinus1.endTs))
        val lastOfYesterday = db.rows.last().ts
        val s = store()

        s.loadAt(db, now, 86_400L)
        assertEquals(listOf(today.startTs to today.startTs, dayMinus1.startTs to dayMinus1.startTs), db.calls)

        // Dziś brak rekordów (ESP offline) — wczoraj nie może być uznane za kompletne.
        db.calls.clear()
        s.loadAt(db, now, 86_400L)
        assertEquals(listOf(today.startTs to today.startTs, dayMinus1.startTs to lastOfYesterday), db.calls)

        // Pierwszy dzisiejszy rekord (FIFO) domyka wczoraj jeszcze w tym samym odświeżeniu.
        db.rows.add(row(today.startTs + 1800))
        db.calls.clear()
        val r = s.loadAt(db, now, 86_400L)
        assertEquals(listOf(today.startTs to today.startTs, dayMinus1.startTs to lastOfYesterday), db.calls)
        assertEquals(1, r.newRows)

        db.calls.clear()
        s.loadAt(db, now, 86_400L)
        assertEquals(listOf(today.startTs to today.startTs + 1800), db.calls)
    }

    @Test
    fun emptyDayClosesOnceNewerRecordsExistAndSurvivesRestart() {
        val dir = tempDir()
        try {
            val db = FakeDb()
            val now = today.startTs + 2 * 3600
            db.rows.addAll(every10Min(dayMinus2.startTs, dayMinus2.endTs)) // dzień -1: brak danych
            db.rows.addAll(every10Min(today.startTs, now))
            store(dir).loadAt(db, now, 2 * 86_400L)
            assertTrue(File(dir, dayMinus1.fileName).isFile)

            db.calls.clear()
            val r = store(dir).loadAt(db, now, 2 * 86_400L)
            assertEquals(listOf(today.startTs to now), db.calls)
            assertEquals(2, r.cachedDays)
        } finally {
            dir.deleteRecursively()
        }
    }

    @Test
    fun failedFetchIsNotCachedStopsFurtherQueriesAndServesCachedRows() {
        val db = FakeDb()
        var now = today.startTs + 6 * 3600
        db.rows.addAll(every10Min(dayMinus2.startTs, now))
        db.failDays.add(today.startTs)
        val s = store()

        val offline = s.loadAt(db, now, 2 * 86_400L)
        assertEquals(listOf(today.startTs to today.startTs), db.calls)
        assertEquals(0, offline.refreshedDays)
        assertEquals(3, offline.failedDays)
        assertTrue(offline.points.isEmpty())

        db.failDays.clear()
        db.calls.clear()
        val online = s.loadAt(db, now, 2 * 86_400L)
        assertEquals(3, online.refreshedDays)
        assertEquals(3, db.calls.size)

        // Ponowny brak sieci: dzisiejsza doba nie odświeżona, ale wykres pokazuje kopię z telefonu.
        db.rows.addAll(every10Min(now + 600, now + 1800))
        now += 1800
        db.failDays.add(today.startTs)
        val partial = s.loadAt(db, now, 2 * 86_400L)
        assertEquals(1, partial.failedDays)
        assertEquals(0, partial.refreshedDays)
        assertEquals(2, partial.cachedDays)
        assertEquals(online.points.filter { it.ts >= (now - 2 * 86_400L) * 1000 }, partial.points)
    }

    @Test
    fun localCopySurvivesNewStoreInstanceWithExactRowFields() {
        val dir = tempDir()
        try {
            val db = FakeDb()
            val now = today.startTs + 6 * 3600
            db.rows.addAll(every10Min(dayMinus2.startTs, now))
            val first = store(dir).loadAt(db, now, 2 * 86_400L)
            assertEquals(setOf(today.fileName, dayMinus1.fileName, dayMinus2.fileName), dir.list()!!.toSet())

            db.calls.clear()
            val restarted = store(dir).loadAt(db, now, 2 * 86_400L)
            assertEquals(listOf(today.startTs to now), db.calls)
            assertEquals(first.points, restarted.points)
        } finally {
            dir.deleteRecursively()
        }
    }

    @Test
    fun codecRoundTripKeepsEveryField() {
        val rows = listOf(
            row(today.startTs + 60),
            TelemRow(
                id = "0011223344556677-0000ffff", seq = 4_000_000_000L, ts = today.startTs + 120, mono = 9_876_543_210L,
                a = listOf(-1270.0, Double.NaN, 12.25, -0.5, 1.0E-7, 15000.0), k = -1, s = -1,
                state = 0, q = 0x3FFFFFL, sim = (1L shl 62) or 5L, simulated = true, stateKnown = false
            ),
            TelemRow(id = "x-empty", seq = 0, ts = today.startTs + 180, mono = 0, a = emptyList(),
                k = 0, s = 0, state = 0, q = 0, sim = 0),
            TelemRow(id = "x-nan", seq = 1, ts = today.startTs + 240, mono = 0, a = listOf(Double.NaN),
                k = 0, s = 0, state = 0, q = 0, sim = 0)
        )
        val entry = TelemDayEntry(today, uniqueSortedRows(rows), today.startTs + 240)
        val text = StringBuilder().also { TelemDayCodec.write(it, entry) }.toString()
        val back = TelemDayCodec.read(StringReader(text).buffered(), today)

        assertEquals(entry.rows, back.rows)
        assertEquals(entry.completeBefore, back.completeBefore)
        assertEquals(emptyList<Double>(), back.rows.first { it.id == "x-empty" }.a)
        assertTrue(back.rows.first { it.id == "x-nan" }.a.single().isNaN())
    }

    @Test
    fun corruptLocalCopyIsDroppedAndDayIsFetchedAgain() {
        val dir = tempDir()
        try {
            val db = FakeDb()
            val now = today.startTs + 6 * 3600
            db.rows.addAll(every10Min(dayMinus1.startTs, now))
            store(dir).loadAt(db, now, 86_400L)
            File(dir, dayMinus1.fileName).writeText("to nie jest gzip")

            db.calls.clear()
            val r = store(dir).loadAt(db, now, 86_400L)
            assertEquals(listOf(today.startTs to now, dayMinus1.startTs to dayMinus1.startTs), db.calls)
            assertEquals(db.rows.filter { it.ts in (now - 86_400L)..now }.size, r.points.size)

            // Kopia naprawiona: kolejny start nie pyta już o wczoraj.
            db.calls.clear()
            store(dir).loadAt(db, now, 86_400L)
            assertEquals(listOf(today.startTs to now), db.calls)
        } finally {
            dir.deleteRecursively()
        }
    }

    @Test
    fun pruneDeletesOnlyOldLocalCopiesNeverAnythingElse() {
        val dir = tempDir()
        try {
            fun dayFile(daysAgo: Int) = File(dir, TelemDay(today.startTs - daysAgo * TelemDay.DAY_SEC).fileName)
            listOf(40, 36, KEEP_DAYS, 1).forEach { dayFile(it).writeText("x") }
            val foreign = File(dir, "notatka.txt").apply { writeText("nie moje") }
            val staleTmp = File(dir, "2026-10-14.tday.gz.123.tmp").apply {
                writeText("x"); setLastModified(System.currentTimeMillis() - 3_600_000L)
            }
            val freshTmp = File(dir, "2026-10-14.tday.gz.456.tmp").apply { writeText("x") }

            val db = FakeDb()
            store(dir).loadAt(db, today.startTs + 3600, 6 * 3600L)

            assertFalse(dayFile(40).exists())
            assertFalse(dayFile(36).exists())
            assertTrue(dayFile(KEEP_DAYS).exists())
            assertTrue(foreign.exists())
            assertFalse(staleTmp.exists())
            assertTrue(freshTmp.exists())
        } finally {
            dir.deleteRecursively()
        }
    }

    @Test
    fun clearRemovesOnlyLocalCopiesAndForcesFullDownload() {
        val dir = tempDir()
        try {
            val db = FakeDb()
            val now = today.startTs + 6 * 3600
            db.rows.addAll(every10Min(dayMinus1.startTs, now))
            val s = store(dir)
            s.loadAt(db, now, 86_400L)
            assertTrue(dir.list()!!.isNotEmpty())

            runBlocking { s.clear() }
            assertTrue(dir.list()!!.isEmpty())

            db.calls.clear()
            s.loadAt(db, now, 86_400L)
            assertEquals(listOf(today.startTs to today.startTs, dayMinus1.startTs to dayMinus1.startTs), db.calls)
        } finally {
            dir.deleteRecursively()
        }
    }

    @Test
    fun widerRangeReusesDaysDownloadedForANarrowerOne() {
        val db = FakeDb()
        val now = today.startTs + 6 * 3600
        db.rows.addAll(every10Min(dayMinus3.startTs, now))
        val s = store()
        s.loadAt(db, now, 86_400L)

        db.calls.clear()
        val wide = s.loadAt(db, now, 3 * 86_400L)
        assertEquals(listOf(today.startTs to now, dayMinus2.startTs to dayMinus2.startTs,
            dayMinus3.startTs to dayMinus3.startTs), db.calls)
        assertEquals(1, wide.cachedDays)
        assertEquals(db.rows.filter { it.ts in (now - 3 * 86_400L)..now }.map { it.ts * 1000 }, wide.points.map { it.ts })
    }

    @Test
    fun mergeNeverMovesCompletenessBackwardsAndIgnoresRowsOfOtherDays() {
        val previous = TelemDayEntry(today, listOf(row(today.startTs + 600)), today.startTs + 600)
        val merged = TelemDaySync.merge(today, previous,
            listOf(row(today.startTs + 600), row(dayMinus1.startTs + 5)), latestKnownBefore = today.startTs + 10)
        assertEquals(today.startTs + 600, merged.completeBefore)
        assertEquals(1, merged.rows.size)
        assertFalse(merged.closed)

        val closed = TelemDaySync.merge(today, previous, emptyList(), latestKnownBefore = today.endTs + 1)
        assertTrue(closed.closed)
        assertNull(TelemDaySync.queryFrom(today, closed))
        assertEquals(today.startTs, TelemDaySync.queryFrom(today, null))
        assertEquals(today.startTs + 600, TelemDaySync.queryFrom(today, previous))
    }

    @Test
    fun pagedFetchUsesInclusiveCursorWithoutLosingOrDuplicatingRecords() = runBlocking {
        val base = today.startTs
        val all = listOf(10L, 20L, 30L, 30L, 30L, 40L, 50L, 60L).mapIndexed { i, off -> row(base + off, id = "r$i") }
        val cursors = ArrayList<Long>()
        val rows = fetchDayPaged(today, base, pageSize = 3) { cursor ->
            cursors.add(cursor)
            val page = all.filter { it.ts >= cursor }.sortedBy { it.ts }.take(3)
            TelemShardPage(page, page.size)
        }
        assertEquals(all.map { it.id }.toSet(), rows.map { it.id }.toSet())
        assertEquals(all.size, rows.size)
        assertEquals(base, cursors.first())

        // Pełna strona z jednym `ts` = brak postępu: kursor musi ruszyć dalej (bez pętli).
        val same = (0 until 3).map { row(base + 5, id = "s$it") }
        val stuck = fetchDayPaged(today, base, pageSize = 3) { cursor ->
            val page = same.filter { it.ts >= cursor }
            TelemShardPage(page, page.size)
        }
        assertEquals(3, stuck.size)
    }

    @Test
    fun emptyRtdbBodiesAreSuccessfulEmptyResults() {
        assertTrue(isEmptyRtdbBody("null"))
        assertTrue(isEmptyRtdbBody("{}"))
        assertTrue(isEmptyRtdbBody(""))
        assertTrue(isEmptyRtdbBody(" null\n"))
        assertFalse(isEmptyRtdbBody("{\"a\":1}"))
    }

    @Test
    fun cacheDirectoryIsScopedToTheDatabaseHost() {
        val root = File("/tmp/cache")
        val dir = TelemetryDayStore.directoryFor(root, "https://akwarium-367be-default-rtdb.europe-west1.firebasedatabase.app")
        assertEquals(File(File(root, "telemetry-1m-v1"), "akwarium-367be-default-rtdb.europe-west1.firebasedatabase.app"), dir)
        // Katalog zawsze leży pod telemetry-1m-v1 — także przy dziwnym adresie bazy.
        assertEquals(File(root, "telemetry-1m-v1"), TelemetryDayStore.directoryFor(root, "https://../x").parentFile)
        assertNotNull(TelemetryDayStore.directoryFor(root, "").name.takeIf { it.isNotEmpty() })
    }
}
