package com.sterownikco.pro.ui.screens

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
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
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.core.Weather
import com.sterownikco.pro.ui.art.Ilu
import com.sterownikco.pro.ui.components.IconBtn
import com.sterownikco.pro.ui.svg.SvgView
import com.sterownikco.pro.ui.theme.Dimens
import com.sterownikco.pro.ui.theme.Pal
import com.sterownikco.pro.ui.theme.Txt

/* ══════════════════════════════════════════════════════════════════════════
   POGODA — `#pg-pogoda` (Piec.html 3232-3390): pasek meteo, hero, bento
   wskaźników, studio wykresów, godziny, prognoza dzienna, łuk słoneczny,
   bilans kolektora i podsumowanie.
   ══════════════════════════════════════════════════════════════════════════ */

@Composable
fun WeatherPage(m: AppModel) {
    val w = m.weather
    val cur = w?.current
    var range by remember { mutableIntStateOf(7) }
    var vbl by remember { mutableStateOf("temp") }
    var hourSel by remember { mutableIntStateOf(-1) }

    Column(
        Modifier.fillMaxWidth().verticalScroll(rememberScrollState())
            .padding(start = Dimens.pagePadH, end = Dimens.pagePadH, top = Dimens.pagePad, bottom = 16.dp),
        verticalArrangement = Arrangement.spacedBy(Dimens.gap)
    ) {
        // `.weather-topbar`
        Row(
            Modifier.fillMaxWidth().background(Brush.linearGradient(listOf(Pal.HeroA, Pal.HeroB)), RoundedCornerShape(Dimens.radiusTile))
                .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(Dimens.radiusTile)).padding(12.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column(Modifier.weight(1f)) {
                Text("METEO & OTOCZENIE KOTŁA", fontSize = 10.5.sp, fontWeight = FontWeight.ExtraBold, color = Pal.Cyan, letterSpacing = 0.6.sp)
                Row(Modifier.padding(top = 6.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    androidx.compose.foundation.layout.Box(Modifier.size(15.dp)) {
                        com.sterownikco.pro.ui.icons.AppIcon("location", size = 15.dp, tint = Pal.Cyan)
                    }
                    Column {
                        Text("Centrala Kotłownia", fontSize = 12.sp, fontWeight = FontWeight.Bold, color = Pal.White)
                        Text(latLon(m), style = Txt.cardDesc)
                    }
                }
            }
            IconBtn("refresh", "Odśwież dane meteo", onClick = { m.refreshWeather(true) })
        }

        // `.weather-hero`
        Row(
            Modifier.fillMaxWidth().background(Pal.Surface, RoundedCornerShape(Dimens.radiusTile))
                .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(Dimens.radiusTile)).padding(14.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column(Modifier.weight(1f)) {
                Text((cur?.temp?.let { fmt1(it) } ?: "--.-") + " °C", style = Txt.heroTemp)
                Text(if (cur == null) (if (m.weatherBusy) "Pobieranie danych…" else "Brak danych — odśwież") else Weather.desc(cur.code), style = Txt.heroD)
                Text(
                    if (cur == null) "—" else "Odczuwalna ${fmt1(cur.feelsLike)} °C · wiatr ${fmt1(cur.wind)} km/h ${cur.windDir}",
                    style = Txt.heroD
                )
                Row(Modifier.padding(top = 8.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    Text("🌡️ Min: " + (w?.daily?.minOfOrNull { it.minT }?.let { Math.round(it).toString() } ?: "--") + "°C · Max: " +
                            (w?.daily?.maxOfOrNull { it.maxT }?.let { Math.round(it).toString() } ?: "--") + "°C",
                        style = Txt.cardDesc
                    )
                }
            }
            SvgView(
                src = remember(m.S.art(cur?.code ?: 0, cur?.isDay ?: true)) { Ilu.heroBoiler(m.S.art(cur?.code ?: 0, cur?.isDay ?: true)) },
                modifier = Modifier.size(96.dp)
            )
        }

        // `.metric-grid` — 6 kart `.mcard`
        MetricGrid(
            listOf(
                MCard("💧 WILGOTNOŚĆ", (cur?.humidity?.let { Math.round(it).toString() } ?: "--") + "%", "w normie"),
                MCard("💨 WIATR", (cur?.wind?.let { fmt1(it) } ?: "--") + " km/h", "porywy " + (cur?.windGusts?.let { fmt1(it) } ?: "--") + " km/h"),
                MCard("🌡️ CIŚNIENIE", (cur?.pressure?.toString() ?: "--") + " hPa", "stabilne"),
                MCard("☁️ ZACHMURZENIE", (cur?.cloud?.let { Math.round(it).toString() } ?: "--") + "%",
                    if ((cur?.cloud ?: 0.0) > 70) "duże" else if ((cur?.cloud ?: 0.0) > 20) "częściowe" else "małe"),
                MCard("🌧️ OPAD", (cur?.precip?.let { fmt1(it) } ?: "--") + " mm",
                    "szansa " + Math.round(w?.hourly?.maxOfOrNull { it.precipProb } ?: 0.0).toString() + "%"),
                MCard("☀️ INDEKS UV", cur?.uv?.let { fmt1(it) } ?: "--",
                    when { (cur?.uv ?: 0.0) < 3 -> "niski"; (cur?.uv ?: 0.0) < 6 -> "umiarkowany"; (cur?.uv ?: 0.0) < 8 -> "wysoki"; else -> "bardzo wysoki" })
            )
        )

        // `.weather-card` — STUDIO POGODOWE
        WCard(
            title = "STUDIO POGODOWE · " + rangeLabel(range),
            sub = "ciągła analiza parametrów",
            trailing = {
                listOf(24 to "24 H", 3 to "3 DNI", 7 to "7 DNI", 14 to "14 DNI").forEach { (v, lbl) ->
                    RangeBtn(lbl, range == v) { range = v }
                }
            }
        ) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                listOf("temp" to "🌡️ Temperatura", "wind" to "💨 Wiatr", "precip" to "🌧️ Opady", "cloud" to "☁️ Zachmurzenie")
                    .forEach { (k, lbl) -> RangeBtn(lbl, vbl == k, flex = true) { vbl = k } }
            }
            Box(
                Modifier.fillMaxWidth().height(Dimens.weatherChartH).background(Pal.CanvasBg, RoundedCornerShape(12.dp))
                    .border(BorderStroke(1.dp, Color(0x0FFFFFFF)), RoundedCornerShape(12.dp))
            ) {
                androidx.compose.foundation.Canvas(Modifier.fillMaxWidth().height(Dimens.weatherChartH)) {
                    drawWeatherSeries(w?.hourly ?: emptyList(), vbl)
                }
            }
            Row(Modifier.fillMaxWidth().background(Pal.Surface2, RoundedCornerShape(8.dp)).padding(horizontal = 10.dp, vertical = 6.dp),
                verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("PROGNOZA", fontSize = 8.sp, fontWeight = FontWeight.ExtraBold, color = Pal.Cyan, letterSpacing = 0.5.sp)
                Text("Dotknij wykresu, aby sprawdzić wartości", style = Txt.monoSm.copy(fontSize = 8.5.sp, color = Color.White))
            }
        }

        // `.weather-card` — PROGNOZA GODZINOWA
        WCard("PROGNOZA GODZINOWA", "dotknij godziny do podglądu") {
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                (w?.hourly ?: emptyList()).take(48).forEachIndexed { i, h ->
                    val on = hourSel == i
                    Column(
                        Modifier.width(64.dp).background(
                            if (on) Pal.rgba(0, 212, 245, .12f) else Pal.Surface2, RoundedCornerShape(10.dp)
                        ).border(BorderStroke(1.dp, if (on) Pal.Cyan else Pal.Border), RoundedCornerShape(10.dp))
                            .clickable { hourSel = i }.padding(vertical = 6.dp),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.spacedBy(2.dp)
                    ) {
                        Text(String.format(java.util.Locale.US, "%02d", java.util.Calendar.getInstance().apply { timeInMillis = h.time }
                            .get(java.util.Calendar.HOUR_OF_DAY)) + ":00", fontSize = 8.sp, fontWeight = FontWeight.Bold, color = Pal.TextDim)
                        Text(Weather.emoji(h.code, h.isDay), fontSize = 15.sp)
                        Text(fmt1(h.temp) + "°", fontSize = 10.sp, fontWeight = FontWeight.ExtraBold, color = Pal.White)
                        Text((if (h.precip > 0) fmt1(h.precip) + "mm" else "—"), fontSize = 7.5.sp, color = Pal.Blue)
                    }
                }
                if (w?.hourly.isNullOrEmpty()) Text("Brak prognozy — odśwież dane meteo.", style = Txt.note)
            }
        }

        // `.weather-card` — PROGNOZA DZIENNA
        WCard("PROGNOZA " + range + "-DNIOWA", "zakresy dobowe (min ── max)") {
            (w?.daily ?: emptyList()).take(range).forEach { d ->
                Row(
                    Modifier.fillMaxWidth().background(Pal.Surface2, RoundedCornerShape(10.dp))
                        .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(10.dp)).padding(10.dp, 8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Text(dayLabel(d.date), fontSize = 10.5.sp, fontWeight = FontWeight.Bold, color = Pal.White, modifier = Modifier.width(66.dp))
                    Text(Weather.emoji(d.code, true), fontSize = 16.sp)
                    Text(fmt1(d.minT) + "°", fontSize = 10.sp, fontWeight = FontWeight.Bold, color = Pal.Blue, modifier = Modifier.width(34.dp))
                    BoxWithConstraints(Modifier.weight(1f).height(6.dp)) {
                        val total = maxWidth
                        val lo = w?.daily?.minOfOrNull { it.minT } ?: 0.0
                        val hi = w?.daily?.maxOfOrNull { it.maxT } ?: 30.0
                        val sp = (hi - lo).coerceAtLeast(1.0)
                        val from = (((d.minT - lo) / sp).toFloat().coerceIn(0f, .95f)) * total
                        val to = (((d.maxT - lo) / sp).toFloat().coerceIn(0.05f, 1f)) * total
                        Box(Modifier.fillMaxWidth().height(6.dp).background(Color(0x14FFFFFF), RoundedCornerShape(3.dp)))
                        Box(Modifier.width(to - from).offset(x = from).height(6.dp)
                            .background(Brush.horizontalGradient(listOf(Pal.Blue, Pal.Accent)), RoundedCornerShape(3.dp)))
                    }
                    Text(fmt1(d.maxT) + "°", fontSize = 10.sp, fontWeight = FontWeight.Bold, color = Pal.Accent, modifier = Modifier.width(34.dp))
                    Text("💧" + fmt1(d.rainSum) + " mm", fontSize = 8.5.sp, color = Pal.TextDim)
                }
            }
        }

        // `.sun-arc-card`
        SunArcCard(m)

        // `.weather-card` — BILANS & ZYSK KOLEKTORA
        WCard("☀️ BILANS & ZYSK KOLEKTORA SŁONECZNEGO", "Korelacja nasłonecznienia z odczytami panelu i bojlera") {
            MetricGrid(
                listOf(
                    MCard("⚡ ZYSK BRUTTO SŁOŃCA", "+" + fmt1(m.solar.accumulatedGrossGain) + "°C", "skumulowany przyrost panelu", Pal.Yellow),
                    MCard("🌡️ ΔT PANEL - ZEWN.", fmt1(m.solar.deltaT) + "°C", "różnica temperatur kolektora", Pal.Cyan),
                    MCard("🚰 POBORY WODY CWU", m.solar.drawCount.toString() + " zdarzeń", "wykryte schłodzenia bojlera", Pal.Live),
                    MCard("🔮 PROGNOZA ZYSKU", "+" + fmt1(m.solar.forecastGain) + "°C", "model radiacji Open-Meteo", Pal.Violet)
                )
            )
            Text(
                "💡 Filtr poboru CWU: Algorytm automatycznie wykrywa nagłe schłodzenie bojlera przez napływ zimnej wody użytkowej " +
                    "i kompensuje ubytek, dzięki czemu zysk słoneczny nie jest zaniżany.",
                style = Txt.note.copy(fontSize = 9.5.sp),
                modifier = Modifier.fillMaxWidth().background(Color(0x08FFFFFF), RoundedCornerShape(8.dp)).padding(8.dp, 10.dp)
            )
        }

        // `.weather-card` — PODSUMOWANIE METEO
        WCard("PODSUMOWANIE METEO", "stacja pogodowa Open-Meteo") {
            val rows = listOf(
                "Źródło" to "Open-Meteo · " + latLon(m),
                "Zakres" to (w?.daily?.size ?: 0).toString() + " dni prognozy",
                "Panel C.O." to fmt1(m.S.valOf("panel", "t_panel")) + " °C",
                "Bojler" to fmt1(m.S.valOf("bojler", "t_bojler")) + " °C"
            )
            rows.forEach { (k, v) ->
                Row(Modifier.fillMaxWidth().padding(vertical = 3.dp), verticalAlignment = Alignment.CenterVertically) {
                    Text(k, style = Txt.rowDesc, modifier = Modifier.weight(1f))
                    Text(v, style = Txt.monoSm.copy(color = Pal.White))
                }
            }
        }
    }
}

