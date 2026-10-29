package com.sterownikco.pro.core

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import java.io.BufferedWriter
import java.io.File
import java.io.OutputStreamWriter
import java.util.zip.GZIPInputStream
import java.util.zip.GZIPOutputStream

/* ══════════════════════════════════════════════════════════════════════════
   [0.31.43] PAMIĘĆ HISTORII TELEMETRII W TELEFONIE

   Problem: do 0.31.42 każde wejście w „Wykresy", każde przełączenie zakresu
   (7 DNI → 30 DNI) i każde „Odśwież" ściągało z Firebase CAŁE wybrane okno
   od nowa. 7 dni = 9944 próbek, 30 dni = ~43 000, mimo że telefon chwilę
   wcześniej miał je już w pamięci (tyle że tylko w RAM, ginącej po zamknięciu
   aplikacji).

   Rozwiązanie:
   1) Historia ląduje na dysku w `filesDir/telemetria/historia.tsv.gz`
      (gzip, jeden wiersz = jedna próbka, 13 kanałów + pompa + bitmapa symulacji).
   2) Przy każdym pobraniu liczymy, czego BRAKUJE w oknie, i pytamy chmurę
      tylko o te przedziały — zwykle jest to ogon „od ostatniej próbki do teraz".
   3) Luki, o które już pytaliśmy i chmura ich nie ma, zapamiętujemy jako
      „potwierdzenie puste" w `luki.tsv` — nie pytamy o nie w kółko.
   4) Chmura bez zasięgu? Wykres i tak się rysuje z pamięci telefonu.

   Plik jest zapisywany atomowo (tmp + rename), więc przerwanie zapisu
   (np. zabicie procesu) nie psuje pamięci — najwyżej zostaje poprzednia wersja.
   ══════════════════════════════════════════════════════════════════════════ */

class TelemetryCache(ctx: Context) {

    companion object {
        /** Nagłówek pliku — pozwala w przyszłości zmienić format bez crasha. */
        private const val MAGIC = "#sterownikco-telemetria-1"

        /** Ile dni historii trzymamy. 30 dni to najdłuższy zakres + 5 dni zapasu. */
        private const val RETENTION_MS = 35L * 86_400_000L

        /**
         * Przerwa krótsza niż tyle nie jest luką — próbki idą co minutę,
         * więc 6 minut ciszy znaczy, że czegoś naprawdę brakuje.
         */
        const val GAP_MS = 6L * 60_000L

        /** Kolejność i liczba kanałów w wierszu (t_zewn … syberka). */
        private const val CHANNELS = 13

        /** Poniżej tej długości nie ma sensu pytać chmury — to nie luka, tylko szum. */
        private const val MIN_QUERY_MS = 60_000L
    }

    /** Co pokazać z pamięci i o co dopytać chmurę. */
    data class Plan(
        /** Próbki z pamięci telefonu w żądanym oknie, rosnąco po czasie. */
        val cached: List<TelemPoint>,
        /** Przedziały (ms), których w pamięci nie ma — tylko o nie pytamy chmurę. */
        val missing: List<LongRange>
    )

    // applicationContext + lazy: konstruktor AppModel nie dotyka dysku,
    // a ścieżka liczy się dopiero przy pierwszym odczycie/zapisie.
    private val appContext = ctx.applicationContext ?: ctx
    private val dir: File by lazy { File(appContext.filesDir, "telemetria") }
    private val dataFile: File by lazy { File(dir, "historia.tsv.gz") }
    private val holesFile: File by lazy { File(dir, "luki.tsv") }

    private val lock = Mutex()
    private var loaded = false
    private val points = LinkedHashMap<Long, TelemPoint>()
    private val holes = ArrayList<LongRange>()

    // ── API ─────────────────────────────────────────────────────────────────

    /**
     * Co mamy i czego brakuje w oknie [fromMs, toMs].
     * Pierwsze wywołanie w procesie wczytuje plik z dysku (raz).
     */
    suspend fun plan(fromMs: Long, toMs: Long): Plan = withContext(Dispatchers.IO) {
        lock.withLock {
            ensureLoadedLocked()
            dropOldLocked(System.currentTimeMillis() - RETENTION_MS)
            val cached = points.values.filter { it.ts in fromMs..toMs }.sortedBy { it.ts }
            Plan(cached, missingLocked(fromMs, toMs, cached))
        }
    }

