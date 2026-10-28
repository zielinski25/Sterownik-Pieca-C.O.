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
   (bramka 25 s, archiwum 5000, zapis co 5 próbek do klucza
   `solar_archive_data`, eksport CSV).
   [0.31.37] Wszystko liczone z `tPanel` = woda w zbiorniku słonecznym na dachu
   (BUS_3), nie z bojlera pieca (tBojler/BUS_2). Pobranie wody = spadek
   podtrzymany ≥ 0,5 °C, skok > 12 °C odrzucany jako usterka sondy.
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
        // [0.31.36] Stare flagi pobrań z wersji do 0.31.35 siedzą w archiwum w pamięci
        // telefonu (np. 24 fałszywe pobrania z 28.10). Przeliczamy je od zera nową regułą,
        // zamiast pokazywać dalej to, co zapisała poprzednia wersja.
        recomputeDraws()
        accumulatedGrossGain = archive.filter { it.tPanel > it.tZewn && it.rad > 30 }.sumOf { it.gain }
        val draws = archive.filter { it.isDraw != 0 }.takeLast(100)
        draws.forEach { waterDrawEvents.add(DrawEvent(it.ts, it.drop, it.tPanel + it.drop, it.tPanel)) }
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
                // [0.31.37] tPanel to WODA w zbiorniku słonecznym na dachu, nie płytka
                // kolektora. Potwierdza to ARCHIWUM_SOLARNE_STAN.md §2–§3: czujnik
                // `czujnik_temp_Panel_Plus` (BUS_3, kanał TELEMETRY_CH_PANEL) leży
                // w wodzie, z boku, w połowie wysokości zbiornika 100 l.
                // `tBojler` (BUS_2) to BOJLER PIECA w kotłowni — inna instalacja.
                // 0.31.36 przez pomyłkę liczyła z bojlera pieca i pokazywała −17,5 °C
                // (woda w kotłowni), gdy zbiornik słoneczny zmienił się o −0,2 °C.
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

    /**
     * [0.31.35 SOLAR-HISTORY] Wciąga DZISIEJSZE próbki z historii chmury do archiwum.
     *
     * Powód: `recordSample()` zbiera próbki tylko wtedy, gdy aplikacja jest otwarta
     * na wierzchu (cykl pollingu żyje w `onResume`/`onPause`). Kto zagląda do pieca
     * wieczorem, ten przez cały dzień nie ma ANI JEDNEJ próbki — karta pokazywała
     * „brak próbek z dziś", mimo że chmura ma temperaturę zbiornika co minutę.
     * Zmierzone 2026-10-28: 27.10 zbiornik +6,0 °C (panel do 31,3 °C), 28.10 −15,8 °C.
     *
     * Metoda liczenia jest TA SAMA co przy próbkach na żywo: zysk = ostatnia minus
     * pierwsza próbka doby; pobranie wody = spadek szybszy niż 0,3 °C/min.
     * Zwraca liczbę próbek, których wcześniej nie było (0 = nic nowego).
     */
    fun mergeCloudToday(points: List<TelemPoint>): Int {
        val start = dayStartMs()
        if (points.isEmpty()) return 0
        val today = java.util.TreeMap<Long, Sample>()
        archive.forEach { if (it.ts >= start) today[it.ts] = it }
        var added = 0
        points.forEach { p ->
            if (p.ts < start || today.containsKey(p.ts)) return@forEach
            val b = p.t_bojler; val pn = p.t_panel; val z = p.t_zewn
            if (b == null || pn == null || z == null) return@forEach
            if (!b.isFinite() || !pn.isFinite() || !z.isFinite()) return@forEach
            if (pn !in 0.0..140.0 || b !in 10.0..95.0 || z !in -25.0..45.0) return@forEach
            today[p.ts] = Sample(
                p.ts, isoFmt.format(p.ts), round1(pn), round1(b), round1(z),
                0, Double.NaN, 0, 0, 0.0, 0.0
            )
            added++
        }
        if (added == 0) return 0
        // Zysk przeliczamy po scaleniu, w kolejności czasu. Pobrania liczy
        // `recomputeDraws()` (wołane z `rebuildDrawEvents()` niżej) — jedną regułą
        // dla próbek lokalnych i chmurowych, z 5-minutowym potwierdzeniem spadku.
        val rebuilt = ArrayList<Sample>(today.size)
        var prev: Sample? = null
        today.values.forEach { cur ->
            val pr = prev
            var gain = 0.0
            if (pr != null) {
                gain = maxOf(maxOf(0.0, cur.tPanel - pr.tPanel), maxOf(0.0, cur.tBojler - pr.tBojler))
            }
            rebuilt.add(cur.copy(gain = round1(gain)))
            prev = cur
        }
        archive.removeAll { it.ts >= start }
        archive.addAll(rebuilt)
        archive.sortBy { it.ts }
        while (archive.size > 5000) archive.removeAt(0)
        rebuildDrawEvents()
        recomputeToday()
        save()
        return added
    }

    private fun round1(v: Double): Double = (v * 10).roundToInt() / 10.0

    /** [0.31.36] Pobranie wody wykrywane z temperatury ZBIORNIKA, nie z szumu czujnika.
     *
     *  Liczone z `tPanel` — czyli z WODY w zbiorniku słonecznym na dachu
     *  (ARCHIWUM_SOLARNE_STAN.md §2–§3), a nie z bojlera pieca w kotłowni.
     *
     *  Stara reguła (`rateBojler < -0.30`) łapała każde zejście o 0,3 °C między dwiema
     *  próbkami minutowymi. Czujnik ma rozdzielczość 0,1 °C, a zbiornik stygnie przez pętlę
     *  termosyfonu — więc takie zejście zdarza się co kilkanaście minut i wychodziło
     *  „Pobrania ciepłej wody: 24 razy · −7,2 °C” (2026-10-28), choć nikt wody nie brał.
     *  Dodatkowo 31,5 − 31,8 w typie Double to −0,3000000000000007, czyli minimalnie
     *  więcej niż próg 0,30 — porównanie łapało granicę przez przypadek.
     *
     *  Teraz pobranie musi być SPADKIEM PODTRZYMANYM:
     *   1. woda schodzi o co najmniej 0,5 °C od maksimum z ostatnich 5 minut,
     *   2. i zostaje niżej przez następne 5 minut.
     *  Drgnięcie czujnika wraca w ciągu minuty, więc punkt 2 je odrzuca.
     *  Epizody bliższe niż 20 minut to jedno pobranie (kąpiel trwa). Spadek większy
     *  niż 12 °C jest ODRZUCANY jako usterka sondy — 100 l nie traci 25 °C od kranu.
     *
     *  Sprawdzone na eksporcie z chmury (28 dni): 26 pobrań w 17 dniach, po 1–2 na dobę,
     *  każde 0,5–5 °C, głównie po południu. 2026-10-28 — zero (nikt nie brał wody).
     */
    private fun recomputeDraws() {
        val n = archive.size
        if (n == 0) return
        // 1) kandydaci: indeks początku spadku, maksimum sprzed spadku, minimum w epizodzie
        val candIdx = ArrayList<Int>()
        val candBase = ArrayList<Double>()
        val candLow = ArrayList<Double>()
        var i = 0
        while (i < n) {
            val ts = archive[i].ts
            var base = archive[i].tPanel
            var j = i
            while (j > 0 && ts - archive[j - 1].ts <= DRAW_WINDOW_MS) {
                j--
                if (archive[j].tPanel > base) base = archive[j].tPanel
            }
            if (base - archive[i].tPanel >= DRAW_STEP_C) {
                var low = archive[i].tPanel
                var k = i + 1
                while (k < n && archive[k].ts - ts <= DRAW_HOLD_MS) {
                    if (archive[k].tPanel < low) low = archive[k].tPanel
                    k++
                }
                // Punkt 2: spadek musi się utrzymać, a nie być jedną próbką szumu.
                if (low <= base - DRAW_STEP_C) {
                    candIdx.add(i); candBase.add(base); candLow.add(low)
                }
            }
            i++
        }
        // 2) scalanie epizodów bliższych niż 20 minut w jedno pobranie
        val flag = IntArray(n)
        val drop = DoubleArray(n)
        var c = 0
        while (c < candIdx.size) {
            val startIdx = candIdx[c]
            var base = candBase[c]
            var low = candLow[c]
            var c2 = c + 1
            while (c2 < candIdx.size &&
                archive[candIdx[c2]].ts - archive[candIdx[c2 - 1]].ts <= DRAW_MERGE_MS
            ) {
                if (candBase[c2] > base) base = candBase[c2]
                if (candLow[c2] < low) low = candLow[c2]
                c2++
            }
            val span = maxOf(0.0, base - low)
            if (span <= DRAW_FAULT_C) {
                flag[startIdx] = 1
                drop[startIdx] = round1(span)
            }
            // span > DRAW_FAULT_C: sonda się odczepiła albo wróciła po restarcie —
            // nie udajemy, że ktoś wylał 20 °C wody.
            c = c2
        }
        // 3) przepisanie flag do archiwum (Sample jest niemutowalny)
        for (idx in 0 until n) {
            val s = archive[idx]
            if (s.isDraw != flag[idx] || s.drop != drop[idx]) {
                archive[idx] = s.copy(isDraw = flag[idx], drop = drop[idx])
            }
        }
    }

    /** Lista pobrań wody z archiwum — wspólna dla startu i dla scalania z chmurą. */
    private fun rebuildDrawEvents() {
        recomputeDraws()
        waterDrawEvents.clear()
        val draws = archive.filter { it.isDraw != 0 }.takeLast(100)
        draws.forEach { waterDrawEvents.add(DrawEvent(it.ts, it.drop, it.tPanel + it.drop, it.tPanel)) }
        drawCount = draws.size
        accumulatedDrawDrop = archive.filter { it.isDraw != 0 }.sumOf { it.drop }
        accumulatedGrossGain = archive.filter { it.tPanel > it.tZewn && it.rad > 30 }.sumOf { it.gain }
    }

    fun recordSample(tPanel: Double, tBojler: Double, tZewn: Double, rad: Double, uv: Double, cloud: Double) {
        if (!listOf(tPanel, tBojler, tZewn, rad, uv, cloud).all { it.isFinite() }) return
        if (tPanel !in 0.0..140.0 || tBojler !in 10.0..95.0 || tZewn !in -25.0..45.0 || rad < 0.0) return
        val now = System.currentTimeMillis()
        if (lastSampleTs > 0 && now - lastSampleTs < 25_000) return
        lastSampleTs = now
        var gain = 0.0
        if (archive.isNotEmpty()) {
            val prev = archive.last()
            val deltaP = maxOf(0.0, tPanel - prev.tPanel)
            val deltaB = maxOf(0.0, tBojler - prev.tBojler)
            gain = maxOf(deltaP, deltaB)
            if (tPanel > tZewn && rad > 30) accumulatedGrossGain += gain
        }
        archive.add(
            Sample(
                now, isoFmt.format(now), (tPanel * 10).roundToInt() / 10.0,
                (tBojler * 10).roundToInt() / 10.0, (tZewn * 10).roundToInt() / 10.0,
                rad.roundToInt(), (uv * 10).roundToInt() / 10.0, cloud.roundToInt(),
                0, 0.0, (gain * 10).roundToInt() / 10.0
            )
        )
        while (archive.size > 5000) archive.removeAt(0)
        // [0.31.36] Pobrania liczymy teraz wstecznie z całego archiwum (recomputeDraws),
        // bo potrzebne jest 5 minut po spadku na potwierdzenie, że to nie szum.
        // Skutek: pobranie pojawia się na karcie dopiero po tych 5 minutach — celowo,
        // bo próg 0,3 °C/min łapał zwykłe stygnięcie zbiornika.
        recomputeDraws()
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
        /** [0.31.37] Progi wykrywania pobrania ciepłej wody — patrz `recomputeDraws()`.
         *  Woda musi zejść o 0,5 °C od maksimum z 5 minut i utrzymać się niżej 5 minut.
         *  Rozdzielczość czujnika to 0,1 °C, więc 0,5 daje czterokrotny zapas nad szumem,
         *  a prawdziwa kąpiel schodzi o kilka °C. */
        private const val DRAW_STEP_C = 0.5
        private const val DRAW_WINDOW_MS = 5 * 60_000L
        private const val DRAW_HOLD_MS = 5 * 60_000L
        private const val DRAW_MERGE_MS = 20 * 60_000L
        private const val DRAW_FAULT_C = 12.0

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
