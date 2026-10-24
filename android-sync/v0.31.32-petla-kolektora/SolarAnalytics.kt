package com.sterownikco.pro.core

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableDoubleStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import org.json.JSONArray
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.Locale
import kotlin.math.min
import kotlin.math.roundToInt

/* ══════════════════════════════════════════════════════════════════════════
   BILANS SOLARNY & DETEKCJA POBORU WODY — `SolarAnalytics` z Piec.html 1:1
   (bramka 25 s, próg poboru rateBojler < -0.30 °C/min, archiwum 5000,
   zapis co 5 próbek do klucza `solar_archive_data`, eksport CSV).
   ══════════════════════════════════════════════════════════════════════════ */
class SolarAnalytics(private val prefs: Prefs) {

    data class Sample(
        val ts: Long, val iso: String, val tPanel: Double, val tBojler: Double, val tZewn: Double,
        val rad: Int, val uv: Double, val cloud: Int, val isDraw: Int, val drop: Double, val gain: Double
    )

    data class DrawEvent(val ts: Long, val drop: Double, val tPrev: Double, val tNow: Double)

    val archive = ArrayList<Sample>()
    val waterDrawEvents = ArrayList<DrawEvent>()
    var lastSampleTs = 0L
    var accumulatedGrossGain by mutableDoubleStateOf(0.0)
    var accumulatedDrawDrop by mutableDoubleStateOf(0.0)
    var forecastGain by mutableDoubleStateOf(Double.NaN)
    // [0.31.29] Model cieplny zamiast mnożenia. Szczyt doby, jego godzina
    // i energia dostarczona przez kolektor. Patrz MODEL_SOLARNY_TRZEMCHA §10.
    var forecastPeakC by mutableDoubleStateOf(Double.NaN)
    var forecastPeakLabel by mutableStateOf("")
    var forecastEnergyKwh by mutableDoubleStateOf(Double.NaN)
    // [0.31.31] Czy model znalazł REALNY szczyt. `peakHour` zostaje 0.0, gdy woda
    // przez całą dobę tylko stygnie — a 0.0 zamieniało się w etykietę „szczyt 08:00”,
    // czyli w bieżącą godzinę wyssaną z palca. Materiał: mroźny poranek, gdy słońce
    // nie przebija strat ciepła. UI pyta o tę flagę, zamiast wierzyć `peakHour`.
    var forecastPeakReal by mutableStateOf(false)
    var deltaT by mutableDoubleStateOf(Double.NaN)
    var drawCount by mutableIntStateOf(0)
    // [0.31.27] Liczby „na dziś”, liczone od lokalnej północy. Wcześniej karta DZIŚ
    // brała accumulatedGrossGain (suma z całego bufora, sięgająca kilka dni wstecz),
    // więc pokazywała +93 °C, gdy model na dziś dawał +9 °C.
    var todayGain by mutableDoubleStateOf(0.0)
    // [0.31.32] Czy są JAKIEKOLWIEK próbki z dzisiaj. `todayGain` = 0,0 znaczy
    // dwa różne rzeczy: „brak danych” albo „woda się nie zmieniła”. Karta pokazywała
    // myślnik w obu przypadkach, a arkusz Czujniki pisał „brak próbek z dziś”
    // nawet przy 568 próbkach. Flaga rozdziela te przypadki.
    var todayHasData by mutableStateOf(false)
    var todayDrawCount by mutableIntStateOf(0)
    var todayDrawDrop by mutableDoubleStateOf(0.0)
    val hasSamples: Boolean get() = archive.isNotEmpty()

