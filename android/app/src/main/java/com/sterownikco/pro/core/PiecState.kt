package com.sterownikco.pro.core

import androidx.compose.runtime.mutableIntStateOf
import org.json.JSONObject
import kotlin.math.roundToInt
import kotlin.math.sin
import kotlin.random.Random

/* ══════════════════════════════════════════════════════════════════════════
   LUSTRO FIREBASE / ESP — przeniesione 1:1 z obiektu `S` w Piec.html
   (sekcja „1. STAN STEROWNIKA”). Nazwy pól zostawione po angielsku/polsku jak
   w RTDB, żeby mapanie danych z `/piec/status` było jawne.
   ══════════════════════════════════════════════════════════════════════════ */

data class SymEntry(var wartosc: Double, var min: Int)
data class HistPoint(val o: Double, val b: Double, val z: Double)

class PiecState {
    // ── dane pomiarowe ──────────────────────────────────────────────────────
    var online = true
    var night = false
    var t_zewn = 11.8
    var t_ogrz = 58.4
    var t_ogrz_sr = 57.1
    var t_bojler = 48.6
    var t_panel = 36.2
    var t_pokoj = 21.4
    var t_powrot = 44.2
    var t_trociny = 31.0
    var cisnienie = 1009.0
    var wilgotnosc = 54.0
    var dym = 412.0
    var dym_wlaczony = true
    var dym_swiezy = true
    var dym_alarm = false
    var alarm_ogrzewanie = false
    var alarm_panel = false

    // ── urządzenia ──────────────────────────────────────────────────────────
    var pompa = true
    var mieszadlo = false
    var rozpalanie = false
    var tryb_serwa = 1
    var trybSerwa = 1
    var klapa = 81
    var syberka = 27
    var wybor = 3
    var pompa_override_min = 0
    var mieszadlo_override_min = 0
    var serwo_override_min = 0

    // ── zegar / identyfikacja ───────────────────────────────────────────────
    var rtc_ok = true
    var dzien = 1
    var miesiac = 1
    var rok = 2026
    var ip = "192.168.1.145"
    var firmware = ""
    var uptime = 0.0
    var appVersion = "v0.30.5"
    var wifi_ssid = ""
    var weatherCode = 0
    var wifi_rssi = -58
    var wifi_quality = 92
    var ping_ms = 22

    // ── ustawienia ESP ──────────────────────────────────────────────────────
    var czasOn = 10
    var czasOff = 30
    var tempOn = 60
    var tempOff = 50
    var antystopWlaczony = true
    var antystopDni = 10
    var mieszadloWlaczony = true
    var mieszadloCzasOn = 30
    var mieszadloCzasOff = 5
    var tempZadServo = 60
    var histerServo = 2
    var skokKlapy = 10
    var skokSyberka = 10
    var odchylTemp = 5
    var mnoznik = 2
    var progAlarmTemp = 80
    var progAlarmDym = 1000
    var dymCzasOn = 30
    var dymCzasOff = 120
    var dymCzasStabilizacji = 10
    var dymProgTemp = 40
    var dymTrybPracy = 0

    /** pola -> {wartosc, min} (symulacja) */
    val sym = HashMap<String, SymEntry>()

    /** kopia ostatniego stanu REAL (przed symulacjami) — odp. `const real` */
    val real = HashMap<String, Double>()

    /** bufor mini-wykresu w hero (ostatnie 60 próbek) */
    val hist = ArrayDeque<HistPoint>()

    /** Licznik wersji stanu — kompozycje go obserwują (odpowiednik render()). */
    val rev = mutableIntStateOf(0)
    fun bump() { rev.intValue = rev.intValue + 1 }

    init {
        syncReal()
        val now = java.util.Calendar.getInstance()
        dzien = now.get(java.util.Calendar.DAY_OF_MONTH)
        miesiac = now.get(java.util.Calendar.MONTH) + 1
        rok = now.get(java.util.Calendar.YEAR)
    }

    fun fmt1(v: Double): String = String.format(java.util.Locale.US, "%.1f", v)

    // ── symulacja pól ───────────────────────────────────────────────────────
    fun symAktywna(pole: String): Boolean = sym.containsKey(pole)
    fun valOf(pole: String, key: String): Double = sym[pole]?.wartosc ?: num(key)

