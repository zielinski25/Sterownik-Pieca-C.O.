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
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.draw.shadow
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
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.core.WHour
import com.sterownikco.pro.core.Weather
import com.sterownikco.pro.ui.art.Ilu
import com.sterownikco.pro.ui.components.IconBtn
import com.sterownikco.pro.ui.svg.SvgView
import com.sterownikco.pro.ui.theme.Dimens
import com.sterownikco.pro.ui.theme.Pal
import com.sterownikco.pro.ui.theme.Txt

/* ══════════════════════════════════════════════════════════════════════════
   POGODA — `#pg-pogoda` 1:1 (markup 3267-3425 + renderWeatherTab 8699-8800 +
   renderWeatherStudioChart + updateSunArcAndSolarMultiDay + SolarAnalytics).
   Dane WYLACZNIE z Open-Meteo (zero symulacji); klikniecia jak w HTML:
   zakres = refetch, godzina = podglad w hero, wykres = celownik + dymek.
   ══════════════════════════════════════════════════════════════════════════ */

private val SolarY = Color(0xFFFFD32A) // --slonce

@Composable
fun WeatherPage(m: AppModel) {
    val w = m.weather
    val cur = w?.current
    val days = m.weatherDays
    val hours = w?.hourly ?: emptyList()
    val dailies = w?.daily ?: emptyList()
    var vbl by remember { mutableStateOf("temp") }
    var hourSel by remember { mutableIntStateOf(-1) }
    var wCross by remember { mutableStateOf<Int?>(null) }
    val hs = hourSel.takeIf { it in hours.indices } ?: -1
    val selH = if (hs >= 0) hours[hs] else null
    val d0 = dailies.getOrNull(0)

    // Hero: wybrane godziny nadpisuja biezace — jak onclick .hcard w HTML.
    val heroTempTxt: String
    val heroDescTxt: String
    val heroFeelsTxt: String
    val heroCode: Int
    val heroIsDay: Boolean
    if (selH != null) {
        heroTempTxt = fmt1(selH.temp) + " °C"
        heroDescTxt = "Prognoza (${hh00(selH.time)}): " + Weather.desc(selH.code)
        heroFeelsTxt = "Wiatr ${fmt0(selH.wind)} km/h · Chmury ${fmt0(selH.cloud)}% · " +
            "Opad ${fmt1(selH.precip)} mm (${Math.round(selH.precipProb)}%)"
        heroCode = selH.code; heroIsDay = selH.isDay
    } else {
        heroTempTxt = (cur?.temp?.let { fmt1(it) } ?: "--.-") + " °C"
        heroDescTxt = if (cur == null) (if (m.weatherBusy) "Pobieranie danych…" else "Brak danych — odśwież")
        else Weather.desc(cur.code)
        heroFeelsTxt = if (cur == null) "—"
        else "Odczuwalna ${fmt1(cur.feelsLike)} °C  ·  ${cur.windDir} ${fmt0(cur.wind)} km/h " +
            "(porywy ${fmt0(cur.windGusts)} km/h)"
        heroCode = cur?.code ?: 0; heroIsDay = cur?.isDay ?: true
    }

    Column(
        Modifier.fillMaxWidth().verticalScroll(rememberScrollState())
            .padding(start = Dimens.pagePadH, end = Dimens.pagePadH, top = Dimens.pagePad, bottom = 28.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        // `.weather-topbar`: label plus a single inline location chip, not a card.
        Row(
            Modifier.fillMaxWidth().padding(bottom = 2.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text("METEO & OTOCZENIE KOTŁA", fontSize = 8.5.sp, fontWeight = FontWeight.ExtraBold,
                    color = Pal.Cyan, letterSpacing = 0.68.sp)
                Row(
                    Modifier.background(Pal.Surface2, RoundedCornerShape(20.dp))
                        .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(20.dp))
                        .padding(horizontal = 10.dp, vertical = 4.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    Box(Modifier.size(14.dp)) {
                        com.sterownikco.pro.ui.icons.AppIcon("location", size = 14.dp, tint = Pal.Cyan)
                    }
                    Text("Centrala Kotłownia", fontSize = 10.sp, fontWeight = FontWeight.ExtraBold, color = Pal.White,
                        maxLines = 1, overflow = TextOverflow.Ellipsis)
                    Text(latLon(m), fontSize = 8.sp, color = Pal.TextDim, maxLines = 1, overflow = TextOverflow.Ellipsis)
                }
            }
            IconBtn("refresh", "Odśwież dane meteo", onClick = { m.refreshWeather(true, true) })
        }

        // `.weather-hero`: gradient hero, subtle cyan glow, weather art and min/max pill.
        val heroShape = RoundedCornerShape(22.dp)
        Box(
            Modifier.fillMaxWidth().shadow(12.dp, heroShape).background(
                Brush.linearGradient(listOf(Color(0xFF102437), Color(0xFF07131F))), heroShape
            ).border(BorderStroke(1.dp, Pal.Border), heroShape)
        ) {
            Box(
                Modifier.align(Alignment.TopEnd).size(150.dp)
                    .background(Brush.radialGradient(listOf(Pal.rgba(0, 212, 245, .15f), Color.Transparent)))
            )
            Row(
                Modifier.fillMaxWidth().padding(horizontal = 18.dp, vertical = 16.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column(Modifier.weight(1f)) {
                    Text(heroTempTxt, style = Txt.heroTemp.copy(fontSize = 36.sp, lineHeight = 38.sp, letterSpacing = (-0.72).sp))
                    Text(heroDescTxt, style = Txt.heroD.copy(fontSize = 11.sp, lineHeight = 13.sp,
                        fontWeight = FontWeight.ExtraBold, color = Pal.Cyan), modifier = Modifier.padding(top = 3.dp, bottom = 2.dp))
                    Text(heroFeelsTxt, style = Txt.heroD.copy(fontSize = 8.5.sp, lineHeight = 10.sp))
                    Text("🌡️ Min: " + (d0?.minT?.let { fmt1(it) } ?: "--") + "°C · Max: " +
                        (d0?.maxT?.let { fmt1(it) } ?: "--") + "°C",
                        fontSize = 8.5.sp, fontWeight = FontWeight.ExtraBold, color = Pal.White,
                        modifier = Modifier.padding(top = 8.dp)
                            .background(Pal.rgba(255, 255, 255, .06f), RoundedCornerShape(10.dp))
                            .border(BorderStroke(1.dp, Pal.rgba(255, 255, 255, .08f)), RoundedCornerShape(10.dp))
                            .padding(horizontal = 8.dp, vertical = 3.dp))
                }
                SvgView(
                    src = remember(heroCode, heroIsDay) { Ilu.heroBoiler(m.S.art(heroCode, heroIsDay)) },
                    modifier = Modifier.size(88.dp)
                )
            }
        }

        // `.metric-grid` — 6 kart z warunkowymi podpisami jak w renderWeatherTab
        MetricGrid(
            listOf(
                MCard("💧 WILGOTNOŚĆ", pct(cur?.humidity),
                    if ((cur?.humidity ?: 0.0) > 70) "podwyższona" else "w normie"),
                MCard("💨 WIATR", fmt0or(cur?.wind) + " km/h",
                    "porywy " + fmt0or(cur?.windGusts) + " km/h · " + (cur?.windDir ?: "—")),
                MCard("🌡️ CIŚNIENIE", (cur?.pressure?.toString() ?: "--") + " hPa",
                    if ((cur?.pressure ?: 0) >= 1013) "stabilny wyż" else "niż baryczny"),
                MCard("☁️ ZACHMURZENIE", pct(cur?.cloud), cloudWord(cur?.cloud ?: 0.0)),
                MCard("🌧️ OPAD", (cur?.precip?.let { fmt1(it) } ?: "--") + " mm",
                    if ((cur?.precip ?: 0.0) > 0) "opad aktywny" else "brak opadów"),
                MCard("☀️ INDEKS UV", cur?.uv?.let { fmt1(it) } ?: "--", uvWord(cur?.uv ?: 0.0))
            )
        )

        // `.weather-card` — STUDIO POGODOWE; zakres buttons share the header row in HTML.
        WCard(title = studioTitle(days), sub = "ciągła analiza parametrów", stackSub = true, trailing = {
            listOf(1 to "24 H", 3 to "3 DNI", 7 to "7 DNI", 14 to "14 DNI").forEach { (v, lbl) ->
                RangeBtn(lbl, days == v) { m.setWeatherDays(v) }
            }
        }) {
            Row(
                Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(bottom = 6.dp),
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                listOf("temp" to "🌡️ Temperatura", "wind" to "💨 Wiatr", "precip" to "🌧️ Opady", "cloud" to "☁️ Zachmurzenie")
                    .forEach { (k, lbl) -> VarChip(lbl, vbl == k) { vbl = k } }
            }
            val measurer = rememberTextMeasurer()
            Box(
                Modifier.fillMaxWidth().height(Dimens.weatherChartH)
                    .background(Pal.CanvasBg, RoundedCornerShape(12.dp))
                    .border(BorderStroke(1.dp, Color(0x0FFFFFFF)), RoundedCornerShape(12.dp))
                    .pointerInput(hours) {
                        val padL = 36.dp.toPx()
                        val padR = 12.dp.toPx()
                        detectTapGestures(onTap = { pos ->
                            wCross = xToWIdx(pos.x, size.width.toFloat(), padL, padR, hours.size)
                        })
                    }
                    .pointerInput(hours) {
                        val padL = 36.dp.toPx()
                        val padR = 12.dp.toPx()
                        detectHorizontalDragGestures(onHorizontalDrag = { change, _ ->
                            change.consume()
                            wCross = xToWIdx(change.position.x, size.width.toFloat(), padL, padR, hours.size)
                        })
                    }
            ) {
                androidx.compose.foundation.Canvas(Modifier.fillMaxWidth().height(Dimens.weatherChartH)) {
                    drawWeatherStudio(hours, vbl, days, wCross, measurer)
                }
                val wc = wCross?.takeIf { it in hours.indices }
                if (wc != null) {
                    Box(Modifier.align(Alignment.TopEnd).padding(8.dp)
                        .background(Color(0xF00C1829), RoundedCornerShape(8.dp))
                        .border(BorderStroke(1.dp, Pal.Cyan), RoundedCornerShape(8.dp))
                        .padding(horizontal = 9.dp, vertical = 5.dp)) {
                        Text(wTip(hours[wc], vbl), fontSize = 8.5.sp, color = Color.White)
                    }
                }
            }
            Row(Modifier.fillMaxWidth().background(Pal.Surface2, RoundedCornerShape(8.dp)).padding(horizontal = 10.dp, vertical = 6.dp),
                verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("PROGNOZA", fontSize = 8.sp, fontWeight = FontWeight.ExtraBold, color = Pal.Cyan, letterSpacing = 0.5.sp)
                val wc = wCross?.takeIf { it in hours.indices }
                Text(if (wc != null) wInsp(hours[wc], vbl) else "Dotknij wykresu, aby sprawdzić wartości",
                    style = Txt.monoSm.copy(fontSize = 8.5.sp, color = Color.White))
            }
        }

        // `.weather-card` — PROGNOZA GODZINOWA (48 h, tap = podglad w hero)
        WCard("PROGNOZA GODZINOWA", "dotknij godziny do podglądu") {
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(bottom = 6.dp),
                horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                hours.take(48).forEachIndexed { i, h ->
                    val on = if (hs < 0) i == 0 else hs == i
                    Column(
                        Modifier.width(62.dp).background(
                            if (on) Pal.Surface3 else Pal.Surface2, RoundedCornerShape(12.dp)
                        ).border(BorderStroke(1.dp, if (on) Pal.Cyan else Pal.Border), RoundedCornerShape(12.dp))
                            .clickable { hourSel = i }.padding(horizontal = 6.dp, vertical = 8.dp),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.spacedBy(4.dp)
                    ) {
                        Text(hh00(h.time), fontSize = 8.5.sp, fontWeight = FontWeight.Bold, color = Pal.TextDim)
                        Text(Weather.emoji(h.code, h.isDay), fontSize = 15.sp)
                        Text(fmt0(h.temp) + "°", fontSize = 11.sp, fontWeight = FontWeight.ExtraBold, color = Pal.White)
                        Text(if (h.precipProb > 10) "${Math.round(h.precipProb)}%" else "", fontSize = 7.5.sp, color = Pal.Blue)
                    }
                }
                if (hours.isEmpty()) Text("Brak prognozy — odśwież dane meteo.", style = Txt.note)
            }
        }

        // `.weather-card` — PROGNOZA DZIENNA (skala barow stala 0..30 jak w HTML)
        WCard("PROGNOZA $days-DNIOWA", "zakresy dobowe (min ── max)") {
            Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                dailies.forEach { d ->
                    val minPct = ((d.minT - 0) / 30 * 100).coerceIn(0.0, 100.0)
                    val maxPct = ((d.maxT - 0) / 30 * 100).coerceIn(0.0, 100.0)
                    val widthPct = maxOf(8.0, maxPct - minPct)
                    Row(
                        Modifier.fillMaxWidth().background(Pal.Surface2, RoundedCornerShape(10.dp))
                            .padding(horizontal = 10.dp, vertical = 6.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(dayNameDaily(d.date), fontSize = 9.sp, fontWeight = FontWeight.Bold, color = Pal.White,
                            modifier = Modifier.width(72.dp))
                        Text(Weather.emoji(d.code, true), fontSize = 14.sp, modifier = Modifier.width(22.dp))
                        Row(Modifier.weight(1f).padding(horizontal = 10.dp), verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            Text(fmt0(d.minT) + "°", fontSize = 8.5.sp, fontWeight = FontWeight.Bold, color = Pal.TextDim,
                                textAlign = TextAlign.End, modifier = Modifier.width(32.dp))
                            BoxWithConstraints(Modifier.weight(1f).height(5.dp)) {
                                Box(Modifier.fillMaxWidth().height(5.dp)
                                    .background(Color(0x14FFFFFF), RoundedCornerShape(3.dp)))
                                Box(Modifier.offset(x = maxWidth * (minPct / 100).toFloat())
                                    .width(maxWidth * (widthPct / 100).toFloat()).height(5.dp)
                                    .background(Brush.horizontalGradient(
                                        listOf(Color(0xFF38BDF8), Color(0xFFFF9F43))), RoundedCornerShape(3.dp)))
                            }
                            Text(fmt0(d.maxT) + "°", fontSize = 9.sp, fontWeight = FontWeight.ExtraBold, color = Pal.White,
                                modifier = Modifier.width(32.dp))
                        }
                    }
                }
            }
        }

        // In HTML the solar balance card touches the sun-arc card; group them to preserve that spacing.
        Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(0.dp)) {
            // `.sun-arc-card` — prawdziwe wschody/zachody z Open-Meteo
            SunArcCard(m)

            // `.weather-card` — BILANS & ZYSK KOLEKTORA (+eksport CSV, siatka 7 dni)
            val sol = m.solar
            val dT = m.S.t_panel - m.S.t_zewn
            WCard("☀️ BILANS & ZYSK KOLEKTORA SŁONECZNEGO",
                "Korelacja nasłonecznienia z odczytami panelu i bojlera",
                titleColor = SolarY, stackSub = true,
                trailing = {
                    Box(Modifier.background(Color(0x26FFD32A), RoundedCornerShape(8.dp))
                        .border(BorderStroke(1.dp, Color(0x66FFD32A)), RoundedCornerShape(8.dp))
                        .clickable { m.exportSolarCsv() }.padding(horizontal = 9.dp, vertical = 5.dp),
                        contentAlignment = Alignment.Center) {
                        Text("📁 Eksportuj CSV", fontSize = 8.5.sp, fontWeight = FontWeight.Bold, color = SolarY)
                    }
                }) {
                MetricGrid(
                    listOf(
                        MCard("⚡ ZYSK BRUTTO SŁOŃCA", "+" + fmt1(sol.accumulatedGrossGain) + "°C",
                            "~" + fmt1(sol.estKwh()) + " kWh energii (dziś)", SolarY),
                        MCard("🌡️ ΔT PANEL - ZEWN.", (if (dT >= 0) "+" else "") + fmt1(dT) + "°C",
                            if (m.S.t_panel > m.S.t_zewn + 5) "🔥 aktywne grzanie" else "temperatura wyrównana",
                            if (dT > 10) SolarY else Pal.Cyan),
                        MCard("🚰 POBORY WODY CWU", sol.drawCount.toString() + " poborów",
                            "skompensowano -" + fmt1(sol.accumulatedDrawDrop) + "°C", Pal.Live),
                        MCard("🔮 PROGNOZA ZYSKU", "+" + fmt1(sol.forecastGain) + "°C",
                            "prognoza: ~" + fmt1(sol.forecastKwh) + " kWh z meteo", Pal.Violet)
                    )
                )
                Column(Modifier.fillMaxWidth().padding(top = 4.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.SpaceBetween) {
                        Text("📅 PROGNOZA POTENCJAŁU SOLARNEGO NA 7 DNI", fontSize = 9.5.sp,
                            fontWeight = FontWeight.ExtraBold, color = SolarY, letterSpacing = 0.4.sp,
                            lineHeight = 12.sp, modifier = Modifier.weight(1f))
                        Text("model radiacji Open-Meteo", fontSize = 8.sp, color = Pal.TextDim)
                    }
                    BoxWithConstraints(Modifier.fillMaxWidth()) {
                        // CSS repeat(auto-fit, minmax(110px, 1fr)) with 6px gap.
                        val columns = ((maxWidth.value + 6f) / 116f).toInt().coerceAtLeast(1)
                        Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                            dailies.take(7).chunked(columns).forEach { row ->
                                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                    row.forEach { d ->
                                        val idx = dailies.indexOf(d)
                                        SolarDayCard(
                                            m,
                                            dayLabelSolar(idx, d.date),
                                            hours.drop(idx * 24).take(24),
                                            idx == 0,
                                            Modifier.weight(1f)
                                        )
                                    }
                                    repeat(columns - row.size) { Box(Modifier.weight(1f)) }
                                }
                            }
                        }
                    }
                }
                Text(
                    "💡 Filtr poboru CWU: Algorytm automatycznie wykrywa nagłe schłodzenie bojlera przez napływ " +
                        "zimnej wody użytkowej i kompensuje ubytek, dzięki czemu bilans zysku słonecznego nie jest zaniżany.",
                    style = Txt.note.copy(fontSize = 9.5.sp),
                    modifier = Modifier.fillMaxWidth().padding(top = 2.dp)
                        .background(Color(0x08FFFFFF), RoundedCornerShape(8.dp)).padding(10.dp, 8.dp)
                )
            }
        }

        // `.weather-card` — PODSUMOWANIE METEO (8 pozycji 1:1)
        WCard("PODSUMOWANIE METEO", "stacja pogodowa Open-Meteo") {
            val tMin = d0?.minT ?: cur?.temp?.minus(4.0)
            val tMax = d0?.maxT ?: cur?.temp?.plus(4.0)
            val items = listOf(
                "🌡️ Maks. dzisiaj" to ((tMax?.let { fmt1(it) } ?: "—") + " °C"),
                "🌡️ Min. dzisiaj" to ((tMin?.let { fmt1(it) } ?: "—") + " °C"),
                "🌧️ Suma opadów" to (fmt1(d0?.rainSum ?: 0.0) + " mm"),
                "💨 Porywy wiatru" to (fmt0or(cur?.windGusts) + " km/h"),
                "☀️ UV maks." to (cur?.uv?.let { fmt1(it) } ?: "—"),
                "☀️ Nasłonecznienie" to (m.radEst(cur).toInt().toString() + " W/m²"),
                "🌅 Wschód słońca" to sunTimeOr(d0?.sunrise, 6, 42),
                "🌇 Zachód słońca" to sunTimeOr(d0?.sunset, 18, 15)
            )
            Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                items.chunked(2).forEach { row ->
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        row.forEach { (k, v) ->
                            Row(Modifier.weight(1f).background(Pal.Surface2, RoundedCornerShape(8.dp))
                                .padding(horizontal = 8.dp, vertical = 6.dp),
                                verticalAlignment = Alignment.CenterVertically,
                                horizontalArrangement = Arrangement.SpaceBetween) {
                                Text(k, fontSize = 8.sp, color = Pal.TextDim)
                                Text(v, fontSize = 9.sp, fontWeight = FontWeight.ExtraBold, color = Pal.White)
                            }
                        }
                        if (row.size == 1) Box(Modifier.weight(1f))
                    }
                }
            }
        }
    }
}

