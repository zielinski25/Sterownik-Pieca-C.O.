package com.sterownikco.pro.ui.screens.demo

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.ui.components.Seg
import com.sterownikco.pro.ui.theme.Pal
import com.sterownikco.pro.ui.theme.Txt

/* ══════════════════════════════════════════════════════════════════════════
   `buildDemo()` — szuflada DEMO (`#demo`, 340 px, tło #0B1526) z Piec.html
   8861-9014. Wszystkie grupy, scenariusze i zakresy suwaków 1:1.
   ══════════════════════════════════════════════════════════════════════════ */

@Composable
fun DemoDrawer(m: AppModel) {
    val S = m.S
    fun bump() { m.S.syncReal(); S.bump() }

    Box(
        Modifier.fillMaxSize().background(Color(0xBF000000)).clickable { m.demoOpen = false },
        contentAlignment = Alignment.CenterEnd
    ) {
        Column(
            Modifier.fillMaxHeight().fillMaxWidth().widthIn(max = 340.dp)
                .background(Pal.DemoBg)
                .border(BorderStroke(1.dp, Pal.BorderStrong), RoundedCornerShape(topStart = 0.dp, bottomStart = 0.dp))
        ) {
            // `.demo-sticky-top`
            Row(
                Modifier.fillMaxWidth().background(Pal.DemoBg)
                    .padding(start = 16.dp, end = 16.dp, top = 16.dp, bottom = 12.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text("⚙ SCENARIUSZE DEMO", fontSize = 13.sp, fontWeight = FontWeight.ExtraBold,
                    color = Pal.Cyan, letterSpacing = 1.sp, modifier = Modifier.weight(1f))
                Box(Modifier.background(Color(0xFFEF4444), RoundedCornerShape(20.dp))
                    .clickable { m.demoOpen = false }.padding(horizontal = 16.dp, vertical = 8.dp)) {
                    Text("✕ ZAMKNIJ", fontSize = 12.sp, fontWeight = FontWeight.ExtraBold, color = Color.White)
                }
            }
            Box(Modifier.fillMaxWidth().height(1.dp).background(Color(0x1AFFFFFF)))

            Column(
                Modifier.weight(1f).fillMaxWidth().verticalScroll(rememberScrollState())
                    .padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(4.dp)
            ) {
                Text(
                    "Wybierz gotowy scenariusz 1-kliknięciem lub precyzyjnie dostosuj parametry dla wszystkich zakładek.",
                    style = Txt.note.copy(fontSize = 10.5.sp, lineHeight = 15.sp), modifier = Modifier.padding(top = 8.dp)
                )

                H5("Szybkie Scenariusze (1-Klik)")
                PRESETS.forEach { p ->
                    Column(
                        Modifier.fillMaxWidth().background(Pal.Surface2, RoundedCornerShape(10.dp))
                            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(10.dp))
                            .clickable { p.apply(S); bump(); m.showToast("DEMO", "Załadowano scenariusz: " + p.name, "ok") }
                            .padding(8.dp, 10.dp)
                    ) {
                        Text(p.name, fontSize = 10.5.sp, fontWeight = FontWeight.Bold, color = Color.White)
                        Text(p.desc, fontSize = 8.sp, color = Pal.TextDim, modifier = Modifier.padding(top = 1.dp))
                    }
                }

                H5("Prędkość symulacji")
                Seg(listOf(1 to "1× Normalna", 5 to "5× Szybka", 20 to "20× Timelapse"), current = m.simSpeed) { v ->
                    m.simSpeed = v; S.simSpeed = v.toDouble(); bump()
                }

                H5("Łączność i System")
                Chk("Sterownik online (LIVE)", S.online) { S.online = it; bump() }
                Chk("Noc (księżyc i gwiazdy)", S.night) { S.night = it; bump() }
                Chk("Błąd zegara RTC", !S.rtc_ok) { S.rtc_ok = !it; bump() }

                H5("Strategia Pompy")
                Seg(listOf(1 to "Trociniak (Czas)", 2 to "Kopciuch (Temp)", 3 to "Auto"), current = S.wybor) { v ->
                    S.wybor = v; bump()
                }

                H5("Urządzenia i Siłowniki")
                Chk("Pompa pracuje", S.pompa) { S.pompa = it; bump() }
                Chk("Mieszadło pracuje", S.mieszadlo) { S.mieszadlo = it; bump() }
                Chk("Rozpalanie (override)", S.rozpalanie) { S.rozpalanie = it; bump() }
                Text("Tryb serwa:", style = Txt.note.copy(fontSize = 10.5.sp))
                Seg(listOf(1 to "Auto", 2 to "Ręczny", 3 to "Bezp."), current = S.tryb_serwa) { v ->
                    S.tryb_serwa = v; S.trybSerwa = v; bump()
                }

                H5("Alarmy instalacji")
                Chk("ALARM dymu", S.dym_alarm) { S.dym_alarm = it; bump() }
                Chk("ALARM przegrzania pieca", S.alarm_ogrzewanie) { S.alarm_ogrzewanie = it; bump() }
                Chk("ALARM przegrzania panelu", S.alarm_panel) { S.alarm_panel = it; bump() }
                Chk("Czujnik dymu: pomiar nieświeży", !S.dym_swiezy) { S.dym_swiezy = !it; bump() }

                H5("Symulacje ESP (dpOtworz)")
                Chk("Symuluj Bojler (65°C, 45 min)", S.symAktywna("bojler")) { v ->
                    if (v) S.sym["bojler"] = com.sterownikco.pro.core.SymEntry(65.0, 45) else S.sym.remove("bojler"); bump()
                }
                Chk("Symuluj Piec (85°C, 30 min)", S.symAktywna("ogrz")) { v ->
                    if (v) S.sym["ogrz"] = com.sterownikco.pro.core.SymEntry(85.0, 30) else S.sym.remove("ogrz"); bump()
                }
                Chk("Symuluj Dym (1500 ADC)", S.symAktywna("dym")) { v ->
                    if (v) { S.sym["dym"] = com.sterownikco.pro.core.SymEntry(1500.0, 20); S.dym_alarm = true }
                    else { S.sym.remove("dym"); S.dym_alarm = false }
                    bump()
                }

                H5("Odczyty czujników")
                Rng(m, "Piec C.O.", "t_ogrz", 15.0, 95.0, "°C")
                Rng(m, "Bojler", "t_bojler", 10.0, 80.0, "°C")
                Rng(m, "Zewnętrzna", "t_zewn", -20.0, 35.0, "°C")
                Rng(m, "Panel słon.", "t_panel", 5.0, 110.0, "°C")
                Rng(m, "Pomieszczenie", "t_pokoj", 10.0, 35.0, "°C")
                Rng(m, "Ciśnienie", "cisnienie", 970.0, 1040.0, "hPa")
                Rng(m, "Wilgotność", "wilgotnosc", 0.0, 100.0, "%")
                Rng(m, "Dym (ADC)", "dym", 0.0, 4095.0, "")

                H5("Scenariusze pogody")
                var sc by remember { mutableStateOf("sun") }
                Seg(listOf("sun" to "Słońce", "rain" to "Deszcz", "storm" to "Burza", "snow" to "Zima"), current = sc) { v ->
                    sc = v
                    m.applyDemoWeather(v)
                    bump()
                }

                H5("Szerokość ramki")
                var frame by remember { mutableIntStateOf(412) }
                Seg(listOf(360 to "360", 412 to "412", 480 to "480", 768 to "tablet"), current = frame) { v ->
                    frame = v; m.frameWidth = v
                }

                Box(
                    Modifier.fillMaxWidth().padding(top = 24.dp).background(Color(0xFFEF4444), RoundedCornerShape(12.dp))
                        .border(BorderStroke(1.dp, Color(0xFFEF4444)), RoundedCornerShape(12.dp))
                        .clickable { m.demoOpen = false }.padding(vertical = 14.dp),
                    contentAlignment = Alignment.Center
                ) { Text("✕ ZAMKNIJ SZUFLADĘ DEMO", fontSize = 13.sp, fontWeight = FontWeight.ExtraBold, color = Color.White) }
            }
        }
    }
}

