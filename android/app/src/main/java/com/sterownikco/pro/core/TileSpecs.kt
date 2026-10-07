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

object TileDefs {
    val TILES = listOf(
        Triple("zewn", 1, "Zewnętrzna"), Triple("ogrz", 1, "Piec C.O."), Triple("bojler", 1, "Bojler"),
        Triple("panel", 1, "Panel słon."), Triple("pokoj", 1, "Pomieszczenie"),
        Triple("cisnienie", 1, "Ciśnienie"), Triple("wilgotnosc", 1, "Wilgotność"),
        Triple("pompa", 2, "Pompa"), Triple("serwo", 2, "Serwo"),
        Triple("mieszadlo", 2, "Mieszadło"), Triple("dym", 2, "Czujnik dymu")
    )
}

fun buildTiles(m: AppModel, weatherCode: Int, isDay: Boolean): List<TileSpec> {
    val S = m.S
    fun fmt1(v: Double) = String.format(java.util.Locale.US, "%.1f", v)
    fun symDesc(pole: String, base: String): String =
        if (S.symAktywna(pole)) "symulacja (${S.sym[pole]?.min ?: 0} min)" else base
    fun badge(pole: String): Pair<String, String>? = if (!S.symAktywna(pole)) null
    else "SYMULACJA " + (S.sym[pole]?.min ?: 0) + "M" to "sim"

    val out = ArrayList<TileSpec>()
    val stale = !S.online && !m.connected

    // ── zewn ────────────────────────────────────────────────────────────────
    val tz = S.valOf("zewn", "t_zewn")
    out.add(
        TileSpec(
            id = "zewn", title = "Zewnętrzna", grid = 1, value = fmt1(tz), unit = "°C",
            cls = if (tz < 0) "c-flame2" else if (tz > 15) "c-ember" else "c-flame",
            frac = (tz + 20) / 60,
            desc = symDesc("zewn", if (tz < 0) "mróz" else if (tz > 25) "upał" else "odczyt stabilny"),
            vcol = if (tz < 0) "#7dd3fc" else if (tz > 15) "#ffb86b" else "",
            badge = badge("zewn")?.first ?: "LIVE", badgeCls = badge("zewn")?.second ?: "live",
            sim = S.symAktywna("zewn"), stale = stale, artKey = "zewn"
        )
    )

    // ── ogrz ────────────────────────────────────────────────────────────────
    val to = S.valOf("ogrz", "t_ogrz")
    out.add(
        TileSpec(
            id = "ogrz", title = "Piec C.O.", grid = 1, value = fmt1(to), unit = "°C", cls = "c-ember",
            frac = to / 160, desc = symDesc("ogrz", "śr. " + fmt1(S.t_ogrz_sr) + "°C"),
            state = if (S.alarm_ogrzewanie) "err" else "",
            badge = if (S.symAktywna("ogrz")) "SYMULACJA " + (S.sym["ogrz"]?.min ?: 0) + "M" else if (S.alarm_ogrzewanie) "ALARM" else "LIVE",
            badgeCls = if (S.symAktywna("ogrz")) "sim" else if (S.alarm_ogrzewanie) "alarm" else "live",
            sim = S.symAktywna("ogrz"), stale = stale, artKey = "ogrz"
        )
    )

    // ── bojler ───────────────────────────────────────────────────────────────
    val tb = S.valOf("bojler", "t_bojler")
    out.add(
        TileSpec(
            id = "bojler", title = "Bojler", grid = 1, value = fmt1(tb), unit = "°C", cls = "c-ember",
            frac = (tb - 15) / 55,
            desc = symDesc("bojler", if (tb > 48) "woda gorąca" else if (tb > 30) "woda ciepła" else "woda chłodna"),
            badge = badge("bojler")?.first ?: "LIVE", badgeCls = badge("bojler")?.second ?: "live",
            sim = S.symAktywna("bojler"), stale = stale, artKey = "bojler"
        )
    )

    // ── panel ────────────────────────────────────────────────────────────────
    val tp = S.valOf("panel", "t_panel")
    out.add(
        TileSpec(
            id = "panel", title = "Panel słon.", grid = 1, value = fmt1(tp), unit = "°C", cls = "c-slonce",
            frac = tp / 120,
            desc = symDesc("panel", if (S.night) "noc" else "nasłonecznienie ok"),
            state = if (S.alarm_panel) "err" else "",
            badge = if (S.symAktywna("panel")) "SYMULACJA " + (S.sym["panel"]?.min ?: 0) + "M" else if (S.alarm_panel) "ALARM" else "LIVE",
            badgeCls = if (S.symAktywna("panel")) "sim" else if (S.alarm_panel) "alarm" else "live",
            sim = S.symAktywna("panel"), stale = stale, artKey = "panel"
        )
    )

    // ── pokoj ────────────────────────────────────────────────────────────────
    val tr = S.valOf("pokoj", "t_pokoj")
    out.add(
        TileSpec(
            id = "pokoj", title = "Pomieszczenie", grid = 1, value = fmt1(tr), unit = "°C", cls = "c-ok",
            frac = (tr - 10) / 20, desc = symDesc("pokoj", "komfort cieplny"),
            badge = badge("pokoj")?.first ?: "LIVE", badgeCls = badge("pokoj")?.second ?: "live",
            sim = S.symAktywna("pokoj"), stale = stale, artKey = "pokoj"
        )
    )

    // ── cisnienie ────────────────────────────────────────────────────────────
    val pc = S.valOf("cisnienie", "cisnienie")
    out.add(
        TileSpec(
            id = "cisnienie", title = "Ciśnienie", grid = 1, value = Math.round(pc).toString(), unit = "hPa",
            cls = "c-fiolet", frac = (pc - 970) / 70,
            desc = symDesc("cisnienie", if (pc < 1000) "niż — możliwy gorszy ciąg" else if (pc > 1020) "wyż" else "stabilnie"),
            badge = badge("cisnienie")?.first ?: "LIVE", badgeCls = badge("cisnienie")?.second ?: "live",
            sim = S.symAktywna("cisnienie"), stale = stale, artKey = "cisnienie"
        )
    )

    // ── wilgotnosc ───────────────────────────────────────────────────────────
    val hu = S.valOf("wilgotnosc", "wilgotnosc")
    out.add(
        TileSpec(
            id = "wilgotnosc", title = "Wilgotność", grid = 1, value = Math.round(hu).toString(), unit = "%",
            cls = "c-flame2", frac = hu / 100,
            desc = symDesc("wilgotnosc", if (hu > 70) "podwyższona" else "w normie"),
            badge = badge("wilgotnosc")?.first ?: "LIVE", badgeCls = badge("wilgotnosc")?.second ?: "live",
            sim = S.symAktywna("wilgotnosc"), stale = stale, artKey = "wilgotnosc"
        )
    )

    // ── pompa ───────────────────────────────────────────────────────────────
    val stratBadge = if (S.pompa) {
        when (S.wybor) { 1 -> "⏱️ CZASOWY"; 2 -> "🌡️ TEMP."; else -> "⚡ AUTO" }
    } else "POSTÓJ"
    val stratDesc = when (S.wybor) {
        1 -> "⏱️ Czasowy: ${S.czasOn}m ON / ${S.czasOff}m OFF" + (if (S.pompa_override_min > 0) " (${S.pompa_override_min}m)" else "")
        2 -> "🌡️ Temp: ON ≥ ${S.tempOn}° / OFF ≤ ${S.tempOff}°" + (if (S.pompa_override_min > 0) " (${S.pompa_override_min}m)" else "")
        else -> "⚡ Automatyczny: start przy ${S.tempOn}°C" + (if (S.pompa_override_min > 0) " (${S.pompa_override_min}m)" else "")
    }
    out.add(
        TileSpec(
            id = "pompa", title = "Pompa", grid = 2, value = if (S.pompa) "WŁĄCZONA" else "WYŁĄCZONA",
            cls = "c-flame", state = if (S.pompa) "ok" else "", frac = if (S.pompa) 1.0 else 0.0,
            desc = stratDesc, badge = stratBadge, badgeCls = if (S.pompa) "live" else "stale",
            stale = stale, artKey = "pompa"
        )
    )

    // ── serwo ───────────────────────────────────────────────────────────────
    val klapaPct = Math.round(S.klapa * 100.0 / 180.0)
    val sybPct = Math.round(S.syberka * 100.0 / 90.0)
    out.add(
        TileSpec(
            id = "serwo", title = "Serwo", grid = 2,
            value = PiecState.TRYB_NAZWA[S.tryb_serwa] ?: "—",
            cls = if (S.tryb_serwa == 1) "c-flame" else "c-none",
            state = when (S.tryb_serwa) { 2 -> "warn"; 3 -> "err"; else -> "" },
            frac = klapaPct / 100.0,
            desc = "klapa ${S.klapa}° • syberka ${S.syberka}°" + (if (S.serwo_override_min > 0) " • ${S.serwo_override_min} min" else ""),
            badge = "K$klapaPct% S$sybPct%", badgeCls = "active", stale = stale, artKey = "serwo"
        )
    )

    // ── mieszadlo ────────────────────────────────────────────────────────────
    out.add(
        TileSpec(
            id = "mieszadlo", title = "Mieszadło", grid = 2,
            value = if (S.mieszadlo) "WŁĄCZONE" else "WYŁĄCZONE", cls = "c-ok",
            state = if (S.mieszadlo) "ok" else "", frac = if (S.mieszadlo) 1.0 else 0.0,
            desc = when {
                S.rozpalanie -> "override: rozpalanie"
                S.mieszadloWlaczony -> "cykl ${S.mieszadloCzasOn} s / ${S.mieszadloCzasOff} min"
                else -> "automatyka wyłączona"
            },
            badge = if (S.mieszadlo) "PRACUJE" else "POSTÓJ",
            badgeCls = if (S.mieszadlo) "live" else "stale", stale = stale, artKey = "mieszadlo"
        )
    )

    // ── dym ─────────────────────────────────────────────────────────────────
    val dymV = S.valOf("dym", "dym")
    val dymDis = !S.dym_wlaczony && !S.symAktywna("dym")
    out.add(
        TileSpec(
            id = "dym", title = "Czujnik dymu", grid = 2,
            value = if (dymDis) "OFF" else Math.round(dymV).toString(),
            unit = if (dymDis) "" else "ADC", cls = "c-none",
            state = if (dymDis) "dis" else if (S.dym_alarm) "err" else "",
            frac = if (dymDis) 0.0 else dymV / 4095,
            desc = when {
                S.symAktywna("dym") -> "symulacja (${S.sym["dym"]?.min ?: 0} min)"
                dymDis -> "piec zimny — czujnik wyłączony"
                S.dym_alarm -> "próg ${S.progAlarmDym} ADC przekroczony"
                S.dym_swiezy -> "próg alarmu ${S.progAlarmDym}"
                else -> "ostatni pomiar"
            },
            badge = if (S.symAktywna("dym")) "SYMULACJA " + (S.sym["dym"]?.min ?: 0) + "M"
            else if (dymDis) "WYŁ." else if (S.dym_alarm) "ALARM" else "OK",
            badgeCls = if (S.symAktywna("dym")) "sim" else if (dymDis) "stale" else if (S.dym_alarm) "alarm" else "live",
            sim = S.symAktywna("dym"), stale = stale, artKey = "dym"
        )
    )

    return out
}
