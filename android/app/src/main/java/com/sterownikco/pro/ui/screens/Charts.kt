package com.sterownikco.pro.ui.screens

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.ui.chart.ChartCanvas
import com.sterownikco.pro.ui.chart.buildChartView
import com.sterownikco.pro.ui.chart.hexColor
import com.sterownikco.pro.ui.components.CheckRow
import com.sterownikco.pro.ui.components.IconBtn
import com.sterownikco.pro.ui.components.Note
import com.sterownikco.pro.ui.components.SectionHeader
import com.sterownikco.pro.ui.components.SetCard
import com.sterownikco.pro.ui.components.Seg
import com.sterownikco.pro.ui.components.UstBtn
import com.sterownikco.pro.ui.icons.AppIcon
import com.sterownikco.pro.ui.screens.sheets.DiagCard
import com.sterownikco.pro.ui.theme.Dimens
import com.sterownikco.pro.ui.theme.Pal
import com.sterownikco.pro.ui.theme.Txt

/* ══════════════════════════════════════════════════════════════════════════
   WYKRESY TELEMETRII — `#pg-wykresy` (Piec.html 3152-3230) + arkusze
   `showSeriesSheet` / `showModeSheet` / `showChartToolsSheet` /
   `showChartAnalysisSheet` (7846-8060).
   ══════════════════════════════════════════════════════════════════════════ */

/** `.chart-chip` (`.on` = cyan 16 % + poświata). */
@Composable
private fun RowScope.ChartChip(label: String, on: Boolean, modifier: Modifier = Modifier, onClick: () -> Unit) {
    Box(
        modifier.weight(1f).heightIn(min = 36.dp).background(
            if (on) Pal.rgba(0, 212, 245, .16f) else Pal.Surface2, RoundedCornerShape(8.dp)
        ).border(BorderStroke(1.dp, if (on) Pal.Cyan else Pal.Border), RoundedCornerShape(8.dp))
            .clickable { onClick() }.padding(horizontal = 4.dp, vertical = 6.dp),
        contentAlignment = Alignment.Center
    ) {
        Text(label, fontSize = 9.sp, fontWeight = FontWeight.Bold,
            color = if (on) Color.White else Pal.TextDim, textAlign = androidx.compose.ui.text.style.TextAlign.Center,
            maxLines = 1, softWrap = false, overflow = TextOverflow.Ellipsis)
    }
}

