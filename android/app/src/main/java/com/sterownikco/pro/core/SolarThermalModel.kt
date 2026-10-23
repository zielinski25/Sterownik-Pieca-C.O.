package com.sterownikco.pro.core

/*
 ══════════════════════════════════════════════════════════════════════════
   MODEL CIEPLNY KOLEKTORA SŁONECZNEGO — Trzemcha Górna
   ══════════════════════════════════════════════════════════════════════════

   Zastępuje:  prognozaGain = kWh/m2 * 4.2        (mnożenie GHI)
               zyskDziś    = suma(max(0, wzrost)) (pomija spadki)

   Ten plik NIE MA żadnych zależności od Androida ani od innych klas
   aplikacji. Można go skopiować do dowolnej wersji projektu.

   Podstawa fizyczna: równanie Hottel–Whillier–Bliss:
       Q = A·η₀·G − a₁·A·(T−Ta) − a₂·A·(T−Ta)²
   oraz bilans zbiornika:
       C·dT/dt = Q − UA·(T−Ta)

   G to GTI — nasłonecznienie W PŁASZCZYŹNIE KOLEKTORA, nie GHI.
   To jest cała różnica: przy pochmurnym dniu GTI jest mniejsze od GHI
   (×0,88), przy bezchmurnym prawie dwa razy większe (×1,83).

   Dokument: MODEL_SOLARNY_TRZEMCHA_2026-10-23.md
 */
object SolarThermalModel {

    /** Ciepło właściwe wody [J/(kg·K)]. */
    private const val C_WATER = 4186.0

    /** Gęstość wody [kg/m³] — przyjmowana stała, błąd <1% w zakresie 20–60 °C. */
    private const val RHO_WATER = 1000.0

    /** Krok całkowania [s]. 30 s wystarcza; przy 60 s wynik różni się o <0,1 K. */
    private const val STEP_SECONDS = 30.0

    private const val STEPS_PER_HOUR = 120  // 3600 / 30

    // ─────────────────────────────────────────────────────────────────────
    //  Parametry
    // ─────────────────────────────────────────────────────────────────────

    /**
     * Parametry instalacji. Wartości domyślne są KATALOGOWE — nie dopasowane
     * do konkretnego dachu. Wystarczają, żeby pomylić się o ~1,3 °C na dobę,
     * podczas gdy model mnożenia GHI mylił się o 4–7 °C.
     *
     * Patrz MODEL_SOLARNY_TRZEMCHA_2026-10-23.md §9.
     */
    data class Params(
        /**
         * Iloczyn A·η₀ [m²]. Apertura 0,95 m² (10 rur 58/1800)
         * × η₀ 0,714 (Apricus ETC-30, Solar Keymark).
         */
        val apertureOptical: Double = 0.678,

        /** Współczynnik strat liniowych kolektora a₁ [W/(m²·K)]. */
        val lossLinear: Double = 1.243,

        /** Współczynnik strat kwadratowych a₂ [W/(m²·K²)]. */
        val lossQuadratic: Double = 0.009,

        /** Straty postojowe zbiornika UA [W/K]. Literatura: 1,07–1,46. */
        val tankUa: Double = 1.4,

        /** Pojemność zbiornika [l]. */
        val tankVolumeL: Double = 100.0,

        /**
         * Różnica między wodą w kolektorze a czujnikiem [K].
         * Kolektor bierze wodę z dołu zbiornika, czujnik jest w połowie.
         * Przy parametrach katalogowych lepiej wychodzi 0 — patrz §9.3.
         */
        val collectorTempOffsetC: Double = 0.0,

        /** Kąt nachylenia kolektora [°]. Używany tylko do zapytania o GTI. */
        val tiltDeg: Int = 45,

        /** Azymut [°]. 0 = południe, −90 = wschód, +90 = zachód. */
        val azimuthDeg: Int = 0
    )

    /** Jedna godzina prognozy: nasłonecznienie w płaszczyźnie kolektora i temperatura powietrza. */
    data class Hour(val gtiWm2: Double, val airTempC: Double)

    /** Wynik symulacji doby. Wszystkie temperatury w °C, energia w kWh. */
    data class Result(
        /** Temperatura wody na koniec doby. */
        val endTempC: Double,
        /** Najwyższa temperatura w ciągu doby. */
        val peakTempC: Double,
        /** Godzina szczytu jako ułamek doby, np. 18.4 = 18:24. */
        val peakHour: Double,
        /** Najniższa temperatura w ciągu doby. */
        val minTempC: Double,
        /** Przyrost od startu do szczytu — to pokazywać jako „prognoza". */
        val deltaPeakC: Double,
        /** Zmiana od startu do końca doby — może być ujemna. */
        val deltaEndC: Double,
        /** Energia dostarczona przez kolektor [kWh]. Szacunek, nie pomiar. */
        val energyKwh: Double
    )

    // ─────────────────────────────────────────────────────────────────────
    //  Symulacja
    // ─────────────────────────────────────────────────────────────────────

