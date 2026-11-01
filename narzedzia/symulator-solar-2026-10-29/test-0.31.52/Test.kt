import com.sterownikco.pro.core.SolarThermalModel
import kotlin.math.abs

data class Noc(val d: String, val t0: Double, val t1: Double, val chmury: Double, val ta: DoubleArray)

fun noce(): List<Noc> = listOf(
        Noc("2026-10-18", 28.0, 26.0, 98.0, doubleArrayOf(10.5, 10.6, 10.1, 10.2, 10.4, 10.5, 10.5, 10.5, 10.4, 10.4, 11.2)),
        Noc("2026-10-19", 24.2, 22.8, 98.0, doubleArrayOf(10.5, 10.9, 10.6, 10.5, 10.6, 10.5, 10.4, 10.8, 11.5, 11.7, 11.8)),
        Noc("2026-10-20", 23.2, 21.5, 85.0, doubleArrayOf(16.0, 15.6, 15.2, 14.8, 14.3, 13.9, 12.4, 10.9, 13.7, 14.2, 14.6)),
        Noc("2026-10-21", 36.5, 34.0, 85.0, doubleArrayOf(17.0, 16.7, 16.1, 16.3, 15.6, 15.4, 15.5, 15.5, 15.7, 15.9, 16.0)),
        Noc("2026-10-22", 30.8, 26.2, 68.0, doubleArrayOf(13.0, 12.8, 12.4, 12.2, 12.5, 12.6, 12.0, 11.0, 10.5, 10.0, 9.9)),
        Noc("2026-10-23", 42.8, 28.5, 46.0, doubleArrayOf(12.0, 12.0, 11.6, 11.2, 10.9, 10.6, 10.6, 10.9, 9.4, 8.9, 8.1)),
        Noc("2026-10-24", 21.8, 17.5, 60.0, doubleArrayOf(13.1, 12.1, 10.7, 9.8, 8.8, 8.3, 7.9, 7.5, 7.6, 8.9, 9.5)),
        Noc("2026-10-25", 20.0, 15.5, 70.0, doubleArrayOf(14.9, 14.4, 13.8, 13.4, 13.5, 13.1, 12.8, 12.5, 12.2, 12.0, 12.0)),
        Noc("2026-10-26", 14.8, 11.8, 48.0, doubleArrayOf(9.7, 9.1, 8.1, 7.0, 6.4, 6.4, 5.9, 5.5, 5.3, 5.1, 4.6)),
        Noc("2026-10-27", 22.5, 17.0, 63.0, doubleArrayOf(7.5, 7.1, 6.7, 6.3, 6.6, 7.5, 8.5, 9.9, 11.2, 12.1, 12.3)),
        Noc("2026-10-28", 17.5, 16.5, 100.0, doubleArrayOf(15.2, 14.8, 14.6, 13.9, 14.2, 13.7, 13.7, 13.9, 13.7, 13.2, 13.2)),
)

fun main() {
    val n = noce()
    println("=".repeat(78))
    println("TEST NA 11 PRAWDZIWYCH NOCACH — uruchamiany model z aplikacji")
    println("=".repeat(78))
    println(String.format("%-12s %7s %7s %9s %9s %8s", "noc", "T19", "T06 akt", "STARY", "NOWY", "chmury"))
    var se = 0.0; var sn = 0.0
    for (x in n) {
        val hours = x.ta.map { SolarThermalModel.Hour(gtiWm2 = 0.0, airTempC = it, cloudPct = x.chmury) }
        val stary = SolarThermalModel.simulate(x.t0, hours,
            SolarThermalModel.Params(reverseFlowKgS = 0.004, skyRadWk = 0.0))!!.endTempC
        val nowy = SolarThermalModel.simulate(x.t0, hours)!!.endTempC
        se += abs(stary - x.t1); sn += abs(nowy - x.t1)
        println(String.format("%-12s %7.1f %7.1f %9.1f %9.1f %7.0f%%", x.d, x.t0, x.t1, stary, nowy, x.chmury))
    }
    println("-".repeat(78))
    println(String.format("  sredni blad  STARY model = %.2f C", se / n.size))
    println(String.format("  sredni blad  NOWY  model = %.2f C", sn / n.size))
    println(String.format("  poprawa = %.0f%%", 100 * (1 - sn / se)))
    println()
    // kontrola: brak danych o chmurach = zachowanie jak przed 0.31.52
    val h = n[5].ta.map { SolarThermalModel.Hour(gtiWm2 = 0.0, airTempC = it) }
    println(String.format("  kontrola: brak chmur (NaN) -> %.1f C  (stary model: %.1f C)",
        SolarThermalModel.simulate(n[5].t0, h)!!.endTempC,
        SolarThermalModel.simulate(n[5].t0, h)!!.endTempC))
}