/** `.series-chip` — kropka w kolorze serii + etykieta. */
@Composable
private fun SeriesChip(label: String, accentHex: String, on: Boolean, onClick: () -> Unit) {
    val c = hexColor(accentHex)
    Row(
        Modifier.background(if (on) c.copy(alpha = .12f) else Pal.Surface2, RoundedCornerShape(12.dp))
            .border(BorderStroke(1.dp, if (on) c.copy(alpha = .7f) else Pal.Border), RoundedCornerShape(12.dp))
            .clickable { onClick() }.padding(horizontal = 9.dp, vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        Box(Modifier.size(6.dp).background(c, RoundedCornerShape(50)))
        Text(label, fontSize = 8.5.sp, fontWeight = FontWeight.Bold, color = if (on) Color.White else Pal.TextDim)
    }
}

@Composable
fun ChartsPage(m: AppModel) {
    val rev = m.sheetTick
    val dataset = remember(m.telemetry.size, m.chartLive.size, m.chartPoints, rev) { m.chartDataset() }
    val cat = m.catalogOn()
    val view = remember(dataset, cat, m.chartZoom, m.chartOffset, m.chartFocus, m.chartMode, m.chartGlitch) {
        buildChartView(dataset, cat, m.chartZoom, m.chartOffset, m.chartFocus, m.chartMode, m.chartGlitch)
    }
    var cross by remember { mutableStateOf<Int?>(null) }
    var expand by remember { mutableStateOf(false) }

    Column(
        Modifier.fillMaxWidth().verticalScroll(rememberScrollState())
            .padding(start = Dimens.pagePadH, end = Dimens.pagePadH, top = Dimens.pagePad, bottom = 16.dp),
        verticalArrangement = Arrangement.spacedBy(Dimens.gap)
    ) {
        PageHead("WYKRESY TELEMETRII", "historia pomiarów · analiza cykli · korelacja",
            action = { IconBtn("refresh", "Odśwież historię", onClick = { m.loadRange(m.rangeSec) }) })

        // `.chart-status-rail`
        Row(
            Modifier.fillMaxWidth().background(Pal.Surface, RoundedCornerShape(12.dp))
                .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(12.dp)).padding(horizontal = 12.dp, vertical = 6.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            AppIcon("clock", size = 16.dp, tint = Pal.Cyan)
            Text(m.chartStatus, style = Txt.sect.copy(fontSize = 9.5.sp, fontWeight = FontWeight.SemiBold,
                letterSpacing = 0.sp, color = Pal.TextDim), modifier = Modifier.weight(1f))
            Text(m.chartReality, fontSize = 8.sp, fontWeight = FontWeight.ExtraBold, color = Pal.Cyan,
                modifier = Modifier.background(Pal.rgba(0, 212, 245, .12f), RoundedCornerShape(10.dp))
                    .border(BorderStroke(1.dp, Pal.rgba(0, 212, 245, .35f)), RoundedCornerShape(10.dp))
                    .padding(horizontal = 8.dp, vertical = 3.dp))
        }
        // `.chart-ctrl-card`
        Column(
            Modifier.fillMaxWidth().background(Pal.Surface, RoundedCornerShape(Dimens.radiusCard))
                .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(Dimens.radiusCard)).padding(10.dp, 12.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            ChartRow("DANE") {
                ChartChip("TEMPERATURY", m.chartFocus == 0) { m.setChartFocus(0) }
                ChartChip("SERWA", m.chartFocus == 1) { m.setChartFocus(1) }
                ChartChip("KORELACJA", m.chartFocus == 2) { m.setChartFocus(2) }
            }
            ChartRow("ZAKRES") {
                ChartChip("6 H", m.rangeSec == 6 * 3600) { m.setRange(6 * 3600) }
                ChartChip("24 H", m.rangeSec == 24 * 3600) { m.setRange(24 * 3600) }
                ChartChip("7 DNI", m.rangeSec == 7 * 86400) { m.setRange(7 * 86400) }
                ChartChip("30 DNI", m.rangeSec == 30 * 86400) { m.setRange(30 * 86400) }
            }
        }

        // `.chart-workspace`
        Column(
            Modifier.fillMaxWidth().background(Pal.Surface, RoundedCornerShape(Dimens.radiusTile))
                .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(Dimens.radiusTile)).padding(12.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Column(Modifier.weight(1f)) {
                    Text(m.chartTitle(), style = Txt.cardTitle)
                    Text(m.chartMeta(), style = Txt.cardDesc)
                }
                Text(m.chartNowStat(), fontSize = 8.5.sp, fontWeight = FontWeight.ExtraBold, color = Pal.Cyan,
                    modifier = Modifier.background(Pal.rgba(0, 212, 245, .12f), RoundedCornerShape(10.dp))
                        .border(BorderStroke(1.dp, Pal.rgba(0, 212, 245, .25f)), RoundedCornerShape(10.dp))
                        .padding(horizontal = 9.dp, vertical = 4.dp))
                IconBtn("reset", "Resetuj widok (1×)", onClick = { m.resetChartView() })
                IconBtn("expand", "Pełny ekran", onClick = { expand = true })
            }

            Row(
                Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                m.activeCatalog().forEach { s ->
                    SeriesChip(label = s.label, accentHex = s.accent, on = m.seriesOn[s.id] ?: s.on,
                        onClick = { m.toggleSeries(s.id) })
                }
                SeriesChip(label = "USTAWIENIA", accentHex = "#00d4f5", on = false,
                    onClick = { m.openSheet("chartSeries") })
            }

            Box(
                Modifier.fillMaxWidth().height(Dimens.chartH)
                    .clip(RoundedCornerShape(14.dp))
                    .background(Pal.CanvasBg, RoundedCornerShape(14.dp))
                    .border(BorderStroke(1.dp, Color(0x0FFFFFFF)), RoundedCornerShape(14.dp))
            ) {
                ChartCanvas(
                    view = view,
                    modifier = Modifier.fillMaxWidth().height(Dimens.chartH),
                    areaBand = m.chartAreaBand,
                    alarmLines = m.chartAlarmLines && m.chartFocus == 0,
                    alarmLevels = m.alarmLevels,
                    mode = m.chartMode,
                    lineStyle = m.chartStyle,
                    crossIdx = cross,
                    onCross = { cross = it },
                    onPan = { m.panChart(it) },
                    onPinch = { f, x -> m.pinchChart(f, x) },
                    onResetView = { m.resetChartView() }
                )
                cross?.let { index ->
                    ChartHoverTooltip(m, view, index, Modifier.align(Alignment.TopStart))
                }
            }

            ChartScrollbar(m)
            Text(
                "Linia ciągła — odczyt rzeczywisty · kreskowany jest tylko odcinek kanału symulowanego",
                style = Txt.tiny.copy(color = Pal.TextDim), modifier = Modifier.fillMaxWidth()
            )

            Box(Modifier.fillMaxWidth().background(Pal.rgba(255, 159, 67, .1f), RoundedCornerShape(8.dp))
                .border(BorderStroke(1.dp, Pal.rgba(255, 159, 67, .25f)), RoundedCornerShape(8.dp))
                .padding(horizontal = 10.dp, vertical = 5.dp)) {
                Text(m.chartPhase(), fontSize = 8.5.sp, fontWeight = FontWeight.ExtraBold, color = Pal.Accent)
            }

            Row(Modifier.fillMaxWidth().background(Pal.Surface2, RoundedCornerShape(8.dp)).padding(horizontal = 10.dp, vertical = 6.dp),
                verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("INSPEKTOR", fontSize = 8.sp, fontWeight = FontWeight.ExtraBold, color = Pal.Cyan, letterSpacing = 0.5.sp)
                Text(inspectorText(m, view, cross), style = Txt.monoSm.copy(fontSize = 8.5.sp, color = Color.White))
            }

            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Kpi("ZAKRES", view.span, Pal.Accent)
                Kpi("PUNKTY", dataset.size.toString(), Color(0xFF4ADE80))
                Kpi("ZOOM", m.chartZoomStat(), Color(0xFF00D4F5))
            }

            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                ActBtn(Pal.Cyan, "chart", "SERIE", "wybór") { m.openSheet("chartSeries") }
                ActBtn(Pal.Accent, "thermo", "OŚ", "skala") { m.openSheet("chartAxis") }
                ActBtn(Pal.Live, "shield", "FILTRY", "anomalia") { m.openSheet("chartTools") }
                ActBtn(Pal.Violet, "settings", "ANALIZA", "cykle A−B") { m.openSheet("chartAnalysis") }
            }
        }

        Row(Modifier.fillMaxWidth().padding(horizontal = 4.dp, vertical = 6.dp),
            verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            AppIcon("info", size = 12.dp, tint = Pal.TextDim2)
            Text("dotknij = kursor • szczypnij = zoom • pasek/przeciągnij = oś czasu • 2× tap = reset",
                style = Txt.tiny)
        }
    }

    if (expand) {
        Dialog(onDismissRequest = { expand = false }, properties = DialogProperties(usePlatformDefaultWidth = false)) {
            Box(Modifier.fillMaxSize()
                .background(Pal.rgba(3, 8, 15, .92f))
                .padding(14.dp),
                contentAlignment = Alignment.Center
            ) {
                Column(
                    Modifier.fillMaxWidth().background(Pal.Surface, RoundedCornerShape(20.dp))
                        .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(20.dp)).padding(14.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                        Column(Modifier.weight(1f)) {
                            Text(m.fsTitle(), fontSize = 18.sp, fontWeight = FontWeight.Bold, color = Color.White)
                            Text(m.fsSubtitle(), style = Txt.cardDesc)
                        }
                        IconBtn("close", "Zamknij", onClick = { expand = false })
                    }
                    Box(Modifier.fillMaxWidth().height(320.dp).clip(RoundedCornerShape(12.dp)).background(Pal.CanvasBg, RoundedCornerShape(12.dp))
                        .border(BorderStroke(1.dp, Color(0x0FFFFFFF)), RoundedCornerShape(12.dp))) {
                        ChartCanvas(
                            view = view, modifier = Modifier.fillMaxWidth().height(320.dp),
                            areaBand = m.chartAreaBand, alarmLines = m.chartAlarmLines && m.chartFocus == 0, alarmLevels = m.alarmLevels,
                            mode = m.chartMode, lineStyle = m.chartStyle, crossIdx = cross, onCross = { cross = it },
                            onPan = { m.panChart(it) }, onPinch = { f, x -> m.pinchChart(f, x) },
                            onResetView = { m.resetChartView() }
                        )
                        cross?.let { index ->
                            ChartHoverTooltip(m, view, index, Modifier.align(Alignment.TopStart))
                        }
                    }
                    ChartScrollbar(m)
                    Text("Linia ciągła — odczyt rzeczywisty · kreskowany — odcinek dotykający próbki symulowanej",
                        style = Txt.tiny, modifier = Modifier.fillMaxWidth())
                    Text("DOTKNIJ kursor • SZCZYP zoom • PASEK / PRZECIĄGNIJ oś czasu • 2× TAP reset",
                        style = Txt.tiny, modifier = Modifier.fillMaxWidth().padding(top = 4.dp),
                        textAlign = androidx.compose.ui.text.style.TextAlign.Center)
                }
            }
        }
    }
}