    /** Wszystkie pola numeryczne `S` (potrzebne do `ustaw <pole> <wartość>`). */
    fun num(key: String): Double = when (key) {
        "t_zewn" -> t_zewn; "t_ogrz" -> t_ogrz; "t_ogrz_sr" -> t_ogrz_sr
        "t_bojler" -> t_bojler; "t_panel" -> t_panel; "t_pokoj" -> t_pokoj
        "t_powrot" -> t_powrot; "t_trociny" -> t_trociny
        "cisnienie" -> cisnienie; "wilgotnosc" -> wilgotnosc; "dym" -> dym
        "klapa" -> klapa.toDouble(); "syberka" -> syberka.toDouble()
        "tryb_serwa" -> tryb_serwa.toDouble(); "trybSerwa" -> trybSerwa.toDouble()
        "wybor" -> wybor.toDouble(); "pompa_override_min" -> pompa_override_min.toDouble()
        "mieszadlo_override_min" -> mieszadlo_override_min.toDouble()
        "serwo_override_min" -> serwo_override_min.toDouble()
        "czasOn" -> czasOn.toDouble(); "czasOff" -> czasOff.toDouble()
        "tempOn" -> tempOn.toDouble(); "tempOff" -> tempOff.toDouble()
        "antystopDni" -> antystopDni.toDouble()
        "mieszadloCzasOn" -> mieszadloCzasOn.toDouble(); "mieszadloCzasOff" -> mieszadloCzasOff.toDouble()
        "tempZadServo" -> tempZadServo.toDouble(); "histerServo" -> histerServo.toDouble()
        "skokKlapy" -> skokKlapy.toDouble(); "skokSyberka" -> skokSyberka.toDouble()
        "odchylTemp" -> odchylTemp.toDouble(); "mnoznik" -> mnoznik.toDouble()
        "progAlarmTemp" -> progAlarmTemp.toDouble(); "progAlarmDym" -> progAlarmDym.toDouble()
        "dymCzasOn" -> dymCzasOn.toDouble(); "dymCzasOff" -> dymCzasOff.toDouble()
        "dymCzasStabilizacji" -> dymCzasStabilizacji.toDouble()
        "dymProgTemp" -> dymProgTemp.toDouble(); "dymTrybPracy" -> dymTrybPracy.toDouble()
        "uptime" -> uptime; "wifi_rssi" -> wifi_rssi.toDouble()
        "wifi_quality" -> wifi_quality.toDouble(); "ping_ms" -> ping_ms.toDouble()
        "dzien" -> dzien.toDouble(); "miesiac" -> miesiac.toDouble(); "rok" -> rok.toDouble()
        else -> 0.0
    }

    fun setNum(key: String, v: Double) {
        when (key) {
            "t_zewn" -> t_zewn = v; "t_ogrz" -> t_ogrz = v; "t_ogrz_sr" -> t_ogrz_sr = v
            "t_bojler" -> t_bojler = v; "t_panel" -> t_panel = v; "t_pokoj" -> t_pokoj = v
            "t_powrot" -> t_powrot = v; "t_trociny" -> t_trociny = v
            "cisnienie" -> cisnienie = v; "wilgotnosc" -> wilgotnosc = v; "dym" -> dym = v
            "klapa" -> klapa = v.roundToInt(); "syberka" -> syberka = v.roundToInt()
            "tryb_serwa" -> { tryb_serwa = v.roundToInt(); trybSerwa = tryb_serwa }
            "trybSerwa" -> { trybSerwa = v.roundToInt(); tryb_serwa = trybSerwa; if (tryb_serwa != 2) serwo_override_min = 0; if (tryb_serwa == 3) { klapa = 0; syberka = 0 } }
            "wybor" -> wybor = v.roundToInt()
            "pompa_override_min" -> pompa_override_min = v.roundToInt()
            "mieszadlo_override_min" -> mieszadlo_override_min = v.roundToInt()
            "serwo_override_min" -> serwo_override_min = v.roundToInt()
            "czasOn" -> czasOn = v.roundToInt(); "czasOff" -> czasOff = v.roundToInt()
            "tempOn" -> tempOn = v.roundToInt(); "tempOff" -> tempOff = v.roundToInt()
            "antystopDni" -> antystopDni = v.roundToInt()
            "mieszadloCzasOn" -> mieszadloCzasOn = v.roundToInt(); "mieszadloCzasOff" -> mieszadloCzasOff = v.roundToInt()
            "tempZadServo" -> tempZadServo = v.roundToInt(); "histerServo" -> histerServo = v.roundToInt()
            "skokKlapy" -> skokKlapy = v.roundToInt(); "skokSyberka" -> skokSyberka = v.roundToInt()
            "odchylTemp" -> odchylTemp = v.roundToInt(); "mnoznik" -> mnoznik = v.roundToInt()
            "progAlarmTemp" -> progAlarmTemp = v.roundToInt(); "progAlarmDym" -> progAlarmDym = v.roundToInt()
            "dymCzasOn" -> dymCzasOn = v.roundToInt(); "dymCzasOff" -> dymCzasOff = v.roundToInt()
            "dymCzasStabilizacji" -> dymCzasStabilizacji = v.roundToInt()
            "dymProgTemp" -> dymProgTemp = v.roundToInt(); "dymTrybPracy" -> dymTrybPracy = v.roundToInt()
            "wifi_rssi" -> wifi_rssi = v.roundToInt()
            "wifi_quality" -> wifi_quality = v.roundToInt(); "ping_ms" -> ping_ms = v.roundToInt()
            "dzien" -> dzien = v.roundToInt(); "miesiac" -> miesiac = v.roundToInt(); "rok" -> rok = v.roundToInt()
        }
    }

