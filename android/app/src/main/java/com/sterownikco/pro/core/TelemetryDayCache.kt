package com.sterownikco.pro.core

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneOffset
import java.util.Locale

/* ══════════════════════════════════════════════════════════════════════════
   DZIENNY CACHE TELEMETRII — pamięć prywatna aplikacji.

   Jeden plik = jeden dzień UTC (`RRRR-MM-DD.json`), dokładnie w tym samym
   podziale, w jakim Firebase trzyma shardy `/piec/telemetry/1m/v1/RRRR/MM/DD`.

   W środku leżą surowe [TelemRow], a nie gotowe punkty wykresu, ponieważ
   korekta czasu ([TelemetryTimeRepair]) musi być policzona na całym zbiorze.
   Zapis punktów po korekcie zamroziłby raz policzone przesunięcie sesji.

   Cache jest wyłącznie optymalizacją: każdy błąd zapisu lub odczytu kończy
   się cichym powrotem do pobierania z Firebase, nigdy wyjątkiem w UI.
   ══════════════════════════════════════════════════════════════════════════ */
class TelemetryDayCache(private val root: File) {

    /** Wersja produkcyjna: katalog w prywatnej pamięci aplikacji. */
    constructor(ctx: Context) : this(File(ctx.filesDir, CACHE_DIR))

    /** Zawartość jednego dnia razem z czasem zapisu pliku. */
    data class Day(val rows: List<TelemRow>, val savedAtMs: Long) {
        /** Najnowszy znacznik czasu w tym dniu — punkt startowy dla przyrostowego dociągania. */
        val maxTs: Long = rows.maxOfOrNull { it.ts } ?: 0L
    }

    /** Zwraca `null`, gdy dnia nie ma w pamięci albo plik jest nieczytelny. */
    fun read(dayKey: String): Day? {
        val f = file(dayKey)
        if (!f.isFile) return null
        return try {
            val text = f.readText(Charsets.UTF_8)
            if (text.isBlank()) return null
            val o = JSONObject(text)
            if (o.optInt("v", 0) != CACHE_VERSION) return null
            val arr = o.optJSONArray("rows") ?: return null
            val rows = ArrayList<TelemRow>(arr.length())
            for (i in 0 until arr.length()) {
                val row = rowFromJson(arr.optJSONArray(i))
                if (row != null) rows.add(row)
            }
            if (rows.isEmpty()) null else Day(rows, o.optLong("saved", 0L))
        } catch (e: Exception) {
            // Uszkodzony plik nie może blokować dnia na zawsze.
            try { f.delete() } catch (_: Exception) { }
            null
        }
    }

    /** Zapisuje cały dzień. Pusty zbiór nie tworzy pliku. */
    fun write(dayKey: String, rows: List<TelemRow>) {
        if (rows.isEmpty()) return
        try {
            if (!root.isDirectory && !root.mkdirs()) return
            val o = JSONObject()
            o.put("v", CACHE_VERSION)
            o.put("day", dayKey)
            o.put("saved", System.currentTimeMillis())
            val arr = JSONArray()
            rows.forEach { arr.put(rowToJson(it)) }
            o.put("rows", arr)
            val target = file(dayKey)
            val tmp = File(root, dayKey + ".json.tmp")
            tmp.writeText(o.toString(), Charsets.UTF_8)
            // Zapis atomowy: przerwany rename nie zostawia połowy dnia.
            if (!tmp.renameTo(target)) {
                target.delete()
                if (!tmp.renameTo(target)) {
                    tmp.delete()
                    return
                }
            }
            enforceLimit()
        } catch (e: Exception) {
            // Brak miejsca lub I/O — cache nie może wywrócić aplikacji.
        }
    }

    fun delete(dayKey: String) {
        try { file(dayKey).delete() } catch (_: Exception) { }
    }

    fun clear() {
        try { root.listFiles()?.forEach { it.delete() } } catch (_: Exception) { }
    }

    fun countDays(): Int = try {
        root.listFiles()?.count { it.isFile && it.name.endsWith(".json") } ?: 0
    } catch (e: Exception) { 0 }

    fun sizeBytes(): Long = try {
        root.listFiles()?.sumOf { if (it.isFile) it.length() else 0L } ?: 0L
    } catch (e: Exception) { 0L }