/**
 * Pasek pozycji pod wykresem: tor = caly zakres, kciuk = widoczne okno.
 * Tap = skok srodkiem okna, drag = przesuwanie. Przy zoom 1× kciuk = calosc.
 */
@Composable
private fun ChartScrollbar(m: AppModel) {
    val (start, end, total) = m.chartWindow()
    if (total < 2) return
    val leftF = start.toFloat() / total
    val rightF = end.toFloat() / total
    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(3.dp)) {
        BoxWithConstraints(
            Modifier.fillMaxWidth().height(30.dp)
                .background(Pal.Surface2, RoundedCornerShape(8.dp))
                .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(8.dp))
                .pointerInput(total) {
                    detectTapGestures(onTap = { pos ->
                        m.seekChartCentered((pos.x / size.width).coerceIn(0f, 1f))
                    })
                }
                .pointerInput(total) {
                    detectHorizontalDragGestures { change, dx ->
                        change.consume()
                        m.seekChartBy(dx / size.width)
                    }
                }
        ) {
            val trackW = maxWidth - 12.dp
            Box(Modifier.align(Alignment.CenterStart).offset(x = 6.dp).width(trackW).height(6.dp)
                .background(Color(0x14FFFFFF), RoundedCornerShape(3.dp)))
            val thumbW = (trackW * (rightF - leftF).coerceAtLeast(0.04f)).coerceAtLeast(22.dp)
            val thumbX = (6.dp + trackW * leftF.coerceIn(0f, 1f)).coerceAtMost(6.dp + trackW - thumbW)
            Box(Modifier.align(Alignment.CenterStart).offset(x = thumbX).width(thumbW).height(12.dp)
                .background(Pal.rgba(0, 212, 245, .55f), RoundedCornerShape(6.dp))
                .border(BorderStroke(1.dp, Pal.Cyan), RoundedCornerShape(6.dp)))
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text(winTime(m, start), style = Txt.tiny)
            Text(winTime(m, end - 1), style = Txt.tiny)
        }
    }
}