/** Karta dnia prognozy solarnej — dokladny port petli z updateSunArcAndSolarMultiDay. */
@Composable
private fun SolarDayCard(m: AppModel, dayLabel: String, dayHours: List<WHour>, today: Boolean, modifier: Modifier = Modifier) {
    val avgCloud = if (dayHours.isNotEmpty()) Math.round(dayHours.sumOf { it.cloud } / dayHours.size).toInt() else 30
    val estRadPeak = Math.round(maxOf(150.0, (100 - avgCloud * .7) * 8.8)).toInt()
    val estTotalKwhM2 = estRadPeak * 7.5 / 1000.0
    val modelGain = Math.round(estTotalKwhM2 * 4.2 * 10) / 10.0
    val estGainC = if (today && m.solar.accumulatedGrossGain > 0) maxOf(m.solar.accumulatedGrossGain, modelGain) else modelGain
    val estEnergyKwh = Math.round(estTotalKwhM2 * 2.0 * .65 * 10) / 10.0
    val sunEmoji = if (avgCloud < 25) "☀️" else if (avgCloud < 60) "🌤️" else "⛅"
    Column(modifier
        .background(if (today) Color(0x17FFD32A) else Color(0x0AFFFFFF), RoundedCornerShape(12.dp))
        .border(BorderStroke(1.dp, if (today) Color(0x59FFD32A) else Color(0x14FFFFFF)), RoundedCornerShape(12.dp))
        .padding(horizontal = 10.dp, vertical = 8.dp),
        verticalArrangement = Arrangement.spacedBy(3.dp)) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            Text(dayLabel, fontSize = 8.5.sp, fontWeight = FontWeight.ExtraBold,
                color = if (today) SolarY else Pal.TextDim, maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text(sunEmoji, fontSize = 8.5.sp)
        }
        Text("+" + fmt1(estGainC) + "°C", fontSize = 13.sp, fontWeight = FontWeight.ExtraBold, color = SolarY)
        Text("~" + fmt1(estEnergyKwh) + " kWh", fontSize = 8.sp, color = Pal.TextDim)
        Text("☀️ do $estRadPeak W/m²", fontSize = 7.5.sp, color = Pal.Cyan)
    }
}

