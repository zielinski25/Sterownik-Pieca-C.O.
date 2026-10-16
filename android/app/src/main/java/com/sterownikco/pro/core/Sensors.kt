package com.sterownikco.pro.core

/**
 * `ALL_SENSORS` z Piec.html:5207 — wykaz czujników pomiarowych instalacji.
 * Kolejność jest kolejnością listy w oryginale (te same 10 pozycji).
 */
data class SensorDef(
    val id: String,
    val name: String,
    val bus: String,
    val unit: String,
    val min: Double,
    val max: Double,
    val step: Double,
    val key: String,
    val ico: String,
    /** nagłówek sheeta (`showSheet(tytuł, ikona, …)`) */
    val sheetTitle: String,
    val sheetIcon: String
) {
    companion object {
        val ALL = listOf(
            SensorDef("zewn", "Temperatura zewnętrzna", "DS18B20 · Magistrala 1-Wire", "°C", -25.0, 45.0, 0.5, "t_zewn", "🌡️",
                "Temperatura zewnętrzna", "outside"),
            SensorDef("bojler", "Bojler C.W.U.", "DS18B20 w gilzie · 1-Wire", "°C", 10.0, 95.0, 0.5, "t_bojler", "💧",
                "Bojler C.W.U.", "thermo"),
            SensorDef("pokoj", "Temperatura pomieszczenia", "DS18B20 strefy dom · 1-Wire", "°C", 8.0, 45.0, 0.5, "t_pokoj", "🏠",
                "Temperatura pomieszczenia", "thermo"),
            SensorDef("ogrz", "Piec C.O. (kocioł)", "DS18B20 płaszcza kotła · 1-Wire", "°C", 10.0, 99.0, 0.5, "t_ogrz", "🔥",
                "Piec C.O. (kocioł)", "thermo"),
            SensorDef("panel", "Panel słoneczny (solary)", "Kolektor glikolowy · 1-Wire / PT1000", "°C", 0.0, 140.0, 0.5, "t_panel", "☀️",
                "Panel słoneczny (solary)", "panel"),
            SensorDef("ogrz_powrot", "Temperatura powrotu kotła", "Przylgowy powrót C.O. · 1-Wire", "°C", 10.0, 95.0, 0.5, "t_powrot", "🔄",
                "Temperatura powrotu kotła", "thermo"),
            SensorDef("ogrz_trociny", "Temperatura podajnika / trocin", "Zasobnik paliwa · 1-Wire", "°C", 0.0, 90.0, 0.5, "t_trociny", "🪵",
                "Temperatura podajnika / trocin", "thermo"),
            SensorDef("cisnienie", "Ciśnienie atmosferyczne", "Barometr cyfrowy BMP280 · I²C", "hPa", 900.0, 1100.0, 1.0, "cisnienie", "⏱️",
                "Ciśnienie atmosferyczne", "settings"),
            SensorDef("wilgotnosc", "Wilgotność powietrza", "Higrometr cyfrowy BME280 · I²C", "%", 10.0, 100.0, 1.0, "wilgotnosc", "💦",
                "Wilgotność powietrza", "settings"),
            SensorDef("dym", "Czujnik dymu i spalin", "Przetwornik optyczny MQ-2 · ADC", "ADC", 0.0, 4095.0, 1.0, "dym", "🛡️",
                "Czujnik dymu i spalin", "shield")
        )

        fun byId(id: String?): SensorDef? = ALL.firstOrNull { it.id == id }

        /** `sen.step >= 1 ? String(Math.round(v)) : fmt1(v)` */
        fun fmt(sen: SensorDef, v: Double): String = if (!v.isFinite()) "—" else
            if (sen.step >= 1.0) Math.round(v).toString() else String.format(java.util.Locale.US, "%.1f", v)
    }
}