private fun winTime(m: AppModel, idx: Int): String {
    val r = m.chartDataset().getOrNull(idx) ?: return "—"
    val c = java.util.Calendar.getInstance().apply { timeInMillis = r.ts }
    return String.format(
        java.util.Locale.US, "%02d.%02d %02d:%02d",
        c.get(java.util.Calendar.DAY_OF_MONTH), c.get(java.util.Calendar.MONTH) + 1,
        c.get(java.util.Calendar.HOUR_OF_DAY), c.get(java.util.Calendar.MINUTE)
    )
}

@Composable
private fun ChartRow(label: String, chips: @Composable RowScope.() -> Unit) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
        Text(label, fontSize = 9.sp, fontWeight = FontWeight.ExtraBold, color = Pal.TextDim, letterSpacing = 0.5.sp,
            modifier = Modifier.width(48.dp))
        Row(Modifier.weight(1f), horizontalArrangement = Arrangement.spacedBy(6.dp), content = chips)
    }
}

@Composable
private fun RowScope.Kpi(t: String, v: String, c: Color) {
    Column(
        Modifier.weight(1f).background(Pal.Surface2, RoundedCornerShape(10.dp))
            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(10.dp)).padding(6.dp, 8.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text(t, fontSize = 7.5.sp, fontWeight = FontWeight.Bold, color = Pal.TextDim)
        Text(v, fontSize = 11.sp, fontWeight = FontWeight.ExtraBold, color = c, modifier = Modifier.padding(top = 2.dp))
    }
}

