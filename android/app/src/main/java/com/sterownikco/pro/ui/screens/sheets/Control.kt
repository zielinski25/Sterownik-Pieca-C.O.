package com.sterownikco.pro.ui.screens.sheets

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.ui.components.CheckRow
import com.sterownikco.pro.ui.components.ManualRow
import com.sterownikco.pro.ui.components.Note
import com.sterownikco.pro.ui.components.NumRow
import com.sterownikco.pro.ui.components.SectionHeader
import com.sterownikco.pro.ui.components.Seg
import com.sterownikco.pro.ui.components.SliderRow
import com.sterownikco.pro.ui.theme.Pal
import com.sterownikco.pro.ui.theme.Txt

/* ══════════════════════════════════════════════════════════════════════════
   Arkusze sterowania: `MENUS.pompa` / `serwo` / `mieszadlo` / `czas`
   (Piec.html 5386-5562) — karty strategii, diagramy trociniaka/kopciucha,
   panel ręczny serwa, override i RTC.
   ══════════════════════════════════════════════════════════════════════════ */

/** `.pump-strat-opt` (`.on` = cyan 15 % + poświata `--cyan-glow`). */
@Composable
private fun StratOpt(ico: String, title: String, sub: String, on: Boolean, onClick: () -> Unit) {
    Column(
        Modifier.weight(1f).background(
            if (on) Pal.rgba(0, 212, 245, .15f) else Pal.Surface2, RoundedCornerShape(12.dp)
        ).border(BorderStroke(1.dp, if (on) Pal.Cyan else Pal.Border), RoundedCornerShape(12.dp))
            .clickable { onClick() }.padding(horizontal = 6.dp, vertical = 10.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(2.dp)
    ) {
        Text(ico, fontSize = 16.sp, modifier = Modifier.padding(bottom = 4.dp))
        Text(title, fontSize = 9.5.sp, fontWeight = FontWeight.ExtraBold, color = Pal.White)
        Text(sub, fontSize = 7.5.sp, color = Pal.TextDim, modifier = Modifier.padding(top = 2.dp))
    }
}

/** `.pump-diagram-box` — kontener diagramu (rgba(14,23,38,.75), radius 14). */
@Composable
private fun DiagramBox(content: @Composable Column.() -> Unit) {
    Column(
        Modifier.fillMaxWidth().padding(vertical = 10.dp)
            .background(Color(0xBF0E1726), RoundedCornerShape(14.dp))
            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(14.dp))
            .padding(12.dp, 14.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp),
        content = content
    )
}

@Composable
private fun DiagHead(title: String, right: String) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Text(title, fontSize = 9.sp, fontWeight = FontWeight.ExtraBold, color = Pal.Cyan, modifier = Modifier.weight(1f))
        Text(right, fontSize = 8.sp, color = Pal.TextDim)
    }
}

