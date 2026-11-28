package com.sterownikco.pro.widget

import java.util.Locale

internal object WidgetTemperatureFormat {
    fun degrees(key: String, value: Double?, simulated: Boolean): String {
        if (value == null || !value.isFinite()) return "—"
        val number = if (key == "t_zewn" || key == "t_zewn_2" || key == "pogoda_temp")
            String.format(Locale.US, "%.1f", value).replace('.', ',')
        else Math.round(value).toString()
        return (if (simulated) "~" else "") + number + "°"
    }
}