@Composable
private fun RowScope.ActBtn(tint: Color, icon: String, title: String, desc: String, onClick: () -> Unit) {
    Column(
        Modifier.weight(1f).background(Pal.Surface2, RoundedCornerShape(10.dp))
            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(10.dp))
            .clickable { onClick() }.padding(horizontal = 4.dp, vertical = 6.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Box(Modifier.size(18.dp).padding(bottom = 2.dp), contentAlignment = Alignment.Center) {
            AppIcon(icon, size = 18.dp, tint = tint)
        }
        Text(title, fontSize = 8.5.sp, fontWeight = FontWeight.ExtraBold, color = Color.White)
        Text(desc, fontSize = 7.5.sp, color = Pal.TextDim)
    }
}

/* ─────────────────────  arkusze wykresów  ───────────────────── */

/** `showSeriesSheet()` — wybór widocznych przebiegów. */
@Composable
fun ChartSeriesSheet(m: AppModel) {
    Note("Wybierz widoczne przebiegi na wykresie. Przynajmniej jedna seria musi pozostać aktywna.")
    m.activeCatalog().forEach { s ->
        val on = m.seriesOn[s.id] ?: s.on
        Row(Modifier.fillMaxWidth().padding(vertical = 5.dp), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("● " + s.label, style = Txt.rowLabel.copy(color = hexColor(s.accent)))
                Text(if (s.servo) "Pozycja serwomechanizmu (%)" else "Kanał telemetryczny (${s.unit})", style = Txt.rowDesc)
            }
            Box(Modifier.size(24.dp).background(if (on) Pal.Cyan else Pal.Surface2, RoundedCornerShape(7.dp))
                .border(BorderStroke(1.dp, if (on) Pal.Cyan else Pal.BorderStrong), RoundedCornerShape(7.dp))
                .clickable { m.toggleSeries(s.id) }, contentAlignment = Alignment.Center) {
                if (on) AppIcon("check", size = 15.dp, tint = Color(0xFF00131F))
            }
        }
    }
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        UstBtn("Tylko główna", modifier = Modifier.weight(1f), onClick = { m.seriesOnlyMain() })
        UstBtn("Wszystkie", modifier = Modifier.weight(1f), onClick = { m.seriesAllOn() })
    }
}

/** `showModeSheet()` — COMMON / NORMALIZED. */
@Composable
fun ChartAxisSheet(m: AppModel) {
    Note("Wybierz tryb prezentacji osi. Zmiana działa natychmiast na aktualnych danych.")
    listOf(
        "COMMON" to ("Wspólna oś" to "Jedna skala (°C) — najlepsza dla porównania temperatur instalacji"),
        "NORMALIZED" to ("Normalizowane 0–100%" to "Każda seria zoptymalizowana w pełnym zakresie wysokości")
    ).forEach { (mode, pair) ->
        SetCard(
            icon = "chart", title = pair.first,
            desc = pair.second + (if (m.chartMode == mode) "  ·  ✓" else ""),
            tint = Pal.Cyan, onClick = { m.setChartMode(mode) }
        )
    }
    SectionHeader("Styl linii")
    Seg(listOf("SMOOTH" to "Gładkie (Bezier)", "STEPPED" to "Schodkowe"), current = m.chartStyle) { v -> m.setChartStyle(v) }
}

