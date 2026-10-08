package com.sterownikco.pro.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.core.HistPoint
import com.sterownikco.pro.ui.LocalModel
import com.sterownikco.pro.ui.art.ArtState
import com.sterownikco.pro.ui.art.Ilu
import com.sterownikco.pro.ui.icons.AppIcon
import com.sterownikco.pro.ui.svg.SvgView
import com.sterownikco.pro.ui.theme.Dimens
import com.sterownikco.pro.ui.theme.Pal
import com.sterownikco.pro.ui.theme.Txt

/* ══════════════════════════════════════════════════════════════════════════
   CHROME — pasek górny, pasek systemowy, banery, hero, kafelki szybkie,
   sekcje, trend, nawigacja, arkusz dolny i toast. Wszystkie klasy CSS
   (.topbar, .sysstrip, .hero, .quick, .qcard, .sect, .trend, .nav, .sheet,
   .toast) mają tu swój odpowiednik 1:1.
   ══════════════════════════════════════════════════════════════════════════ */

/** `.topbar` — wysokość 62, brand + ikony + chip stanu. */
@Composable
fun TopBar(m: AppModel) {
    Row(
        modifier = Modifier.fillMaxWidth().height(Dimens.topBarH)
            .background(Brush.linearGradient(listOf(Pal.Top, Pal.Top2)))
            .border(1.dp, Pal.TopBorder, RoundedCornerShape(0.dp))
            .statusBarsPadding()
            .padding(start = 14.dp, end = 10.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(Modifier.weight(1f)) {
            Text("STEROWNIK CO", style = Txt.topTitle)
            Text("CENTRALA • piec_co", style = Txt.topSub)
        }
        // (Przełącznik PC/Smartfon usunięty z APK — tylko w Piec.html.)
        Chip(m)
        IconBtn("more", "Menu sesji") { m.openSheet("sesja") }
    }
}

private data class ChipStyle(val text: String, val fg: Color, val bg: Color, val bd: Color)

@Composable
fun Chip(m: AppModel) {
    val st = when {
        m.connected -> ChipStyle("● LIVE", Pal.Live, Pal.rgba(74, 222, 128, .13f), Pal.rgba(74, 222, 128, .35f))
        else -> ChipStyle("○ BRAK SESJI", Pal.TextDim, Pal.rgba(127, 147, 163, .12f), Pal.rgba(127, 147, 163, .3f))
    }
    val (txt, fg, bg, bd) = st
    Box(
        modifier = Modifier.height(24.dp).background(bg, RoundedCornerShape(999.dp))
            .border(1.dp, bd, RoundedCornerShape(999.dp)).padding(horizontal = 9.dp),
        contentAlignment = Alignment.Center
    ) { Text(txt, style = Txt.chip, color = fg, maxLines = 1) }
}

@Composable
fun IconBtn(icon: String, title: String, tint: Color = Pal.TextDim, onClick: () -> Unit) {
    Box(
        modifier = Modifier.size(36.dp).background(Pal.rgba(255, 255, 255, .04f), CircleShape)
            .border(1.dp, Pal.Border, CircleShape).clickable { onClick() },
        contentAlignment = Alignment.Center
    ) {
        AppIcon(icon, size = 18.dp, tint = tint)
    }
}

/** `.sysstrip` — CENTRALA CO + podtytuł + pill stanu. */
@Composable
fun SysStrip(sub: String, state: String, stateCls: String, onClick: () -> Unit) {
    Row(
        modifier = Modifier.fillMaxWidth().background(Pal.Surface, RoundedCornerShape(14.dp))
            .border(1.dp, Pal.Border, RoundedCornerShape(14.dp)).clickable { onClick() }.padding(10.dp, 9.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(9.dp)
    ) {
        Box(
            modifier = Modifier.size(30.dp).background(Pal.rgba(0, 212, 245, .1f), RoundedCornerShape(10.dp))
                .border(1.dp, Pal.Border, RoundedCornerShape(10.dp)),
            contentAlignment = Alignment.Center
        ) { AppIcon("shield", size = 16.dp, tint = Pal.Cyan) }
        Column(Modifier.weight(1f)) {
            Text("CENTRALA CO", style = Txt.diagLbl)
            Text(sub, style = Txt.lead, maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
        val c = when (stateCls) { "err" -> Pal.Err; "warn" -> Pal.Warn; else -> Pal.Live }
        Box(
            modifier = Modifier.height(20.dp).background(c.copy(alpha = .14f), RoundedCornerShape(999.dp))
                .border(1.dp, c.copy(alpha = .4f), RoundedCornerShape(999.dp)).padding(horizontal = 8.dp),
            contentAlignment = Alignment.Center
        ) { Text(state, style = Txt.pill, color = c) }
    }
}

/** `.conn-banner` / `.alarmbanner` */
@Composable
fun Banner(text: String, kind: String, action: String? = null, onAction: (() -> Unit)? = null) {
    val c = if (kind == "alarm") Pal.Err else Pal.Warn
    Row(
        modifier = Modifier.fillMaxWidth().background(c.copy(alpha = .1f), RoundedCornerShape(13.dp))
            .border(1.dp, c.copy(alpha = .38f), RoundedCornerShape(13.dp)).padding(10.dp, 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        AppIcon("shield", size = 16.dp, tint = c)
        Text(text, style = Txt.cardDesc, color = Pal.Text, modifier = Modifier.weight(1f))
        if (action != null && onAction != null) {
            Box(
                modifier = Modifier.height(26.dp).background(c.copy(alpha = .18f), RoundedCornerShape(9.dp))
                    .border(1.dp, c.copy(alpha = .5f), RoundedCornerShape(9.dp)).clickable { onAction() }
                    .padding(horizontal = 9.dp),
                contentAlignment = Alignment.Center
            ) { Text(action, style = Txt.pill, color = c) }
        }
    }
}

/** `.hero` — temperatura + ilustracja kotła + 3 statystyki. */
@Composable
fun Hero(m: AppModel, art: ArtState, alarm: Boolean, heroSub: String) {
    Column(
        modifier = Modifier.fillMaxWidth()
            .background(Brush.linearGradient(listOf(Pal.HeroA, Pal.HeroB)), RoundedCornerShape(20.dp))
            .border(1.dp, Pal.BorderStrong, RoundedCornerShape(20.dp))
            .padding(start = 14.dp, end = 14.dp, top = 12.dp, bottom = 10.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("GŁÓWNY OBIEG KOTŁA", style = Txt.heroK)
                Spacer(Modifier.height(2.dp))
                Text(
                    (if (m.S.symAktywna("ogrz")) "~" else "") + m.S.fmt1(m.S.valOf("ogrz", "t_ogrz")) + " °C",
                    style = Txt.heroTemp,
                    color = if (alarm) Pal.Err else Pal.White
                )
                Text(heroSub, style = Txt.heroD, maxLines = 2, overflow = TextOverflow.Ellipsis)
            }
            Box(Modifier.size(100.dp)) {
                SvgView(
                    src = remember(art) { Ilu.heroBoiler(art) },
                    modifier = Modifier.fillMaxSize(),
                    animFor = { t -> Ilu.anim(t, art) }
                )
            }
        }
        Spacer(Modifier.height(8.dp))
        Row(modifier = Modifier.fillMaxWidth().background(Pal.rgba(8, 20, 33, .55f), RoundedCornerShape(13.dp)).padding(9.dp, 7.dp)) {
            HStat("TRYB SERWA", PiecTrybNazwa(m), if (!m.S.hasData("tryb_serwa")) Pal.TextDim else if (m.S.tryb_serwa == 2) Pal.Warn else if (m.S.tryb_serwa == 3) Pal.Err else Pal.White, Modifier.weight(1f))
            HStat("POMPA C.O.", if (m.S.hasData("pompa")) if (m.S.pompa) "ON" else "OFF" else "—", if (!m.S.hasData("pompa")) Pal.TextDim else if (m.S.pompa) Pal.Live else Pal.TextDim, Modifier.weight(1f))
            HStat("BEZPIECZEŃSTWO", alarmSummary(m), if (anyAlarm(m)) Pal.Err else if (m.S.hasAllData("dym_alarm", "alarm_ogrzewanie", "alarm_panel")) Pal.Live else Pal.TextDim, Modifier.weight(1f))
        }
    }
}

private fun PiecTrybNazwa(m: AppModel): String =
    if (m.S.hasData("tryb_serwa") && m.S.tryb_serwa in 1..3) {
        com.sterownikco.pro.core.PiecState.TRYB_NAZWA[m.S.tryb_serwa] ?: "—"
    } else "—"
fun anyAlarm(m: AppModel): Boolean =
    (m.S.hasData("dym_alarm") && m.S.dym_alarm) ||
        (m.S.hasData("alarm_ogrzewanie") && m.S.alarm_ogrzewanie) ||
        (m.S.hasData("alarm_panel") && m.S.alarm_panel)
private fun alarmSummary(m: AppModel): String = when {
    anyAlarm(m) -> "ALARM"
    m.S.hasAllData("dym_alarm", "alarm_ogrzewanie", "alarm_panel") -> "OK"
    else -> "—"
}

@Composable
private fun HStat(lbl: String, value: String, color: Color, modifier: Modifier = Modifier) {
    Column(modifier, verticalArrangement = Arrangement.spacedBy(2.dp)) {
        Text(lbl, style = Txt.hStatLbl, maxLines = 1)
        Text(value, style = Txt.hStatVal, color = color, maxLines = 1, overflow = TextOverflow.Ellipsis)
    }
}

/** `.qcard` — szybka karta 3-kolumnowa. */
@Composable
fun RowScope.QCard(title: String, icon: String, value: String, sub: String, state: String, onClick: () -> Unit) {
    val bd = when (state) {
        "ok" -> Pal.rgba(74, 222, 128, .45f); "warn" -> Pal.rgba(251, 191, 36, .5f)
        "err" -> Pal.rgba(255, 95, 120, .55f); else -> Pal.Border
    }
    val vc = when (state) { "ok" -> Pal.Live; "warn" -> Pal.Warn; "err" -> Pal.Err; else -> Pal.White }
    Column(
        modifier = Modifier.weight(1f).height(Dimens.quickH)
            .background(Pal.Surface, RoundedCornerShape(15.dp)).border(1.dp, bd, RoundedCornerShape(15.dp))
            .clickable { onClick() }.padding(9.dp, 8.dp),
        verticalArrangement = Arrangement.SpaceBetween
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(title, style = Txt.quickTitle, modifier = Modifier.weight(1f), maxLines = 1)
            AppIcon(icon, size = 15.dp, tint = Pal.Cyan)
        }
        Text(value, style = Txt.quickVal, color = vc, maxLines = 1, overflow = TextOverflow.Ellipsis)
        Text(sub, style = Txt.quickSub, maxLines = 1, overflow = TextOverflow.Ellipsis)
    }
}

/** `.sect` — pasek sekcji. */
@Composable
fun Sect(title: String) {
    Row(
        modifier = Modifier.fillMaxWidth().padding(top = 14.dp, bottom = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(9.dp)
    ) {
        Box(Modifier.width(3.dp).height(16.dp).background(Pal.Cyan, RoundedCornerShape(4.dp)))
        Text(title.uppercase(), style = Txt.sect)
        Box(Modifier.weight(1f).height(1.dp).background(Pal.rgba(62, 90, 112, .2f)))
    }
}

/** `.trend canvas` — mini-trend 60 rzeczywistych próbek (piec / bojler / zewn.). */
@Composable
fun Trend(hist: List<HistPoint>) {
    val ready = hist.size >= 2
    Column(
        modifier = Modifier.fillMaxWidth().padding(top = 10.dp)
            .background(Pal.rgba(21, 34, 52, .18f), RoundedCornerShape(Dimens.radiusCard))
            .border(1.dp, Pal.Border, RoundedCornerShape(Dimens.radiusCard))
            .padding(start = 14.dp, end = 14.dp, top = 12.dp, bottom = 10.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("TREND OSTATNICH POMIARÓW", style = Txt.tileTitle)
                Text("rzeczywiste odczyty • źródło: telemetria", style = Txt.tiny)
            }
            Text(if (ready) "${hist.size} HIST." else "BRAK DANYCH", style = Txt.badge, color = if (ready) Pal.Live else Pal.TextDim)
        }
        if (ready) {
            Canvas(Modifier.fillMaxWidth().height(Dimens.trendH).padding(top = 6.dp)) {
                drawTrendSeries(hist.map { it.o }, Color(0xFFFF9F43), size.width, 80f)
                drawTrendSeries(hist.map { it.b }, Color(0xFFFFD166), size.width, 80f)
                drawTrendSeries(hist.map { it.z }, Pal.Cyan, size.width, 80f)
            }
        } else {
            Box(Modifier.fillMaxWidth().height(Dimens.trendH), contentAlignment = Alignment.Center) {
                Text("Oczekiwanie na co najmniej 2 rzeczywiste próbki", style = Txt.note, color = Pal.TextDim)
            }
        }
    }
}

private fun DrawScope.drawTrendSeries(vals: List<Double>, color: Color, w: Float, h: Float) {
    if (vals.size < 2) return
    val mn = vals.min() - 1
    val mx = vals.max() + 1
    val range = (mx - mn).let { if (it == 0.0) 1.0 else it }
    val path = Path()
    vals.forEachIndexed { i, v ->
        val x = i.toFloat() / (vals.size - 1) * w
        val y = 72f - ((v - mn) / range).toFloat() * 60f
        if (i == 0) path.moveTo(x, y) else path.lineTo(x, y)
    }
    drawPath(path, color, style = Stroke(width = 1.6f))
    val last = vals.last()
    val ly = 72f - ((last - mn) / range).toFloat() * 60f
    drawCircle(color, radius = 2.4f, center = Offset(w - 2f, ly))
}

/** `.nav` — nawigacja dolna (5 zakładek jak PAGES w Piec.html). */
@Composable
fun NavBar(page: Int, onNav: (Int) -> Unit) {
    val items = listOf("dashboard" to "Dashboard", "chart" to "Wykresy", "weather" to "Pogoda", "settings" to "Ustawienia", "more" to "Więcej")
    Row(
        modifier = Modifier.fillMaxWidth().height(Dimens.navH)
            .background(Brush.linearGradient(listOf(Pal.Nav, Pal.Nav2)))
            .border(1.dp, Pal.NavBorder, RoundedCornerShape(0.dp))
            .padding(start = 9.dp, end = 9.dp, top = 6.dp, bottom = 9.dp),
        horizontalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        items.forEachIndexed { i, (ico, label) ->
            val on = i == page
            Column(
                modifier = Modifier.weight(1f).height(Dimens.navBtnH)
                    .background(if (on) Pal.rgba(0, 212, 245, .1f) else Color.Transparent, RoundedCornerShape(16.dp))
                    .border(1.dp, if (on) Pal.rgba(0, 212, 245, .25f) else Color.Transparent, RoundedCornerShape(16.dp))
                    .clickable { onNav(i) },
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.Center
            ) {
                Box(Modifier.size(24.dp), contentAlignment = Alignment.Center) {
                    AppIcon(ico, size = 22.dp, tint = if (on) Pal.Cyan else Pal.TextDim2)
                }
                Spacer(Modifier.height(2.dp))
                Text(label, style = Txt.navLabel, color = if (on) Pal.Cyan else Pal.TextDim2, maxLines = 1)
            }
        }
    }
}

/** `.sheet` — arkusz dolny (75% wys., radius 24, uchwyt 42×5). */
@Composable
fun Sheet(title: String, icon: String, onDismiss: () -> Unit, content: @Composable ColumnScope.() -> Unit) {
    Box(
        modifier = Modifier.fillMaxSize().background(Pal.rgba(3, 8, 15, .62f)).clickable { onDismiss() },
        contentAlignment = Alignment.BottomCenter
    ) {
        Column(
            modifier = Modifier.fillMaxWidth().fillMaxHeight(.75f)
                .background(Pal.Surface, RoundedCornerShape(topStart = Dimens.radiusSheet, topEnd = Dimens.radiusSheet))
                .border(1.dp, Pal.BorderStrong, RoundedCornerShape(topStart = Dimens.radiusSheet, topEnd = Dimens.radiusSheet))
                .clickable(enabled = false) {}
        ) {
            Box(Modifier.fillMaxWidth().padding(top = 8.dp), contentAlignment = Alignment.Center) {
                Box(Modifier.width(42.dp).height(5.dp).background(Pal.rgba(120, 150, 175, .35f), RoundedCornerShape(99.dp)))
            }
            Row(
                modifier = Modifier.fillMaxWidth().padding(start = 14.dp, end = 8.dp, top = 8.dp, bottom = 10.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                Box(
                    modifier = Modifier.size(36.dp).background(Pal.Surface2, RoundedCornerShape(12.dp))
                        .border(1.dp, Pal.Border, RoundedCornerShape(12.dp)),
                    contentAlignment = Alignment.Center
                ) { AppIcon(icon, size = 19.dp, tint = Pal.Cyan) }
                Text(title, style = Txt.sheetTitle, modifier = Modifier.weight(1f), maxLines = 1, overflow = TextOverflow.Ellipsis)
                Box(Modifier.size(30.dp).clickable { onDismiss() }, contentAlignment = Alignment.Center) {
                    AppIcon("close", size = 17.dp, tint = Pal.TextDim)
                }
            }
            Column(
                Modifier.weight(1f).fillMaxWidth().verticalScroll(rememberScrollState())
                    .padding(start = 14.dp, end = 14.dp, bottom = 20.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp),
                content = content
            )
        }
    }
}

/** `.toast` — potwierdzenie polecenia (wait / ok / err). */
@Composable
fun Toast(text: String, stage: String, cls: String) {
    val c = when (cls) { "err" -> Pal.Err; "wait" -> Pal.Cyan; "warn" -> Pal.Warn; else -> Pal.Live }
    Row(
        modifier = Modifier.padding(bottom = 88.dp).widthIn(min = 260.dp, max = 380.dp)
            .background(Pal.Surface2.copy(alpha = .97f), RoundedCornerShape(14.dp))
            .border(1.dp, c.copy(alpha = .5f), RoundedCornerShape(14.dp)).padding(11.dp, 10.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(9.dp)
    ) {
        Box(Modifier.size(22.dp).background(c.copy(alpha = .16f), RoundedCornerShape(8.dp)), contentAlignment = Alignment.Center) {
            AppIcon(when (cls) { "err" -> "cross"; "wait" -> "refresh"; "warn" -> "info"; else -> "check" }, size = 13.dp, tint = c)
        }
        Column(Modifier.weight(1f)) {
            // `.c` — samo polecenie (mono, tekst-dim), potem `.s` — status
            Text(text, style = Txt.monoSm, color = Pal.TextDim, maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text(stage, style = Txt.pill, color = c)
        }
    }
}