private data class MCard(val lbl: String, val val_: String, val sub: String, val tint: Color = Pal.White)

@Composable
private fun MetricGrid(items: List<MCard>) {
    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        items.chunked(2).forEach { row ->
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                row.forEach { it2 ->
                    Column(
                        Modifier.weight(1f).background(Pal.Surface2, RoundedCornerShape(10.dp))
                            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(10.dp)).padding(8.dp, 9.dp)
                    ) {
                        Text(it2.lbl, style = Txt.mcardLbl)
                        Text(it2.val_, style = Txt.mcardVal.copy(color = it2.tint))
                        Text(it2.sub, style = Txt.mcardSub)
                    }
                }
                if (row.size == 1) Box(Modifier.weight(1f))
            }
        }
    }
}

@Composable
private fun WCard(
    title: String,
    sub: String,
    titleColor: Color = Pal.White,
    trailing: (@Composable RowScope.() -> Unit)? = null,
    content: @Composable ColumnScope.() -> Unit
) {
    Column(
        Modifier.fillMaxWidth().background(Pal.Surface, RoundedCornerShape(Dimens.radiusTile))
            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(Dimens.radiusTile)).padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Column(Modifier.weight(1f)) {
                Text(title, fontSize = 11.sp, fontWeight = FontWeight.ExtraBold, color = titleColor, letterSpacing = 0.4.sp)
                Text(sub, style = Txt.cardDesc)
            }
            if (trailing != null) Row(horizontalArrangement = Arrangement.spacedBy(4.dp), content = trailing)
        }
        content()
    }
}