/** `showChartToolsSheet()` — filtry anomalii, area band, bezier i poziomy odniesienia. */
@Composable
fun ChartToolsSheet(m: AppModel) {
    Note("Filtry zmieniają wygląd wykresu; dane w Firebase pozostają bez zmian.")
    CheckRow("Filtr anomalii i szpilek (Hampel / Glitch)", null, m.chartGlitch) { v -> m.setChartGlitch(v) }
    CheckRow("Wypełnienie gradientowe (Area Band)", null, m.chartAreaBand) { v -> m.setChartAreaBand(v) }
    CheckRow("Gładkie linie Bezier (odznacz: schodkowe)", null, m.chartStyle == "SMOOTH") { v -> m.setChartStyle(if (v) "SMOOTH" else "STEPPED") }
    CheckRow("Poziome linie progów temperatury", null, m.chartAlarmLines) { v -> m.setChartAlarmLines(v) }
    SectionHeader("Poziomy odniesienia · wykres temperatur")
    Note("Dla wszystkich widocznych serii temperatur na wspólnej osi (°C). Linie odniesienia, nie alarmy sterownika; zapis po puszczeniu suwaka.")
    listOf(
        Triple("lolo", "LOLO · bardzo niski", 25.0),
        Triple("lo", "LO · niski", 35.0),
        Triple("hi", "HI · wysoki", 75.0),
        Triple("hihi", "HIHI · bardzo wysoki", 85.0)
    ).forEach { (key, label, default) ->
        AlarmThresholdRow(m, key, label, default)
    }
}

@Composable
private fun AlarmThresholdRow(m: AppModel, key: String, label: String, default: Double) {
    var value by remember(key) { mutableFloatStateOf((m.alarmLevels[key] ?: default).toFloat()) }
    Column(Modifier.fillMaxWidth().padding(vertical = 1.dp)) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Text(label, fontSize = 9.5.sp, fontWeight = FontWeight.Bold, color = Color.White,
                modifier = Modifier.weight(1f))
            Text("${value.toInt()} °C", fontSize = 10.sp, fontWeight = FontWeight.ExtraBold, color = Pal.White)
        }
        Slider(
            value = value,
            onValueChange = { value = it },
            onValueChangeFinished = { m.setAlarmLevel(key, value.toDouble()) },
            valueRange = 0f..120f,
            steps = 119,
            colors = SliderDefaults.colors(thumbColor = Pal.Cyan, activeTrackColor = Pal.Cyan, inactiveTrackColor = Pal.Surface3)
        )
    }
}

/** `showChartAnalysisSheet()` — statystyki serii, cykle grzania, różnica A−B. */
@Composable
fun ChartAnalysisSheet(m: AppModel) {
    val dataset = remember { m.chartDataset() }
    Note("Analiza liczona z pobranych próbek telemetrycznych. Statystyki min/max, średnie, cykle grzania i różnice A−B.")
    m.catalogOn().forEach { s ->
        val vals = dataset.mapNotNull { com.sterownikco.pro.core.ChartSeries.value(it, s.ch) }.filter { it.isFinite() }
        if (vals.isEmpty()) return@forEach
        val mn = vals.min(); val mx = vals.max()
        val avg = vals.sum() / vals.size
        val delta = vals.last() - vals.first()
        DiagCard(
            lbl = "● " + s.label.uppercase() + " (" + s.unit + ")",
            value = "MIN ${fmt1(mn)} ${s.unit}  ·  MAX ${fmt1(mx)} ${s.unit}",
            sub = "Średnia: ${fmt1(avg)} ${s.unit} · Zmiana okresu: ${(if (delta >= 0) "+" else "") + fmt1(delta)} ${s.unit}"
        )
    }
    SectionHeader("Wykryte cykle pracy kotła")
    val pumpThresholdsKnown = m.S.hasAllData("tempOn", "tempOff")
    DiagCard(
        lbl = "ANALIZA CYKLI GRZANIA",
        value = "Cykliczna praca paleniska w wybranym zakresie",
        sub = if (pumpThresholdsKnown) {
            "Próg załączenia pompy: ${m.S.tempOn}°C (histereza ${m.S.tempOn - m.S.tempOff}°C)"
        } else {
            "Progi pracy pompy: brak odczytu z centrali"
        }
    )
    val cat = m.catalogOn()
    if (cat.size >= 2 && m.chartFocus == 0) {
        val s1 = cat[0]; val s2 = cat[1]
        val nowDelta = m.S.valOf(s1.id, s1.ch) - m.S.valOf(s2.id, s2.ch)
        SectionHeader("Różnica temperatur A − B")
        DiagCard(
            lbl = s1.label + " − " + s2.label,
            value = "Aktualna różnica: ${fmt1(nowDelta)} °C",
            sub = "Różnica liczona na bieżąco ze strumienia /piec/status"
        )
    }
    UstBtn("Włącz/Wyłącz linie progów", modifier = Modifier.fillMaxWidth(), onClick = {
        m.setChartAlarmLines(!m.chartAlarmLines)
        m.showToast("Wykres", (if (m.chartAlarmLines) "Włączono" else "Wyłączono") + " linie progów", "ok")
    })
}

