package com.sterownikco.pro.core

import android.os.SystemClock
import androidx.compose.runtime.mutableIntStateOf
import com.sterownikco.pro.BuildConfig
import org.json.JSONObject
import kotlin.math.roundToInt

/* ══════════════════════════════════════════════════════════════════════════
   Stan odczytany z Firebase / ESP32. Wartości pomiarowe pozostają niedostępne
   do chwili odebrania poprawnego pola; nie są zastępowane próbkami przykładowymi.
   ══════════════════════════════════════════════════════════════════════════ */

data class SymEntry(var wartosc: Double, var min: Int, val expiresAtNanos: Long)
data class HistPoint(val o: Double, val b: Double, val z: Double)

class PiecState {
    // ── dane pomiarowe (NaN = brak rzeczywistego odczytu) ────────────────────
    var online = false
    var night = false
    var t_zewn = Double.NaN
    var t_ogrz = Double.NaN
    var t_ogrz_sr = Double.NaN
    var t_bojler = Double.NaN
    var t_panel = Double.NaN
    var t_pokoj = Double.NaN
    var t_powrot = Double.NaN
    var t_trociny = Double.NaN
    var cisnienie = Double.NaN
    var wilgotnosc = Double.NaN
    var dym = Double.NaN
    var dym_wlaczony = false
    var dym_swiezy = false
    var dym_alarm = false
    var alarm_ogrzewanie = false
    var alarm_panel = false

    // ── urządzenia ──────────────────────────────────────────────────────────
    var pompa = false
    var mieszadlo = false
    var rozpalanie = false
    var tryb_serwa = 0
    var trybSerwa = 0
    var klapa = 0
    var syberka = 0
    var wybor = 0
    var pompa_override_min = 0
    var mieszadlo_override_min = 0
    var serwo_override_min = 0

    // ── zegar / identyfikacja ───────────────────────────────────────────────
    var rtc_ok = false
    var dzien = 0
    var miesiac = 0
    var rok = 0
    var ip = ""
    var firmware = ""
    var uptime = Double.NaN
    var appVersion = BuildConfig.VERSION_NAME
    var wifi_ssid = ""
    var weatherCode = -1
    var wifi_rssi = 0
    var wifi_quality = 0
    var ping_ms = 0

    // ── ustawienia ESP: 0 jest wyłącznie wartością wewnętrzną; UI sprawdza hasData ─
    var czasOn = 0
    var czasOff = 0
    var tempOn = 0
    var tempOff = 0
    var antystopWlaczony = false
    var antystopDni = 0
    var mieszadloWlaczony = false
    var mieszadloCzasOn = 0
    var mieszadloCzasOff = 0
    var tempZadServo = 0
    var histerServo = 0
    var skokKlapy = 0
    var skokSyberka = 0
    var odchylTemp = 0
    var mnoznik = 0
    var progAlarmTemp = 0
    var progAlarmDym = 0
    var dymCzasOn = 0
    var dymCzasOff = 0
    var dymCzasStabilizacji = 0
    var dymProgTemp = 0
    var dymTrybPracy = 0

    /** Symulacje wyłącznie po ręcznym uruchomieniu przez operatora. */
    val sym = HashMap<String, SymEntry>()

    /** Pola potwierdzone rzeczywistym, poprawnym odczytem z ostatniej odpowiedzi. */
    private val receivedFields = mutableSetOf<String>()
    fun hasData(key: String): Boolean = key in receivedFields
    fun hasAllData(vararg keys: String): Boolean = keys.all(::hasData)

    /** Bufor miniwykresu — dopisywane wyłącznie przy trzech poprawnych pomiarach. */
    val hist = ArrayDeque<HistPoint>()

    /** Licznik zmian odczytywany przez Compose. */
    val rev = mutableIntStateOf(0)
    fun bump() { rev.intValue = rev.intValue + 1 }

    fun fmt1(v: Double): String = if (v.isFinite()) String.format(java.util.Locale.US, "%.1f", v) else "—"

    fun symAktywna(pole: String): Boolean = sym.containsKey(pole)

