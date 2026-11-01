// minimalna atrapa WHour — tylko pola uzywane przez fromWeather
package com.sterownikco.pro.core
data class WHour(
    val time: Long = 0L,
    val temp: Double = 0.0,
    val wind: Double = 0.0,
    val cloud: Double = 0.0,
    val shortwaveRadiation: Double? = null,
    val tiltedIrradiance: Double? = null
)
