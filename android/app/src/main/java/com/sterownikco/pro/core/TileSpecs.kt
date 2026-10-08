package com.sterownikco.pro.core

/* ══════════════════════════════════════════════════════════════════════════
   `renderDashboard()` z Piec.html — czysta funkcja: stan → specyfikacja
   kafelków (wartość, jednostka, opis, plakietka, kolor, pasek, ilustracja).
   ══════════════════════════════════════════════════════════════════════════ */

data class TileSpec(
    val id: String,
    val title: String,
    val grid: Int,
    val value: String,
    val unit: String = "",
    val desc: String = "",
    val badge: String = "LIVE",
    val badgeCls: String = "live",
    val cls: String = "c-none",
    val state: String = "",
    val frac: Double = 0.0,
    val vcol: String = "",
    val sim: Boolean = false,
    val stale: Boolean = false,
    val artKey: String = ""
)

data class DashboardTileDef(
    val id: String,
    val title: String,
    val grid: Int,
    val description: String,
    val defaultOn: Boolean = false
)

object TileDefs {
    val TILES = listOf(
        DashboardTileDef("zewn", "Zewnętrzna", 1, "Temperatura na zewnątrz", true),
        DashboardTileDef("ogrz", "Piec C.O.", 1, "Bieżąca temperatura pieca", true),
        DashboardTileDef("bojler", "Bojler", 1, "Temperatura wody w bojlerze", true),
        DashboardTileDef("panel", "Panel słoneczny", 1, "Temperatura kolektora", true),
        DashboardTileDef("pokoj", "Pomieszczenie", 1, "Temperatura w pomieszczeniu", true),
        DashboardTileDef("cisnienie", "Ciśnienie", 1, "Ciśnienie atmosferyczne", true),
        DashboardTileDef("wilgotnosc", "Wilgotność", 1, "Wilgotność powietrza", true),
        DashboardTileDef("powrot", "Powrót C.O.", 1, "Temperatura wody wracającej z instalacji"),
        DashboardTileDef("ogrz_sr", "Średnia pieca", 1, "Uśredniony odczyt temperatury pieca"),
        DashboardTileDef("trociny", "Temperatura trocin", 1, "Temperatura czujnika zasobnika"),
        DashboardTileDef("weather", "Pogoda teraz", 1, "Aktualna pogoda z Open-Meteo"),
        DashboardTileDef("pompa", "Pompa", 2, "Stan i wybrana strategia pompy", true),
        DashboardTileDef("serwo", "Serwo", 2, "Tryb i położenie klapy", true),
        DashboardTileDef("mieszadlo", "Mieszadło", 2, "Stan pracy mieszadła", true),
        DashboardTileDef("dym", "Czujnik dymu", 2, "Odczyt ADC i stan czujnika", true),
        DashboardTileDef("alarmy", "Bezpieczeństwo", 2, "Podsumowanie alarmów kotła i kolektora")
    )
    val DEFAULT_VISIBLE_IDS: Set<String> = TILES.filter { it.defaultOn }.map { it.id }.toSet()
}