    /**
     * Usuwa dni starsze niż [keepDays] względem [todayKey].
     * Wywoływane raz na proces, przy pierwszym pobraniu historii.
     */
    fun prune(todayKey: String, keepDays: Int = KEEP_DAYS) {
        val cutoff = try {
            LocalDate.parse(todayKey).minusDays(keepDays.toLong()).toString()
        } catch (e: Exception) {
            return
        }
        try {
            root.listFiles()?.forEach { f ->
                val key = dayKeyOf(f.name) ?: return@forEach
                if (key < cutoff) try { f.delete() } catch (_: Exception) { }
            }
        } catch (e: Exception) { }
    }

    /** Twardy limit katalogu — po przekroczeniu usuwamy najstarsze dni. */
    private fun enforceLimit() {
        val files = try { root.listFiles() ?: return } catch (e: Exception) { return }
        var total = files.sumOf { if (it.isFile) it.length() else 0L }
        if (total <= MAX_BYTES) return
        val oldest = files.filter { it.isFile && dayKeyOf(it.name) != null }.sortedBy { it.name }
        for (f in oldest) {
            if (total <= MAX_BYTES) break
            val len = f.length()
            val removed = try { f.delete() } catch (e: Exception) { false }
            if (removed) total -= len
        }
    }

    /** `RRRR-MM-DD` z nazwy pliku cache albo `null` dla plików tymczasowych. */
    private fun dayKeyOf(name: String): String? {
        if (!name.endsWith(".json")) return null
        val key = name.substring(0, name.length - 5)
        if (key.length != 10) return null
        if (key[4] != '-' || key[7] != '-') return null
        return key
    }

    private fun file(dayKey: String) = File(root, dayKey + ".json")

    /** Surowe [TelemRow] → tablica pól; NaN zapisujemy jako null (JSON go nie zna). */
    private fun rowToJson(r: TelemRow): JSONArray {
        val out = JSONArray()
        out.put(r.id)
        out.put(r.seq)
        out.put(r.ts)
        out.put(r.mono)
        val a = JSONArray()
        r.a.forEach { v -> if (v.isFinite()) a.put(v) else a.put(JSONObject.NULL) }
        out.put(a)
        out.put(r.k)
        out.put(r.s)
        out.put(r.state)
        out.put(r.q)
        out.put(r.sim)
        out.put(if (r.simulated) 1 else 0)
        out.put(if (r.stateKnown) 1 else 0)
        return out
    }

    private fun rowFromJson(arr: JSONArray?): TelemRow? {
        if (arr == null || arr.length() < 12) return null
        val id = arr.optString(0, "")
        if (id.isEmpty()) return null
        val raw = arr.optJSONArray(4)
        val a = ArrayList<Double>(raw?.length() ?: 0)
        if (raw != null) {
            for (i in 0 until raw.length()) {
                val v = if (raw.isNull(i)) null else (raw.opt(i) as? Number)?.toDouble()
                a.add(v ?: Double.NaN)
            }
        }
        return TelemRow(
            id = id,
            seq = arr.optLong(1, 0L),
            ts = arr.optLong(2, 0L),
            mono = arr.optLong(3, 0L),
            a = a,
            k = arr.optLong(5, -1L),
            s = arr.optLong(6, -1L),
            state = arr.optLong(7, 0L),
            q = arr.optLong(8, 0L),
            sim = arr.optLong(9, 0L),
            simulated = arr.optInt(10, 0) != 0,
            stateKnown = arr.optInt(11, 0) != 0
        )
    }

    companion object {
        private const val CACHE_VERSION = 1
        private const val CACHE_DIR = "telemetry/v1"

        /** 45 dni zapasu — wykres 30-dniowy i raporty mieszczą się z nadwyżką. */
        private const val KEEP_DAYS = 45

        /** Twardy limit katalogu; po przekroczeniu usuwamy najstarsze dni. */
        private const val MAX_BYTES = 24L * 1024L * 1024L

        /** Klucz dnia UTC (`RRRR-MM-DD`) — ten sam podział, co shardy Firebase. */
        fun utcDayKey(epochSec: Long): String {
            val d: LocalDate = Instant.ofEpochSecond(epochSec).atZone(ZoneOffset.UTC).toLocalDate()
            return String.format(Locale.US, "%04d-%02d-%02d", d.year, d.monthValue, d.dayOfMonth)
        }
    }
}
