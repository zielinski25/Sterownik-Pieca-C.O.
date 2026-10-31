package com.sterownikco.pro.core

import androidx.compose.runtime.mutableIntStateOf
import com.sterownikco.pro.BuildConfig
import org.json.JSONObject
import kotlin.math.roundToInt

/* ══════════════════════════════════════════════════════════════════════════
   Stan odczytany z Firebase / ESP32. Wartości pomiarowe pozostają niedostępne
   do chwili odebrania poprawnego pola; nie są zastępowane próbkami przykładowymi.
   ══════════════════════════════════════════════════════════════════════════ */

data class SymEntry(val min: Int)
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
    var alarm_dym_wyciszony = false
    var alarm_dym_wyciszenie_ms = 0L
    var alarm_ogrz_wyciszony = false
    var alarm_ogrz_wyciszenie_ms = 0L
    var alarm_sound_dym = ""
    var alarm_sound_ogrz = ""
    var alarm_sound_both = ""

    // ── urządzenia ──────────────────────────────────────────────────────────
    var pompa = false
    var mieszadlo = false
    var rozpalanie = false // wspólny tryb serw i mieszadła
    var rozpalanie_potwierdzone = false
    var rozpalanie_pozostalo_s = 0
    var rozpalanie_serwa_aktywne = false
    var rozpalanie_serwa_dostepne = false
    var rozpalanie_serwa_blokada_dymu = false
    var tryb_serwa = 0
    var trybSerwa = 0
    var klapa = 0
    var syberka = 0
    // [0.31.22] Bufor offline — pola z /piec/status (firmware v3.54.0+).
    // spoolRt = 1 wlaczony / 0 wylaczony; reszta to liczniki od startu centrali.
    var spoolRt = 0
    // [0.31.64] Zapis historii na flash centrali (firmware v3.57.4+).
    // histFs = 1 zapis wlaczony / 0 wylaczony (ustawienie w NVS centrali).
    var histFs = 0
    var spoolPages = 0
    var spoolSpill = 0
    var spoolReplay = 0
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

    // ── [v0.31.33 CLOUD-MARK] stan wysyłki telemetrii (pola tel_* z /piec/status) ──
    /** seq ostatniej próbki, którą chmura POTWIERDZIŁA (0 = od restartu nic). */
    var tel_cloud_seq = 0L
    /** unix tej próbki; 0 = brak. */
    var tel_cloud_ts = 0L
    /** ile sekund temu potwierdzono ostatnią próbkę; -1 = brak danych. */
    var tel_cloud_ago = -1L
    /** ile próbek czeka w kolejce sterownika. */
    var tel_ring = 0L
    /** true = w tej chwili leci (lub czeka na ponowienie) paczka do chmury. */
    var tel_pending = false
    /** czy firmware w ogóle przysyła te pola (starsze wersje nie przysyłają). */
    var tel_known = false

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

    /** Aktywne symulacje raportowane przez sterownik Firebase, nigdy lokalne stany optymistyczne. */
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

    fun valOf(@Suppress("UNUSED_PARAMETER") pole: String, key: String): Double = if (!hasData(key)) Double.NaN else when (key) {
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
    fun art(weatherCode: Int, isDay: Boolean, servoMode: Int? = null): com.sterownikco.pro.ui.art.ArtState {
        fun artValue(v: Double) = if (v.isFinite()) v.toFloat() else Float.NaN
        val modeForArt = servoMode ?: tryb_serwa.takeIf { hasData("tryb_serwa") }
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
            dymKnown = hasData("dym"),
            dymAlarm = dym_alarm,
            klapaAktywne = modeForArt == 1,
            cisnienie = artValue(cisnienie),
            wilgotnosc = artValue(wilgotnosc),
            godz = java.util.Calendar.getInstance().get(java.util.Calendar.HOUR_OF_DAY),
            min = java.util.Calendar.getInstance().get(java.util.Calendar.MINUTE)
        )
    }

    /** Stosuje wyłącznie pola obecne, poprawne i mieszczące się w zakresach z odpowiedzi Firebase. */
    fun applyIncomingData(d: JSONObject) {
        receivedFields.clear()
        sym.clear()

        // Każdy snapshot jest nowym obrazem statusu. Brak pola ma oznaczać
        // BRAK DANYCH, a nie zachowanie poprzedniego odczytu jako świeżego.
        listOf("t_ogrz", "t_ogrz_sr", "t_bojler", "t_zewn", "t_panel", "t_pokoj", "t_powrot", "t_trociny", "cisnienie", "wilgotnosc", "dym")
            .forEach { clearSensorValue(it) }
        online = false; night = false
        dym_wlaczony = false; dym_swiezy = false; dym_alarm = false
        alarm_ogrzewanie = false; alarm_panel = false
        alarm_dym_wyciszony = false; alarm_dym_wyciszenie_ms = 0L
        alarm_ogrz_wyciszony = false; alarm_ogrz_wyciszenie_ms = 0L
        alarm_sound_dym = ""; alarm_sound_ogrz = ""; alarm_sound_both = ""
        pompa = false; mieszadlo = false; rozpalanie = false
        rozpalanie_potwierdzone = false; rozpalanie_pozostalo_s = 0
        rozpalanie_serwa_aktywne = false; rozpalanie_serwa_dostepne = false
        rozpalanie_serwa_blokada_dymu = false
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
        tel_cloud_seq = 0; tel_cloud_ts = 0; tel_cloud_ago = -1; tel_ring = 0
        tel_pending = false; tel_known = false
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
            val pole = if (key == "t_ogrz_sr") "ogrz"
                else SYM_POLA.entries.firstOrNull { it.value == key }?.key
            val range = if (pole != null && symAktywna(pole)) SYM_BOUNDS[pole] else null
            assign(readNumber(key, range?.first ?: min, range?.second ?: max) ?: Double.NaN)
        }

        fun flag(key: String, assign: (Boolean) -> Unit) {
            if (!d.has(key) || d.isNull(key)) { receivedFields.remove(key); return }
            val value = parseAlarmFlag(d.opt(key))
            if (value == null) receivedFields.remove(key)
            else { receivedFields.add(key); assign(value) }
        }

        fun text(key: String, assign: (String) -> Unit) {
            val value = if (d.has(key) && !d.isNull(key)) d.opt(key) as? String else null
            if (value.isNullOrBlank()) receivedFields.remove(key)
            else { receivedFields.add(key); assign(value.trim()) }
        }

        fun soundId(key: String, assign: (String) -> Unit) {
            val value = if (d.has(key) && !d.isNull(key)) d.opt(key) as? String else null
            val normalized = value?.trim()?.lowercase()
            if (normalized.isNullOrBlank() || normalized !in AlarmSoundSync.SOUND_IDS) {
                receivedFields.remove(key)
            } else {
                receivedFields.add(key)
                assign(normalized)
            }
        }

        fun intField(key: String, min: Int, max: Int, assign: (Int) -> Unit) {
            assign(readNumber(key, min.toDouble(), max.toDouble(), integer = true)?.toInt() ?: 0)
        }

        fun longField(key: String, min: Long, max: Long, assign: (Long) -> Unit) {
            assign(readNumber(key, min.toDouble(), max.toDouble(), integer = true)?.toLong() ?: 0L)
        }

        // Te flagi pochodzą z /piec/status. Odczytujemy je przed walidacją
        // temperatur, bo sterownik dopuszcza dla symulacji szerszy zakres niż
        // dla fizycznych czujników (np. piec 0–160°C).
        SYM_POLA.keys.forEach { pole ->
            var active = false
            val flagKey = "symulacja_$pole"
            flag(flagKey) { active = it }
            val remaining = readNumber("${flagKey}_min", 0.0, 180.0, integer = true)?.toInt() ?: 0
            if (active) sym[pole] = SymEntry(remaining)
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
        flag("alarm_dym_wyciszony") { alarm_dym_wyciszony = it }
        flag("alarm_ogrz_wyciszony") { alarm_ogrz_wyciszony = it }
        longField("alarm_dym_wyciszenie_ms", -1L, 3_600_000L) { alarm_dym_wyciszenie_ms = it }
        // [v0.31.33 CLOUD-MARK] „ostatnia próbka w chmurze" — tylko odczyt, bez wpływu
        // na logikę. `longField` woła przypisanie także przy braku pola, dlatego
        // najpierw sprawdzamy, czy firmware w ogóle przysyła te pola (starsze nie).
        if (d.has("tel_cloud_seq") && !d.isNull("tel_cloud_seq")) {
            tel_known = true
            longField("tel_cloud_seq", 0L, 4_000_000_000L) { tel_cloud_seq = it }
            longField("tel_cloud_ts", 0L, 4_000_000_000L) { tel_cloud_ts = it }
            longField("tel_cloud_ago", -1L, 4_000_000_000L) { tel_cloud_ago = it }
            longField("tel_ring", 0L, 100_000L) { tel_ring = it }
            flag("tel_pending") { tel_pending = it }
        }
        longField("alarm_ogrz_wyciszenie_ms", -1L, 3_600_000L) { alarm_ogrz_wyciszenie_ms = it }
        soundId("alarm_sound_dym") { alarm_sound_dym = it }
        soundId("alarm_sound_ogrz") { alarm_sound_ogrz = it }
        soundId("alarm_sound_both") { alarm_sound_both = it }
        flag("pompa") { pompa = it }
        flag("mieszadlo") { mieszadlo = it }
        flag("rozpalanie") { rozpalanie = it }
        flag("rozpalanie_potwierdzone") { rozpalanie_potwierdzone = it }
        intField("rozpalanie_pozostalo_s", 0, 3600) { rozpalanie_pozostalo_s = it }
        flag("rozpalanie_serwa_aktywne") { rozpalanie_serwa_aktywne = it }
        flag("rozpalanie_serwa_dostepne") { rozpalanie_serwa_dostepne = it }
        flag("rozpalanie_serwa_blokada_dymu") { rozpalanie_serwa_blokada_dymu = it }
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
        intField("spool_rt", 0, 1) { spoolRt = it }
        intField("hist_fs", 0, 1) { histFs = it }
        intField("spool_pages", 0, 9_999_999) { spoolPages = it }
        intField("spool_spill", 0, 9_999_999) { spoolSpill = it }
        intField("spool_replay", 0, 9_999_999) { spoolReplay = it }
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

    companion object {
        val SYM_POLA = linkedMapOf(
            "zewn" to "t_zewn", "ogrz" to "t_ogrz", "bojler" to "t_bojler", "panel" to "t_panel",
            "pokoj" to "t_pokoj", "ogrz_powrot" to "t_powrot", "ogrz_trociny" to "t_trociny",
            "cisnienie" to "cisnienie", "wilgotnosc" to "wilgotnosc", "dym" to "dym"
        )
        // Zakresy przyjmowane przez parser polecenia symulacji w firmware.
        val SYM_BOUNDS = mapOf(
            "zewn" to (-30.0 to 50.0), "ogrz" to (0.0 to 160.0), "bojler" to (0.0 to 160.0),
            "panel" to (-30.0 to 160.0), "pokoj" to (-50.0 to 50.0), "ogrz_powrot" to (-30.0 to 120.0),
            "ogrz_trociny" to (-30.0 to 120.0), "cisnienie" to (970.0 to 1040.0),
            "wilgotnosc" to (0.0 to 100.0), "dym" to (0.0 to 4095.0)
        )
        private val ONLINE_FIELDS = setOf(
            "t_ogrz", "t_bojler", "t_zewn", "t_panel", "t_pokoj", "cisnienie", "wilgotnosc", "dym",
            "pompa", "mieszadlo", "tryb_serwa", "klapa", "syberka", "ip", "fw", "wifi_rssi"
        )
        val TRYB_NAZWA = mapOf(1 to "AUTO", 2 to "RĘCZNY", 3 to "BEZPIECZNY")
        fun pad2(v: Int) = if (v < 10) "0$v" else "$v"
    }
}
