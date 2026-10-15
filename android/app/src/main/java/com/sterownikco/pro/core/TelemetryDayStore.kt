package com.sterownikco.pro.core

import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import java.io.BufferedOutputStream
import java.io.BufferedReader
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.IOException
import java.time.LocalDate
import java.util.zip.GZIPInputStream
import java.util.zip.GZIPOutputStream

/* ══════════════════════════════════════════════════════════════════════════
   LOKALNA PAMIĘĆ POBRANYCH DÓB TELEMETRII (wykresy)

   Wykres 30-dniowy pobierał przy każdym otwarciu cały zakres (~9 MB) z RTDB.
   Teraz telefon trzyma kopię pobranych dób (szardy UTC
   `/piec/telemetry/1m/v1/RRRR/MM/DD`) i z Firebase dociąga wyłącznie to,
   czego jeszcze nie ma.

   Firebase pozostaje jedynym źródłem prawdy: ten kod NIGDY niczego w bazie
   nie usuwa ani nie zmienia. Kasuje wyłącznie własne pliki w katalogu cache
   aplikacji (kopie starsze niż [KEEP_DAYS] dni, wylogowanie, „pobierz od nowa”).

   Kiedy doba jest kompletna („zamknięta”) i nie trzeba o nią więcej pytać:
   ESP wysyła telemetrię FIFO — najstarsze próbki pierwsze (ring w PSRAM;
   strony spoolu z LittleFS są odtwarzane przed ringiem). Jeżeli więc przed
   zapytaniem o dobę D widzieliśmy już w bazie rekord nowszy niż koniec D,
   to w chwili zapytania wszystkie rekordy D były już w bazie. Dla doby
   otwartej pamiętamy granicę `completeBefore` (wszystko z ts < granicy jest
   lokalnie) i pytamy tylko o `ts >= granica` — włącznie, a ewentualny
   duplikat na granicy usuwa deduplikacja po id rekordu.
   Dlatego doby odpytujemy od najnowszej do najstarszej.
   ══════════════════════════════════════════════════════════════════════════ */

/** Ile dni lokalnych kopii trzymać (zakres 30 dni + częściowa doba + zapas). */
internal const val KEEP_DAYS = 35

/** Doba UTC — jednostka lokalnej pamięci, 1:1 ze shardem telemetrii w RTDB. */
internal data class TelemDay(val startTs: Long) {
    val endTs: Long get() = startTs + DAY_SEC - 1
    val date: LocalDate get() = LocalDate.ofEpochDay(startTs.floorDiv(DAY_SEC))

    /** Ścieżka shardu, np. `/piec/telemetry/1m/v1/2026/10/15`. */
    val shardPath: String
        get() {
            val d = date
            return SHARD_ROOT + "/" + d.year + "/" + pad2(d.monthValue) + "/" + pad2(d.dayOfMonth)
        }

    /** Nazwa pliku lokalnej kopii, np. `2026-10-15.tday.gz`. */
    val fileName: String get() = date.toString() + FILE_SUFFIX

    companion object {
        const val DAY_SEC = 86_400L
        const val SHARD_ROOT = "/piec/telemetry/1m/v1"
        const val FILE_SUFFIX = ".tday.gz"

        fun of(ts: Long): TelemDay = TelemDay(ts.floorDiv(DAY_SEC) * DAY_SEC)

        /** Doby UTC pokrywające `[startTs, endTs]`, od najstarszej. */
        fun covering(startTs: Long, endTs: Long): List<TelemDay> {
            if (endTs < startTs) return emptyList()
            val first = of(startTs).startTs
            val last = of(endTs).startTs
            val out = ArrayList<TelemDay>(((last - first) / DAY_SEC + 1).toInt())
            var cur = first
            while (cur <= last) {
                out.add(TelemDay(cur))
                cur += DAY_SEC
            }
            return out
        }

        fun fromFileName(name: String): TelemDay? {
            if (!name.endsWith(FILE_SUFFIX)) return null
            return try {
                TelemDay(LocalDate.parse(name.removeSuffix(FILE_SUFFIX)).toEpochDay() * DAY_SEC)
            } catch (e: Exception) {
                null
            }
        }

        private fun pad2(v: Int): String = if (v < 10) "0$v" else v.toString()
    }
}

