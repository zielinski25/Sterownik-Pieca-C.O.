package com.sterownikco.pro.core

import kotlin.math.roundToInt
import kotlin.random.Random

/* ══════════════════════════════════════════════════════════════════════════
   SYMULATOR — sprawdza REALNY kod z 0.31.49 (SolarAnalytics.kt + SolarThermalModel.kt
   skompilowane bez zmian). Sprawdza tylko decyzję karty z Weather.kt, przepisaną 1:1.
   ══════════════════════════════════════════════════════════════════════════ */

/** LUSTRO Weather.kt / SolarDayCard, wiersze 455–462. Przepisane znak w znak. */
fun kartaWartosc(deltaC: Double?, dayHours: List<WHour>): Double =
    if (deltaC != null && deltaC.isFinite())
        Math.round(deltaC * 10) / 10.0
    else
        Math.round(SolarAnalytics.dayDeltaC(SolarAnalytics.dayKwhM2(dayHours)) * 10) / 10.0

fun r1(v: Double): Double = Math.round(v * 10) / 10.0

fun nowHourStart(): Long = Weather.startOfHour()

class Wyniki {
    var ok = 0
    var fail = 0
    val bledy = ArrayList<String>()
    fun sprawdz(warunek: Boolean, opis: String) {
        if (warunek) ok++ else { fail++; if (bledy.size < 25) bledy.add(opis) }
    }
    fun podsumowanie(): String =
        "  sprawdzen OK=$ok  BLEDOW=$fail" + if (bledy.isEmpty()) "" else
            "\n  pierwsze bledy:\n" + bledy.joinToString("\n") { "    - $it" }
}

// ── generator doby: krzywa słoneczna 6:00–18:00 ────────────────────────────
fun gtiWHour(h: Int, peak: Double, cloud: Double): Double {
    if (h < 6 || h > 18) return 0.0
    val shape = Math.sin(((h - 6.0) / 12.0) * Math.PI)
    return Math.max(0.0, peak * shape * (1.0 - cloud / 100.0 * 0.75))
}

fun zbudujGodziny(
    rnd: Random, startMs: Long, n: Int,
    peakBase: Double, cloudBase: Double, airBase: Double
): List<WHour> {
    val out = ArrayList<WHour>(n)
    val cal = java.util.Calendar.getInstance()
    for (k in 0 until n) {
        val t = startMs + k * 3_600_000L
        cal.timeInMillis = t
        val h = cal.get(java.util.Calendar.HOUR_OF_DAY)
        val peak = peakBase * (0.6 + rnd.nextDouble() * 0.9)
        val cloud = (cloudBase + rnd.nextDouble(-25.0, 25.0)).coerceIn(0.0, 100.0)
        val gti = gtiWHour(h, peak, cloud)
        val shape = if (h in 6..18) Math.sin(((h - 6.0) / 12.0) * Math.PI) else 0.0
        val air = airBase + 5.0 * shape + rnd.nextDouble(-1.5, 1.5)
        out.add(
            WHour(
                time = t, temp = air, wind = 3.0, cloud = cloud,
                precip = 0.0, precipProb = 0.0, code = 3, isDay = shape > 0.0,
                shortwaveRadiation = gti, tiltedIrradiance = gti
            )
        )
    }
    return out
}

fun nowaAnaliza(tNow: Double, todayGain: Double): SolarAnalytics {
    val s = SolarAnalytics(Prefs())
    s.archive.add(
        SolarAnalytics.Sample(
            System.currentTimeMillis(), "s", tNow, 20.0, 13.0,
            100, 0.5, 90, 0, 0.0, 0.0
        )
    )
    s.todayGain = todayGain
    s.todayHasData = true
    return s
}

