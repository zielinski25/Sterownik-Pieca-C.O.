package com.sterownikco.pro.ui.screens.sheets

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.core.SensorDef
import com.sterownikco.pro.ui.components.ManualRow
import com.sterownikco.pro.ui.components.Note
import com.sterownikco.pro.ui.components.NumRow
import com.sterownikco.pro.ui.components.SectionHeader
import com.sterownikco.pro.ui.components.Seg
import com.sterownikco.pro.ui.components.UstBtn
import com.sterownikco.pro.ui.theme.Pal
import com.sterownikco.pro.ui.theme.Txt

/* ══════════════════════════════════════════════════════════════════════════
   Arkusze czujników — port `sensorModalContent()` / `symulacjaSekcjaHtml()` /
   `MENUS.czujniki` / `MENUS.dym` / `overheat()` (Piec.html 5104-5345, 5574).
   ══════════════════════════════════════════════════════════════════════════ */

/** `.status-pill` (+ `.sim`); `.ok`/`.err` korzystają z barw `.pill`. */
@Composable
fun StatusPill(text: String, cls: String) {
    val (fg, bg) = when (cls) {
        "sim" -> Pal.Cyan to Pal.rgba(0, 212, 245, .15f)
        "err" -> Pal.Err to Pal.rgba(255, 95, 120, .18f)
        "stale" -> Pal.TextDim to Pal.rgba(148, 163, 184, .10f)
        else -> Pal.Live to Pal.rgba(74, 222, 128, .12f)
    }
    val bd = when (cls) {
        "sim" -> Pal.rgba(0, 212, 245, .4f)
        "err" -> Pal.rgba(255, 95, 120, .45f)
        "stale" -> Pal.rgba(148, 163, 184, .25f)
        else -> Pal.rgba(74, 222, 128, .35f)
    }
    Box(
        Modifier.padding(start = 6.dp)
            .background(bg, RoundedCornerShape(999.dp))
            .border(BorderStroke(1.dp, bd), RoundedCornerShape(999.dp))
            .padding(horizontal = 7.dp, vertical = 3.dp)
    ) { Text(text, style = Txt.pill, color = fg) }
}

/**
 * `symulacjaSekcjaHtml(pole, etykieta, jednostka, min, max, krok, statusKey)` —
 * przycisk „Symulacja” rozwijający panel z suwakami wartości / czasu.
 */
@Composable
fun SymUnit(m: AppModel, pole: String, etykieta: String, jednostka: String, min: Double, max: Double, krok: Double, statusKey: String = pole) {
    val S = m.S
    val f: (Double) -> String = { v -> if (krok >= 1.0) Math.round(v).toString() else S.fmt1(v) }
    var open by remember(pole) { mutableStateOf(S.symAktywna(pole)) }
    val initialValue = S.valOf(pole, statusKey).takeIf { it.isFinite() } ?: (min + max) / 2.0
    var value by remember(pole) { mutableFloatStateOf(initialValue.toFloat()) }
    var czas by remember(pole) { mutableIntStateOf(S.sym[pole]?.min ?: 60) }

    Column(Modifier.fillMaxWidth().padding(top = 4.dp)) {
        SectionHeader("Symulacja — $etykieta")
        Seg(listOf(0 to "Symulacja"), current = if (open) 0 else -1, columns = 1, onPick = { open = !open })
        if (open) {
            Column(
                Modifier.fillMaxWidth().padding(top = 10.dp)
                    .background(Pal.rgba(17, 31, 53, .65f), RoundedCornerShape(12.dp))
                    .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(12.dp))
                    .padding(12.dp, 14.dp),
                verticalArrangement = Arrangement.spacedBy(2.dp)
            ) {
                Text("Wyłącznie lokalny test oznaczony SYM — nie trafia do historii ani do sterownika.",
                    style = Txt.tiny, color = Pal.Warn, modifier = Modifier.padding(bottom = 6.dp))
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    Text("Wartość", style = Txt.rowDesc, modifier = Modifier.weight(1f))
                    Text("${f(value.toDouble())} $jednostka", style = Txt.monoVal, color = Pal.White)
                }
                Slider(
                    value = value.coerceIn(min.toFloat(), max.toFloat()),
                    onValueChange = { v ->
                        val step = krok.toFloat().coerceAtLeast(0.1f)
                        value = min.toFloat() + Math.round((v - min.toFloat()) / step) * step
                    },
                    valueRange = min.toFloat()..max.toFloat(),
                    colors = SliderDefaults.colors(thumbColor = Pal.Cyan, activeTrackColor = Pal.Cyan, inactiveTrackColor = Pal.Surface3)
                )
                Row(Modifier.fillMaxWidth().padding(top = 10.dp), verticalAlignment = Alignment.CenterVertically) {
                    Text("Czas trwania", style = Txt.rowDesc, modifier = Modifier.weight(1f))
                    Text("$czas min", style = Txt.monoVal, color = Pal.White)
                }
                Slider(
                    value = czas.toFloat(), onValueChange = { v -> czas = Math.round(v / 5f) * 5 },
                    valueRange = 5f..180f,
                    colors = SliderDefaults.colors(thumbColor = Pal.Cyan, activeTrackColor = Pal.Cyan, inactiveTrackColor = Pal.Surface3)
                )
                Row(Modifier.fillMaxWidth().padding(top = 12.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    UstBtn("Zastosuj", variant = "primary", modifier = Modifier.weight(1f), onClick = {
                        m.send("symuluj $pole ${f(value.toDouble())} $czas")
                        open = true
                        m.refreshSheet()
                    })
                    UstBtn("Wyłącz teraz", variant = "danger", modifier = Modifier.weight(1f), onClick = {
                        m.send("symuluj_stop $pole")
                        open = false
                        m.refreshSheet()
                    })
                }
            }
        }
        val info = if (S.symAktywna(pole)) "Aktywna — jeszcze ok. ${S.sym[pole]?.min ?: 0} min" else ""
        if (info.isNotEmpty()) {
            Text(info, style = Txt.rowDesc.copy(fontSize = 9.5.sp, fontWeight = FontWeight.Bold, lineHeight = 13.sp),
                color = Pal.Warn, modifier = Modifier.fillMaxWidth().padding(top = 6.dp).padding(vertical = 4.dp))
        }
    }
}