/** `#demo h5`. */
@Composable
private fun H5(text: String) {
    Text(text, fontSize = 9.5.sp, fontWeight = FontWeight.ExtraBold, color = Pal.TextDim,
        letterSpacing = 0.8.sp, modifier = Modifier.fillMaxWidth().padding(top = 14.dp, bottom = 6.dp))
}

/** `#demo label` + `input[type=checkbox]`. */
@Composable
private fun Chk(label: String, checked: Boolean, onChange: (Boolean) -> Unit) {
    Row(
        Modifier.fillMaxWidth().padding(vertical = 5.dp).clickable { onChange(!checked) },
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Box(
            Modifier.size(16.dp).background(if (checked) Pal.Cyan else Pal.Surface2, RoundedCornerShape(4.dp))
                .border(BorderStroke(1.dp, if (checked) Pal.Cyan else Pal.BorderStrong), RoundedCornerShape(4.dp)),
            contentAlignment = Alignment.Center
        ) { if (checked) Text("✓", fontSize = 11.sp, color = Color(0xFF00131F), fontWeight = FontWeight.ExtraBold) }
        Text(label, fontSize = 11.5.sp, color = Pal.Text)
    }
}

/** `.rng` — suwak `real[key] / S[key]` z wartością w monospace. */
@Composable
private fun Rng(m: AppModel, label: String, key: String, min: Double, max: Double, unit: String) {
    val S = m.S
    var v by remember(key) { mutableStateOf(S.num(key).toFloat()) }
    Column(Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Text(label, fontSize = 11.sp, color = Pal.TextDim, modifier = Modifier.weight(1f))
            Text("${v.toInt()} $unit", style = Txt.monoSm.copy(color = Color.White, fontWeight = FontWeight.Medium))
        }
        androidx.compose.material3.Slider(
            value = v,
            onValueChange = { n ->
                v = Math.round(n).toFloat()
                S.setNum(key, v.toDouble())
                S.syncReal(); S.bump()
            },
            valueRange = min.toFloat()..max.toFloat(),
            colors = androidx.compose.material3.SliderDefaults.colors(
                thumbColor = Pal.Accent, activeTrackColor = Pal.Accent, inactiveTrackColor = Pal.Surface3
            )
        )
    }
}