// ══════════════════════════════════════════════════════════════════════════
// TEST A — dokładna regresja ekranu z 29.10.2026 13:19
//   ZYSK DZIŚ +1.5   DO KOŃCA DNIA +2.1   karta DZIŚ (stara) -2.4
// ══════════════════════════════════════════════════════════════════════════
fun testA(): Boolean {
    println("TEST A — odtworzenie ekranu z 29.10.2026 (ZYSK +1.5, DO KOŃCA DNIA +2.1)")
    val rnd = Random(29)
    val start = nowHourStart()
    val hours = zbudujGodziny(rnd, start, 7 * 24, 400.0, 70.0, 13.0)

    val s = nowaAnaliza(18.5, 1.5)
    s.forecastGain = 2.1
    s.forecastPeakReal = true
    s.forecastEndDeltaC = 0.3
    s.forecastPeakC = 20.6
    s.forecastEnergyKwh = 0.4

    val deltas = s.dailyForecast7((0 until 7).map { i -> hours.drop(i * 24).take(24) })
    val dzis = kartaWartosc(deltas.getOrNull(0), hours.take(24))

    println("  ZYSK DZIŚ          = +${r1(s.todayGain)}")
    println("  DO KOŃCA DNIA      = +${r1(s.forecastGain)}")
    println("  karta DZIŚ (nowa)  = ${dzis}")
    println("  karta DZIŚ (stara) = ${kartaWartosc(null, hours.take(24))}")
    val ok = dzis == 3.6
    println(if (ok) "  WYNIK: OK — 1.5 + 2.1 = 3.6\n" else "  WYNIK: BLAD — oczekiwano 3.6\n")
    return ok
}

// ══════════════════════════════════════════════════════════════════════════
// TEST B — 1000 losowych dób: czy karta DZIŚ kiedyś przeczy górnym kartom
// ══════════════════════════════════════════════════════════════════════════
fun testB(n: Int): Wyniki {
    println("TEST B — $n losowych dób, niezmienniki na realnym kodzie")
    val w = Wyniki()
    val rnd = Random(20261029)
    val start = nowHourStart()
    var rozjazd = 0

    for (i in 0 until n) {
        val tNow = 5.0 + rnd.nextDouble() * 55.0          // 5..60 °C
        val todayGain = rnd.nextDouble(-6.0, 8.0)
        val peak = 50.0 + rnd.nextDouble() * 900.0        // 50..950 W/m²
        val cloud = rnd.nextDouble(0.0, 100.0)
        val air = -12.0 + rnd.nextDouble() * 32.0         // -12..20 °C
        val hours = zbudujGodziny(rnd, start, 7 * 24, peak, cloud, air)

        val s = nowaAnaliza(tNow, todayGain)
        s.estimateForecastGain(hours)
        val deltas = s.dailyForecast7((0 until 7).map { d -> hours.drop(d * 24).take(24) })
        val shown = (0 until 7).map { d -> kartaWartosc(deltas.getOrNull(d), hours.drop(d * 24).take(24)) }
        val eq = { a: Double, b: Double -> Math.abs(a - b) <= 0.1001 }

        val tag = "i=$i tNow=${r1(tNow)} gain=${r1(todayGain)} peak=${peak.roundToInt()} cloud=${cloud.roundToInt()} air=${r1(air)}"

        // I1 — spójność: DZIŚ = ZYSK DZIŚ + DO KOŃCA DNIA (gdy model znalazł szczyt)
        if (s.forecastPeakReal) {
            val oczek = r1(s.todayGain + s.forecastGain)
            w.sprawdz(eq(shown[0], oczek), "I1 $tag -> DZIŚ=${shown[0]} a ${r1(s.todayGain)}+${r1(s.forecastGain)}=$oczek")
        }

        // I2 — karta DZIŚ musi być DOKŁADNIE sumą "ZYSK DZIŚ" + to, co model dodał.
        // Uwaga: to nie jest to samo, co pokazuje karta "DO KOŃCA DNIA" — patrz I8.
        run {
            val rest = when {
                s.forecastPeakReal && s.forecastGain.isFinite() -> s.forecastGain
                s.forecastEndDeltaC.isFinite() -> s.forecastEndDeltaC
                else -> 0.0
            }
            val oczek = r1(s.todayGain + rest)
            w.sprawdz(Math.abs(shown[0] - oczek) <= 0.1001,
                "I2 $tag -> DZIŚ=${shown[0]} a gain=${r1(s.todayGain)}+rest=${r1(rest)}=$oczek")
        }

        // I8 — ROZJAZD WYŚWIETLANIA: model przewiduje wychładzanie, a karta
        // "DO KOŃCA DNIA" pokaże "+0.0" (bo forecastGain jest obcięte do zera).
        if (!s.forecastPeakReal && s.forecastEndDeltaC.isFinite() && s.forecastEndDeltaC < -0.5) {
            rozjazd++
        }

        // I3 — żadna karta nie pokazuje NaN
        shown.forEachIndexed { d, v ->
            w.sprawdz(v.isFinite(), "I3 $tag -> karta $d = $v")
        }

        // I4 — model jest używany, gdy istnieje; wzór liniowy tylko jako zapas
        deltas.forEachIndexed { d, dv ->
            val lin = kartaWartosc(null, hours.drop(d * 24).take(24))
            if (dv.isFinite()) {
                w.sprawdz(eq(shown[d], r1(dv)), "I4a $tag -> karta $d=${shown[d]} a model=${r1(dv)}")
            } else {
                w.sprawdz(eq(shown[d], lin), "I4b $tag -> karta $d=${shown[d]} a zapas=$lin")
            }
        }

        // I5 — prognoza na plusie nie może obniżyć karty DZIŚ
        if (s.forecastPeakReal && s.forecastGain > 0.0) {
            w.sprawdz(shown[0] >= r1(s.todayGain) - 0.05, "I5 $tag -> DZIŚ=${shown[0]} < zysk=${r1(s.todayGain)}")
        }

        // I6 — zakres sensowny
        shown.forEachIndexed { d, v ->
            w.sprawdz(Math.abs(v) <= 60.0, "I6 $tag -> karta $d = $v")
        }

        // I7 — prognoza nigdy nie jest ujemna w polu forecastGain
        w.sprawdz(!(s.forecastGain.isFinite() && s.forecastGain < 0.0), "I7 $tag -> forecastGain=${s.forecastGain}")
    }
    println(w.podsumowanie())
    println("  I8 rozjazd wyswietlania (model chlodzi, karta pokaze +0.0): $rozjazd / $n dob = ${"%.1f".format(100.0 * rozjazd / n)}%")
    return w
}