/** Kopia jednej doby: surowe rekordy + granica kompletności. */
internal class TelemDayEntry(
    val day: TelemDay,
    /** Rekordy doby — unikalne po id, posortowane po (ts, seq, id). */
    val rows: List<TelemRow>,
    /** Wszystkie rekordy tej doby z `ts < completeBefore` są już w kopii. */
    val completeBefore: Long
) {
    /** Doba kompletna — kolejnych zapytań do Firebase o nią już nie będzie. */
    val closed: Boolean get() = completeBefore > day.endTs
    val latestTs: Long? get() = rows.lastOrNull()?.ts
}

/** Czyste reguły synchronizacji (bez sieci i dysku) — testowane jednostkowo. */
internal object TelemDaySync {
    /** Od jakiego `ts` (włącznie) pytać Firebase o tę dobę; null = doba kompletna. */
    fun queryFrom(day: TelemDay, entry: TelemDayEntry?): Long? = when {
        entry == null -> day.startTs
        entry.closed -> null
        else -> maxOf(day.startTs, entry.completeBefore)
    }

    /**
     * Scala wynik udanego zapytania o `ts >= queryFrom(...)`.
     * [latestKnownBefore] = najnowszy `ts` widziany w bazie w zapytaniach zakończonych
     * PRZED wysłaniem tego (wcześniejsze odświeżenia i nowsze doby tego odświeżenia).
     */
    fun merge(day: TelemDay, previous: TelemDayEntry?, fetched: List<TelemRow>, latestKnownBefore: Long?): TelemDayEntry {
        val inDay = fetched.filter { it.ts in day.startTs..day.endTs }
        val rows = uniqueSortedRows(if (previous == null) inDay else previous.rows + inDay)
        var bound = latestKnownBefore ?: Long.MIN_VALUE
        val fetchedMax = inDay.maxOfOrNull { it.ts }
        if (fetchedMax != null && fetchedMax > bound) bound = fetchedMax
        val previousComplete = previous?.completeBefore ?: day.startTs
        val complete = maxOf(previousComplete, minOf(bound, day.endTs + 1))
        return TelemDayEntry(day, rows, complete)
    }
}

private val ROW_ORDER = compareBy<TelemRow>({ it.ts }, { it.seq }, { it.id })

/** Deduplikacja po id rekordu (nowsza kopia wygrywa) + sortowanie (ts, seq, id). */
internal fun uniqueSortedRows(rows: List<TelemRow>): List<TelemRow> {
    val byId = LinkedHashMap<String, TelemRow>(rows.size * 2 + 1)
    rows.forEach { byId[it.id] = it }
    return byId.values.sortedWith(ROW_ORDER)
}

/** Pusta odpowiedź RTDB (brak węzła / brak wyników zapytania) to poprawny, pusty wynik — nie błąd. */
internal fun isEmptyRtdbBody(body: String): Boolean {
    val t = body.trim()
    return t.isEmpty() || t == "null" || t == "{}"
}

/** Jedna strona odpowiedzi zapytania `orderBy="ts"`: rekordy doby + liczba węzłów w odpowiedzi. */
internal class TelemShardPage(val rows: List<TelemRow>, val rawCount: Int)

/**
 * Rekordy doby z `ts >= fromTs`, stronami po [pageSize] (`startAt` włącznie — rekordy
 * z tym samym `ts` na granicy stron nie giną, duplikaty usuwa deduplikacja po id).
 */
internal suspend fun fetchDayPaged(
    day: TelemDay,
    fromTs: Long,
    pageSize: Int,
    maxPages: Int = 64,
    fetchPage: suspend (cursor: Long) -> TelemShardPage
): List<TelemRow> {
    val rows = ArrayList<TelemRow>()
    var cursor = maxOf(fromTs, day.startTs)
    repeat(maxPages) {
        val page = fetchPage(cursor)
        rows.addAll(page.rows)
        if (page.rawCount < pageSize) return uniqueSortedRows(rows)
        val maxTs = page.rows.maxOfOrNull { it.ts }
            ?: throw IOException("Nieczytelna strona telemetrii dla doby ${day.date}")
        cursor = if (maxTs > cursor) maxTs else cursor + 1
        if (cursor > day.endTs) return uniqueSortedRows(rows)
    }
    throw IOException("Zbyt wiele stron telemetrii dla doby ${day.date}")
}