private data class MCard(val lbl: String, val val_: String, val sub: String, val tint: Color = Pal.White)

@Composable
private fun MetricGrid(items: List<MCard>) {
    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        // CSS .metric-grid: three equal columns; four solar metrics therefore flow 3 + 1.
        items.chunked(3).forEach { row ->
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                row.forEach { item ->
                    Column(
                        Modifier.weight(1f).background(Pal.Surface, RoundedCornerShape(14.dp))
                            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(14.dp)).padding(10.dp)
                    ) {
                        Text(item.lbl, style = Txt.mcardLbl)
                        Text(item.val_, style = Txt.mcardVal.copy(color = item.tint))
                        Text(item.sub, style = Txt.mcardSub)
                    }
                }
                repeat(3 - row.size) { Box(Modifier.weight(1f)) }
            }
        }
    }
}

@Composable
private fun WCard(
    title: String,
    sub: String,
    titleColor: Color = Pal.White,
    stackSub: Boolean = false,
    trailing: (@Composable RowScope.() -> Unit)? = null,
    content: @Composable ColumnScope.() -> Unit
) {
    Column(
        Modifier.fillMaxWidth().background(Pal.Surface, RoundedCornerShape(Dimens.radiusTile))
            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(Dimens.radiusTile))
            .padding(horizontal = 14.dp, vertical = 12.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Row(Modifier.fillMaxWidth().padding(bottom = 2.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            if (stackSub) {
                Column(Modifier.weight(1f)) {
                    Text(title, fontSize = 11.sp, fontWeight = FontWeight.ExtraBold, color = titleColor,
                        letterSpacing = 0.6.sp, lineHeight = 13.sp)
                    Text(sub, style = Txt.cardDesc.copy(fontSize = 8.5.sp, lineHeight = 10.sp))
                }
            } else {
                Text(title, fontSize = 11.sp, fontWeight = FontWeight.ExtraBold, color = titleColor,
                    letterSpacing = 0.6.sp, maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.weight(1f))
                Text(sub, style = Txt.cardDesc.copy(fontSize = 8.5.sp, lineHeight = 10.sp),
                    maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.End)
            }
            if (trailing != null) Row(horizontalArrangement = Arrangement.spacedBy(4.dp), content = trailing)
        }
        content()
    }
}

@Composable
private fun RowScope.RangeBtn(label: String, on: Boolean, flex: Boolean = false, onClick: () -> Unit) {
    val mod = if (flex) Modifier.weight(1f) else Modifier
    val shape = RoundedCornerShape(6.dp)
    Box(
        mod.background(if (on) Pal.rgba(0, 212, 245, .15f) else Pal.Surface2, shape)
            .border(BorderStroke(1.dp, if (on) Pal.Cyan else Pal.Border), shape)
            .clickable { onClick() }.padding(horizontal = 6.dp, vertical = 3.dp),
        contentAlignment = Alignment.Center
    ) {
        Text(label, fontSize = 8.sp, fontWeight = FontWeight.Bold, color = if (on) Color.White else Pal.TextDim,
            maxLines = 1, overflow = TextOverflow.Ellipsis)
    }
}

@Composable
private fun RowScope.VarChip(label: String, on: Boolean, onClick: () -> Unit) {
    val shape = RoundedCornerShape(12.dp)
    Box(
        Modifier.background(if (on) Pal.rgba(0, 212, 245, .14f) else Pal.Surface2, shape)
            .border(BorderStroke(1.dp, if (on) Pal.Cyan else Pal.Border), shape)
            .clickable { onClick() }.padding(horizontal = 9.dp, vertical = 4.dp),
        contentAlignment = Alignment.Center
    ) {
        Text(label, fontSize = 8.5.sp, fontWeight = FontWeight.Bold,
            color = if (on) Color.White else Pal.TextDim, maxLines = 1)
    }
}

@Composable
private fun SunArcCard(m: AppModel) {
    val w = m.weather
    val c = w?.current
    val d0 = w?.daily?.getOrNull(0)
    val now = System.currentTimeMillis()
    val sunriseTs = d0?.sunrise?.let { Weather.parseIso(it) }.takeIf { (it ?: 0L) > 0L } ?: todayAt(6, 42)
    val sunsetTs = d0?.sunset?.let { Weather.parseIso(it) }.takeIf { (it ?: 0L) > 0L } ?: todayAt(18, 15)
    val noonTs = (sunriseTs + sunsetTs) / 2
    val isDayNow = now in sunriseTs..sunsetTs
    val span = maxOf(1L, sunsetTs - sunriseTs)
    val p = ((now - sunriseTs).toFloat() / span.toFloat()).coerceIn(0f, 1f)
    val rad = m.radEst(c).toInt()
    val elevTxt = if (isDayNow) "${Math.round(kotlin.math.sin(p * Math.PI) * 48)}° (nad horyzontem)"
    else "0° (noc / pod horyzontem)"
    val countTxt: String
    val countCol: Color
    val countBd: Color
    if (isDayNow) {
        val left = maxOf(0L, sunsetTs - now)
        countTxt = "🌇 Zachód za ok. ${left / 3600000}h ${pad2((left % 3600000) / 60000)}m"
        countCol = Pal.Warn; countBd = Pal.rgba(251, 191, 36, .3f)
    } else {
        val next = if (now > sunsetTs) sunriseTs + 24 * 3600000 else sunriseTs
        val left = maxOf(0L, next - now)
        countTxt = "🌅 Wschód za ok. ${left / 3600000}h ${pad2((left % 3600000) / 60000)}m"
        countCol = Pal.Cyan; countBd = Pal.rgba(0, 212, 245, .3f)
    }
    val dl = maxOf(0L, sunsetTs - sunriseTs)
    val sunArcShape = RoundedCornerShape(16.dp)
    Column(
        Modifier.fillMaxWidth().shadow(4.dp, sunArcShape)
            .background(Brush.linearGradient(listOf(Pal.rgba(15, 36, 56, .6f), Pal.rgba(8, 20, 33, .8f))), sunArcShape)
            .border(BorderStroke(1.dp, Pal.rgba(0, 212, 245, .25f)), sunArcShape)
            .padding(horizontal = 14.dp, vertical = 12.dp)
    ) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("ŁUK SŁONECZNY & POTENCJAŁ SOLARNY", fontSize = 10.5.sp, lineHeight = 14.sp,
                    fontWeight = FontWeight.ExtraBold, color = Pal.Cyan, letterSpacing = 0.6.sp)
                Text("wpływ nasłonecznienia na panel słoneczny C.O.", style = Txt.cardDesc.copy(fontSize = 8.sp))
            }
            Text(countTxt, fontSize = 8.5.sp, fontWeight = FontWeight.ExtraBold, color = countCol,
                modifier = Modifier.background(countCol.copy(alpha = .12f), RoundedCornerShape(10.dp))
                    .border(BorderStroke(1.dp, countBd), RoundedCornerShape(10.dp)).padding(horizontal = 8.dp, vertical = 2.dp))
        }
        val sunTextMeasurer = rememberTextMeasurer()
        androidx.compose.foundation.Canvas(Modifier.fillMaxWidth().padding(top = 8.dp).height(Dimens.sunArcH)) {
            val sx = size.width / 320f
            val sy = size.height / 68f
            fun X(v: Float) = v * sx
            fun Y(v: Float) = v * sy
            val fill = Path().apply {
                moveTo(X(20f), Y(54f)); quadraticBezierTo(X(160f), Y(8f), X(300f), Y(54f)); close()
            }
            drawPath(fill, Color(0x0AFFD94D))
            drawLine(Color(0x4D5E829B), Offset(X(10f), Y(54f)), Offset(X(310f), Y(54f)), 1.5.dp.toPx(),
                pathEffect = PathEffect.dashPathEffect(floatArrayOf(2.dp.toPx(), 2.dp.toPx())))
            val arc = Path().apply {
                moveTo(X(20f), Y(54f)); quadraticBezierTo(X(160f), Y(8f), X(300f), Y(54f))
            }
            drawPath(arc, Brush.horizontalGradient(listOf(Color(0x66FF9F43), Color(0xF2FFD94D), Color(0x66FF6B6B))),
                style = Stroke(2.5.dp.toPx(), pathEffect = PathEffect.dashPathEffect(floatArrayOf(4.dp.toPx(), 4.dp.toPx()))))
            if (isDayNow) {
                val sunX = X(20f + p * 280f)
                val sunY = Y(54f - 4f * (54f - 8f) * p * (1f - p))
                drawCircle(Color(0x47FFD94D), radius = 14.dp.toPx(), center = Offset(sunX, sunY))
                drawCircle(Color(0xFFFFD94D), radius = 7.5.dp.toPx(), center = Offset(sunX, sunY))
                drawCircle(Color.White, radius = 7.5.dp.toPx(), center = Offset(sunX, sunY), style = Stroke(1.5.dp.toPx()))
            }
            val labelStyle = TextStyle(fontSize = 8.5.sp, fontWeight = FontWeight.Bold, color = Pal.TextDim)
            val sunriseLabel = sunTextMeasurer.measure(AnnotatedString(hhmm(sunriseTs) + " (Wschód)"), labelStyle)
            val noonLabel = sunTextMeasurer.measure(AnnotatedString(hhmm(noonTs) + " (Zenit)"), labelStyle.copy(color = Pal.Yellow))
            val sunsetLabel = sunTextMeasurer.measure(AnnotatedString(hhmm(sunsetTs) + " (Zachód)"), labelStyle)
            drawText(sunriseLabel, topLeft = Offset(X(20f), Y(56f)))
            drawText(noonLabel, topLeft = Offset(X(160f) - noonLabel.size.width / 2f, Y(56f)))
            drawText(sunsetLabel, topLeft = Offset(X(300f) - sunsetLabel.size.width, Y(56f)))
        }
        Row(
            Modifier.fillMaxWidth().padding(top = 6.dp).drawBehind {
                drawLine(Color(0x0FFFFFFF), Offset(0f, 0f), Offset(size.width, 0f), 1.dp.toPx())
            }.padding(top = 6.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(buildAnnotatedString {
                append("☀️ Promieniowanie: ")
                withStyle(SpanStyle(color = SolarY, fontWeight = FontWeight.Bold)) { append("$rad W/m²") }
            }, fontSize = 8.5.sp, color = Pal.TextDim,
                maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.weight(1f))
            Text(buildAnnotatedString {
                append("📐 Elewacja słońca: ")
                withStyle(SpanStyle(color = Pal.Cyan, fontWeight = FontWeight.Bold)) { append(elevTxt) }
            }, fontSize = 8.5.sp, color = Pal.TextDim,
                maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, modifier = Modifier.weight(1.2f))
            Text(buildAnnotatedString {
                append("⏳ Długość dnia: ")
                withStyle(SpanStyle(color = Color.White, fontWeight = FontWeight.Bold)) {
                    append("${dl / 3600000}h ${(dl % 3600000) / 60000}m")
                }
            }, fontSize = 8.5.sp, color = Pal.TextDim,
                maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.End, modifier = Modifier.weight(0.8f))
        }
    }
}