    fun setBool(key: String, v: Boolean) {
        when (key) {
            "pompa" -> pompa = v
            "mieszadlo" -> mieszadlo = v
            "dym_wlaczony" -> dym_wlaczony = v
            "dym_swiezy" -> dym_swiezy = v
            "dym_alarm" -> dym_alarm = v
            "alarm_ogrzewanie" -> alarm_ogrzewanie = v
            "alarm_panel" -> alarm_panel = v
            "rozpalanie" -> rozpalanie = v
            "antystopWlaczony" -> antystopWlaczony = v
            "mieszadloWlaczony" -> mieszadloWlaczony = v
            "rtc_ok" -> rtc_ok = v
            "online" -> online = v
            "night" -> night = v
        }
    }

    fun bool(key: String): Boolean = when (key) {
        "pompa" -> pompa; "mieszadlo" -> mieszadlo; "dym_wlaczony" -> dym_wlaczony
        "dym_swiezy" -> dym_swiezy; "dym_alarm" -> dym_alarm
        "alarm_ogrzewanie" -> alarm_ogrzewanie; "alarm_panel" -> alarm_panel
        "rozpalanie" -> rozpalanie; "antystopWlaczony" -> antystopWlaczony
        "mieszadloWlaczony" -> mieszadloWlaczony; "rtc_ok" -> rtc_ok
        "online" -> online; "night" -> night; else -> false
    }

    /** czy pole `key` istnieje w stanie (używane przez `ustaw`) */
    fun hasField(key: String): Boolean = key in FIELDS

    fun syncReal() {
        real.clear()
        for (k in NUM_FIELDS) real[k] = num(k)
        for (k in BOOL_FIELDS) real[k] = (if (bool(k)) 1.0 else 0.0)
    }

    private fun restoreReal(key: String) {
        real[key]?.let { v ->
            if (key in BOOL_FIELDS) setBool(key, v != 0.0) else setNum(key, v)
        }
    }

    /** Ubranie: ArtState dla silnika ilustracji. */
    fun art(weatherCode: Int, isDay: Boolean): com.sterownikco.pro.ui.art.ArtState =
        com.sterownikco.pro.ui.art.ArtState(
            night = night, weatherCode = weatherCode, isDay = isDay,
            tZewn = valOf("zewn", "t_zewn").toFloat(), tOgrz = valOf("ogrz", "t_ogrz").toFloat(),
            tBojler = valOf("bojler", "t_bojler").toFloat(), tPanel = valOf("panel", "t_panel").toFloat(),
            tPokoj = valOf("pokoj", "t_pokoj").toFloat(),
            alarmOgrz = alarm_ogrzewanie, alarmPanel = alarm_panel,
            pompa = pompa, klapa = klapa.toFloat(), syberka = syberka.toFloat(),
            mieszadlo = mieszadlo, dymAlarm = dym_alarm,
            klapaAktywne = tryb_serwa == 1,
            cisnienie = cisnienie.toFloat(), wilgotnosc = wilgotnosc.toFloat(),
            godz = java.util.Calendar.getInstance().get(java.util.Calendar.HOUR_OF_DAY),
            min = java.util.Calendar.getInstance().get(java.util.Calendar.MINUTE)
        )