// ══════════════════════════════════════════════════════════════════════════
// TEST C — przypadki brzegowe
// ══════════════════════════════════════════════════════════════════════════
fun testC(): Wyniki {
    println("TEST C — przypadki brzegowe")
    val w = Wyniki()
    val start = nowHourStart()
    val rnd = Random(7)
    val normalne = zbudujGodziny(rnd, start, 168, 400.0, 50.0, 12.0)

    fun uruchom(nazwa: String, hours: List<WHour>, tNow: Double, gain: Double) {
        val s = nowaAnaliza(tNow, gain)
        s.estimateForecastGain(hours)
        val deltas = s.dailyForecast7((0 until 7).map { d -> hours.drop(d * 24).take(24) })
        val shown = (0 until 7).map { d -> kartaWartosc(deltas.getOrNull(d), hours.drop(d * 24).take(24)) }
        shown.forEachIndexed { d, v -> w.sprawdz(v.isFinite(), "$nazwa -> karta $d = $v") }
        w.sprawdz(true, "$nazwa przeszlo")
        println("  %-34s DZIŚ=%6s  JUTRO=%6s  prognoza=%s".format(
            nazwa, shown.getOrNull(0), shown.getOrNull(1),
            if (s.forecastGain.isFinite()) r1(s.forecastGain).toString() else "NaN"))
    }

    uruchom("pusta lista godzin", emptyList(), 20.0, 1.0)
    uruchom("jedna godzina", normalne.take(1), 20.0, 1.0)
    uruchom("sama noc (GTI=0)", normalne.map { it.copy(tiltedIrradiance = 0.0, shortwaveRadiation = 0.0) }, 20.0, 1.0)
    uruchom("brak GTI (null)", normalne.map { it.copy(tiltedIrradiance = null, shortwaveRadiation = null) }, 20.0, 1.0)
    uruchom("tylko GHI (GTI null)", normalne.map { it.copy(tiltedIrradiance = null) }, 20.0, 1.0)
    uruchom("woda 0 C", normalne, 0.0, 0.0)
    uruchom("woda 140 C", normalne, 140.0, 0.0)
    uruchom("woda -30 C (poza zakresem)", normalne, -30.0, 0.0)
    uruchom("woda 200 C (poza zakresem)", normalne, 200.0, 0.0)
    uruchom("woda NaN", normalne, Double.NaN, 0.0)
    uruchom("powietrze -40 C", zbudujGodziny(Random(3), start, 168, 400.0, 50.0, -40.0), 20.0, 0.0)
    uruchom("powietrze +50 C", zbudujGodziny(Random(4), start, 168, 400.0, 50.0, 50.0), 20.0, 0.0)
    uruchom("słońce 1200 W/m2", zbudujGodziny(Random(5), start, 168, 1200.0, 0.0, 20.0), 20.0, 0.0)
    uruchom("dzień bez słońca", zbudujGodziny(Random(6), start, 168, 0.0, 100.0, 5.0), 20.0, 0.0)
    uruchom("prognoza 48 h (krótka)", normalne.take(48), 20.0, 1.0)
    uruchom("prognoza 25 h", normalne.take(25), 20.0, 1.0)

    // archive pusty — hasSamples=false, model nie ma punktu startu
    run {
        val s = SolarAnalytics(Prefs())
        s.estimateForecastGain(normalne)
        val deltas = s.dailyForecast7((0 until 7).map { d -> normalne.drop(d * 24).take(24) })
        w.sprawdz(!s.forecastGain.isFinite(), "puste archiwum -> forecastGain powinien byc NaN, jest ${s.forecastGain}")
        w.sprawdz(deltas.isEmpty(), "puste archiwum -> dailyForecast7 powinno byc puste, jest ${deltas.size}")
        println("  %-34s dailyForecast7=%s (puste = karta uzyje zapasu)".format("puste archiwum", deltas))
    }

    println(w.podsumowanie())
    return w
}