    /**
     * Zapisuje to, co przyszło z chmury, i zapamiętuje potwierdzone puste luki.
     * [confirmedEmpty] wolno podać wyłącznie dla zapytań, które się udały —
     * inaczej zapamiętalibyśmy jako „puste" miejsce, o które nie zdążyliśmy spytać.
     */
    suspend fun store(fetched: List<TelemPoint>, confirmedEmpty: List<LongRange>) =
        withContext(Dispatchers.IO) {
            lock.withLock {
                ensureLoadedLocked()
                fetched.forEach { p -> if (p.ts > 0L && p.hasValues()) points[p.ts] = p }
                confirmedEmpty.forEach { h -> if (h.last - h.first >= MIN_QUERY_MS) addHoleLocked(h) }
                dropOldLocked(System.currentTimeMillis() - RETENTION_MS)
                forgetHolesWithDataLocked()
                writePointsLocked()
                writeHolesLocked()
            }
        }

    /** Fragmenty [seg], w których po udanym zapytaniu nie było ANI JEDNEJ próbki. */
    fun emptyParts(seg: LongRange, found: List<TelemPoint>): List<LongRange> =
        uncovered(seg.first, seg.last, found.sortedBy { it.ts })

    /** Czyści pamięć (przydatne po wylogowaniu albo przy testach). */
    suspend fun clear() = withContext(Dispatchers.IO) {
        lock.withLock {
            points.clear()
            holes.clear()
            loaded = true
            try { dataFile.delete() } catch (e: Exception) { /* brak pliku = OK */ }
            try { holesFile.delete() } catch (e: Exception) { /* brak pliku = OK */ }
        }
    }

    // ── liczenie braków ─────────────────────────────────────────────────────

    /**
     * Fragmenty okna, w których nie ma próbek. Kolejno: początek okna → pierwsza
     * próbka, dziury między próbkami, ostatnia próbka → koniec okna.
     * Na koniec odejmujemy luki już potwierdzone jako puste.
     */
    private fun missingLocked(fromMs: Long, toMs: Long, cached: List<TelemPoint>): List<LongRange> {
        var out = uncovered(fromMs, toMs, cached)
        for (h in holes.sortedBy { it.first }) {
            if (out.isEmpty()) break
            out = out.flatMap { r -> subtract(r, h) }
        }
        return out.filter { it.last - it.first >= MIN_QUERY_MS }
    }

    private fun uncovered(fromMs: Long, toMs: Long, cached: List<TelemPoint>): List<LongRange> {
        if (toMs <= fromMs) return emptyList()
        if (cached.isEmpty()) return listOf(fromMs..toMs)
        val out = ArrayList<LongRange>()
        var cursor = fromMs
        for (p in cached) {
            if (p.ts - cursor > GAP_MS) out.add(cursor..(p.ts - 1L))
            if (p.ts + 1L > cursor) cursor = p.ts + 1L
        }
        if (toMs - cursor > GAP_MS) out.add(cursor..toMs)
        return out
    }

    /** [r] bez części pokrytej przez [h]; wynik to 0–2 fragmenty. */
    private fun subtract(r: LongRange, h: LongRange): List<LongRange> {
        if (h.last < r.first || h.first > r.last) return listOf(r)
        val out = ArrayList<LongRange>(2)
        if (h.first > r.first) out.add(r.first..(h.first - 1L))
        if (h.last < r.last) out.add((h.last + 1L)..r.last)
        return out
    }

    // ── luki potwierdzone jako puste ─────────────────────────────────────────

    private fun addHoleLocked(h: LongRange) {
        var start = h.first
        var end = h.last
        val it = holes.iterator()
        while (it.hasNext()) {
            val e = it.next()
            if (e.last < start - 1_000L || e.first > end + 1_000L) continue
            if (e.first < start) start = e.first
            if (e.last > end) end = e.last
            it.remove()
        }
        holes.add(start..end)
    }

    /** Luka, w której pojawiły się dane, przestaje być luką. */
    private fun forgetHolesWithDataLocked() {
        if (holes.isEmpty() || points.isEmpty()) return
        val it = holes.iterator()
        while (it.hasNext()) {
            val h = it.next()
            if (points.keys.any { ts -> ts in h }) it.remove()
        }
    }

    // ── wczytywanie / zapis ─────────────────────────────────────────────────

    private fun ensureLoadedLocked() {
        if (loaded) return
        loaded = true
        readPointsLocked()
        readHolesLocked()
    }

    private fun readPointsLocked() {
        points.clear()
        if (!dataFile.isFile) return
        try {
            val lines: List<String> = GZIPInputStream(dataFile.inputStream().buffered()).use { input ->
                input.bufferedReader(Charsets.UTF_8).readLines()
            }
            lines.forEach { line ->
                if (line.isEmpty() || line.startsWith("#")) return@forEach
                decodePoint(line)?.let { p -> points[p.ts] = p }
            }
        } catch (e: Exception) {
            // Plik uszkodzony (np. starsza wersja) — startujemy z pustą pamięcią,
            // a nie z wyjątkiem w środku rysowania wykresu.
            points.clear()
        }
    }

