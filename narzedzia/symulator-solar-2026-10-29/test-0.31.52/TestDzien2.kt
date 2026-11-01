import com.sterownikco.pro.core.SolarThermalModel
import kotlin.math.*

fun godziny(od: Int, doGodz: Int, chmury: Double): List<SolarThermalModel.Hour> =
    (od..doGodz).map { h ->
        val g = if (h in 7..18) max(0.0, 900.0 * sin(Math.PI * (h - 7) / 11.0)) else 0.0
        SolarThermalModel.Hour(gtiWm2 = g, airTempC = 6.0 + 7.0 * sin(Math.PI * (h - 6) / 14.0), cloudPct = chmury)
    }

fun main() {
    println("=".repeat(76))
    println("KONTROLA DZIENNA: start o 06:00 ze ZMIERZONEJ temperatury (tak liczy aplikacja)")
    println("=".repeat(76))
    for (start in listOf(15.0, 25.0, 40.0)) {
        for (ch in listOf(0.0, 50.0, 100.0)) {
            val stary = SolarThermalModel.simulate(start, godziny(6, 23, ch),
                SolarThermalModel.Params(reverseFlowKgS = 0.004, skyRadWk = 0.0))!!
            val nowy = SolarThermalModel.simulate(start, godziny(6, 23, ch))!!
            println(String.format("  start %.0f C, chmury %3.0f%%:  szczyt %.2f -> %.2f (%+.2f)   koniec %.2f -> %.2f (%+.2f)",
                start, ch, stary.peakTempC, nowy.peakTempC, nowy.peakTempC - stary.peakTempC,
                stary.endTempC, nowy.endTempC, nowy.endTempC - stary.endTempC))
        }
    }
}