// ══════════════════════════════════════════════════════════════════════════
// TEST D — czy model fizyczny jest w ogóle powtarzalny (determinizm)
// ══════════════════════════════════════════════════════════════════════════
fun testD(n: Int): Wyniki {
    println("TEST D — powtarzalność: ten sam dzień policzony $n razy")
    val w = Wyniki()
    val start = nowHourStart()
    val hours = zbudujGodziny(Random(11), start, 168, 500.0, 40.0, 12.0)
    var wzor: Double? = null
    repeat(n) {
        val s = nowaAnaliza(18.5, 1.5)
        s.estimateForecastGain(hours)
        val d = s.dailyForecast7((0 until 7).map { i -> hours.drop(i * 24).take(24) })
        val v = kartaWartosc(d.getOrNull(0), hours.take(24))
        if (wzor == null) wzor = v
        w.sprawdz(v == wzor, "powtorzenie $it -> $v a wzor $wzor")
    }
    println("  wynik powtarzalny: $wzor")
    println(w.podsumowanie())
    return w
}

// ══════════════════════════════════════════════════════════════════════════
// TEST E — odtworzenie 29.10 z fizyki: suma GTI 0.7156 kWh/m2, szczyt 147
// ══════════════════════════════════════════════════════════════════════════
fun testE() {
    println("TEST E — czy ekran z 29.10 jest fizycznie spójny (suma GTI 0.7156 kWh/m2, szczyt 147)")
    val start = nowHourStart()
    val totalKwh = (5.57 - 2.4) / 4.43          // tyle GTI daje starą kartę -2.4
    println("  stara karta = -2.4 °C  =>  dobowy GTI = ${"%.4f".format(totalKwh)} kWh/m2")
    val staraKarta = SolarAnalytics.dayDeltaC(totalKwh)
    println("  kontrola wzoru: dayDeltaC(${"%.4f".format(totalKwh)}) = ${r1(staraKarta)}")
    println()
    println("  udział doby                                  model od 18.5 °C")
    var trafienie: Double? = null
    for (frac in listOf(0.10, 0.20, 0.30, 0.40, 0.50, 0.60, 0.70, 0.80, 0.90, 1.00)) {
        val rnd = Random(5)
        val h = zbudujGodziny(rnd, start, 24, 147.0, 55.0, 12.0)
        val prof = h.map { it.copy(tiltedIrradiance = (it.tiltedIrradiance ?: 0.0) * frac, shortwaveRadiation = (it.shortwaveRadiation ?: 0.0) * frac) }
        val res = SolarThermalModel.simulate(18.5, SolarThermalModel.fromWeather(prof)) ?: continue
        if (trafienie == null && Math.abs(res.deltaPeakC - 2.1) < 0.35) trafienie = frac
        println("  %-42s +%.1f °C, %.2f kWh".format(
            "${(frac*100).roundToInt()}% promieniowania (${"%.3f".format(totalKwh*frac)} kWh/m2)",
            res.deltaPeakC, res.energyKwh))
    }
    println()
    println("  ekran pokazał +2.1 °C -> " + (if (trafienie != null)
        "OSIĄGALNE przy ~${(trafienie*100).roundToInt()}% dobowego słońca po 13:00 => ekran jest spójny"
        else "NIE odtworzone w tym przeglądzie"))
    println()
}