/** `MENUS.pompa` — strategie Trociniak / Kopciuch / Auto + diagramy + nastawy. */
@Composable
fun PompaSheet(m: AppModel) {
    val S = m.S
    var wybor by remember { mutableIntStateOf(S.wybor) }
    Note("Wybierz strategię pracy pompy. Trociniak steruje cyklem czasowym, a Kopciuch załącza pompę na podstawie temperatury pieca.")
    Row(Modifier.fillMaxWidth().padding(vertical = 10.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        StratOpt("⏱️", "TROCINIAK", "Tryb Czasowy", wybor == 1) {
            wybor = 1; m.send("ustaw wybor 1"); m.refreshSheet()
        }
        StratOpt("🌡️", "KOPCIUCH", "Temperaturowy", wybor == 2) {
            wybor = 2; m.send("ustaw wybor 2"); m.refreshSheet()
        }
        StratOpt("⚡", "AUTO", "Inteligentny", wybor == 3) {
            wybor = 3; m.send("ustaw wybor 3"); m.refreshSheet()
        }
    }

    if (wybor == 1) {
        val totalMin = (S.czasOn + S.czasOff).takeIf { it > 0 } ?: 40
        val pctOn = Math.round(S.czasOn * 100.0 / totalMin).toInt()
        DiagramBox {
            DiagHead("⏱️ TROCINIAK — CYKL CZASOWY POMPY", "Pełny cykl: $totalMin min")
            Row(Modifier.fillMaxWidth().height(22.dp), verticalAlignment = Alignment.CenterVertically) {
                Box(
                    Modifier.weight(pctOn.toFloat().coerceAtLeast(0.01f)).fillMaxHeight()
                        .background(Brush.horizontalGradient(listOf(Color(0xFF4ADE80), Color(0xFF22C55E))))
                ) {
                    Box(Modifier.fillMaxWidth().fillMaxHeight(), contentAlignment = Alignment.Center) {
                        Text("PRACA: ${S.czasOn}m ($pctOn%)", fontSize = 8.sp, fontWeight = FontWeight.ExtraBold, color = Color(0xFF022C10))
                    }
                }
                Box(
                    Modifier.weight((100 - pctOn).toFloat().coerceAtLeast(0.01f)).fillMaxHeight()
                        .background(Color(0x1AFFFFFF))
                ) {
                    Box(Modifier.fillMaxWidth().fillMaxHeight(), contentAlignment = Alignment.Center) {
                        Text("POSTÓJ: ${S.czasOff}m (${100 - pctOn}%)", fontSize = 8.sp, fontWeight = FontWeight.ExtraBold, color = Pal.TextDim)
                    }
                }
            }
            Row(Modifier.fillMaxWidth().padding(top = 6.dp)) {
                Text("🟢 Załączenie pompy przez ${S.czasOn} min", fontSize = 7.5.sp, color = Pal.TextDim, modifier = Modifier.weight(1f))
                Text("⚪ Przerwa/odpoczynek kotła ${S.czasOff} min", fontSize = 7.5.sp, color = Pal.TextDim)
            }
        }
    }
    if (wybor == 2) {
        val minScale = 20; val maxScale = 90
        val stopPct = ((S.tempOff - minScale) * 100.0 / (maxScale - minScale)).coerceIn(0.0, 100.0)
        val startPct = ((S.tempOn - minScale) * 100.0 / (maxScale - minScale)).coerceIn(0.0, 100.0)
        val curTemp = S.t_ogrz
        val curPct = ((curTemp - minScale) * 100.0 / (maxScale - minScale)).coerceIn(0.0, 100.0)
        val running = curTemp >= S.tempOn || (curTemp > S.tempOff && S.pompa)
        DiagramBox {
            DiagHead("🌡️ KOPCIUCH — PROGI TEMPERATURY PIECA", "Histereza: ${S.tempOn - S.tempOff}°C")
            BoxWithConstraints(Modifier.fillMaxWidth().height(14.dp).padding(top = 18.dp, bottom = 12.dp)) {
                val w = maxWidth
                Box(Modifier.fillMaxWidth().height(14.dp)
                    .background(Brush.horizontalGradient(listOf(Color(0xFF38BDF8), Pal.Yellow, Pal.Accent, Pal.Err)), RoundedCornerShape(7.dp)))
                // .temp-live-needle — biała igła z poświatą
                Marker(w * (stopPct / 100f), "STOP ${S.tempOff}°C", Color(0xFF38BDF8), Color(0xFF082F49))
                Marker(w * (startPct / 100f), "START ${S.tempOn}°C", Pal.Accent, Color(0xFF431407))
                Box(Modifier.offset(x = w * (curPct / 100f) - 1.5.dp, y = (-4).dp).width(3.dp).height(22.dp)
                    .background(Color.White, RoundedCornerShape(2.dp)))
            }
            Row(Modifier.fillMaxWidth()) {
                Text("$minScale°C (zimny)", fontSize = 7.5.sp, fontWeight = FontWeight.Bold, color = Pal.TextDim, modifier = Modifier.weight(1f))
                Text(
                    "▲ Aktualnie piec: ${S.fmt1(curTemp)}°C (${if (running) "POMPA PRACUJE" else "POMPA STOI"})",
                    fontSize = 7.5.sp, fontWeight = FontWeight.Bold, color = if (running) Pal.Live else Pal.Warn, modifier = Modifier.weight(1.4f)
                )
                Text("$maxScale°C (gorący)", fontSize = 7.5.sp, fontWeight = FontWeight.Bold, color = Pal.TextDim)
            }
        }
    }

    ManualRow("pompa", onSend = { m.send(it) })
    if (S.pompa_override_min > 0) {
        Text(
            "Wymuszenie ręczne jeszcze ok. ${S.pompa_override_min} min, potem powrót do automatyki.",
            style = Txt.note.copy(fontSize = 9.5.sp), color = Pal.Warn, modifier = Modifier.padding(top = 6.dp)
        )
    }
    SectionHeader("Parametry trybu Czasowego (Trociniak)")
    NumRow("Czas ON (Praca)", "Minuty załączenia pompy w cyklu", S.czasOn, 1, 180) { v -> m.commitNum("Czas ON (Praca)", "czasOn", 1, 180, v) }
    NumRow("Czas OFF (Postój)", "Minuty wyłączenia pompy w cyklu", S.czasOff, 1, 180) { v -> m.commitNum("Czas OFF (Postój)", "czasOff", 1, 180, v) }
    SectionHeader("Parametry trybu Temperaturowego (Kopciuch)")
    NumRow("Temp START (ON)", "°C pieca — uruchomienie pompy", S.tempOn, 0, 100) { v -> m.commitNum("Temp START (ON)", "tempOn", 0, 100, v) }
    NumRow("Temp STOP (OFF)", "°C pieca — wyłączenie pompy (histereza)", S.tempOff, 0, 100) { v -> m.commitNum("Temp STOP (OFF)", "tempOff", 0, 100, v) }
    SectionHeader("Zabezpieczenie Antystop")
    CheckRow("Włączony antystop", null, S.antystopWlaczony) { v -> m.commitBool("Antystop", "antystopWlaczony", v) }
    NumRow("Dni bez ruchu", "Wymuszony puls 45s po X dniach bezczynności", S.antystopDni, 1, 45) { v ->
        m.commitNum("Dni bez ruchu", "antystopDni", 1, 45, v)
    }
}

/** `.temp-stop-marker` / `.temp-start-marker` + `.temp-marker-label`. */
@Composable
private fun Marker(x: androidx.compose.ui.unit.Dp, label: String, bg: Color, fg: Color) {
    Box(Modifier.offset(x = x - 1.dp, y = (-14).dp).width(2.dp).height(32.dp).background(Color.White))
    Box(Modifier.offset(x = x - 26.dp, y = (-14).dp)) {
        Text(
            label, fontSize = 7.5.sp, fontWeight = FontWeight.ExtraBold, color = fg,
            modifier = Modifier.background(bg, RoundedCornerShape(4.dp)).padding(horizontal = 4.dp, vertical = 1.dp)
        )
    }
}

/** `MENUS.serwo` — tryb klapy, panel ręczny (`.manual.on`), automatyka. */
@Composable
fun SerwoSheet(m: AppModel) {
    val S = m.S
    Note("Tryb pracy klapy i syberka")
    var tryb by remember { mutableIntStateOf(S.tryb_serwa) }
    Seg(listOf(1 to "Auto", 2 to "Ręczny", 3 to "Bezpieczna"), current = tryb) { v ->
        tryb = v
        m.send("ustaw trybSerwa $v")
        m.refreshSheet()
    }
    if (tryb == 2) {
        Column(Modifier.fillMaxWidth().padding(top = 10.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Box(Modifier.fillMaxWidth().height(1.dp).background(Color(0x14FFFFFF)))
            Text("Ręczna pozycja", style = Txt.rowLabel)
            SliderRow(
                name = "Klapa", value = Math.round(S.klapa / 1.8).toFloat(), min = 0f, max = 100f, step = 1f,
                fmt = { v -> v.toInt().toString() + "%" }, onValue = { }, onCommit = { v -> m.send("klapa ${v.toInt()}") }
            )
            SliderRow(
                name = "Syberek", value = Math.round(S.syberka / 0.9).toFloat(), min = 0f, max = 100f, step = 1f,
                fmt = { v -> v.toInt().toString() + "%" }, onValue = { }, onCommit = { v -> m.send("syberek ${v.toInt()}") }
            )
            if (S.serwo_override_min > 0) {
                Text(
                    "Ręczna pozycja jeszcze ok. ${S.serwo_override_min} min, potem powrót do automatyki.",
                    style = Txt.note.copy(fontSize = 9.5.sp), color = Pal.Warn
                )
            }
        }
    }
    SectionHeader("Automatyka (tryb Auto)")
    NumRow("Temperatura zadana", "°C — punkt odniesienia", S.tempZadServo, 0, 100) { v -> m.commitNum("Temperatura zadana", "tempZadServo", 0, 100, v) }
    NumRow("Histereza", "°C — pasmo bez reakcji", S.histerServo, 0, 50) { v -> m.commitNum("Histereza", "histerServo", 0, 50, v) }
    NumRow("Skok klapy", "% na jedno wywołanie", S.skokKlapy, 1, 100) { v -> m.commitNum("Skok klapy", "skokKlapy", 1, 100, v) }
    NumRow("Skok syberka", "% na jedno wywołanie", S.skokSyberka, 1, 100) { v -> m.commitNum("Skok syberka", "skokSyberka", 1, 100, v) }
    NumRow("Odchylenie przyspieszające", "°C — powyżej: krok × mnożnik", S.odchylTemp, 1, 50) { v ->
        m.commitNum("Odchylenie przyspieszające", "odchylTemp", 1, 50, v)
    }
    NumRow("Mnożnik korekty", "Mnożnik kroku przy dużym odchyleniu", S.mnoznik, 1, 10) { v -> m.commitNum("Mnożnik korekty", "mnoznik", 1, 10, v) }
}

/** `MENUS.mieszadlo` — załączenie, override ręczny, cykl pracy. */
@Composable
fun MieszadloSheet(m: AppModel) {
    val S = m.S
    CheckRow("Włączone", null, S.mieszadloWlaczony) { v -> m.commitBool("Mieszadło", "mieszadloWlaczony", v) }
    ManualRow("mieszadlo", onSend = { m.send(it) })
    if (S.mieszadlo_override_min > 0) {
        Text(
            "Ręczny override jeszcze ok. ${S.mieszadlo_override_min} min, potem powrót do automatyki.",
            style = Txt.note.copy(fontSize = 9.5.sp), color = Pal.Warn, modifier = Modifier.padding(top = 6.dp)
        )
    }
    SectionHeader("Cykl pracy")
    NumRow("Czas ON", "s — jak długo przekaźnik załączony", S.mieszadloCzasOn, 1, 255) { v ->
        m.commitNum("Czas ON", "mieszadloCzasOn", 1, 255, v)
    }
    NumRow("Czas OFF", "min — przerwa między cyklami", S.mieszadloCzasOff, 1, 180) { v ->
        m.commitNum("Czas OFF", "mieszadloCzasOff", 1, 180, v)
    }
}

/** `MENUS.czas` — zegar telefonu + RTC centrali + synchronizacja. */
@Composable
fun CzasSheet(m: AppModel) {
    val S = m.S
    val now = remember { java.util.Calendar.getInstance() }
    fun p(v: Int) = if (v < 10) "0$v" else "$v"
    Text("CZAS TELEFONU", style = Txt.note.copy(fontSize = 10.sp))
    Text(
        "${p(now.get(java.util.Calendar.DAY_OF_MONTH))}.${p(now.get(java.util.Calendar.MONTH) + 1)}." +
            now.get(java.util.Calendar.YEAR) + " " + p(now.get(java.util.Calendar.HOUR_OF_DAY)) + ":" +
            p(now.get(java.util.Calendar.MINUTE)) + ":" + p(now.get(java.util.Calendar.SECOND)),
        fontSize = 20.sp, fontWeight = FontWeight.Bold, color = Pal.White, modifier = Modifier.padding(top = 3.dp)
    )
    SectionHeader("Zegar sterownika (RTC)")
    Row(Modifier.fillMaxWidth().padding(vertical = 5.dp)) {
        Column(Modifier.weight(1f)) {
            Text(if (S.rtc_ok) "RTC gotowy" else "RTC niegotowy", style = Txt.rowLabel)
            Text("data w sterowniku: ${S.dzien}.${S.miesiac}.${S.rok}", style = Txt.rowDesc)
        }
    }
    com.sterownikco.pro.ui.components.UstBtn(
        "Synchronizuj z czasem urządzenia", variant = "primary",
        modifier = Modifier.fillMaxWidth(), onClick = { m.rtcSync() }
    )
}