    fun valOf(pole: String, key: String): Double = sym[pole]?.wartosc ?: if (!hasData(key)) Double.NaN else when (key) {
        "t_zewn" -> t_zewn
        "t_ogrz" -> t_ogrz
        "t_ogrz_sr" -> t_ogrz_sr
        "t_bojler" -> t_bojler
        "t_panel" -> t_panel
        "t_pokoj" -> t_pokoj
        "t_powrot" -> t_powrot
        "t_trociny" -> t_trociny
        "cisnienie" -> cisnienie
        "wilgotnosc" -> wilgotnosc
        "dym" -> dym
        else -> Double.NaN
    }

    private fun clearSensorValue(key: String) {
        receivedFields.remove(key)
        when (key) {
            "t_zewn" -> t_zewn = Double.NaN
            "t_ogrz" -> t_ogrz = Double.NaN
            "t_ogrz_sr" -> t_ogrz_sr = Double.NaN
            "t_bojler" -> t_bojler = Double.NaN
            "t_panel" -> t_panel = Double.NaN
            "t_pokoj" -> t_pokoj = Double.NaN
            "t_powrot" -> t_powrot = Double.NaN
            "t_trociny" -> t_trociny = Double.NaN
            "cisnienie" -> cisnienie = Double.NaN
            "wilgotnosc" -> wilgotnosc = Double.NaN
            "dym" -> dym = Double.NaN
        }
    }

    /** Ilustracje otrzymują wartości odczytane; brak pomiaru nie trafia do tekstowych odczytów. */
    fun art(weatherCode: Int, isDay: Boolean): com.sterownikco.pro.ui.art.ArtState {
        fun artValue(v: Double) = if (v.isFinite()) v.toFloat() else Float.NaN
        return com.sterownikco.pro.ui.art.ArtState(
            night = night,
            weatherCode = weatherCode,
            isDay = isDay,
            tZewn = artValue(valOf("zewn", "t_zewn")),
            tOgrz = artValue(valOf("ogrz", "t_ogrz")),
            tBojler = artValue(valOf("bojler", "t_bojler")),
            tPanel = artValue(valOf("panel", "t_panel")),
            tPokoj = artValue(valOf("pokoj", "t_pokoj")),
            alarmOgrz = alarm_ogrzewanie,
            alarmPanel = alarm_panel,
            pompa = pompa,
            pompaKnown = hasData("pompa"),
            klapa = if (hasData("klapa") && klapa in 0..180) klapa.toFloat() else Float.NaN,
            klapaKnown = hasData("klapa") && klapa in 0..180,
            syberka = if (hasData("syberka") && syberka in 0..90) syberka.toFloat() else Float.NaN,
            mieszadlo = mieszadlo,
            mieszadloKnown = hasData("mieszadlo"),
            dymKnown = hasData("dym") || symAktywna("dym"),
            dymAlarm = dym_alarm,
            klapaAktywne = hasData("tryb_serwa") && tryb_serwa in 1..3 && tryb_serwa == 1,
            cisnienie = artValue(cisnienie),
            wilgotnosc = artValue(wilgotnosc),
            godz = java.util.Calendar.getInstance().get(java.util.Calendar.HOUR_OF_DAY),
            min = java.util.Calendar.getInstance().get(java.util.Calendar.MINUTE)
        )
    }