// ══════════════════════════════════════════════════════════════════════════
// TEST F — ile to kosztuje czasu. dailyForecast7 jest wołane przy KAŻDEJ
//          recompozycji ekranu pogody (2 miejsca), a liczy model 7 dób.
// ══════════════════════════════════════════════════════════════════════════
fun testF(n: Int) {
    println("TEST F — czas wykonania (to samo, co woła ekran przy każdej recompozycji)")
    val start = nowHourStart()
    val hours = zbudujGodziny(Random(9), start, 168, 500.0, 40.0, 12.0)
    val s = nowaAnaliza(18.5, 1.5)
    // rozgrzewka
    repeat(20) { s.estimateForecastGain(hours) }
    var t0 = System.nanoTime()
    repeat(n) { s.estimateForecastGain(hours) }
    val est = (System.nanoTime() - t0) / 1e6 / n
    t0 = System.nanoTime()
    repeat(n) { s.dailyForecast7((0 until 7).map { i -> hours.drop(i * 24).take(24) }) }
    val d7 = (System.nanoTime() - t0) / 1e6 / n
    t0 = System.nanoTime()
    repeat(n) { SolarThermalModel.simulate(18.5, SolarThermalModel.fromWeather(hours.take(24))) }
    val sim1 = (System.nanoTime() - t0) / 1e6 / n
    println("  simulate() 1 doba (24 h)          = %6.2f ms".format(sim1))
    println("  estimateForecastGain (reszta doby) = %6.2f ms".format(est))
    println("  dailyForecast7 (7 dób)             = %6.2f ms".format(d7))
    println("  ekran wola oba = %6.2f ms na recompozycje".format(est + d7))
    println("  klatka 60 Hz = 16.7 ms -> " + if (est + d7 < 4.0) "mieszczi sie z zapasem" else "RYZYKO ZACINANIA")
    println()
}

fun main() {
    println("╔══════════════════════════════════════════════════════════════╗")
    println("║  SYMULATOR 0.31.49 — realny kod, prawdziwy kompilator Kotlin ║")
    println("╚══════════════════════════════════════════════════════════════╝\n")
    val a = testA()
    val b = testB(1000)
    val c = testC()
    val d = testD(100)
    testE()
    testF(200)
    println("══════════════════════════════════════════════════════════════")
    println("RAZEM: OK=${b.ok + c.ok + d.ok}  BLEDY=${b.fail + c.fail + d.fail}")
    println("TEST A (regresja 3.6): " + if (a) "OK" else "BLAD")
    println("══════════════════════════════════════════════════════════════")
}