/** Nagłówek karty czujnika (`.sensor-full-card` → `.sensor-full-header` + stopka zakresów). */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun SensorFullCard(m: AppModel, sen: SensorDef, listStyle: Boolean, onClickHeader: (() -> Unit)? = null) {
    val S = m.S
    val cur = S.valOf(sen.id, sen.key)
    val sim = S.symAktywna(sen.id)
    val available = S.hasData(sen.key)
    val stale = !m.isFbFresh || !S.online
    val pill = when {
        sen.id == "dym" && S.hasData("dym_alarm") && S.dym_alarm -> "ALARM" to "err"
        sen.id == "dym" && !S.hasData("dym_alarm") -> "STAN ALARMU — BRAK DANYCH" to "stale"
        sim -> "SYMULACJA LOKALNA" to "sim"
        sen.id == "dym" && S.hasData("dym_wlaczony") && !S.dym_wlaczony -> "WYŁ." to "stale"
        !available -> "BRAK DANYCH" to "stale"
        stale -> "NIEAKTUALNE" to "stale"
        sen.id == "dym" -> "BRAK ALARMU" to "ok"
        else -> "LIVE" to "ok"
    }
    Column(
        Modifier.fillMaxWidth().background(Pal.Surface2, RoundedCornerShape(14.dp))
            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(14.dp))
            .then(if (onClickHeader != null) Modifier.clickable { onClickHeader() } else Modifier)
            .padding(12.dp, 14.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    Text(if (sen.ico.isNotEmpty()) sen.ico else "🌡️", fontSize = 20.sp)
                    Column {
                        Text(sen.name, style = Txt.cardTitle)
                        Text(
                            if (sen.id == "dym" && !listStyle) "Przetwornik optyczny MQ-2 · ADC1 GPIO36" else sen.bus,
                            style = Txt.cardDesc, modifier = Modifier.padding(top = 2.dp)
                        )
                    }
                }
            }
            Column(horizontalAlignment = Alignment.End) {
                Text(
                    (if (sim) "~" else "") + SensorDef.fmt(sen, cur) + " " + sen.unit,
                    style = Txt.monoVal.copy(fontSize = 15.sp, color = Pal.Cyan)
                )
                StatusPill(pill.first, pill.second)
            }
        }
        if (!listStyle) {
            // Trzy dlugie napisy nie mieszcza sie w jednym wierszu — Row + SpaceBetween
            // sklejal je w "…ADCMagistrala:…MQ-2Odczyt:…". FlowRow ladnie je zawija.
            FlowRow(
                Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(10.dp),
                verticalArrangement = Arrangement.spacedBy(2.dp)
            ) {
                Text("Zakres roboczy: ${sen.min} do ${sen.max} ${sen.unit}", style = Txt.tiny)
                Text("Magistrala: ${sen.bus.split("·")[0].trim()}", style = Txt.tiny)
                Text(
                    when {
                        sim -> "Odczyt: SYMULACJA — nie jest danymi sterownika"
                        available -> if (stale) "Odczyt: ostatnia odebrana wartość (nieaktualna)" else "Odczyt: odebrany ze sterownika"
                        else -> "Odczyt: oczekiwanie na dane sterownika"
                    },
                    style = Txt.tiny.copy(color = if (sim) Pal.Warn else if (available && !stale) Pal.Live else Pal.TextDim)
                )
            }
        }
    }
}