private fun fmt1(v: Double): String = if (v.isFinite()) String.format(java.util.Locale.US, "%.1f", v) else "—"

private fun chartTimeLabel(m: AppModel, ts: Long): String {
    val c = java.util.Calendar.getInstance().apply { timeInMillis = ts }
    fun p(v: Int) = if (v < 10) "0$v" else "$v"
    return p(c.get(java.util.Calendar.HOUR_OF_DAY)) + ":" + p(c.get(java.util.Calendar.MINUTE)) +
        (if (m.rangeSec > 48 * 3600) " (" + p(c.get(java.util.Calendar.DAY_OF_MONTH)) + "." + p(c.get(java.util.Calendar.MONTH) + 1) + ")"
         else ":" + p(c.get(java.util.Calendar.SECOND)))
}

/** Dymek nad wykresem: każda wartość używa koloru swojej serii, jak w Piec.html. */
@Composable
private fun ChartHoverTooltip(
    m: AppModel,
    view: com.sterownikco.pro.ui.chart.ChartView,
    index: Int,
    modifier: Modifier = Modifier
) {
    val row = view.rows.getOrNull(index) ?: return
    Column(
        modifier.padding(8.dp)
            .widthIn(max = 220.dp)
            .background(Pal.rgba(9, 19, 35, .96f), RoundedCornerShape(8.dp))
            .border(BorderStroke(1.dp, Pal.rgba(0, 212, 245, .75f)), RoundedCornerShape(8.dp))
            .padding(horizontal = 8.dp, vertical = 6.dp),
        verticalArrangement = Arrangement.spacedBy(2.dp)
    ) {
        Text(chartTimeLabel(m, row.ts), fontSize = 8.5.sp, fontWeight = FontWeight.ExtraBold, color = Pal.Cyan)
        view.cat.forEach { s ->
            val x = view.values[s.id]?.getOrNull(index)
            val value = if (x != null && x.isFinite()) (if (row.isSimulated(s.ch)) "~" else "") + fmt1(x) + " " + s.unit else "—"
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                Text("●", fontSize = 7.sp, color = hexColor(s.accent))
                Text("${s.label}: ", fontSize = 8.sp, color = hexColor(s.accent), fontWeight = FontWeight.SemiBold)
                Text(value, fontSize = 8.sp, color = Color.White, fontWeight = FontWeight.Bold)
            }
        }
    }
}

/** `#chartInspectorVal` — czas w cyan, wartości w kolorach odpowiadających seriom. */
private fun inspectorText(m: AppModel, view: com.sterownikco.pro.ui.chart.ChartView, cross: Int?) = buildAnnotatedString {
    val row = cross?.let { view.rows.getOrNull(it) }
    if (cross == null || row == null) {
        withStyle(SpanStyle(color = Pal.TextDim)) { append("Dotknij wykresu / najedź kursorem") }
        return@buildAnnotatedString
    }
    withStyle(SpanStyle(color = Pal.Cyan, fontWeight = FontWeight.Bold)) { append(chartTimeLabel(m, row.ts)) }
    append(": ")
    view.cat.forEachIndexed { index, s ->
        if (index > 0) append("  ·  ")
        val x = view.values[s.id]?.getOrNull(cross)
        val value = if (x != null && x.isFinite()) (if (row.isSimulated(s.ch)) "~" else "") + fmt1(x) + " " + s.unit else "—"
        withStyle(SpanStyle(color = hexColor(s.accent))) {
            append("● ${s.label}: $value")
        }
    }
}