/** Wynik ładowania zakresu wykresu. */
internal class TelemRangeResult(
    /** Punkty wykresu w zakresie, chronologicznie. */
    val points: List<TelemPoint>,
    /** Liczba dób UTC w zakresie. */
    val days: Int,
    /** Doby kompletne podane z pamięci telefonu — bez żadnego zapytania do Firebase. */
    val cachedDays: Int,
    /** Doby odświeżone w Firebase w tym ładowaniu (zwykle dzisiejsza). */
    val refreshedDays: Int,
    /** Doby, których nie udało się odświeżyć (błąd sieci) — pokazane z kopii, o ile była. */
    val failedDays: Int,
    /** Nowe rekordy pobrane z Firebase w tym ładowaniu. */
    val newRows: Int
)

/** Zapis/odczyt kopii doby: gzip, tekst rozdzielany tabulatorami, wersjonowany nagłówek. */
internal object TelemDayCodec {
    const val MAGIC = "#TELDAY1"
    private const val FIELDS = 11

    fun write(out: Appendable, entry: TelemDayEntry) {
        // Klucze RTDB nie mogą zawierać znaków sterujących; obrona na wypadek uszkodzonych danych.
        val rows = entry.rows.filter { r -> r.id.isNotEmpty() && r.id.none { it == '\t' || it == '\n' || it == '\r' } }
        out.append(MAGIC).append('\t').append(entry.day.startTs.toString())
            .append('\t').append(entry.completeBefore.toString())
            .append('\t').append(rows.size.toString()).append('\n')
        for (r in rows) {
            val flags = (if (r.simulated) 1 else 0) or (if (r.stateKnown) 2 else 0)
            out.append(r.id).append('\t')
                .append(r.seq.toString()).append('\t')
                .append(r.ts.toString()).append('\t')
                .append(r.mono.toString()).append('\t')
                .append(r.k.toString()).append('\t')
                .append(r.s.toString()).append('\t')
                .append(r.state.toString()).append('\t')
                .append(r.q.toString()).append('\t')
                .append(r.sim.toString()).append('\t')
                .append(flags.toString()).append('\t')
            r.a.forEachIndexed { i, v ->
                if (i > 0) out.append(',')
                out.append(formatValue(v))
            }
            out.append('\n')
        }
    }

    fun read(reader: BufferedReader, day: TelemDay): TelemDayEntry {
        val header = (reader.readLine() ?: throw IOException("pusty plik")).split('\t')
        if (header.size != 4 || header[0] != MAGIC) throw IOException("nieznany format")
        val start = header[1].toLong()
        val complete = header[2].toLong()
        val count = header[3].toInt()
        if (start != day.startTs || complete < day.startTs || complete > day.endTs + 1 || count < 0) {
            throw IOException("niezgodny nagłówek")
        }
        val rows = ArrayList<TelemRow>(count)
        while (true) {
            val line = reader.readLine() ?: break
            rows.add(parseRow(line, day))
        }
        if (rows.size != count) throw IOException("niekompletny plik")
        return TelemDayEntry(day, uniqueSortedRows(rows), complete)
    }

    private fun parseRow(line: String, day: TelemDay): TelemRow {
        val f = line.split('\t')
        if (f.size != FIELDS || f[0].isEmpty()) throw IOException("uszkodzony wiersz")
        val ts = f[2].toLong()
        if (ts !in day.startTs..day.endTs) throw IOException("ts poza dobą")
        val flags = f[9].toInt()
        val a = if (f[10].isEmpty()) emptyList() else f[10].split(',').map { if (it == "n") Double.NaN else it.toDouble() }
        return TelemRow(
            id = f[0], seq = f[1].toLong(), ts = ts, mono = f[3].toLong(), a = a,
            k = f[4].toLong(), s = f[5].toLong(), state = f[6].toLong(), q = f[7].toLong(),
            sim = f[8].toLong(), simulated = (flags and 1) != 0, stateKnown = (flags and 2) != 0
        )
    }

    /** Liczby całkowite bez „.0”, ułamki dokładnie (Double.toString), NaN jako `n`. */
    private fun formatValue(v: Double): String = when {
        v.isNaN() -> "n"
        v == Math.rint(v) && v > -1e15 && v < 1e15 -> v.toLong().toString()
        else -> v.toString()
    }
}