@Composable
private fun RowScope.RangeBtn(label: String, on: Boolean, flex: Boolean = false, onClick: () -> Unit) {
    val mod = if (flex) Modifier.weight(1f) else Modifier
    Box(
        mod.background(if (on) Pal.rgba(0, 212, 245, .16f) else Pal.Surface2, RoundedCornerShape(8.dp))
            .border(BorderStroke(1.dp, if (on) Pal.Cyan else Pal.Border), RoundedCornerShape(8.dp))
            .clickable { onClick() }.padding(horizontal = 8.dp, vertical = 5.dp),
        contentAlignment = Alignment.Center
    ) {
        Text(label, fontSize = 8.5.sp, fontWeight = FontWeight.Bold, color = if (on) Color.White else Pal.TextDim)
    }
}

@Composable
private fun SunArcCard(m: AppModel) {
    val c = m.weather?.current
    val rad = m.radEst(c)
    Column(
        Modifier.fillMaxWidth().background(Pal.Surface, RoundedCornerShape(Dimens.radiusTile))
            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(Dimens.radiusTile)).padding(12.dp)
    ) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("ŁUK SŁONECZNY & POTENCJAŁ SOLARNY", fontSize = 10.5.sp, fontWeight = FontWeight.ExtraBold, color = Pal.Cyan, letterSpacing = 0.5.sp)
                Text("wpływ nasłonecznienia na panel słoneczny C.O.", style = Txt.cardDesc)
            }
            Text(
                if (rad > 400) "WYSOKI POTENCJAŁ" else if (rad > 150) "UMIARKOWANY" else "NISKI",
                fontSize = 8.5.sp, fontWeight = FontWeight.ExtraBold, color = Pal.Warn,
                modifier = Modifier.background(Pal.rgba(251, 191, 36, .12f), RoundedCornerShape(10.dp))
                    .border(BorderStroke(1.dp, Pal.rgba(251, 191, 36, .3f)), RoundedCornerShape(10.dp)).padding(horizontal = 8.dp, vertical = 2.dp)
            )
        }
        androidx.compose.foundation.Canvas(
            Modifier.fillMaxWidth().height(Dimens.sunArcH).padding(top = 6.dp)
        ) {
            val w = size.width; val h = size.height
            val base = h * 0.79f
            val arc = Path().apply {
                moveTo(20f / 320f * w, base)
                quadraticBezierTo(w / 2, h * 0.12f, (320f - 20f) / 320f * w, base)
            }
            drawLine(Pal.rgba(94, 130, 155, .3f), Offset(10f / 320f * w, base), Offset(310f / 320f * w, base), 1.5.dp.toPx())
            drawPath(arc, Brush.horizontalGradient(listOf(
                Color(0x66FF9F43), Color(0xF2FFD94D), Color(0x66FF6B6B)
            )), style = Stroke(2.5.dp.toPx()))
            val t = dayFraction()
            val sx = lerp(20f / 320f * w, w / 2f, t * 2f).let { if (t < 0.5f) it else lerp(w / 2f, (320f - 20f) / 320f * w, (t - 0.5f) * 2f) }
            val sy = base - (base - h * 0.12f) * (1f - kotlin.math.abs(t * 2f - 1f)) * 0.94f
            drawCircle(Pal.rgba(255, 217, 77, .28f), radius = 14.dp.toPx(), center = Offset(sx, sy))
            drawCircle(Pal.ArtSun, radius = 7.5.dp.toPx(), center = Offset(sx, sy))
            drawCircle(Color.White, radius = 7.5.dp.toPx(), center = Offset(sx, sy), style = Stroke(1.5.dp.toPx()))
        }
        Row(Modifier.fillMaxWidth().padding(top = 2.dp), horizontalArrangement = Arrangement.SpaceBetween) {
            Text(sunriseLabel() + " (Wschód)", fontSize = 8.5.sp, fontWeight = FontWeight.Bold, color = Pal.TextDim)
            Text(noonLabel() + " (Zenit)", fontSize = 8.5.sp, fontWeight = FontWeight.Bold, color = Pal.Yellow)
            Text(sunsetLabel() + " (Zachód)", fontSize = 8.5.sp, fontWeight = FontWeight.Bold, color = Pal.TextDim)
        }
        Row(
            Modifier.fillMaxWidth().padding(top = 6.dp).border(BorderStroke(1.dp, Color(0x0FFFFFFF)), RoundedCornerShape(0.dp))
                .padding(top = 6.dp),
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Text("☀️ Promieniowanie: " + rad.toInt() + " W/m²", fontSize = 8.5.sp, color = Pal.TextDim)
            Text("📐 Elewacja słońca: " + Math.round(elevation(c)).toString() + "°", fontSize = 8.5.sp, color = Pal.Cyan)
            Text("⏳ Długość dnia: " + daylightLabel(), fontSize = 8.5.sp, color = Color.White)
        }
    }
}

