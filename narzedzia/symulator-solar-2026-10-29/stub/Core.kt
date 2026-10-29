// STUB modelu zależności — Prefs, Weather.startOfHour, WHour.
package com.sterownikco.pro.core

import java.util.Calendar

class Prefs {
    fun get(key: String): String? = null
    fun set(key: String, value: String) {}
    companion object { const val K_SOLAR_REAL_ARCHIVE = "solar_real_archive_v1" }
}

data class WHour(
    val time: Long, val temp: Double, val wind: Double, val cloud: Double,
    val precip: Double, val precipProb: Double, val code: Int, val isDay: Boolean,
    val shortwaveRadiation: Double? = null,
    val tiltedIrradiance: Double? = null
)

object Weather {
    fun startOfHour(nowMillis: Long = System.currentTimeMillis()): Long =
        Calendar.getInstance().apply {
            timeInMillis = nowMillis
            set(Calendar.MINUTE, 0); set(Calendar.SECOND, 0); set(Calendar.MILLISECOND, 0)
        }.timeInMillis
}

// Kopia 1:1 z core/Telemetry.kt (pola potrzebne przez SolarAnalytics.mergeCloudToday)
data class TelemPoint(
    val ts: Long, val seq: Long,
    val t_zewn: Double?, val t_bojler: Double?, val t_ogrz: Double?, val t_ogrz_sr: Double?,
    val t_powrot: Double?, val t_panel: Double?, val t_pokoj: Double?, val t_trociny: Double?,
    val wilgotnosc: Double?, val cisnienie: Double?, val dym: Double?,
    val klapa: Double?, val syberka: Double?,
    val sim: Long = 0L,
    val simulated: Boolean = false,
    val isLive: Boolean = false,
    val pumpOn: Boolean? = null,
    val sourceId: String = ""
)
