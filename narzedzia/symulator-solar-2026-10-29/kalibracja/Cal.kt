package com.sterownikco.pro.core

fun r1c(v: Double) = Math.round(v * 10) / 10.0

fun noc(t0: Double, air: Double, h: Double, p: SolarThermalModel.Params): Double {
    val n = (0 until h.toInt()).map { SolarThermalModel.Hour(0.0, air) }
    val r = SolarThermalModel.simulate(t0, n, p) ?: return Double.NaN
    return (r.endTempC - t0) / h
}

fun dzien(t0: Double, air: Double, gtiPeak: Double, p: SolarThermalModel.Params): Double {
    // 12 godzin slonca: sinus 6-18
    val hours = (6..18).map { h ->
        val shape = Math.sin(((h - 6.0) / 12.0) * Math.PI)
        SolarThermalModel.Hour(gtiPeak * shape, air + 5.0 * shape)
    }
    val r = SolarThermalModel.simulate(t0, hours, p) ?: return Double.NaN
    return r.deltaPeakC
}

fun main() {
    val base = SolarThermalModel.Params()
    println("╔══════════════════════════════════════════════════════════════════════╗")
    println("║  KALIBRACJA: co trzeba zmienic, zeby noc chlodzila jak w rzeczywistosci ║")
    println("╚══════════════════════════════════════════════════════════════════════╝")
    println()
    println("POMIARY Z ARCHIWUM (z MODEL_SOLARNY §18.2, policzone z danych centrali):")
    println("  21.10 wieczor (po pochmurnym)  4.0 K / 8.8 h  = 0.45 °C/h   UA_eff 2.3 W/K")
    println("  22.10 noc    (po pochmurnym)   3.4 K / 8.3 h  = 0.41 °C/h   UA_eff 2.3 W/K")
    println("  23.10 wieczor (po SLONECZNYM) 11.4 K / 8.9 h  = 1.28 °C/h   UA_eff 4.6 W/K")
    println("  24.10 noc    (po SLONECZNYM)   8.8 K / 9.3 h  = 0.95 °C/h   UA_eff 4.3 W/K")
    println("  29.10 14:30-17:20 (dzis)       1.6 K / 2.8 h  = 0.57 °C/h")
    println()
    println("MODEL TERAZ (tankUa 1.4, reverseFlow 0.004, loopUa 1.0):")
    for ((t, a) in listOf(19.5 to 11.0, 27.0 to 8.0, 41.0 to 8.0, 35.0 to 8.0))
        println("  woda %.1f / powietrze %.1f  ->  %.2f °C/h".format(t, a, noc(t, a, 6.0, base)))
    println()
    println("══════════════════════════════════════════════════════════════════════")
    println("SWEEP: zmieniamy loopUaWk (straty petli: manifold + rury na dachu)")
    println("══════════════════════════════════════════════════════════════════════")
    println("loopUa  | noc 19.5/11  noc 27/8  noc 41/8 | dzien 500W/m2  dzien 900W/m2")
    println("--------+----------------------------------+----------------------------")
    var best: Pair<Double, Double>? = null
    for (lua in listOf(1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 5.0)) {
        val p = base.copy(loopUaWk = lua)
        val n1 = noc(19.5, 11.0, 6.0, p); val n2 = noc(27.0, 8.0, 6.0, p); val n3 = noc(41.0, 8.0, 6.0, p)
        val d1 = dzien(20.0, 12.0, 500.0, p); val d2 = dzien(20.0, 12.0, 900.0, p)
        println("%6.1f  | %11.2f  %7.2f  %7.2f | %13.1f  %12.1f".format(lua, n1, n2, n3, d1, d2))
        // kryterium: noc 41 C ma dac ok. -1.1 do -1.3 °C/h (pomiar 23.10: -1.28)
        if (best == null && n3 <= -1.05) best = lua to n3
    }
    println()
    println("Pierwszy wariant, ktory daje noc przy 41 °C <= -1.05 °C/h: loopUa = ${best?.first} (${"%.2f".format(best?.second)} °C/h)")
    println()
    println("══════════════════════════════════════════════════════════════════════")
    println("SWEEP: zmieniamy reverseFlowKgS (cyrkulacja wsteczna termosyfonu)")
    println("══════════════════════════════════════════════════════════════════════")
    println("revFlow | noc 19.5/11  noc 27/8  noc 41/8 | dzien 500W/m2  dzien 900W/m2")
    println("--------+----------------------------------+----------------------------")
    for (rf in listOf(0.002, 0.004, 0.008, 0.012, 0.020, 0.030, 0.050)) {
        val p = base.copy(reverseFlowKgS = rf)
        println("%7.3f | %11.2f  %7.2f  %7.2f | %13.1f  %12.1f".format(rf,
            noc(19.5, 11.0, 6.0, p), noc(27.0, 8.0, 6.0, p), noc(41.0, 8.0, 6.0, p),
            dzien(20.0, 12.0, 500.0, p), dzien(20.0, 12.0, 900.0, p)))
    }
    println()
    println("══════════════════════════════════════════════════════════════════════")
    println("CZY DA SIE NAPRAWIC NOC BEZ ZEPSUCIA DNIA? (dwa parametry naraz)")
    println("══════════════════════════════════════════════════════════════════════")
    println("cel: noc 41/8 = -1.28 °C/h (pomiar 23.10)  i  dzien 900W/m2 = 33.3 °C (bez zmian)")
    println("loopUa  aperture | noc 19.5/11  noc 41/8 | dzien 900  | eta0 = aperture/0.95")
    println("--------+---------+----------------------+------------+-----------------")
    for (lua in listOf(2.5, 3.0, 4.0, 5.0)) {
        for (ap in listOf(0.678, 0.75, 0.80, 0.85)) {
            val p = base.copy(loopUaWk = lua, apertureOptical = ap)
            val n3 = noc(41.0, 8.0, 6.0, p); val d2 = dzien(20.0, 12.0, 900.0, p)
            val okNoc = n3 <= -1.20; val okDzien = Math.abs(d2 - 33.3) < 1.0
            if (okNoc || (Math.abs(ap - 0.678) < 0.001 && (lua == 3.0 || lua == 5.0)))
                println("%6.1f  %7.3f | %11.2f  %7.2f | %10.1f | %6.2f  %s%s".format(
                    lua, ap, noc(19.5, 11.0, 6.0, p), n3, d2, ap / 0.95,
                    if (okNoc) "NOC-OK " else "", if (okDzien) "DZIEN-OK" else ""))
        }
    }
    println("  (eta0 powyzej 0,75 jest nierealne dla kolektora prozniowego — to znak, ze dopasowanie idzie w zla strone)")
    println()
    println("══════════════════════════════════════════════════════════════════════")
    println("SWEEP: zmieniamy tankUa (straty postojowe zbiornika)")
    println("══════════════════════════════════════════════════════════════════════")
    println("tankUa  | noc 19.5/11  noc 27/8  noc 41/8 | dzien 500W/m2  dzien 900W/m2")
    println("--------+----------------------------------+----------------------------")
    for (tu in listOf(1.4, 2.0, 2.5, 3.0, 3.5, 4.0)) {
        val p = base.copy(tankUa = tu)
        println("%6.1f  | %11.2f  %7.2f  %7.2f | %13.1f  %12.1f".format(tu,
            noc(19.5, 11.0, 6.0, p), noc(27.0, 8.0, 6.0, p), noc(41.0, 8.0, 6.0, p),
            dzien(20.0, 12.0, 500.0, p), dzien(20.0, 12.0, 900.0, p)))
    }
}