/* ─────────── studio pogody: wykres 1:1 z renderWeatherStudioChart ─────────── */

private data class WCfg(val v: (WHour) -> Double, val unit: String, val col: Color, val label: String)

private fun wCfg(vbl: String): WCfg = when (vbl) {
    "wind" -> WCfg({ it.wind }, "km/h", Color(0xFF38BDF8), "Wiatr")
    "precip" -> WCfg({ it.precip }, "mm", Color(0xFF4ADE80), "Opady")
    "cloud" -> WCfg({ it.cloud }, "%", Color(0xFFFFD166), "Zachmurzenie")
    else -> WCfg({ it.temp }, "°C", Color(0xFF00D4F5), "Temperatura")
}

/** Indeks celownika z pozycji dotyku — jak handleMove w initWeatherChartInteractivity. */
private fun xToWIdx(x: Float, wPx: Float, padL: Float, padR: Float, n: Int): Int? {
    if (n < 2 || wPx <= padL + padR) return null
    val cx = x.coerceIn(padL, wPx - padR)
    return Math.round((cx - padL) / (wPx - padL - padR) * (n - 1))
}

private fun DrawScope.drawWeatherStudio(
    hours: List<WHour>, vbl: String, days: Int, cross: Int?,
    measurer: androidx.compose.ui.text.TextMeasurer
) {
    if (hours.size < 2) return
    val cfg = wCfg(vbl)
    val vals = hours.map(cfg.v)
    val minVal = vals.min()
    val maxVal = vals.max()
    val pad = maxOf(1.0, (maxVal - minVal) * 0.15)
    val yMin = if (vbl == "precip" || vbl == "cloud") 0.0 else minVal - pad
    val yMax = if (vbl == "cloud") 100.0 else maxVal + pad
    val w = size.width
    val h = size.height
    val padL = 36.dp.toPx()
    val padR = 12.dp.toPx()
    val padT = 16.dp.toPx()
    val padB = 22.dp.toPx()
    val plotW = w - padL - padR
    val plotH = h - padT - padB
    if (plotW <= 0 || plotH <= 0) return

    // Siatka + etykiety osi Y
    val axisY = TextStyle(fontSize = 8.sp, fontWeight = FontWeight.SemiBold, color = Color(0xFF8EA6BA))
    for (i in 0..3) {
        val frac = i / 3f
        val valY = yMax - frac * (yMax - yMin)
        val yPos = padT + frac * plotH
        drawLine(Color(0x0FFFFFFF), Offset(padL, yPos), Offset(w - padR, yPos), 0.5.dp.toPx())
        val tp = measurer.measure(
            AnnotatedString((if (vbl == "precip") fmt1(valY) else fmt0(valY)) + cfg.unit), axisY
        )
        drawText(tp, topLeft = Offset(padL - 5.dp.toPx() - tp.size.width, yPos - tp.size.height / 2f))
    }
    // Etykiety osi X
    val axisX = TextStyle(fontSize = 8.sp, color = Color(0xFF5E829B))
    val xStep = maxOf(1, hours.size / (if (days <= 3) 6 else 7))
    var i = 0
    while (i < hours.size) {
        val xPos = padL + (i.toFloat() / (hours.size - 1).toFloat()) * plotW
        val t = hours[i].time
        val lbl = if (days <= 2) hh00(t) else dowShort(t) + " " + hh2(t) + "h"
        val tp = measurer.measure(AnnotatedString(lbl), axisX)
        drawText(tp, topLeft = Offset(xPos - tp.size.width / 2f, h - 6.dp.toPx() - tp.size.height))
        i += xStep
    }
    // Krzywa + wypelnienie
    val pts = hours.mapIndexed { k, hh ->
        Offset(
            padL + (k.toFloat() / (hours.size - 1).toFloat()) * plotW,
            (padT + plotH * (1 - (cfg.v(hh) - yMin) / (yMax - yMin))).toFloat()
        )
    }
    val fill = Path().apply {
        moveTo(pts[0].x, padT + plotH)
        pts.forEach { lineTo(it.x, it.y) }
        lineTo(pts.last().x, padT + plotH)
        close()
    }
    drawPath(fill, Brush.verticalGradient(listOf(cfg.col.copy(alpha = .18f), Color.Transparent)))
    val line = Path().apply {
        pts.forEachIndexed { k, p -> if (k == 0) moveTo(p.x, p.y) else lineTo(p.x, p.y) }
    }
    drawPath(line, cfg.col, style = Stroke(2.dp.toPx()))
    // Celownik
    val ci = cross?.takeIf { it in hours.indices }
    if (ci != null) {
        val tp0 = pts[ci]
        drawLine(Color.White.copy(alpha = .3f), Offset(tp0.x, padT), Offset(tp0.x, padT + plotH), 1.dp.toPx(),
            pathEffect = PathEffect.dashPathEffect(floatArrayOf(3.dp.toPx(), 3.dp.toPx())))
        drawCircle(Color.White, radius = 4.dp.toPx(), center = tp0)
        drawCircle(cfg.col, radius = 4.dp.toPx(), center = tp0, style = Stroke(2.dp.toPx()))
    }
}