/** `presets[]` z `buildDemo()` — 9 scenariuszy 1-klik (ciała `apply()` 1:1). */
private class Preset(val name: String, val desc: String, val apply: (com.sterownikco.pro.core.PiecState) -> Unit)

private val PRESETS = listOf(
    Preset("🟢 Standardowa praca", "58°C, Pompa ON, Auto") { S ->
        S.online = true; S.t_ogrz = 58.4; S.real["t_ogrz"] = 58.4; S.t_bojler = 48.6; S.real["t_bojler"] = 48.6
        S.pompa = true; S.wybor = 3; S.tryb_serwa = 1; S.klapa = 81; S.dym_alarm = false; S.alarm_ogrzewanie = false
    },
    Preset("⏱️ Trociniak (Czasowy)", "Cykl 10m ON / 30m OFF") { S ->
        S.online = true; S.wybor = 1; S.czasOn = 10; S.czasOff = 30; S.pompa = true
    },
    Preset("🌡️ Kopciuch (Temp.)", "Start 60°C / Stop 50°C") { S ->
        S.online = true; S.wybor = 2; S.tempOn = 60; S.tempOff = 50; S.t_ogrz = 62.0; S.real["t_ogrz"] = 62.0; S.pompa = true
    },
    Preset("🔥 Pełne palenie", "78°C, Klapa 85%, Grzanie") { S ->
        S.online = true; S.t_ogrz = 78.0; S.real["t_ogrz"] = 78.0; S.t_bojler = 56.0; S.real["t_bojler"] = 56.0
        S.pompa = true; S.klapa = 153; S.syberka = 36; S.alarm_ogrzewanie = false
    },
    Preset("🚨 Alarm przegrzania", "88°C, Alarm kotła") { S ->
        S.online = true; S.t_ogrz = 88.5; S.real["t_ogrz"] = 88.5; S.alarm_ogrzewanie = true; S.pompa = true
    },
    Preset("💨 Alarm dymu", "Dym 2400 ADC, Syrena") { S ->
        S.online = true; S.dym = 2400.0; S.real["dym"] = 2400.0; S.dym_alarm = true
    },
    Preset("☀️ Letni dzień", "Panel 68°C, Kocioł zimny") { S ->
        S.online = true; S.night = false; S.t_ogrz = 24.0; S.real["t_ogrz"] = 24.0
        S.t_panel = 68.0; S.real["t_panel"] = 68.0; S.t_zewn = 28.0; S.pompa = false
    },
    Preset("❄️ Mroźna noc", "-12°C zewn., Księżyc") { S ->
        S.online = true; S.night = true; S.t_zewn = -12.4; S.real["t_zewn"] = -12.4; S.t_ogrz = 42.0; S.real["t_ogrz"] = 42.0
    },
    Preset("🔌 Brak zasilania", "Offline / STALE") { S -> S.online = false }
)