    /** Stosuje wyłącznie pola obecne, poprawne i mieszczące się w zakresach z odpowiedzi Firebase. */
    fun applyIncomingData(d: JSONObject) {
        receivedFields.clear()

        // Każdy snapshot jest nowym obrazem statusu. Brak pola ma oznaczać
        // BRAK DANYCH, a nie zachowanie poprzedniego odczytu jako świeżego.
        listOf("t_ogrz", "t_ogrz_sr", "t_bojler", "t_zewn", "t_panel", "t_pokoj", "t_powrot", "t_trociny", "cisnienie", "wilgotnosc", "dym")
            .forEach { clearSensorValue(it) }
        online = false; night = false
        dym_wlaczony = false; dym_swiezy = false; dym_alarm = false
        alarm_ogrzewanie = false; alarm_panel = false
        pompa = false; mieszadlo = false; rozpalanie = false
        antystopWlaczony = false; mieszadloWlaczony = false; rtc_ok = false
        tryb_serwa = 0; trybSerwa = 0; klapa = 0; syberka = 0; wybor = 0
        pompa_override_min = 0; mieszadlo_override_min = 0; serwo_override_min = 0
        czasOn = 0; czasOff = 0; tempOn = 0; tempOff = 0; antystopDni = 0
        mieszadloCzasOn = 0; mieszadloCzasOff = 0; tempZadServo = 0; histerServo = 0
        skokKlapy = 0; skokSyberka = 0; odchylTemp = 0; mnoznik = 0
        progAlarmTemp = 0; progAlarmDym = 0; dymCzasOn = 0; dymCzasOff = 0
        dymCzasStabilizacji = 0; dymProgTemp = 0; dymTrybPracy = 0
        dzien = 0; miesiac = 0; rok = 0; uptime = Double.NaN
        wifi_rssi = 0; wifi_quality = 0; ping_ms = 0
        ip = ""; firmware = ""; wifi_ssid = ""

        fun readNumber(key: String, min: Double, max: Double, integer: Boolean = false): Double? {
            if (!d.has(key) || d.isNull(key)) { receivedFields.remove(key); return null }
            val value = (d.opt(key) as? Number)?.toDouble()
            if (value == null || !value.isFinite() || value < min || value > max || (integer && value % 1.0 != 0.0)) {
                receivedFields.remove(key)
                return null
            }
            receivedFields.add(key)
            return value
        }

        fun sensor(key: String, min: Double, max: Double, assign: (Double) -> Unit) {
            assign(readNumber(key, min, max) ?: Double.NaN)
        }

        fun flag(key: String, assign: (Boolean) -> Unit) {
            if (!d.has(key) || d.isNull(key)) { receivedFields.remove(key); return }
            val value = when (val raw = d.opt(key)) {
                is Boolean -> raw
                is Number -> raw.toDouble().takeIf { it.isFinite() && (it == 0.0 || it == 1.0) }?.let { it == 1.0 }
                else -> null
            }
            if (value == null) receivedFields.remove(key)
            else { receivedFields.add(key); assign(value) }
        }

        fun text(key: String, assign: (String) -> Unit) {
            val value = if (d.has(key) && !d.isNull(key)) d.opt(key) as? String else null
            if (value.isNullOrBlank()) receivedFields.remove(key)
            else { receivedFields.add(key); assign(value.trim()) }
        }

        fun intField(key: String, min: Int, max: Int, assign: (Int) -> Unit) {
            assign(readNumber(key, min.toDouble(), max.toDouble(), integer = true)?.toInt() ?: 0)
        }

        sensor("t_ogrz", 10.0, 99.0) { t_ogrz = it }
        sensor("t_ogrz_sr", 10.0, 99.0) { t_ogrz_sr = it }
        sensor("t_bojler", 10.0, 95.0) { t_bojler = it }
        sensor("t_zewn", -25.0, 45.0) { t_zewn = it }
        sensor("t_panel", 0.0, 140.0) { t_panel = it }
        sensor("t_pokoj", 8.0, 45.0) { t_pokoj = it }
        sensor("t_powrot", 10.0, 95.0) { t_powrot = it }
        sensor("t_trociny", 0.0, 90.0) { t_trociny = it }
        sensor("cisnienie", 900.0, 1100.0) { cisnienie = it }
        sensor("wilgotnosc", 10.0, 100.0) { wilgotnosc = it }
        sensor("dym", 0.0, 4095.0) { dym = it }

        flag("online") { online = it }
        flag("night") { night = it }
        flag("dym_wlaczony") { dym_wlaczony = it }
        flag("dym_swiezy") { dym_swiezy = it }
        flag("dym_alarm") { dym_alarm = it }
        flag("alarm_ogrzewanie") { alarm_ogrzewanie = it }
        flag("alarm_panel") { alarm_panel = it }
        flag("pompa") { pompa = it }
        flag("mieszadlo") { mieszadlo = it }
        flag("rozpalanie") { rozpalanie = it }
        flag("antystopWlaczony") { antystopWlaczony = it }
        flag("mieszadloWlaczony") { mieszadloWlaczony = it }
        flag("rtc_ok") { rtc_ok = it }

        val mode = readNumber("tryb_serwa", 1.0, 3.0, integer = true)
            ?: readNumber("trybSerwa", 1.0, 3.0, integer = true)?.also { receivedFields.remove("trybSerwa") }
        if (mode != null) {
            tryb_serwa = mode.toInt(); trybSerwa = mode.toInt(); receivedFields.add("tryb_serwa")
        } else {
            tryb_serwa = 0; trybSerwa = 0; receivedFields.remove("tryb_serwa")
        }

        intField("klapa", 0, 180) { klapa = it }
        intField("syberka", 0, 90) { syberka = it }
        intField("wybor", 1, 3) { wybor = it }
        intField("pompa_override_min", 0, 1440) { pompa_override_min = it }
        intField("mieszadlo_override_min", 0, 1440) { mieszadlo_override_min = it }
        intField("serwo_override_min", 0, 1440) { serwo_override_min = it }
        intField("czasOn", 1, 255) { czasOn = it }
        intField("czasOff", 1, 255) { czasOff = it }
        intField("tempOn", 0, 100) { tempOn = it }
        intField("tempOff", 0, 100) { tempOff = it }
        intField("antystopDni", 1, 45) { antystopDni = it }
        intField("mieszadloCzasOn", 1, 255) { mieszadloCzasOn = it }
        intField("mieszadloCzasOff", 1, 180) { mieszadloCzasOff = it }
        intField("tempZadServo", 0, 100) { tempZadServo = it }
        intField("histerServo", 0, 50) { histerServo = it }
        intField("skokKlapy", 1, 100) { skokKlapy = it }
        intField("skokSyberka", 1, 100) { skokSyberka = it }
        intField("odchylTemp", 1, 50) { odchylTemp = it }
        intField("mnoznik", 1, 10) { mnoznik = it }
        intField("progAlarmTemp", 0, 100) { progAlarmTemp = it }
        intField("progAlarmDym", 0, 4095) { progAlarmDym = it }
        intField("dymCzasOn", 20, 600) { dymCzasOn = it }
        intField("dymCzasOff", 10, 600) { dymCzasOff = it }
        intField("dymCzasStabilizacji", 5, 590) { dymCzasStabilizacji = it }
        intField("dymProgTemp", 0, 200) { dymProgTemp = it }
        intField("dymTrybPracy", 0, 1) { dymTrybPracy = it }
        intField("dzien", 1, 31) { dzien = it }
        intField("miesiac", 1, 12) { miesiac = it }
        intField("rok", 2000, 2100) { rok = it }

        readNumber("uptime", 0.0, Long.MAX_VALUE.toDouble())?.let { uptime = it }
        readNumber("wifi_rssi", -127.0, 0.0)?.let { wifi_rssi = it.roundToInt() }
        readNumber("wifi_quality", 0.0, 100.0, integer = true)?.let { wifi_quality = it.roundToInt() }
        readNumber("ping_ms", 0.0, 100_000.0, integer = true)?.let { ping_ms = it.roundToInt() }
        text("ip") { ip = it }
        text("fw") { firmware = it }
        text("wifi_ssid") { wifi_ssid = it }

        if (!hasData("wifi_rssi")) {
            val alias = readNumber("rssi", -127.0, 0.0)
            if (alias != null) {
                wifi_rssi = alias.roundToInt()
                receivedFields.remove("rssi")
                receivedFields.add("wifi_rssi")
            }
        }

        // Firmware may omit `online`; infer it only from a valid telemetry or
        // identity field, never from a malformed mode/flag or a default.
        if (!hasData("online")) {
            online = receivedFields.any { it in ONLINE_FIELDS }
        }
        pushHist()
    }