/** `sensorModalContent(sen)` — karta telemetryczna + sekcja symulacji. */
@Composable
fun SensorModal(m: AppModel, sen: SensorDef) {
    SensorFullCard(m, sen, listStyle = false)
    if (sen.id == "panel") SolarMetricsCard(m)
    SymUnit(m, sen.id, sen.name, sen.unit, sen.min, sen.max, sen.step, sen.key)
}

/** `MENUS.czujniki` — lista 10 czujników + podsumowanie magistral. */
@Composable
fun SensorListSheet(m: AppModel) {
    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        Column(
            Modifier.fillMaxWidth().background(Pal.Surface2, RoundedCornerShape(12.dp))
                .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(12.dp)).padding(12.dp, 14.dp)
        ) {
            Text("MAGISTRALA CZUJNIKÓW TELEMETRYCZNYCH", style = Txt.diagLbl)
            val received = SensorDef.ALL.count { m.S.hasData(it.key) }
            Text("$received / ${SensorDef.ALL.size} odczytów odebranych", style = Txt.diagVal, modifier = Modifier.padding(top = 4.dp))
            Text(
                if (received == 0) "Oczekiwanie na rzeczywiste dane sterownika."
                else "Licznik dotyczy pól z ostatniej odpowiedzi Firebase; brak pola nie jest zastępowany wartością przykładową.",
                style = Txt.diagSub, modifier = Modifier.padding(top = 4.dp)
            )
        }
        Note("Każdy odczyt pochodzi z odpowiedzi sterownika. Opcjonalna symulacja jest ręczna, lokalna, oznaczona SYM i nigdy nie trafia do historii.")
        SensorDef.ALL.forEach { sen ->
            Column(Modifier.fillMaxWidth()) {
                SensorFullCard(m, sen, listStyle = true)
                SymUnit(m, sen.id, sen.name, sen.unit, sen.min, sen.max, sen.step, sen.key)
            }
        }
    }
}

/** `MENUS.dym` — karta MQ-2 + progi alarmu + cykl pracy czujnika. */
@Composable
fun DymSheet(m: AppModel) {
    val S = m.S
    val sen = SensorDef.byId("dym")!!
    SensorFullCard(m, sen, listStyle = false)
    SectionHeader("Ustawienia alarmu dymu")
    NumRow("Próg alarmu", "Surowy odczyt ADC czujnika (0-4095)", S.progAlarmDym, 0, 4095, known = S.hasData("progAlarmDym")) { v ->
        m.commitNum("Próg alarmu", "progAlarmDym", 0, 4095, v)
    }
    SectionHeader("Aktywacja od temperatury pieca")
    NumRow("Próg temperatury", "°C — poniżej tej temp. pieca czujnik jest wyłączony (histereza 3°C)", S.dymProgTemp, 0, 200, known = S.hasData("dymProgTemp")) { v ->
        m.commitNum("Próg temperatury", "dymProgTemp", 0, 200, v)
    }
    Row(Modifier.fillMaxWidth().padding(vertical = 5.dp)) {
        Text("Tryb pracy po aktywacji", style = Txt.rowLabel, modifier = Modifier.weight(1f))
    }
    Seg(listOf(0 to "Impulsowo", 1 to "Ciągle"), current = if (S.hasData("dymTrybPracy")) S.dymTrybPracy else -1, onPick = { v ->
        m.send("ustaw dymTrybPracy $v")
    })
    SectionHeader("Cykl pracy czujnika (tryb Impulsowo)")
    NumRow("Czas WŁ.", "s — rozgrzewanie + pomiar", S.dymCzasOn, 20, 600, known = S.hasData("dymCzasOn")) { v -> m.commitNum("Czas WŁ.", "dymCzasOn", 20, 600, v) }
    NumRow("Czas WYŁ.", "s — między pomiarami", S.dymCzasOff, 10, 600, known = S.hasData("dymCzasOff")) { v -> m.commitNum("Czas WYŁ.", "dymCzasOff", 10, 600, v) }
    NumRow("Czas stabilizacji", "s — część Czasu WŁ. zanim odczyt zaufany (dotyczy obu trybów)", S.dymCzasStabilizacji, 5, 590, known = S.hasData("dymCzasStabilizacji")) { v ->
        m.commitNum("Czas stabilizacji", "dymCzasStabilizacji", 5, 590, v)
    }
    SymUnit(m, "dym", "Dym", "ADC", 0.0, 4095.0, 1.0)
}