/** Doba w RAM: punkty wykresu + (tylko dla doby otwartej) surowe rekordy do scalania. */
private class CachedDay(
    val day: TelemDay,
    val completeBefore: Long,
    val latestTs: Long?,
    /** Surowe rekordy trzymamy tylko dla doby otwartej; zamknięta = same punkty wykresu. */
    val openRows: List<TelemRow>?,
    val points: List<TelemPoint>
) {
    val closed: Boolean get() = completeBefore > day.endTs

    fun asEntry(): TelemDayEntry? = openRows?.let { TelemDayEntry(day, it, completeBefore) }

    companion object {
        fun of(entry: TelemDayEntry) = CachedDay(
            day = entry.day,
            completeBefore = entry.completeBefore,
            latestTs = entry.latestTs,
            openRows = if (entry.closed) null else entry.rows,
            points = entry.rows.asSequence().map { it.toPoint() }.filter { it.hasValues() }.toList()
        )
    }
}

/**
 * Pamięć dób: RAM (tylko doby bieżącego zakresu) + pliki w [dir] (katalog cache
 * aplikacji, bez backupu). `dir == null` = sam RAM.
 *
 * Instancja należy do modelu ekranu (AppModel), więc RAM zwalnia się razem z nim;
 * pliki zostają na kolejne uruchomienia.
 */