/** Dymek nad wykresem studia — jak #wChartFloatingTooltip w HTML. */
private fun wTip(h: WHour, vbl: String): AnnotatedString {
    val cfg = wCfg(vbl)
    return buildAnnotatedString {
        withStyle(SpanStyle(fontWeight = FontWeight.Bold, color = Color.White)) { append(wTimeStr(h.time)) }
        append("\n")
        append(cfg.label + ": ")
        withStyle(SpanStyle(fontWeight = FontWeight.Bold, color = cfg.col)) {
            append(fmt1(cfg.v(h)) + " " + cfg.unit)
        }
        append(" · " + Weather.desc(h.code))
    }
}

/** Tekst inspektora pod wykresem — jak #wInspectorVal w HTML. */
private fun wInsp(h: WHour, vbl: String): String {
    val cfg = wCfg(vbl)
    return wTimeStr(h.time) + " — " + cfg.label + ": " + fmt1(cfg.v(h)) + " " + cfg.unit +
        " (" + Weather.desc(h.code) + ")"
}

/* ─────────── pomocnicze formatowanie (port helperow renderWeatherTab) ─────────── */

private fun fmt1(v: Double) = String.format(java.util.Locale.US, "%.1f", v)
private fun fmt0(v: Double) = String.format(java.util.Locale.US, "%.0f", v)
private fun fmt0or(v: Double?) = v?.let { fmt0(it) } ?: "--"
private fun pct(v: Double?) = (v?.let { Math.round(it).toString() } ?: "--") + "%"
private fun pad2(v: Long) = v.toString().padStart(2, '0')
private fun cloudWord(c: Double) = if (c > 70) "duże" else if (c > 30) "umiarkowane" else "małe"
private fun uvWord(u: Double) = if (u >= 5) "wysoki" else if (u >= 3) "umiarkowany" else "niski"