    /**
     * Liczy dobowy przebieg temperatury wody.
     *
     * Odtwarza cztery dni pomiarów z błędem średnim 1,3 °C (parametry katalogowe,
     * zero dopasowania). Poprawnie przewiduje też WYCHŁADZANIE w pochmurny dzień —
     * czego obecny model w aplikacji nie potrafi wcale.
     *
     * @param startTempC temperatura wody na początku doby, z czujnika w zbiorniku
     * @param hours kolejne godziny; kolejność musi być rosnąca
     * @return null gdy brak danych albo temperatura startowa jest spoza zakresu
     */
    fun simulate(
        startTempC: Double,
        hours: List<Hour>,
        params: Params = Params()
    ): Result? {
        if (hours.isEmpty()) return null
        if (!startTempC.isFinite() || startTempC < -30.0 || startTempC > 150.0) return null

        val heatCapacity = params.tankVolumeL * RHO_WATER / 1000.0 * C_WATER  // J/K

        var temp = startTempC
        var peak = startTempC
        var peakHour = 0.0
        var min = startTempC
        var energyJoules = 0.0

        for ((index, hour) in hours.withIndex()) {
            val gti = if (hour.gtiWm2.isFinite()) maxOf(0.0, hour.gtiWm2) else 0.0
            val air = if (hour.airTempC.isFinite()) hour.airTempC else temp

            for (step in 0 until STEPS_PER_HOUR) {
                // Temperatura, którą „widzi" absorber.
                val collectorTemp = temp + params.collectorTempOffsetC
                val dT = collectorTemp - air

                // Moc użyteczna. Przycięta do zera: nocą kolektor nie grzeje
                // (i nie „chłodzi na minus" — chłodzenie liczy człon zbiornika).
                var q = params.apertureOptical * gti
                    - params.lossLinear * dT
                    - params.lossQuadratic * dT * dT
                if (q < 0.0) q = 0.0

                energyJoules += q * STEP_SECONDS

                // Bilans zbiornika: grzanie z kolektora minus strata postojowa.
                val lossWatts = params.tankUa * (temp - air)
                temp += (q - lossWatts) * STEP_SECONDS / heatCapacity

                val hourFraction = index + (step + 1).toDouble() / STEPS_PER_HOUR
                if (temp > peak) { peak = temp; peakHour = hourFraction }
                if (temp < min) min = temp
            }
        }

        return Result(
            endTempC = temp,
            peakTempC = peak,
            peakHour = peakHour,
            minTempC = min,
            deltaPeakC = peak - startTempC,
            deltaEndC = temp - startTempC,
            energyKwh = energyJoules / 3_600_000.0
        )
    }

    // ─────────────────────────────────────────────────────────────────────
    //  Szybki szacunek bez symulacji
    // ─────────────────────────────────────────────────────────────────────

    /**
     * Dobowy przyrost temperatury z jednego wzoru liniowego:
     *
     *     przyrost = 4,43 × GTI[kWh/m²] − 5,57
     *
     * Dopasowane do czterech dni pomiarów, błąd średni 1,3 °C.
     * Próg równowagi 1,26 kWh/m² — poniżej tej sumy zbiornik się wychładza.
     *
     * UWAGA: współczynnik 4,2, który jest teraz w aplikacji, jest niemal
     * poprawny — ale tylko dla GTI. Aplikacja podaje mu GHI. Stąd błąd.
     *
     * @param gtiKwhM2 dobowa suma nasłonecznienia w płaszczyźnie kolektora
     */
    fun quickDeltaC(gtiKwhM2: Double): Double {
        if (!gtiKwhM2.isFinite()) return 0.0
        return 4.43 * gtiKwhM2 - 5.57
    }

    /** Dobowa suma GTI z listy godzin [kWh/m²]. */
    fun dailyGtiKwhM2(hours: List<Hour>): Double =
        hours.sumOf { if (it.gtiWm2.isFinite()) maxOf(0.0, it.gtiWm2) else 0.0 } / 1000.0

    /** Dobowa suma z wartości godzinowych [kWh/m²]. */
    fun dailyKwhM2(perHourWm2: List<Double>): Double =
        perHourWm2.sumOf { if (it.isFinite()) maxOf(0.0, it) else 0.0 } / 1000.0

    // ─────────────────────────────────────────────────────────────────────
    //  Mostek do prognozy pogody
    // ─────────────────────────────────────────────────────────────────────

    /**
     * Przepisuje godziny z prognozy ([WHour]) na wejście modelu.
     *
     * Preferuje [WHour.tiltedIrradiance] (GTI — właściwe). Gdy API go nie zwróci,
     * schodzi na [WHour.shortwaveRadiation] (GHI — zaniża w słoneczny dzień
     * nawet o połowę, patrz §6 dokumentu). Gdy brak obu — traktuje jak noc.
     *
     * Wywołujący powinien odróżnić te przypadki, jeśli chce pokazać użytkownikowi
     * jakość prognozy. Ten mostek nigdy nie rzuca wyjątku.
     */
    fun fromWeather(hours: List<WHour>): List<Hour> =
        hours.map { w ->
            Hour(
                gtiWm2 = w.tiltedIrradiance ?: w.shortwaveRadiation ?: 0.0,
                airTempC = w.temp
            )
        }
}