    private fun pushHist() {
        // Miniwykres pozostaje z prawdziwych pomiarów — jawna symulacja go nie zanieczyszcza.
        if (sym.keys.any { it in setOf("ogrz", "bojler", "zewn") }) return
        if (!online || !hasAllData("t_ogrz", "t_bojler", "t_zewn")) return
        val o = t_ogrz
        val b = t_bojler
        val z = t_zewn
        if (!o.isFinite() || !b.isFinite() || !z.isFinite()) return
        hist.addLast(HistPoint(o, b, z))
        while (hist.size > 60) hist.removeFirst()
    }

    /** Polecenia symulacji są obsługiwane tylko po jawnej akcji operatora. */
    fun applySimulationCommand(command: String, nowNanos: Long = SystemClock.elapsedRealtimeNanos()): Any {
        val parts = command.trim().split(Regex("\\s+"))
        return when (parts.firstOrNull()?.lowercase()) {
            "symuluj" -> {
                val pole = parts.getOrNull(1) ?: return "nie podano pola"
                if (pole !in SYM_POLA) return "nieznane pole $pole"
                val value = parts.getOrNull(2)?.toDoubleOrNull()?.takeIf { it.isFinite() }
                    ?: return "podaj poprawną wartość"
                val bounds = SYM_BOUNDS.getValue(pole)
                if (value < bounds.first || value > bounds.second) {
                    return "wartość poza zakresem ${bounds.first}–${bounds.second}"
                }
                val minutes = parts.getOrNull(3)?.toIntOrNull() ?: 60
                if (minutes !in 1..180) return "czas symulacji musi wynosić 1–180 min"
                sym[pole] = SymEntry(value, minutes, nowNanos + minutes * NANOS_PER_MINUTE)
                true
            }
            "symuluj_stop" -> {
                val pole = parts.getOrNull(1) ?: return "nie podano pola"
                if (pole !in SYM_POLA) return "nieznane pole $pole"
                sym.remove(pole)
                true
            }
            else -> "nieznane polecenie symulacji"
        }
    }

