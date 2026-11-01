package com.sterownikco.pro.core
fun main() {
    val lines = java.io.File("/tmp/cal/noce.csv").readLines().drop(1)
    println("╔══════════════════════════════════════════════════════════════════════════════╗")
    println("║  MODEL vs RZECZYWISTOSC — 11 prawdziwych nocy z Twojej centrali             ║")
    println("╚══════════════════════════════════════════════════════════════════════════════╝")
    println()
    println("noc          T19:00  zmierzone  model   blad     ocena")
    println("--------     ------  ---------  -----  ------   ------")
    var sumErr = 0.0; var n = 0; var worse = 0; var better = 0
    for (l in lines) {
        val f = l.split(",")
        if (f.size < 3) continue
        val data = f[0]
        val t0 = f[1].toDouble(); val t1 = f[2].toDouble()
        val ta = f.drop(3).mapNotNull { it.toDoubleOrNull() }.filter { !it.isNaN() }
        if (ta.isEmpty()) continue
        val hours = ta.map { SolarThermalModel.Hour(0.0, it) }
        val r = SolarThermalModel.simulate(t0, hours) ?: continue
        val zm = t1 - t0
        val mo = r.endTempC - t0
        val err = mo - zm
        sumErr += Math.abs(err); n++
        if (Math.abs(err) < 0.5) better++ else worse++
        println("%-12s %6.1f  %+9.1f  %+5.1f  %+6.1f   %s".format(
            data, t0, zm, mo, err, if (Math.abs(err) < 0.5) "OK" else if (err < 0) "model za szybko chlodzi" else "model za wolno chlodzi"))
    }
    println()
    println("Sredni blad bezwzgledny: %.2f °C na noc (11 h)".format(sumErr / n))
    println("Nocy w granicach 0,5 °C: $better / $n")
}