/* ─────────── pomocnicze przeliczenia (port logiki renderWeatherTab) ─────────── */

private fun fmt1(v: Double) = String.format(java.util.Locale.US, "%.1f", v)

/** `#weatherLocSub` — szerokość/długość z ustawień panelu. */
private fun latLon(m: AppModel): String {
    val la = m.prefs.get(com.sterownikco.pro.core.Prefs.K_LAT) ?: "51.066389"
    val lo = m.prefs.get(com.sterownikco.pro.core.Prefs.K_LON) ?: "21.509167"
    fun f(v: String) = try { String.format(java.util.Locale.US, "%.4f", v.toDouble()) } catch (e: Exception) { v }
    return f(la) + "° N · " + f(lo) + "° E"
}
private fun rangeLabel(r: Int) = if (r == 24) "24 H" else "$r DNI"
private fun dayLabel(ts: Long): String {
    val c = java.util.Calendar.getInstance().apply { timeInMillis = ts }
    return listOf("Pon", "Wt", "Śr", "Czw", "Pt", "Sob", "Nied")[((c.get(java.util.Calendar.DAY_OF_WEEK) + 5) % 7)] +
        " " + c.get(java.util.Calendar.DAY_OF_MONTH) + "." + (c.get(java.util.Calendar.MONTH) + 1)
}