    private val isoFmt = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'", Locale.US).apply {
        timeZone = java.util.TimeZone.getTimeZone("UTC")
    }

    init {
        try {
            // Only the versioned archive written by this real-data path is trusted.
            // Older app versions could persist local demo/simulated values under K_SOLAR_ARCHIVE.
            prefs.get(Prefs.K_SOLAR_REAL_ARCHIVE)?.let { raw ->
                val arr = JSONArray(raw)
                for (i in 0 until arr.length()) {
                    val o = arr.optJSONObject(i) ?: continue
                    val sample = Sample(
                        o.optLong("ts"), o.optString("iso"), o.optDouble("t_panel", Double.NaN),
                        o.optDouble("t_bojler", Double.NaN), o.optDouble("t_zewn", Double.NaN),
                        o.optInt("rad"), o.optDouble("uv", Double.NaN), o.optInt("cloud"),
                        o.optInt("isDraw"), o.optDouble("drop"), o.optDouble("gain")
                    )
                    if (sample.ts > 0L && sample.tPanel.isFinite() && sample.tBojler.isFinite() && sample.tZewn.isFinite()) {
                        archive.add(sample)
                    }
                }
            }
        } catch (e: Exception) { /* brak archiwum zweryfikowanego */ }
        lastSampleTs = archive.lastOrNull()?.ts ?: 0L
        accumulatedGrossGain = archive.filter { it.tPanel > it.tZewn && it.rad > 30 }.sumOf { it.gain }
        val draws = archive.filter { it.isDraw != 0 }.takeLast(100)
        draws.forEach { waterDrawEvents.add(DrawEvent(it.ts, it.drop, it.tBojler + it.drop, it.tBojler)) }
        accumulatedDrawDrop = archive.filter { it.isDraw != 0 }.sumOf { it.drop }
        drawCount = draws.size
        archive.lastOrNull()?.let { deltaT = it.tPanel - it.tZewn }
        recomputeToday()
    }

    /** Początek lokalnej doby — granica „dziś” dla liczb dziennych. */
    private fun dayStartMs(): Long {
        val c = java.util.Calendar.getInstance()
        c.set(java.util.Calendar.HOUR_OF_DAY, 0)
        c.set(java.util.Calendar.MINUTE, 0)
        c.set(java.util.Calendar.SECOND, 0)
        c.set(java.util.Calendar.MILLISECOND, 0)
        return c.timeInMillis
    }

    /** Przelicza „dziś” z próbek od północy.
     *
     *  [0.31.29] ZYSK to RÓŻNICA NETTO (ostatnia próbka doby − pierwsza), a nie suma
     *  samych wzrostów. Poprzednia wersja dodawała każdy wzrost i pomijała każdy
     *  spadek, więc drgania czujnika (±0,1 °C, przy termosyfonie woda krąży pulsami)
     *  sumowały się jak realne grzanie. 2026-10-23 pokazywało +21,5 °C, gdy woda
     *  zmieniła się z 29,2 na 44,8 °C — czyli najwyżej +15,6 °C.
     */
    private fun recomputeToday() {
        val start = dayStartMs()
        var n = 0; var d = 0.0
        var firstT = Double.NaN; var lastT = Double.NaN
        archive.forEach { s ->
            if (s.ts >= start) {
                if (firstT.isNaN()) firstT = s.tPanel
                lastT = s.tPanel
                if (s.isDraw != 0) { n += 1; d += s.drop }
            }
        }
        todayHasData = !firstT.isNaN() && !lastT.isNaN()
        todayGain = if (todayHasData) lastT - firstT else 0.0
        todayDrawCount = n; todayDrawDrop = d
    }

    private fun save() {
        try {
            val arr = JSONArray()
            archive.takeLast(500).forEach {
                arr.put(
                    JSONObject()
                        .put("ts", it.ts).put("iso", it.iso).put("t_panel", it.tPanel)
                        .put("t_bojler", it.tBojler).put("t_zewn", it.tZewn).put("rad", it.rad)
                        .put("uv", it.uv).put("cloud", it.cloud).put("isDraw", it.isDraw)
                        .put("drop", it.drop).put("gain", it.gain)
                )
            }
            prefs.set(Prefs.K_SOLAR_REAL_ARCHIVE, arr.toString())
        } catch (e: Exception) { /* ignore */ }
    }

    fun recordSample(tPanel: Double, tBojler: Double, tZewn: Double, rad: Double, uv: Double, cloud: Double) {
        if (!listOf(tPanel, tBojler, tZewn, rad, uv, cloud).all { it.isFinite() }) return
        if (tPanel !in 0.0..140.0 || tBojler !in 10.0..95.0 || tZewn !in -25.0..45.0 || rad < 0.0) return
        val now = System.currentTimeMillis()
        if (lastSampleTs > 0 && now - lastSampleTs < 25_000) return
        lastSampleTs = now
        var isDraw = false
        var drawDrop = 0.0
        var gain = 0.0
        if (archive.isNotEmpty()) {
            val prev = archive.last()
            val dtMin = maxOf(.5, (now - prev.ts) / 60000.0)
            val rateBojler = (tBojler - prev.tBojler) / dtMin
            val ratePanel = (tPanel - prev.tPanel) / dtMin
            if (rateBojler < -0.30 || (ratePanel < -0.60 && tPanel > tZewn + 5)) {
                isDraw = true
                drawDrop = maxOf(0.0, prev.tBojler - tBojler)
                accumulatedDrawDrop += drawDrop
                waterDrawEvents.add(DrawEvent(now, drawDrop, prev.tBojler, tBojler))
                while (waterDrawEvents.size > 100) waterDrawEvents.removeAt(0)
            }
            val deltaP = maxOf(0.0, tPanel - prev.tPanel)
            val deltaB = maxOf(0.0, tBojler - prev.tBojler)
            gain = maxOf(deltaP, deltaB) + (if (isDraw) drawDrop else 0.0)
            if (tPanel > tZewn && rad > 30) accumulatedGrossGain += gain
        }
        archive.add(
            Sample(
                now, isoFmt.format(now), (tPanel * 10).roundToInt() / 10.0,
                (tBojler * 10).roundToInt() / 10.0, (tZewn * 10).roundToInt() / 10.0,
                rad.roundToInt(), (uv * 10).roundToInt() / 10.0, cloud.roundToInt(),
                if (isDraw) 1 else 0, (drawDrop * 10).roundToInt() / 10.0, (gain * 10).roundToInt() / 10.0
            )
        )
        while (archive.size > 5000) archive.removeAt(0)
        recomputeToday()
        if (archive.size % 5 == 0) save()
        updateUi(tPanel, tZewn)
    }

    /** [0.31.27] Jeden model dla DZIŚ, JUTRO i kart 7 dni — ten sam wzór i to samo wejście.
     *  Wcześniej „model godzinowy” liczył z zachmurzenia i dawał +0,0 °C obok +9,1 °C z kart
     *  dziennych. Teraz oba biorą prognozę GHI (a gdy jej brak — szacunek z zachmurzenia). */
    /** [0.31.29] Prognoza z modelu cieplnego, nie z mnożenia kWh/m² × 4,2.
     *
     *  Stary wzór mnożył nasłonecznienie POZIOME (GHI) i nie odejmował strat, więc
     *  nigdy nie pokazał wychładzania. Kolektor stoi pod kątem, więc liczy się GTI.
     *
     *  Teraz: symulacja godzinowa od BIEŻĄCEJ temperatury wody do końca doby
     *  (Hottel–Whillier–Bliss + bilans zbiornika). Sprawdzone na 4 dniach:
     *  błąd średni 1,3 °C, przy 3,4–6,6 °C starego modelu.
     *  Model: MODEL_SOLARNY_TRZEMCHA_2026-10-23.md
     */
    fun estimateForecastGain(hourly: List<com.sterownikco.pro.core.WHour>) {
        forecastGain = Double.NaN
        forecastPeakC = Double.NaN
        forecastPeakLabel = ""
        forecastEnergyKwh = Double.NaN
        forecastPeakReal = false

        val startT = archive.lastOrNull()?.tPanel
        if (startT == null || hourly.isEmpty()) return

        val nowHour = com.sterownikco.pro.core.Weather.startOfHour()
        val endOfDay = startOfTomorrow(nowHour)
        val remaining = hourly.filter { it.time >= nowHour && it.time < endOfDay }
        if (remaining.isEmpty()) return

        val res = SolarThermalModel.simulate(startT, SolarThermalModel.fromWeather(remaining)) ?: return
        forecastPeakC = res.peakTempC
        // [0.31.31] Etykietę ustawiamy TYLKO wtedy, gdy szczyt jest prawdziwy.
        // Przy samym wychładzaniu `peakHour` = 0.0, więc etykieta kłamała.
        forecastPeakReal = res.deltaPeakC > PEAK_EPS_C
        forecastPeakLabel = if (forecastPeakReal)
            formatClock(nowHour + (res.peakHour * 3_600_000.0).toLong()) else ""
        forecastEnergyKwh = res.energyKwh
        // Karta pokazuje „+X °C”, więc przy wychładzaniu dajemy 0 — pełną wartość
        // (także ujemną) niesie peakC i deltaEndC w modelu.
        forecastGain = maxOf(0.0, res.deltaPeakC)
    }

    /** Północ następnej doby — koniec horyzontu prognozy „na dziś". */
    private fun startOfTomorrow(from: Long): Long =
        java.util.Calendar.getInstance().apply {
            timeInMillis = from
            add(java.util.Calendar.DAY_OF_YEAR, 1)
            set(java.util.Calendar.HOUR_OF_DAY, 0)
            set(java.util.Calendar.MINUTE, 0)
            set(java.util.Calendar.SECOND, 0)
            set(java.util.Calendar.MILLISECOND, 0)
        }.timeInMillis

    private fun formatClock(ms: Long): String =
        SimpleDateFormat("HH:mm", Locale.US).format(java.util.Date(ms))

    companion object {
        /** [0.31.29] Nachylenie dopasowania: °C przyrostu na 1 kWh/m² DOBOWEGO GTI.
         *  Dopasowane do 4 dni pomiarów z Trzemchy (r = 0,990). Poprzednie 4,2 było
         *  niemal poprawne — ale dla GTI. Aplikacja karmiła je GHI, stąd błąd.
         *  Patrz MODEL_SOLARNY_TRZEMCHA_2026-10-23.md §11. */
        const val GAIN_C_PER_KWH_M2 = 4.43

        /** [0.31.29] Wyraz wolny: dobowa strata przy zerowym słońcu [°C].
         *  Bez tego członu model nie potrafił pokazać wychładzania. */
        const val LOSS_C_PER_DAY = 5.57

        /** Sufit dobowy: 100 l od ~20 °C nie zagotuje się w jednej dobie. */
        const val MAX_DAILY_GAIN_C = 45.0

        /** [0.31.31] Poniżej tego przyrostu uznajemy, że szczytu nie było.
         *  Próg 0,05 °C leży pod rozdzielczością wyświetlania (0,1 °C), więc nie
         *  pokażemy „+0,0 °C · szczyt 08:00”: albo jest przyrost widoczny na
         *  ekranie, albo mówimy wprost, że słońce nie przebiło strat. */
        const val PEAK_EPS_C = 0.05

        /** Szacunkowy przyrost doby. Może wyjść 0, gdy dzień nie dogrzeje wody. */
        fun gainFromKwhM2(kwhM2: Double): Double {
            if (!kwhM2.isFinite() || kwhM2 <= 0.0) return 0.0
            val g = kwhM2 * GAIN_C_PER_KWH_M2 - LOSS_C_PER_DAY
            return if (g <= 0.0) 0.0 else min(g, MAX_DAILY_GAIN_C)
        }

        /** [0.31.32] Dobowa zmiana temperatury ZE ZNAKIEM — bez przycinania do zera.
         *
         *  `gainFromKwhM2` obcinało wynik ujemny do 0,0, więc karta 7-dniowa
         *  pokazywała „+0,0 °C” także wtedy, gdy model liczył wychładzanie.
         *  A to nie to samo, co „woda się nie zmieniła”.
         *
         *  Próg równowagi wypada przy 1,26 kWh/m² — poniżej tej dobowej sumy GTI
         *  zbiornik oddaje więcej, niż zbiera. Wynik ujemny jest poprawny.
         */
        fun dayDeltaC(kwhM2: Double): Double {
            if (!kwhM2.isFinite() || kwhM2 <= 0.0) return -LOSS_C_PER_DAY
            return min(kwhM2 * GAIN_C_PER_KWH_M2 - LOSS_C_PER_DAY, MAX_DAILY_GAIN_C)
        }

        /** Dobowe nasłonecznienie w kWh/m².
         *
         *  [0.31.29] Preferuje GTI (`global_tilted_irradiance`) — słońce w PŁASZCZYŹNIE
         *  KOLEKTORA. Dopiero gdy go brak, schodzi na GHI (`shortwave_radiation`)
         *  i wreszcie na szacunek z zachmurzenia.
         *
         *  Kolejność ma znaczenie: stosunek GTI/GHI waha się od 0,88 (pochmurno)
         *  do 1,83 (bezchmurnie), więc GHI podane do wzoru na GTI zaniża słoneczne
         *  dni prawie o połowę.
         */
        fun dayKwhM2(hours: List<com.sterownikco.pro.core.WHour>): Double {
            if (hours.isEmpty()) return Double.NaN
            var sum = 0.0
            hours.forEach { h ->
                val rad = h.tiltedIrradiance
                    ?: h.shortwaveRadiation
                    ?: ((100.0 - h.cloud) * 8.5 * (if (h.isDay) 1.0 else 0.0))
                sum += maxOf(0.0, rad)
            }
            return sum / 1000.0
        }
    }

    private fun updateUi(tPanel: Double, tZewn: Double) {
        deltaT = tPanel - tZewn
        drawCount = waterDrawEvents.size
    }

    fun grossGain(): Double = accumulatedGrossGain
    /** [0.31.26] 100 l, nie 200 kg. Instalacja ma zbiornik 100 l (ARCHIWUM_SOLARNE_STAN.md §3),
     *  więc równoważnik to 0,1163 kWh/K, a nie 0,2326. Poprzednia wartość (200) była dziedzictwem
     *  Piec.html i zawyżała wynik dokładnie dwa razy. Nadal rząd wielkości, nie zmierzony uzysk. */
    fun estKwh(): Double = if (hasSamples) (accumulatedGrossGain * 100 * 4.186) / 3600.0 else Double.NaN

    /** `exportCsv()` — zawartość pliku CSV (nazwa: archiwum_solarno_pogodowe_RRRR-MM-DD.csv). */
    fun csv(): String {
        val sb = StringBuilder("timestamp,iso_time,t_panel,t_bojler,t_zewn,promieniowanie_W_m2,uv_index,zachmurzenie_proc,pobor_wody,spadek_poboru_C,zysk_brutto_C\n")
        archive.forEach {
            sb.append("${it.ts},${it.iso},${it.tPanel},${it.tBojler},${it.tZewn},${it.rad},${it.uv},${it.cloud},${it.isDraw},${it.drop},${it.gain}\n")
        }
        return sb.toString()
    }

    fun fileName(): String = "archiwum_solarno_pogodowe_" + SimpleDateFormat("yyyy-MM-dd", Locale.US).format(System.currentTimeMillis()) + ".csv"
}