/** `overheat(panel)` — próg przegrzania pieca / kolektora. */
@Composable
fun OverheatSheet(m: AppModel, panel: Boolean) {
    val S = m.S
    if (panel) SolarMetricsCard(m)
    SectionHeader("Zabezpieczenie przed przegrzaniem")
    Note("Wspólny próg dla pieca C.O. i panelu słonecznego — histereza: piec -10°C, panel -4°C.")
    NumRow("Próg alarmu przegrzania", "°C", S.progAlarmTemp, 0, 100, known = S.hasData("progAlarmTemp")) { v ->
        m.commitNum("Próg alarmu przegrzania", "progAlarmTemp", 0, 100, v)
    }
    if (panel) SymUnit(m, "panel", "Panel słoneczny", "°C", -30.0, 160.0, 0.5)
    else SymUnit(m, "ogrz", "Piec C.O.", "°C", 0.0, 160.0, 0.5)
}

@Composable
private fun SolarMetricsCard(m: AppModel) {
    val s = m.S
    val solar = m.solar
    val panelTemp = s.valOf("panel", "t_panel")
    val outsideTemp = s.valOf("zewn", "t_zewn")
    val panelSim = s.symAktywna("panel")
    val outsideSim = s.symAktywna("zewn")
    val deltaKnown = s.hasData("t_panel") && s.hasData("t_zewn") && !panelSim && !outsideSim
    val delta = if (deltaKnown) s.t_panel - s.t_zewn else Double.NaN
    val panelText = if (panelTemp.isFinite()) (if (panelSim) "~" else "") + s.fmt1(panelTemp) + "°C" else "—"
    val outsideText = if (outsideTemp.isFinite()) (if (outsideSim) "~" else "") + s.fmt1(outsideTemp) + "°C" else "—"

    Column(
        Modifier.fillMaxWidth()
            .background(Pal.Surface2, RoundedCornerShape(12.dp))
            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(12.dp))
            .padding(horizontal = 12.dp, vertical = 8.dp)
    ) {
        Text("BILANS SOLARNY", style = Txt.diagLbl)
        Spacer(Modifier.height(2.dp))
        SolarMetricRow("Panel teraz / zewnątrz", "$panelText / $outsideText")
        SolarMetricRow(
            "Zysk brutto słońca",
            if (solar.hasSamples) "+${s.fmt1(solar.accumulatedGrossGain)}°C · ~${s.fmt1(solar.estKwh())} kWh" else "brak próbek"
        )
        SolarMetricRow(
            "ΔT panel − zewnątrz",
            if (delta.isFinite()) (if (delta >= 0) "+" else "") + s.fmt1(delta) + "°C" else "brak danych"
        )
        SolarMetricRow(
            "Pobory wody C.W.U.",
            if (solar.hasSamples) "${solar.drawCount} wykrytych · −${s.fmt1(solar.accumulatedDrawDrop)}°C" else "brak próbek"
        )
        SolarMetricRow(
            "Prognoza zysku · model godzinowy",
            if (solar.forecastGain.isFinite() && solar.forecastKwh.isFinite())
                "+${s.fmt1(solar.forecastGain)}°C · ~${s.fmt1(solar.forecastKwh)} kWh" else "brak prognozy"
        )
    }
}

@Composable
private fun SolarMetricRow(title: String, value: String) {
    Column {
        Box(Modifier.fillMaxWidth().height(1.dp).background(Pal.Border))
        Row(
            Modifier.fillMaxWidth().padding(vertical = 8.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                title,
                style = Txt.rowLabel.copy(fontSize = 9.5.sp, lineHeight = 12.sp),
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier.weight(1f)
            )
            Text(
                value,
                style = Txt.monoVal.copy(fontSize = 9.5.sp, lineHeight = 12.sp, color = Color(0xFFFFD32A)),
                textAlign = TextAlign.End,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier.padding(start = 8.dp)
            )
        }
    }
}