    /** `applyIncomingData(d)` z Piec.html — bez zmian kolejności i warunków. */
    fun applyIncomingData(d: JSONObject) {
        fun num(k: String) = if (d.has(k) && !d.isNull(k) && d.opt(k) is Number) d.getDouble(k) else null
        fun flag(k: String) = if (d.has(k) && !d.isNull(k) && (d.opt(k) is Boolean)) d.getBoolean(k) else null
        fun str(k: String) = if (d.has(k) && !d.isNull(k) && (d.opt(k) is String)) d.getString(k) else null

        num("t_ogrz")?.let { if (!symAktywna("ogrz")) t_ogrz = it }
        num("t_ogrz_sr")?.let { t_ogrz_sr = it }
        num("t_bojler")?.let { if (!symAktywna("bojler")) t_bojler = it }
        num("t_zewn")?.let { if (!symAktywna("zewn")) t_zewn = it }
        num("t_panel")?.let { if (!symAktywna("panel")) t_panel = it }
        num("t_pokoj")?.let { if (!symAktywna("pokoj")) t_pokoj = it }
        num("t_powrot")?.let { if (!symAktywna("ogrz_powrot")) t_powrot = it }
        num("t_trociny")?.let { if (!symAktywna("ogrz_trociny")) t_trociny = it }
        num("cisnienie")?.let { if (!symAktywna("cisnienie")) cisnienie = it }
        num("wilgotnosc")?.let { if (!symAktywna("wilgotnosc")) wilgotnosc = it }
        num("dym")?.let { if (!symAktywna("dym")) dym = it }
        flag("dym_wlaczony")?.let { dym_wlaczony = it }
        num("dym_alarm")?.let { if (!symAktywna("dym")) dym_alarm = it != 0.0 } ?: flag("dym_alarm")?.let { if (!symAktywna("dym")) dym_alarm = it }
        flag("dym_swiezy")?.let { dym_swiezy = it }
        num("alarm_ogrzewanie")?.let { if (!symAktywna("ogrz")) alarm_ogrzewanie = it != 0.0 } ?: flag("alarm_ogrzewanie")?.let { if (!symAktywna("ogrz")) alarm_ogrzewanie = it }
        num("alarm_panel")?.let { if (!symAktywna("panel")) alarm_panel = it != 0.0 } ?: flag("alarm_panel")?.let { if (!symAktywna("panel")) alarm_panel = it }
        flag("pompa")?.let { pompa = it }
        num("tryb_serwa")?.let { tryb_serwa = it.roundToInt(); trybSerwa = tryb_serwa }
        num("klapa")?.let { klapa = it.roundToInt() }
        num("syberka")?.let { syberka = it.roundToInt() }
        num("wybor")?.let { wybor = it.roundToInt() }
        num("czasOn")?.let { czasOn = it.roundToInt() }
        num("czasOff")?.let { czasOff = it.roundToInt() }
        num("tempOn")?.let { tempOn = it.roundToInt() }
        num("tempOff")?.let { tempOff = it.roundToInt() }
        num("progAlarmTemp")?.let { progAlarmTemp = it.roundToInt() }
        num("progAlarmDym")?.let { progAlarmDym = it.roundToInt() }
        flag("mieszadlo")?.let { mieszadlo = it }
        flag("mieszadloWlaczony")?.let { mieszadloWlaczony = it }
        num("mieszadloCzasOn")?.let { mieszadloCzasOn = it.roundToInt() }
        num("mieszadloCzasOff")?.let { mieszadloCzasOff = it.roundToInt() }
        str("ip")?.let { ip = it }
        str("fw")?.let { if (it.trim().isNotEmpty()) firmware = it.trim() }
        num("uptime")?.let { uptime = it }
        num("wifi_rssi")?.let { wifi_rssi = it.roundToInt() }
        str("wifi_ssid")?.let { wifi_ssid = it }
        if (num("wifi_rssi") == null) num("rssi")?.let { wifi_rssi = it.roundToInt() }
        online = true
        syncReal()
        pushHist()
    }

    fun pushHist() {
        hist.addLast(HistPoint(valOf("ogrz", "t_ogrz"), valOf("bojler", "t_bojler"), valOf("zewn", "t_zewn")))
        while (hist.size > 60) hist.removeFirst()
    }