    private fun readHolesLocked() {
        holes.clear()
        if (!holesFile.isFile) return
        try {
            val lines: List<String> = holesFile.bufferedReader(Charsets.UTF_8).readLines()
            lines.forEach { line ->
                if (line.isEmpty() || line.startsWith("#")) return@forEach
                val c = line.split(';')
                if (c.size < 2) return@forEach
                val a = c[0].toLongOrNull() ?: return@forEach
                val b = c[1].toLongOrNull() ?: return@forEach
                if (b > a) holes.add(a..b)
            }
        } catch (e: Exception) {
            holes.clear()
        }
    }

    private fun writePointsLocked() {
        try {
            if (!dir.isDirectory && !dir.mkdirs()) return
            val tmp = File(dir, "historia.tsv.gz.tmp")
            GZIPOutputStream(tmp.outputStream().buffered()).use { out ->
                val w = BufferedWriter(OutputStreamWriter(out, Charsets.UTF_8))
                w.write(MAGIC)
                w.write("\n")
                points.values.sortedBy { it.ts }.forEach { p ->
                    w.write(encodePoint(p))
                    w.write("\n")
                }
                w.flush()
            }
            if (!tmp.renameTo(dataFile)) {
                if (dataFile.exists()) dataFile.delete()
                tmp.renameTo(dataFile)
            }
        } catch (e: Exception) {
            // Brak miejsca / błąd zapisu — pamięć zostaje w RAM, wykres działa.
        }
    }

    private fun writeHolesLocked() {
        try {
            if (!dir.isDirectory && !dir.mkdirs()) return
            val tmp = File(dir, "luki.tsv.tmp")
            BufferedWriter(OutputStreamWriter(tmp.outputStream(), Charsets.UTF_8)).use { w ->
                w.write("#poczatek;koniec (ms) — przedzialy, o ktore pytalismy i sa puste")
                w.write("\n")
                holes.sortedBy { it.first }.forEach { h ->
                    w.write(h.first.toString())
                    w.write(";")
                    w.write(h.last.toString())
                    w.write("\n")
                }
            }
            if (!tmp.renameTo(holesFile)) {
                if (holesFile.exists()) holesFile.delete()
                tmp.renameTo(holesFile)
            }
        } catch (e: Exception) {
            // j.w. — brak pamięci na luki to tylko kilka dodatkowych zapytań
        }
    }

    /** Kasuje próbki starsze niż [cutoffMs] — inaczej plik rósłby w nieskończoność. */
    private fun dropOldLocked(cutoffMs: Long) {
        if (points.isEmpty()) return
        points.keys.filter { it < cutoffMs }.forEach { points.remove(it) }
        holes.removeAll { it.last < cutoffMs }
    }

    // ── wiersz próbki: ts;seq;sim;pompa;13 kanałów ──────────────────────────

    private fun encodePoint(p: TelemPoint): String {
        val sb = StringBuilder(96)
        sb.append(p.ts).append(';').append(p.seq).append(';').append(p.sim).append(';')
        when (p.pumpOn) {
            true -> sb.append('1')
            false -> sb.append('0')
            null -> Unit
        }
        val ch = listOf(
            p.t_zewn, p.t_bojler, p.t_ogrz, p.t_ogrz_sr, p.t_powrot, p.t_panel,
            p.t_pokoj, p.t_trociny, p.wilgotnosc, p.cisnienie, p.dym, p.klapa, p.syberka
        )
        ch.forEach { v ->
            sb.append(';')
            if (v != null && v.isFinite()) sb.append(v)
        }
        return sb.toString()
    }

    private fun decodePoint(line: String): TelemPoint? {
        val c = line.split(';')
        if (c.size < 4 + CHANNELS) return null
        val ts = c[0].toLongOrNull() ?: return null
        if (ts <= 0L) return null
        fun num(i: Int): Double? = c[i].trim().takeIf { it.isNotEmpty() }?.toDoubleOrNull()?.takeIf { it.isFinite() }
        val pump = when (c[3].trim()) {
            "1" -> true
            "0" -> false
            else -> null
        }
        return TelemPoint(
            ts = ts,
            seq = c[1].toLongOrNull() ?: 0L,
            t_zewn = num(4),
            t_bojler = num(5),
            t_ogrz = num(6),
            t_ogrz_sr = num(7),
            t_powrot = num(8),
            t_panel = num(9),
            t_pokoj = num(10),
            t_trociny = num(11),
            wilgotnosc = num(12),
            cisnienie = num(13),
            dym = num(14),
            klapa = num(15),
            syberka = num(16),
            sim = c[2].toLongOrNull() ?: 0L,
            simulated = false,
            isLive = false,
            pumpOn = pump
        )
    }
}