internal class TelemetryDayStore(
    private val dir: File?,
    private val io: CoroutineDispatcher = Dispatchers.IO,
    private val keepDays: Int = KEEP_DAYS
) {
    private val mutex = Mutex()
    private val memory = HashMap<Long, CachedDay>()

    /**
     * Zwraca punkty z `[startTs, endTs]`. Doby kompletne idą z pamięci; o doby otwarte
     * (zwykle tylko dzisiejszą) pyta [fetch] = „rekordy doby z `ts >= fromTs`” (rzuca przy błędzie).
     * Po pierwszym błędzie kolejne doby nie są już odpytywane (brak sieci = nie czekamy N razy).
     * [onProgress] jest wołane w kontekście wywołującego po każdej odpytywanej dobie.
     */
    suspend fun load(
        startTs: Long,
        endTs: Long,
        onProgress: ((done: Int, total: Int) -> Unit)? = null,
        fetch: suspend (day: TelemDay, fromTs: Long) -> List<TelemRow>
    ): TelemRangeResult = mutex.withLock {
        val days = TelemDay.covering(startTs, endTs)
        val wanted = HashSet<Long>(days.size * 2 + 1)
        days.forEach { wanted.add(it.startTs) }
        // RAM tylko dla bieżącego zakresu — reszta zostaje na dysku.
        memory.keys.retainAll(wanted)
        val missing = days.filter { it.startTs !in memory }
        if (dir != null) {
            val loaded = withContext(io) {
                prune(TelemDay.of(endTs))
                missing.mapNotNull { day -> readDay(day)?.let { CachedDay.of(it) } }
            }
            loaded.forEach { memory[it.day.startTs] = it }
        }

        var latestKnown: Long? = days.mapNotNull { memory[it.startTs]?.latestTs }.maxOrNull()
        // Od najnowszej doby: jej rekordy domykają starsze doby jeszcze w tym samym odświeżeniu.
        val pending = days.asReversed().filter { memory[it.startTs]?.closed != true }
        val cachedDays = days.size - pending.size
        var refreshed = 0
        var failed = 0
        var newRows = 0
        var aborted = false
        pending.forEachIndexed { index, day ->
            if (aborted) {
                failed++
            } else {
                val cached = memory[day.startTs]
                val previous = cached?.asEntry()
                val from = TelemDaySync.queryFrom(day, previous) ?: day.startTs
                val knownBefore = latestKnown
                val outcome: Pair<CachedDay, Int>? = try {
                    withContext(io) {
                        val fetched = fetch(day, from)
                        val merged = TelemDaySync.merge(day, previous, fetched, knownBefore)
                        val added = merged.rows.size - (previous?.rows?.size ?: 0)
                        val changed = if (previous == null) {
                            merged.rows.isNotEmpty() || merged.completeBefore != day.startTs
                        } else {
                            added != 0 || merged.completeBefore != previous.completeBefore
                        }
                        val next = if (changed || cached == null) CachedDay.of(merged) else cached
                        if (changed) writeDay(merged)
                        next to added
                    }
                } catch (e: CancellationException) {
                    throw e
                } catch (e: Exception) {
                    currentCoroutineContext().ensureActive()
                    null
                }
                if (outcome == null) {
                    aborted = true
                    failed++
                } else {
                    val (next, added) = outcome
                    memory[day.startTs] = next
                    refreshed++
                    newRows += maxOf(0, added)
                    val dayLatest = next.latestTs
                    val known = latestKnown
                    if (dayLatest != null && (known == null || dayLatest > known)) latestKnown = dayLatest
                }
            }
            onProgress?.invoke(index + 1, pending.size)
        }

        val startMs = startTs * 1000
        val endMs = endTs * 1000
        val points = ArrayList<TelemPoint>(days.sumOf { memory[it.startTs]?.points?.size ?: 0 })
        for (day in days) {
            val c = memory[day.startTs] ?: continue
            if (day.startTs >= startTs && day.endTs <= endTs) points.addAll(c.points)
            else c.points.filterTo(points) { it.ts in startMs..endMs }
        }
        TelemRangeResult(points, days.size, cachedDays, refreshed, failed, newRows)
    }

    /** Usuwa WYŁĄCZNIE lokalne kopie (RAM + pliki w telefonie). Firebase pozostaje nietknięty. */
    suspend fun clear() {
        mutex.withLock {
            memory.clear()
            val d = dir ?: return@withLock
            withContext(io) { d.listFiles()?.forEach { it.delete() } }
        }
    }

    private fun readDay(day: TelemDay): TelemDayEntry? {
        val d = dir ?: return null
        val f = File(d, day.fileName)
        if (!f.isFile) return null
        return try {
            FileInputStream(f).use { fis ->
                GZIPInputStream(fis.buffered()).use { gz ->
                    TelemDayCodec.read(gz.bufferedReader(Charsets.UTF_8), day)
                }
            }
        } catch (e: Exception) {
            f.delete() // uszkodzona/nieznana kopia — dobę pobierzemy ponownie
            null
        }
    }

    /** Atomowy zapis (plik tymczasowy + rename). Błąd zapisu nie psuje ładowania — to tylko cache. */
    private fun writeDay(entry: TelemDayEntry): Boolean {
        val d = dir ?: return false
        val tmp = File(d, entry.day.fileName + "." + System.nanoTime() + ".tmp")
        return try {
            if (!d.isDirectory && !d.mkdirs()) return false
            FileOutputStream(tmp).use { fos ->
                val writer = GZIPOutputStream(BufferedOutputStream(fos, 32 * 1024)).bufferedWriter(Charsets.UTF_8)
                TelemDayCodec.write(writer, entry)
                writer.close()
            }
            val target = File(d, entry.day.fileName)
            tmp.renameTo(target) || (target.delete() && tmp.renameTo(target))
        } catch (e: Exception) {
            false
        } finally {
            if (tmp.exists()) tmp.delete()
        }
    }

    /** Kasuje lokalne kopie starsze niż [keepDays] dni i porzucone pliki tymczasowe. */
    private fun prune(today: TelemDay) {
        val d = dir ?: return
        val files = d.listFiles() ?: return
        val keepFrom = today.startTs - keepDays * TelemDay.DAY_SEC
        val now = System.currentTimeMillis()
        for (f in files) {
            val name = f.name
            if (name.endsWith(".tmp")) {
                if (now - f.lastModified() > 10 * 60_000L) f.delete()
                continue
            }
            if (!name.endsWith(TelemDay.FILE_SUFFIX)) continue
            val day = TelemDay.fromFileName(name)
            if (day == null || day.startTs < keepFrom) f.delete()
        }
    }

    companion object {
        /** Katalog kopii: `<cacheDir>/telemetry-1m-v1/<host bazy>` (inna baza = osobne kopie). */
        fun directoryFor(cacheRoot: File, dbUrl: String): File {
            val host = dbUrl.substringAfter("://").substringBefore('/')
                .filter { it.isLetterOrDigit() || it == '.' || it == '-' || it == '_' }
                .trim('.')
                .ifEmpty { "default" }
            return File(File(cacheRoot, "telemetry-1m-v1"), host)
        }
    }
}