    /** `liveTick()` — symulacja drgań czujników (tylko w trybie SYMULACJI). */
    var tick = 0
    var simSpeed = 1.0
    fun liveTick(demoMode: Boolean, fbFresh: Boolean) {
        tick += simSpeed.toInt().coerceAtLeast(1)
        // wygasanie symulacji i override'ów
        val toDel = ArrayList<String>()
        for ((p, e) in sym) {
            if (tick % 30 == 0) {
                e.min--
                if (e.min <= 0) toDel.add(p)
            }
        }
        for (p in toDel) {
            sym.remove(p)
            when (p) {
                "dym" -> dym_alarm = dym >= progAlarmDym
                "ogrz" -> alarm_ogrzewanie = t_ogrz >= progAlarmTemp
                "panel" -> alarm_panel = t_panel >= progAlarmTemp
            }
        }
        for (k in listOf("pompa", "mieszadlo", "serwo")) {
            val key = k + "_override_min"
            if (num(key) > 0 && tick % 30 == 0) setNum(key, num(key) - 1)
        }
        when {
            fbFresh -> pushHist()
            demoMode -> {
                if (!symAktywna("ogrz")) {
                    t_ogrz = (real["t_ogrz"] ?: t_ogrz) + sin(tick / 9.0) * 1.5 + (Random.nextDouble() - .5) * .2
                    t_ogrz_sr = t_ogrz - .8
                }
                if (!symAktywna("bojler")) t_bojler = (real["t_bojler"] ?: t_bojler) + sin(tick / 15.0) * .7
                if (!symAktywna("zewn")) t_zewn = (real["t_zewn"] ?: t_zewn) + sin(tick / 21.0) * .35
                if (!symAktywna("panel")) t_panel = (real["t_panel"] ?: t_panel) + sin(tick / 12.0) * 1.1
                if (!symAktywna("pokoj")) t_pokoj = (real["t_pokoj"] ?: t_pokoj) + sin(tick / 30.0) * .15
                if (!symAktywna("ogrz_powrot")) t_powrot = t_ogrz - 14 + sin(tick / 14.0) * .5
                if (!symAktywna("ogrz_trociny")) t_trociny = (real["t_trociny"] ?: t_trociny) + sin(tick / 25.0) * .3
                if (!symAktywna("wilgotnosc")) wilgotnosc = minOf(100.0, maxOf(0.0, (real["wilgotnosc"] ?: wilgotnosc) + sin(tick / 17.0) * 2))
                if (!symAktywna("dym")) {
                    dym = (real["dym"] ?: dym) + sin(tick / 5.0) * 20 + Random.nextDouble() * 8
                    if (dym_alarm) dym = progAlarmDym + 200 + Random.nextDouble() * 50
                }
                if (alarm_ogrzewanie && !symAktywna("ogrz")) t_ogrz = progAlarmTemp + 4 + Random.nextDouble()
                if (alarm_panel && !symAktywna("panel")) t_panel = progAlarmTemp + 5 + Random.nextDouble()
                dym_wlaczony = t_ogrz >= dymProgTemp || dym_alarm || symAktywna("dym")
                pushHist()
            }
        }
    }