private val W_DOW = listOf("Nd", "Pn", "Wt", "Śr", "Cz", "Pt", "Sb")

private fun calOf(ts: Long) = java.util.Calendar.getInstance().apply { timeInMillis = ts }
private fun hh00(ts: Long) = pad2(calOf(ts).get(java.util.Calendar.HOUR_OF_DAY).toLong()) + ":00"
private fun hh2(ts: Long) = pad2(calOf(ts).get(java.util.Calendar.HOUR_OF_DAY).toLong())
private fun hhmm(ts: Long): String {
    val c = calOf(ts)
    return pad2(c.get(java.util.Calendar.HOUR_OF_DAY).toLong()) + ":" +
        pad2(c.get(java.util.Calendar.MINUTE).toLong())
}

private fun dowShort(ts: Long) = W_DOW[calOf(ts).get(java.util.Calendar.DAY_OF_WEEK) - 1]

/** `Czw 07.10` — jak wiersz dzienny w HTML. */
private fun dayNameDaily(ts: Long): String {
    val c = calOf(ts)
    return W_DOW[c.get(java.util.Calendar.DAY_OF_WEEK) - 1] + " " +
        pad2(c.get(java.util.Calendar.DAY_OF_MONTH).toLong()) + "." +
        pad2((c.get(java.util.Calendar.MONTH) + 1).toLong())
}