/** 0..1 — położenie słońca w dniu (południe = 0.5). */
private fun dayFraction(): Float {
    val c = java.util.Calendar.getInstance()
    val min = c.get(java.util.Calendar.HOUR_OF_DAY) * 60 + c.get(java.util.Calendar.MINUTE)
    return ((min - 360).coerceAtLeast(0).toFloat() / (12 * 60f)).coerceIn(0f, 1f)
}

private fun lerp(a: Float, b: Float, t: Float) = a + (b - a) * t.coerceIn(0f, 1f)

private fun elevation(c: com.sterownikco.pro.core.WCurrent?): Double {
    val uv = c?.uv ?: 0.0
    return (uv * 12.0).coerceIn(0.0, 66.0)
}

private fun sunriseLabel(): String = "06:42"
private fun sunsetLabel(): String = "18:15"
private fun noonLabel(): String = "12:28"
private fun daylightLabel(): String = "11h 33m"

/** Uproszczony wykres serii pogodowej (godziny × wybrana zmienna). */
private fun DrawScope.drawWeatherSeries(hours: List<com.sterownikco.pro.core.WHour>, vbl: String) {
    if (hours.size < 2) {
        return
    }
    val vals = hours.take(48).map { h ->
        when (vbl) { "wind" -> h.wind; "precip" -> h.precip; "cloud" -> h.cloud; else -> h.temp }
    }
    val lo = vals.min(); val hi = vals.max()
    val sp = (hi - lo).let { if (it < 1e-6) 1.0 else it }
    val padL = 8.dp.toPx(); val padR = 8.dp.toPx(); val padT = 10.dp.toPx(); val padB = 10.dp.toPx()
    val w = size.width - padL - padR; val h = size.height - padT - padB
    val pts = vals.mapIndexed { i, v ->
        androidx.compose.ui.geometry.Offset(
            padL + w * (i.toFloat() / (vals.size - 1).toFloat()),
            padT + h * (1f - ((v - lo) / sp).toFloat())
        )
    }
    val path = Path()
    pts.forEachIndexed { i, p -> if (i == 0) path.moveTo(p.x, p.y) else path.lineTo(p.x, p.y) }
    val fill = Path().apply {
        addPath(path)
        lineTo(pts.last().x, padT + h); lineTo(pts.first().x, padT + h); close()
    }
    val col = when (vbl) { "wind" -> Pal.Blue; "precip" -> Pal.Cyan; "cloud" -> Pal.TextDim; else -> Pal.Accent }
    drawPath(fill, Brush.verticalGradient(listOf(col.copy(alpha = .22f), col.copy(alpha = 0f))))
    drawPath(path, col, style = Stroke(2.dp.toPx()))
    pts.forEach { drawCircle(col, 1.8.dp.toPx(), center = it) }
}