    /** `applyCommand(cmd)` — zwraca true albo tekst błędu (jak w oryginale). */
    fun applyCommand(cmd: String): Any {
        val p = cmd.trim().split(Regex("\\s+"))
        when (p[0]) {
            "pompa_wl" -> { pompa = true; pompa_override_min = 30; return true }
            "pompa_wyl" -> { pompa = false; pompa_override_min = 30; return true }
            "pompa_auto" -> {
                pompa_override_min = 0
                pompa = wybor != 2 || t_ogrz >= tempOn
                return true
            }
            "mieszadlo_wl" -> { mieszadlo = true; mieszadlo_override_min = 30; return true }
            "mieszadlo_wyl" -> { mieszadlo = false; mixin0(); return true }
            "mieszadlo_auto" -> { mieszadlo_override_min = 0; mieszadlo = false; return true }
            "klapa" -> {
                if (tryb_serwa != 2) return "serwo nie jest w trybie ręcznym"
                klapa = ((p.getOrNull(1)?.toDoubleOrNull() ?: 0.0) * 1.8).roundToInt()
                serwo_override_min = 60
                return true
            }
            "syberek" -> {
                if (tryb_serwa != 2) return "serwo nie jest w trybie ręcznym"
                syberka = ((p.getOrNull(1)?.toDoubleOrNull() ?: 0.0) * .9).roundToInt()
                serwo_override_min = 60
                return true
            }
            "symuluj" -> {
                val pole = p.getOrNull(1) ?: return "nieznane pole " + p.getOrNull(1)
                val mapped = SYM_POLA[pole] ?: return "nieznane pole $pole"
                val wartosc = p.getOrNull(2)?.toDoubleOrNull() ?: return "podaj wartość"
                val minuty = (p.getOrNull(3)?.toIntOrNull() ?: 60)
                sym[pole] = SymEntry(wartosc, minuty)
                if (mapped in BOOL_FIELDS) setBool(mapped, wartosc != 0.0) else setNum(mapped, wartosc)
                real[mapped] = wartosc
                if (pole == "dym") dym_alarm = wartosc >= progAlarmDym
                if (pole == "ogrz") alarm_ogrzewanie = wartosc >= progAlarmTemp
                if (pole == "panel") alarm_panel = wartosc >= progAlarmTemp
                return true
            }
            "symuluj_stop" -> {
                val pole = p.getOrNull(1) ?: return "nieznane pole"
                val mapped = SYM_POLA[pole]
                sym.remove(pole)
                if (mapped != null) restoreReal(mapped)
                when (pole) {
                    "dym" -> dym_alarm = dym >= progAlarmDym
                    "ogrz" -> alarm_ogrzewanie = t_ogrz >= progAlarmTemp
                    "panel" -> alarm_panel = t_panel >= progAlarmTemp
                }
                return true
            }
            "ustaw" -> {
                val k = p.getOrNull(1) ?: return "nieznane pole"
                val v = p.getOrNull(2)?.toDoubleOrNull() ?: return "nieznana wartość"
                if (!hasField(k)) return "nieznane pole $k"
                if (k in BOOL_FIELDS) setBool(k, v == 1.0) else setNum(k, v)
                return true
            }
            "status" -> return true
            "reboot" -> return true
            else -> return "nieznana komenda"
        }
    }

    private fun mixin0() { mieszadlo_override_min = 30 }

    companion object {
        val SYM_POLA = linkedMapOf(
            "zewn" to "t_zewn", "ogrz" to "t_ogrz", "bojler" to "t_bojler", "panel" to "t_panel",
            "pokoj" to "t_pokoj", "ogrz_powrot" to "t_powrot", "ogrz_trociny" to "t_trociny",
            "cisnienie" to "cisnienie", "wilgotnosc" to "wilgotnosc", "dym" to "dym"
        )
        val BOOL_FIELDS = setOf(
            "online", "night", "dym_wlaczony", "dym_swiezy", "dym_alarm", "alarm_ogrzewanie",
            "alarm_panel", "pompa", "mieszadlo", "rozpalanie", "antystopWlaczony",
            "mieszadloWlaczony", "rtc_ok"
        )
        val NUM_FIELDS = listOf(
            "t_zewn", "t_ogrz", "t_ogrz_sr", "t_bojler", "t_panel", "t_pokoj", "t_powrot", "t_trociny",
            "cisnienie", "wilgotnosc", "dym", "klapa", "syberka", "tryb_serwa", "trybSerwa", "wybor",
            "pompa_override_min", "mieszadlo_override_min", "serwo_override_min", "czasOn", "czasOff",
            "tempOn", "tempOff", "antystopDni", "mieszadloCzasOn", "mieszadloCzasOff", "tempZadServo",
            "histerServo", "skokKlapy", "skokSyberka", "odchylTemp", "mnoznik", "progAlarmTemp",
            "progAlarmDym", "dymCzasOn", "dymCzasOff", "dymCzasStabilizacji", "dymProgTemp",
            "dymTrybPracy", "uptime", "wifi_rssi", "wifi_quality", "ping_ms", "dzien", "miesiac", "rok"
        )
        val FIELDS = (NUM_FIELDS + BOOL_FIELDS).toSet()
        val TRYB_NAZWA = mapOf(1 to "AUTO", 2 to "RĘCZNY", 3 to "BEZPIECZNY")
        fun pad2(v: Int) = if (v < 10) "0$v" else "$v"
        fun sin(x: Double): Double = kotlin.math.sin(x)
    }
}
