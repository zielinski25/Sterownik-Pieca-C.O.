package com.sterownikco.pro.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.core.buildTiles
import com.sterownikco.pro.ui.components.Banner
import com.sterownikco.pro.ui.components.Hero
import com.sterownikco.pro.ui.components.QCard
import com.sterownikco.pro.ui.components.Sect
import com.sterownikco.pro.ui.components.SysStrip
import com.sterownikco.pro.ui.components.Trend
import com.sterownikco.pro.ui.components.anyAlarm
import com.sterownikco.pro.ui.icons.AppIcon
import com.sterownikco.pro.ui.theme.Dimens
import com.sterownikco.pro.ui.theme.Pal
import com.sterownikco.pro.ui.theme.Txt
import com.sterownikco.pro.ui.components.Tile

/* ══════════════════════════════════════════════════════════════════════════
   PULPIT — `#pg-pulpit` z Piec.html: sysstrip → banery → hero → quick →
   POMIARY (g1) → STEROWANIE (g2) → ANALIZA I SYSTEM → TREND.
   ══════════════════════════════════════════════════════════════════════════ */

@Composable
fun Dashboard(m: AppModel) {
    val rev = m.S.rev.intValue // subskrypcja zmian stanu
    val S = m.S
    val weatherCode = m.weather?.current?.code ?: (if (S.valOf("zewn", "t_zewn") < 0) 71 else 0)
    val isDay = m.weather?.current?.isDay ?: (!S.night)
    val art = remember(rev, weatherCode, isDay) { S.art(weatherCode, isDay) }
    val tiles = remember(rev, weatherCode, isDay) { buildTiles(m, weatherCode, isDay) }
    val to = S.valOf("ogrz", "t_ogrz")
    val tb = S.valOf("bojler", "t_bojler")
    val anyAl = anyAlarm(m)

    Column(
        modifier = Modifier.fillMaxWidth().verticalScroll(rememberScrollState())
            .padding(start = Dimens.pagePadH, end = Dimens.pagePadH, top = Dimens.pagePad, bottom = 16.dp),
        verticalArrangement = Arrangement.spacedBy(Dimens.gap)
    ) {
        SysStrip(
            sub = m.sysStripSub(),
            state = if (!S.online) "BRAK ŁĄCZNOŚCI" else if (anyAl) "ALARM" else if (to > 45) "GRZANIE" else "CZUWANIE",
            stateCls = if (!S.online) "warn" else if (anyAl) "err" else "live",
            onClick = { m.openSheet("sesja") }
        )
        if (!m.connected) {
            Banner(
                text = "Brak połączenia z bazą Firebase — połącz na żywo",
                kind = "warn",
                action = "🔑 POŁĄCZ",
                onAction = { m.authOpen = true }
            )
        }
        if (anyAl && S.online) {
            Banner(
                text = when {
                    S.dym_alarm -> "ALARM DYMU — sprawdź kotłownię"
                    S.alarm_ogrzewanie -> "PRZEGRZANIE PIECA — " + S.fmt1(to) + " °C"
                    else -> "PRZEGRZANIE PANELU — " + S.fmt1(tb) + " °C"
                },
                kind = "alarm",
                onAction = null
            )
        }
        Hero(m, art, alarm = S.alarm_ogrzewanie, heroSub = "temperatura pieca C.O. · " + (if (to > 70) "grzanie na maks." else "odczyt stabilny"))

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            QCard(
                title = "POMPA", icon = "pump",
                value = if (S.pompa) "WŁĄCZONA" else "WYŁĄCZONA",
                sub = if (S.wybor == 1) "⏱️ Czasowy" else if (S.wybor == 2) "🌡️ Temp." else "⚡ Auto",
                state = if (S.pompa) "ok" else "",
                onClick = { m.openSheet("pompa") }
            )
            QCard(
                title = "SERWO", icon = "servo",
                value = when (S.tryb_serwa) {
                    1 -> "AUTO (" + Math.round(S.klapa * 100.0 / 180.0) + "%)"
                    2 -> "RĘCZNY (" + Math.round(S.klapa * 100.0 / 180.0) + "%)"
                    3 -> "BEZPIECZNY"
                    else -> "AUTO"
                },
                sub = if (S.tryb_serwa == 3) "klapa 0° (stop)" else "klapa ${S.klapa}°",
                state = when (S.tryb_serwa) { 1 -> "ok"; 2 -> "warn"; 3 -> "err"; else -> "" },
                onClick = { m.openSheet("serwo") }
            )
            QCard(
                title = "MIESZADŁO", icon = "mixer",
                value = if (S.mieszadlo) "WŁĄCZONE" else "WYŁĄCZONE",
                sub = if (S.mieszadlo) "praca aktywna" else "ster. ręczne",
                state = if (S.mieszadlo) "ok" else "",
                onClick = { m.openSheet("mieszadlo") }
            )
        }

        Sect("Pomiary")
        TileGrid(tiles.filter { it.grid == 1 }, m, art)
        Sect("Sterowanie · bezpieczeństwo")
        TileGrid(tiles.filter { it.grid == 2 }, m, art)
        Sect("Analiza i system")

        AnalysisCard(
            accent = Pal.Cyan, icon = "chart", title = "WYKRESY",
            sub = "historia • 6 H / 24 H / 7 DNI / 30 DNI",
            onClick = { m.page = 1 }
        )
        AnalysisCard(
            accent = Color(0xFFB7C4D4), icon = "clock",
            title = String.format(java.util.Locale.US, "%02d:%02d",
                java.util.Calendar.getInstance().get(java.util.Calendar.HOUR_OF_DAY),
                java.util.Calendar.getInstance().get(java.util.Calendar.MINUTE)),
            sub = if (S.rtc_ok) "${S.dzien}.${S.miesiac}.${S.rok}" else "RTC niegotowy",
            onClick = { m.openSheet("czas") }
        )

        Trend(S.hist.toList())
    }
}

@Composable
private fun TileGrid(list: List<com.sterownikco.pro.core.TileSpec>, m: AppModel, art: com.sterownikco.pro.ui.art.ArtState) {
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        list.chunked(2).forEach { row ->
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                row.forEach { spec ->
                    Box(Modifier.weight(1f)) {
                        Tile(spec = spec, art = art, onClick = { m.openSheet(m.menuForTile(spec.id)) })
                    }
                }
                if (row.size == 1) Spacer(Modifier.weight(1f))
            }
        }
    }
}

/** `.acard` — karta analizy (74 px, ikona + tytuł + opis + strzałka). */
@Composable
fun AnalysisCard(accent: Color, icon: String, title: String, sub: String, onClick: () -> Unit) {
    Row(
        modifier = Modifier.fillMaxWidth().height(Dimens.analysisH)
            .background(Pal.Surface, RoundedCornerShape(Dimens.radiusTile))
            .border(1.dp, Pal.Border, RoundedCornerShape(Dimens.radiusTile))
            .clickable { onClick() }.padding(start = 12.dp, end = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        Box(
            modifier = Modifier.size(34.dp).background(accent.copy(alpha = .12f), RoundedCornerShape(11.dp)),
            contentAlignment = Alignment.Center
        ) { AppIcon(icon, size = 18.dp, tint = accent) }
        Column(Modifier.weight(1f)) {
            Text(title, style = Txt.cardTitle, maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text(sub, style = Txt.cardDesc, maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
        AppIcon("next", size = 16.dp, tint = Pal.TextDim2)
    }
}
