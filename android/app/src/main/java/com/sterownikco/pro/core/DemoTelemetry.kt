package com.sterownikco.pro.core

import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt
import kotlin.math.sin

/* ══════════════════════════════════════════════════════════════════════════
   `generateTelemetryHistory(rangeSec)` — syntetyczna historia dla trybu
   symulacji (taka sama siatka próbek i te same cykle sin/cos co w Piec.html).
   ══════════════════════════════════════════════════════════════════════════ */
object DemoTelemetry {

    fun generate(rangeSec: Int, night: Boolean): List<TelemPoint> {
        val pts = when {
            rangeSec <= 6 * 3600 -> 72
            rangeSec <= 24 * 3600 -> 288
            rangeSec <= 7 * 86400 -> 504
            else -> 720
        }
        val stepSec = rangeSec.toDouble() / pts
        val nowTs = System.currentTimeMillis()
        val rows = ArrayList<TelemPoint>(pts)
        val baseOgrz = 58.0; val baseBojler = 48.0; val baseZewn = 11.5; val basePanel = 35.0
        for (i in 0 until pts) {
            val ts = nowTs - ((pts - 1 - i) * stepSec * 1000).toLong()
            val tNorm = i.toDouble() / pts
            val cycle = sin(tNorm * Math.PI * 8)
            val dayCycle = sin(tNorm * Math.PI * 2)
            val og = baseOgrz + cycle * 12 + sin(tNorm * 40) * 1.5
            val bj = baseBojler + sin((tNorm - .05) * Math.PI * 8) * 8
            val zw = baseZewn + dayCycle * 7 + sin(tNorm * 30) * .6
            val pn = if (night) 10.0 else max(12.0, basePanel + dayCycle * 25 + sin(tNorm * 20) * 3)
            val kl = max(10.0, min(100.0, 50.0 - cycle * 40))
            val sy = max(5.0, min(80.0, 30.0 - cycle * 20))
            rows.add(
                TelemPoint(
                    ts = ts, seq = (i + 1).toLong(),
                    t_zewn = r1(zw), t_bojler = r1(max(15.0, bj)), t_ogrz = r1(max(20.0, og)),
                    t_ogrz_sr = r1(max(20.0, og - 1.2)), t_powrot = r1(max(18.0, og - 14)),
                    t_panel = r1(pn), t_pokoj = r1(21.4 + sin(tNorm * 10) * .5),
                    t_trociny = r1(31.0 + sin(tNorm * 6) * 2),
                    wilgotnosc = r1(min(95.0, max(35.0, 55.0 - dayCycle * 15))),
                    cisnienie = r1(1009.0 + sin(tNorm * 4) * 6),
                    dym = (18 + sin(tNorm * 12) * 8).roundToInt().toDouble(),
                    klapa = kl.roundToInt().toDouble(), syberka = sy.roundToInt().toDouble(),
                    pompa = og > 50, sim = 1
                )
            )
        }
        return rows
    }

    private fun r1(v: Double): Double = (v * 10).roundToInt() / 10.0
}