    /** Aktualizuje pozostały czas wyłącznie dla ręcznych symulacji; nie generuje pomiarów. */
    fun liveTick(nowNanos: Long = SystemClock.elapsedRealtimeNanos()): Boolean {
        if (sym.isEmpty()) return false
        val expired = ArrayList<String>()
        var changed = false
        for ((pole, entry) in sym) {
            val remainingNanos = entry.expiresAtNanos - nowNanos
            val remainingMinutes = if (remainingNanos <= 0L) 0L
                else (remainingNanos + NANOS_PER_MINUTE - 1L) / NANOS_PER_MINUTE
            if (remainingMinutes <= 0L) {
                expired.add(pole)
            } else if (entry.min != remainingMinutes.toInt()) {
                entry.min = remainingMinutes.toInt()
                changed = true
            }
        }
        expired.forEach(sym::remove)
        return changed || expired.isNotEmpty()
    }

    companion object {
        private const val NANOS_PER_MINUTE = 60_000_000_000L
        val SYM_POLA = linkedMapOf(
            "zewn" to "t_zewn", "ogrz" to "t_ogrz", "bojler" to "t_bojler", "panel" to "t_panel",
            "pokoj" to "t_pokoj", "ogrz_powrot" to "t_powrot", "ogrz_trociny" to "t_trociny",
            "cisnienie" to "cisnienie", "wilgotnosc" to "wilgotnosc", "dym" to "dym"
        )
        val SYM_BOUNDS = mapOf(
            "zewn" to (-25.0 to 45.0), "ogrz" to (10.0 to 99.0), "bojler" to (10.0 to 95.0),
            "panel" to (0.0 to 140.0), "pokoj" to (8.0 to 45.0), "ogrz_powrot" to (10.0 to 95.0),
            "ogrz_trociny" to (0.0 to 90.0), "cisnienie" to (900.0 to 1100.0),
            "wilgotnosc" to (10.0 to 100.0), "dym" to (0.0 to 4095.0)
        )
        private val ONLINE_FIELDS = setOf(
            "t_ogrz", "t_bojler", "t_zewn", "t_panel", "t_pokoj", "cisnienie", "wilgotnosc", "dym",
            "pompa", "mieszadlo", "tryb_serwa", "klapa", "syberka", "ip", "fw", "wifi_rssi"
        )
        val TRYB_NAZWA = mapOf(1 to "AUTO", 2 to "RĘCZNY", 3 to "BEZPIECZNY")
        fun pad2(v: Int) = if (v < 10) "0$v" else "$v"
    }
}