fun buildTiles(m: AppModel, weatherCode: Int, isDay: Boolean): List<TileSpec> {
    val S = m.S
    fun fmt1(v: Double) = if (v.isFinite()) String.format(java.util.Locale.US, "%.1f", v) else "—"
    fun rounded(v: Double) = if (v.isFinite()) Math.round(v).toString() else "—"
    fun norm(v: Double, min: Double, max: Double) =
        if (v.isFinite() && max > min) ((v - min) / (max - min)).coerceIn(0.0, 1.0) else 0.0
    fun symDesc(pole: String, base: String): String =
        if (S.symAktywna(pole)) "symulacja (${S.sym[pole]?.min ?: 0} min)" else base
    fun badge(pole: String, dataKey: String? = null): Pair<String, String>? {
        if (S.symAktywna(pole)) return ("SYMULACJA " + (S.sym[pole]?.min ?: 0) + "M") to "sim"
        val key = dataKey ?: PiecState.SYM_POLA[pole] ?: pole
        if (!S.hasData(key)) return "BRAK DANYCH" to "stale"
        if (!S.online || !m.isFbFresh) return "NIEAKTUALNE" to "stale"
        return null
    }

    val out = ArrayList<TileSpec>()
    val stale = !S.online || !m.isFbFresh

    // ── zewn ────────────────────────────────────────────────────────────────
    val tz = S.valOf("zewn", "t_zewn")
    out.add(
        TileSpec(
            id = "zewn", title = "Zewnętrzna", grid = 1, value = fmt1(tz), unit = "°C",
            cls = if (!tz.isFinite()) "c-none" else if (tz < 0) "c-flame2" else if (tz > 15) "c-ember" else "c-flame",
            frac = norm(tz, -25.0, 45.0),
            desc = symDesc("zewn", if (!tz.isFinite()) "oczekiwanie na odczyt" else if (tz < 0) "mróz" else if (tz > 25) "upał" else "odczyt stabilny"),
            vcol = if (tz < 0) "#7dd3fc" else if (tz > 15) "#ffb86b" else "",
            badge = badge("zewn")?.first ?: "LIVE", badgeCls = badge("zewn")?.second ?: "live",
            sim = S.symAktywna("zewn"), stale = stale, artKey = "zewn"
        )
    )

    // ── ogrz ────────────────────────────────────────────────────────────────
    val to = S.valOf("ogrz", "t_ogrz")
    val ogrzAlarmKnown = S.hasData("alarm_ogrzewanie")
    val ogrzAlarm = ogrzAlarmKnown && S.alarm_ogrzewanie
    val ogrzDataBadge = badge("ogrz")
    val ogrzBadge = when {
        ogrzAlarm -> "ALARM" to "alarm"
        S.symAktywna("ogrz") -> "SYMULACJA" to "sim"
        ogrzDataBadge != null -> ogrzDataBadge
        ogrzAlarmKnown -> "BRAK ALARMU" to "live"
        else -> "ALARM? BRAK DANYCH" to "stale"
    }
    out.add(
        TileSpec(
            id = "ogrz", title = "Piec C.O.", grid = 1, value = fmt1(to), unit = "°C", cls = if (to.isFinite()) "c-ember" else "c-none",
            frac = norm(to, 10.0, 99.0), desc = symDesc("ogrz", if (!to.isFinite()) "oczekiwanie na odczyt" else "śr. " + fmt1(S.valOf("ogrz_sr", "t_ogrz_sr")) + "°C") +
                if (!ogrzAlarmKnown) " · alarm: brak danych" else if (ogrzAlarm) " · alarm" else "",
            state = if (ogrzAlarm) "err" else "",
            badge = ogrzBadge.first, badgeCls = ogrzBadge.second,
            sim = S.symAktywna("ogrz"), stale = stale, artKey = "ogrz"
        )
    )

    // ── bojler ───────────────────────────────────────────────────────────────
    val tb = S.valOf("bojler", "t_bojler")
    out.add(
        TileSpec(
            id = "bojler", title = "Bojler", grid = 1, value = fmt1(tb), unit = "°C", cls = if (tb.isFinite()) "c-ember" else "c-none",
            frac = norm(tb, 10.0, 95.0),
            desc = symDesc("bojler", if (!tb.isFinite()) "oczekiwanie na odczyt" else if (tb > 48) "woda gorąca" else if (tb > 30) "woda ciepła" else "woda chłodna"),
            badge = badge("bojler")?.first ?: "LIVE", badgeCls = badge("bojler")?.second ?: "live",
            sim = S.symAktywna("bojler"), stale = stale, artKey = "bojler"
        )
    )

    // ── panel ────────────────────────────────────────────────────────────────
    val tp = S.valOf("panel", "t_panel")
    val panelAlarmKnown = S.hasData("alarm_panel")
    val panelAlarm = panelAlarmKnown && S.alarm_panel
    val panelDataBadge = badge("panel")
    val panelBadge = when {
        panelAlarm -> "ALARM" to "alarm"
        S.symAktywna("panel") -> "SYMULACJA" to "sim"
        panelDataBadge != null -> panelDataBadge
        panelAlarmKnown -> "BRAK ALARMU" to "live"
        else -> "ALARM? BRAK DANYCH" to "stale"
    }
    out.add(
        TileSpec(
            id = "panel", title = "Panel słon.", grid = 1, value = fmt1(tp), unit = "°C", cls = if (tp.isFinite()) "c-slonce" else "c-none",
            frac = norm(tp, 0.0, 140.0),
            desc = symDesc("panel", if (!tp.isFinite()) "oczekiwanie na odczyt" else if (S.night) "noc" else "nasłonecznienie ok") +
                if (!panelAlarmKnown) " · alarm: brak danych" else if (panelAlarm) " · alarm" else "",
            state = if (panelAlarm) "err" else "",
            badge = panelBadge.first, badgeCls = panelBadge.second,
            sim = S.symAktywna("panel"), stale = stale, artKey = "panel"
        )
    )

    // ── pokoj ────────────────────────────────────────────────────────────────
    val tr = S.valOf("pokoj", "t_pokoj")
    out.add(
        TileSpec(
            id = "pokoj", title = "Pomieszczenie", grid = 1, value = fmt1(tr), unit = "°C", cls = if (tr.isFinite()) "c-ok" else "c-none",
            frac = norm(tr, 8.0, 45.0), desc = symDesc("pokoj", if (tr.isFinite()) "komfort cieplny" else "oczekiwanie na odczyt"),
            badge = badge("pokoj")?.first ?: "LIVE", badgeCls = badge("pokoj")?.second ?: "live",
            sim = S.symAktywna("pokoj"), stale = stale, artKey = "pokoj"
        )
    )

    // ── cisnienie ────────────────────────────────────────────────────────────
    val pc = S.valOf("cisnienie", "cisnienie")
    out.add(
        TileSpec(
            id = "cisnienie", title = "Ciśnienie", grid = 1, value = rounded(pc), unit = "hPa",
            cls = if (pc.isFinite()) "c-fiolet" else "c-none", frac = norm(pc, 900.0, 1100.0),
            desc = symDesc("cisnienie", if (!pc.isFinite()) "oczekiwanie na odczyt" else if (pc < 1000) "niż — możliwy gorszy ciąg" else if (pc > 1020) "wyż" else "stabilnie"),
            badge = badge("cisnienie")?.first ?: "LIVE", badgeCls = badge("cisnienie")?.second ?: "live",
            sim = S.symAktywna("cisnienie"), stale = stale, artKey = "cisnienie"
        )
    )

    // ── wilgotnosc ───────────────────────────────────────────────────────────
    val hu = S.valOf("wilgotnosc", "wilgotnosc")
    out.add(
        TileSpec(
            id = "wilgotnosc", title = "Wilgotność", grid = 1, value = rounded(hu), unit = "%",
            cls = if (hu.isFinite()) "c-flame2" else "c-none", frac = norm(hu, 10.0, 100.0),
            desc = symDesc("wilgotnosc", if (!hu.isFinite()) "oczekiwanie na odczyt" else if (hu > 70) "podwyższona" else "w normie"),
            badge = badge("wilgotnosc")?.first ?: "LIVE", badgeCls = badge("wilgotnosc")?.second ?: "live",
            sim = S.symAktywna("wilgotnosc"), stale = stale, artKey = "wilgotnosc"
        )
    )

    // Dodatkowe kafelki do wyboru w ustawieniach pulpitu.
    val powrot = S.valOf("ogrz_powrot", "t_powrot")
    out.add(
        TileSpec(
            id = "powrot", title = "Powrót C.O.", grid = 1, value = fmt1(powrot), unit = "°C",
            cls = if (powrot.isFinite()) "c-fiolet" else "c-none", frac = norm(powrot, 10.0, 95.0),
            desc = symDesc("ogrz_powrot", if (powrot.isFinite()) "temperatura powracającej wody" else "oczekiwanie na odczyt"),
            badge = badge("ogrz_powrot")?.first ?: "LIVE", badgeCls = badge("ogrz_powrot")?.second ?: "live",
            sim = S.symAktywna("ogrz_powrot"), stale = stale, artKey = "ogrz"
        )
    )
    val trociny = S.valOf("ogrz_trociny", "t_trociny")
    out.add(
        TileSpec(
            id = "trociny", title = "Temperatura trocin", grid = 1, value = fmt1(trociny), unit = "°C",
            cls = if (trociny.isFinite()) "c-ember" else "c-none", frac = norm(trociny, 0.0, 90.0),
            desc = symDesc("ogrz_trociny", if (trociny.isFinite()) "czujnik zasobnika" else "oczekiwanie na odczyt"),
            badge = badge("ogrz_trociny")?.first ?: "LIVE", badgeCls = badge("ogrz_trociny")?.second ?: "live",
            sim = S.symAktywna("ogrz_trociny"), stale = stale, artKey = "bojler"
        )
    )
    val ogrzAvg = S.valOf("ogrz_sr", "t_ogrz_sr")
    out.add(
        TileSpec(
            id = "ogrz_sr", title = "Średnia pieca", grid = 1, value = fmt1(ogrzAvg), unit = "°C",
            cls = if (ogrzAvg.isFinite()) "c-ember" else "c-none", frac = norm(ogrzAvg, 10.0, 99.0),
            desc = if (ogrzAvg.isFinite()) "uśredniony odczyt temperatury pieca" else "oczekiwanie na odczyt",
            badge = badge("ogrz_sr", "t_ogrz_sr")?.first ?: "LIVE",
            badgeCls = badge("ogrz_sr", "t_ogrz_sr")?.second ?: "live",
            stale = stale, artKey = "ogrz"
        )
    )
    val currentWeather = m.weather?.current
    out.add(
        TileSpec(
            id = "weather", title = "Pogoda teraz", grid = 1,
            value = currentWeather?.let { fmt1(it.temp) } ?: "—",
            unit = if (currentWeather != null) "°C" else "",
            cls = "c-flame2", frac = currentWeather?.let { ((it.temp + 20) / 60).coerceIn(0.0, 1.0) } ?: 0.0,
            desc = currentWeather?.let { Weather.desc(it.code) } ?: if (m.weatherBusy) "pobieranie danych" else "brak danych meteo",
            badge = if (currentWeather != null) "METEO" else if (m.weatherBusy) "ŁADOWANIE" else "BRAK DANYCH",
            badgeCls = if (currentWeather != null) "live" else "stale",
            state = if (currentWeather == null) "dis" else "", stale = currentWeather == null || stale,
            artKey = "zewn"
        )
    )

    // ── pompa ───────────────────────────────────────────────────────────────
    val pumpKnown = S.hasData("pompa")
    val strategyKnown = S.hasData("wybor") && S.wybor in 1..3
    val stratBadge = when {
        !pumpKnown -> "BRAK DANYCH"
        !S.pompa -> "POSTÓJ"
        strategyKnown && S.wybor == 1 -> "⏱️ CZASOWY"
        strategyKnown && S.wybor == 2 -> "🌡️ TEMP."
        strategyKnown && S.wybor == 3 -> "⚡ AUTO"
        else -> "PRACA"
    }
    val stratDesc = when {
        !pumpKnown -> "oczekiwanie na stan pompy"
        !strategyKnown -> "odebrano stan pompy; tryb pracy nieznany"
        S.wybor == 1 && S.hasAllData("czasOn", "czasOff") -> "⏱️ Czasowy: ${S.czasOn}m ON / ${S.czasOff}m OFF"
        S.wybor == 2 && S.hasAllData("tempOn", "tempOff") -> "🌡️ Temp: ON ≥ ${S.tempOn}° / OFF ≤ ${S.tempOff}°"
        S.wybor == 3 && S.hasData("tempOn") -> "⚡ Automatyczny: start przy ${S.tempOn}°C"
        else -> "stan odebrany; oczekiwanie na konfigurację"
    }
    out.add(
        TileSpec(
            id = "pompa", title = "Pompa", grid = 2,
            value = if (!pumpKnown) "—" else if (S.pompa) "WŁĄCZONA" else "WYŁĄCZONA",
            cls = if (pumpKnown) "c-flame" else "c-none", state = if (pumpKnown && S.pompa) "ok" else "",
            frac = if (pumpKnown && S.pompa) 1.0 else 0.0,
            desc = stratDesc, badge = stratBadge, badgeCls = if (pumpKnown && !stale) "live" else "stale",
            stale = stale || !pumpKnown, artKey = "pompa"
        )
    )

    // ── serwo ───────────────────────────────────────────────────────────────
    val servoKnown = S.hasData("tryb_serwa") && S.tryb_serwa in 1..3
    val klapaKnown = S.hasData("klapa") && S.klapa in 0..180
    val syberkaKnown = S.hasData("syberka") && S.syberka in 0..90
    val klapaPct = if (klapaKnown) Math.round(S.klapa * 100.0 / 180.0) else null
    val sybPct = if (syberkaKnown) Math.round(S.syberka * 100.0 / 90.0) else null
    out.add(
        TileSpec(
            id = "serwo", title = "Serwo", grid = 2,
            value = if (servoKnown) PiecState.TRYB_NAZWA[S.tryb_serwa] ?: "—" else "—",
            cls = if (servoKnown) "c-flame" else "c-none",
            state = if (!servoKnown) "" else when (S.tryb_serwa) { 2 -> "warn"; 3 -> "err"; else -> "" },
            frac = if (klapaPct != null) klapaPct / 100.0 else 0.0,
            desc = "klapa ${if (klapaKnown) "${S.klapa}°" else "—"} • syberek ${if (syberkaKnown) "${S.syberka}°" else "—"}",
            badge = if (klapaPct != null && sybPct != null) "K$klapaPct% S$sybPct%" else "BRAK DANYCH",
            badgeCls = if (servoKnown && klapaKnown && syberkaKnown && !stale) "active" else "stale",
            stale = stale || !servoKnown || !klapaKnown || !syberkaKnown, artKey = "serwo"
        )
    )

    // ── mieszadlo ────────────────────────────────────────────────────────────
    val mixerKnown = S.hasData("mieszadlo")
    out.add(
        TileSpec(
            id = "mieszadlo", title = "Mieszadło", grid = 2,
            value = if (!mixerKnown) "—" else if (S.mieszadlo) "WŁĄCZONE" else "WYŁĄCZONE",
            cls = if (mixerKnown) "c-ok" else "c-none",
            state = if (mixerKnown && S.mieszadlo) "ok" else "",
            frac = if (mixerKnown && S.mieszadlo) 1.0 else 0.0,
            desc = when {
                !mixerKnown -> "oczekiwanie na stan mieszadła"
                S.rozpalanie && S.hasData("rozpalanie") -> "override: rozpalanie"
                S.mieszadloWlaczony && S.hasData("mieszadloWlaczony") && S.hasAllData("mieszadloCzasOn", "mieszadloCzasOff") -> "cykl ${S.mieszadloCzasOn} s / ${S.mieszadloCzasOff} min"
                else -> "stan odebrany"
            },
            badge = if (!mixerKnown) "BRAK DANYCH" else if (S.mieszadlo) "PRACUJE" else "POSTÓJ",
            badgeCls = if (mixerKnown && !stale) "live" else "stale",
            stale = stale || !mixerKnown, artKey = "mieszadlo"
        )
    )

    // ── dym ─────────────────────────────────────────────────────────────────
    val dymV = S.valOf("dym", "dym")
    val dymSimulation = S.symAktywna("dym")
    val dymKnown = dymSimulation || S.hasData("dym")
    val dymDis = S.hasData("dym_wlaczony") && !S.dym_wlaczony && !dymSimulation
    val dymAlarmKnown = S.hasData("dym_alarm")
    val dymAlarm = dymAlarmKnown && S.dym_alarm
    val smokeBadge = when {
        dymAlarm -> "ALARM" to "alarm"
        dymSimulation -> badge("dym")!!
        !dymKnown && !dymDis -> "BRAK DANYCH" to "stale"
        dymDis -> "WYŁ." to "stale"
        !dymAlarmKnown -> "ALARM? BRAK DANYCH" to "stale"
        else -> badge("dym") ?: ("BRAK ALARMU" to "live")
    }
    out.add(
        TileSpec(
            id = "dym", title = "Czujnik dymu", grid = 2,
            value = when { dymSimulation -> rounded(dymV); dymDis -> "OFF"; dymKnown -> rounded(dymV); else -> "—" },
            unit = if (dymSimulation || (dymKnown && !dymDis)) "ADC" else "", cls = "c-none",
            state = if (dymAlarm) "err" else if (dymDis) "dis" else "",
            frac = if (dymKnown) norm(dymV, 0.0, 4095.0) else 0.0,
            desc = when {
                dymAlarm -> "ALARM: próg ${if (S.hasData("progAlarmDym")) S.progAlarmDym else "—"} ADC przekroczony"
                dymSimulation -> "symulacja sterownika (${S.sym["dym"]?.min ?: 0} min)"
                dymDis -> "czujnik potwierdzony jako wyłączony"
                !dymKnown -> "oczekiwanie na rzeczywisty odczyt"
                S.hasData("dym_swiezy") && S.dym_swiezy && S.hasData("progAlarmDym") -> "próg alarmu ${S.progAlarmDym}"
                else -> "odczyt z centrali"
            },
            badge = smokeBadge.first, badgeCls = smokeBadge.second,
            sim = dymSimulation, stale = stale || (!dymKnown && !dymDis), artKey = "dym"
        )
    )

    // ── skrót bezpieczeństwa ────────────────────────────────────────────────
    val anyAlarm = (S.hasData("dym_alarm") && S.dym_alarm) ||
        (S.hasData("alarm_ogrzewanie") && S.alarm_ogrzewanie) ||
        (S.hasData("alarm_panel") && S.alarm_panel)
    val alarmFlagsKnown = S.hasAllData("dym_alarm", "alarm_ogrzewanie", "alarm_panel")
    out.add(
        TileSpec(
            id = "alarmy", title = "Bezpieczeństwo", grid = 2,
            value = if (anyAlarm) "ALARM" else if (alarmFlagsKnown) "OK" else "—",
            cls = if (anyAlarm) "c-ember" else if (alarmFlagsKnown) "c-ok" else "c-none",
            state = if (anyAlarm) "err" else "",
            desc = when {
                S.hasData("dym_alarm") && S.dym_alarm -> "wykryto dym"
                S.hasData("alarm_ogrzewanie") && S.alarm_ogrzewanie -> "przekroczony próg temperatury pieca"
                S.hasData("alarm_panel") && S.alarm_panel -> "przekroczony próg temperatury panelu"
                alarmFlagsKnown -> "brak aktywnych alarmów"
                else -> "oczekiwanie na flagi alarmów sterownika"
            },
            badge = if (anyAlarm) "UWAGA" else if (alarmFlagsKnown) "BEZPIECZNIE" else "BRAK DANYCH",
            badgeCls = if (anyAlarm) "alarm" else if (alarmFlagsKnown && !stale) "live" else "stale",
            stale = stale || !alarmFlagsKnown, artKey = "dym"
        )
    )

    return out
}