/** Etykieta karty solarnej: DZIŚ / JUTRO / `Czw 07.10`. */
private fun dayLabelSolar(idx: Int, ts: Long): String = when (idx) {
    0 -> "DZIŚ"
    1 -> "JUTRO"
    else -> dayNameDaily(ts)
}

private fun wTimeStr(ts: Long): String {
    val c = calOf(ts)
    return W_DOW[c.get(java.util.Calendar.DAY_OF_WEEK) - 1] + " " +
        pad2(c.get(java.util.Calendar.DAY_OF_MONTH).toLong()) + "." +
        pad2((c.get(java.util.Calendar.MONTH) + 1).toLong()) + " " + hh00(ts)
}

/** `STUDIO POGODOWE · 24H / 3 DNI / 7 DNI / 14 DNI` — titleMap z HTML. */
private fun studioTitle(days: Int) = when (days) {
    1 -> "STUDIO POGODOWE · 24H"
    3 -> "STUDIO POGODOWE · 3 DNI"
    7 -> "STUDIO POGODOWE · 7 DNI"
    14 -> "STUDIO POGODOWE · 14 DNI"
    else -> "STUDIO POGODOWE · $days DNI"
}

/** `#weatherLocSub` — szerokość/długość z ustawień panelu. */
private fun latLon(m: AppModel): String {
    val la = m.prefs.get(com.sterownikco.pro.core.Prefs.K_LAT) ?: "51.066389"
    val lo = m.prefs.get(com.sterownikco.pro.core.Prefs.K_LON) ?: "21.509167"
    fun f(v: String) = try {
        String.format(java.util.Locale.US, "%.4f", v.toDouble())
    } catch (e: Exception) {
        v
    }
    return f(la) + "° N · " + f(lo) + "° E"
}

private fun todayAt(h: Int, mi: Int): Long {
    val c = java.util.Calendar.getInstance()
    c.set(java.util.Calendar.HOUR_OF_DAY, h)
    c.set(java.util.Calendar.MINUTE, mi)
    c.set(java.util.Calendar.SECOND, 0)
    c.set(java.util.Calendar.MILLISECOND, 0)
    return c.timeInMillis
}

/** Wschód/zachód z Open-Meteo (ISO) albo zapas — `HH:MM` jak w podsumowaniu. */
private fun sunTimeOr(iso: String?, fbH: Int, fbM: Int): String {
    val ts = iso?.let { Weather.parseIso(it) } ?: 0L
    return hhmm(if (ts > 0L) ts else todayAt(fbH, fbM))
}
