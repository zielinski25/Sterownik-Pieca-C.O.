package com.sterownikco.pro.ui.screens

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.core.Prefs
import com.sterownikco.pro.core.SensorDef
import com.sterownikco.pro.ui.components.Sheet
import com.sterownikco.pro.ui.icons.AppIcon
import com.sterownikco.pro.ui.screens.sheets.CzasSheet
import com.sterownikco.pro.ui.screens.sheets.DymSheet
import com.sterownikco.pro.ui.screens.sheets.LogsSheet
import com.sterownikco.pro.ui.screens.sheets.MieszadloSheet
import com.sterownikco.pro.ui.screens.sheets.OverheatSheet
import com.sterownikco.pro.ui.screens.sheets.OtaSheet
import com.sterownikco.pro.ui.screens.sheets.PompaSheet
import com.sterownikco.pro.ui.screens.sheets.SensorListSheet
import com.sterownikco.pro.ui.screens.sheets.SensorModal
import com.sterownikco.pro.ui.screens.sheets.SerwoSheet
import com.sterownikco.pro.ui.screens.sheets.SessionSheet
import com.sterownikco.pro.ui.screens.sheets.TerminalSheet
import com.sterownikco.pro.ui.screens.sheets.TelegramSheet
import com.sterownikco.pro.ui.screens.sheets.WifiSheet
import com.sterownikco.pro.ui.theme.Pal
import com.sterownikco.pro.ui.theme.Txt

/* ══════════════════════════════════════════════════════════════════════════
   NAKŁADKI: dyspozytor arkuszy (`MENUS` + `showSheet`) i modal logowania
   Firebase (`#authModal`).
   ══════════════════════════════════════════════════════════════════════════ */

/**
 * Odpowiednik `MENUS` — identyfikatory arkuszy to dokładnie klucze obiektu
 * z Piec.html:5272-5586 (`zewn … sesja`). Arkusze otwierane z kafelków,
 * szybkich kart i zakładki „Więcej” używają tych samych id.
 */
@Composable
fun SheetHost(m: AppModel) {
    val id = m.sheet ?: return
    val sen = SensorDef.byId(id)
    val title: String
    val icon: String
    when {
        sen != null -> { title = sen.sheetTitle; icon = sen.sheetIcon }
        id == "ogrz" -> { title = "Próg przegrzania — Piec C.O."; icon = "thermo" }
        id == "panel" -> { title = "Kolektor słoneczny próżniowy"; icon = "panel" }
        else -> when (id) {
            "dym" -> { title = "Czujnik dymu i spalin"; icon = "shield" }
            "czujniki" -> { title = "Czujniki instalacji (10 czujników)"; icon = "outside" }
            "pompa" -> { title = "Pompa C.O. — Strategia i sterowanie"; icon = "pump" }
            "serwo" -> { title = "Serwo"; icon = "servo" }
            "mieszadlo" -> { title = "Mieszadło"; icon = "mixer" }
            "czas" -> { title = "Data i czas"; icon = "clock" }
            "wifi" -> { title = "Sieci Wi-Fi & Łączność"; icon = "wifi" }
            "telegram" -> { title = "Powiadomienia Telegram"; icon = "telegram" }
            "terminal" -> { title = "Zdalny Terminal Diagnostyczny (21 DLOG)"; icon = "terminal" }
            "logi" -> { title = "Logi sterownika"; icon = "logs" }
            "ota" -> { title = "Aktualizacja firmware (OTA & GitHub)"; icon = "upload" }
            "sesja" -> { title = "Sesja operatora & Firebase"; icon = "session" }
            "chartSeries" -> { title = "Wybór serii wykresu"; icon = "chart" }
            "chartAxis" -> { title = "Skala i osie wykresu"; icon = "thermo" }
            "chartTools" -> { title = "Narzędzia i filtry"; icon = "shield" }
            "chartAnalysis" -> { title = "Analiza PRO & Diagnostyka"; icon = "settings" }
            else -> { title = id; icon = "settings" }
        }
    }
    Sheet(title = title, icon = icon, onDismiss = { m.sheet = null }) {
        // `#sheetBody` w oryginale jest odświeżany przy każdej zmianie stanu;
        // w Compose wystarczy lektura licznika `S.rev`.
        m.S.rev.intValue
        when {
            sen != null -> SensorModal(m, sen)
            id == "ogrz" -> OverheatSheet(m, panel = false)
            id == "panel" -> OverheatSheet(m, panel = true)
            id == "dym" -> DymSheet(m)
            id == "czujniki" -> SensorListSheet(m)
            id == "pompa" -> PompaSheet(m)
            id == "serwo" -> SerwoSheet(m)
            id == "mieszadlo" -> MieszadloSheet(m)
            id == "czas" -> CzasSheet(m)
            id == "wifi" -> WifiSheet(m)
            id == "telegram" -> TelegramSheet(m)
            id == "terminal" -> TerminalSheet(m)
            id == "logi" -> LogsSheet(m)
            id == "ota" -> OtaSheet(m)
            id == "sesja" -> SessionSheet(m)
            id == "chartSeries" -> ChartSeriesSheet(m)
            id == "chartAxis" -> ChartAxisSheet(m)
            id == "chartTools" -> ChartToolsSheet(m)
            id == "chartAnalysis" -> ChartAnalysisSheet(m)
        }
    }
}

/**
 * `#authModal` — modal logowania Firebase UserAuth. Wartości domyślne pól
 * są takimi samymi jak w prefillu panelu HTML (`value="…"` w markupie).
 */
@Composable
fun AuthModal(m: AppModel) {
    var apiKey by remember { mutableStateOf(m.prefs.get(Prefs.K_API_KEY) ?: "AIzaSyDcheuRNcNo4mzNpaTzn-19Ntw62djkfVU") }
    var email by remember { mutableStateOf(m.prefs.get(Prefs.K_EMAIL) ?: "piec_co_boot@akwarium.local") }
    var pass by remember { mutableStateOf(m.prefs.get(Prefs.K_PASS) ?: "PcTg8plOcvRrMSojv79X") }
    var cmdToken by remember { mutableStateOf(m.prefs.get(Prefs.K_CMD_TOKEN) ?: "sterownikco-cmd-2026") }
    var rememberCreds by remember { mutableStateOf(true) }

    Box(
        Modifier.fillMaxSize().background(Color(0xD9040912)).clickable(enabled = false) {},
        contentAlignment = Alignment.Center
    ) {
        Column(
            Modifier.fillMaxWidth().padding(horizontal = 16.dp)
                .background(
                    Brush.linearGradient(listOf(Color(0xFF111F35), Color(0xFF0C1829), Color(0xFF070E1A))),
                    RoundedCornerShape(20.dp)
                )
                .border(BorderStroke(1.dp, Pal.rgba(0, 212, 245, .3f)), RoundedCornerShape(20.dp))
                .padding(24.dp, 22.dp)
        ) {
            Box(Modifier.fillMaxWidth().height(3.dp)
                .background(Brush.horizontalGradient(listOf(Color(0xFF0070FF), Pal.Cyan, Color(0xFF4ADE80))), RoundedCornerShape(2.dp)))
            Row(
                Modifier.fillMaxWidth().padding(top = 18.dp, bottom = 14.dp)
                    .border(BorderStroke(1.dp, Color(0x14FFFFFF)), RoundedCornerShape(0.dp)),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                Box(
                    Modifier.size(42.dp).background(Pal.rgba(0, 212, 245, .12f), RoundedCornerShape(12.dp))
                        .border(BorderStroke(1.dp, Pal.rgba(0, 212, 245, .25f)), RoundedCornerShape(12.dp)),
                    contentAlignment = Alignment.Center
                ) { AppIcon("shield", size = 22.dp, tint = Pal.Cyan) }
                Column(Modifier.weight(1f)) {
                    Text("POŁĄCZENIE Z PIECEM", fontSize = 15.sp, fontWeight = FontWeight.ExtraBold, color = Color.White, letterSpacing = 0.6.sp)
                    Text("Firebase Realtime Database & UserAuth", style = Txt.cardDesc, modifier = Modifier.padding(top = 2.dp))
                }
                Box(Modifier.size(32.dp).background(Color(0x0DFFFFFF), RoundedCornerShape(8.dp))
                    .border(BorderStroke(1.dp, Color(0x1AFFFFFF)), RoundedCornerShape(8.dp))
                    .clickable { m.authOpen = false }, contentAlignment = Alignment.Center) {
                    AppIcon("close", size = 16.dp, tint = Pal.TextDim)
                }
            }

            Column(Modifier.fillMaxWidth().verticalScroll(rememberScrollState())) {
                com.sterownikco.pro.ui.screens.sheets.SheetField("Firebase Web API Key", apiKey, { apiKey = it }, "AIzaSy...")
                Box(Modifier.height(12.dp))
                com.sterownikco.pro.ui.screens.sheets.SheetField("E-mail operatora", email, { email = it }, "twoj-email@domena.pl")
                Box(Modifier.height(12.dp))
                com.sterownikco.pro.ui.screens.sheets.SheetField("Hasło", pass, { pass = it }, "••••••••••••", password = true)
                Box(Modifier.height(12.dp))
                com.sterownikco.pro.ui.screens.sheets.SheetField(
                    "Token autoryzacji komend (CMD Token)", cmdToken, { cmdToken = it },
                    "Wpisz token komend /piec/cmd", password = true
                )
                Row(
                    Modifier.fillMaxWidth().padding(vertical = 14.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Box(
                        Modifier.size(18.dp).background(
                            if (rememberCreds) Pal.Cyan else Pal.Surface2, RoundedCornerShape(5.dp)
                        ).border(BorderStroke(1.dp, if (rememberCreds) Pal.Cyan else Pal.BorderStrong), RoundedCornerShape(5.dp))
                            .clickable { rememberCreds = !rememberCreds },
                        contentAlignment = Alignment.Center
                    ) {
                        if (rememberCreds) AppIcon("check", size = 12.dp, tint = Color(0xFF00131F))
                    }
                    Text("Zapamiętaj poświadczenia w tym urządzeniu", fontSize = 12.sp, color = Pal.TextDim)
                }

                val kind = m.authStatusKind
                Box(
                    Modifier.fillMaxWidth().background(
                        when (kind) {
                            "ok" -> Pal.rgba(74, 222, 128, .1f)
                            "err" -> Pal.rgba(255, 77, 109, .1f)
                            "warn" -> Pal.rgba(251, 191, 36, .1f)
                            else -> Color(0x08FFFFFF)
                        }, RoundedCornerShape(8.dp)
                    ).border(
                        BorderStroke(1.dp, when (kind) {
                            "ok" -> Pal.rgba(74, 222, 128, .3f)
                            "err" -> Pal.rgba(255, 77, 109, .3f)
                            "warn" -> Pal.rgba(251, 191, 36, .3f)
                            else -> Color(0x0FFFFFFF)
                        }), RoundedCornerShape(8.dp)
                    ).padding(horizontal = 12.dp, vertical = 8.dp)
                ) {
                    Text(
                        m.authStatus, fontSize = 11.sp, lineHeight = 15.sp,
                        color = when (kind) {
                            "ok" -> Color(0xFF4ADE80); "err" -> Color(0xFFFF4D6D)
                            "warn" -> Color(0xFFFBBF24); else -> Pal.TextDim
                        }
                    )
                }
                Box(Modifier.height(14.dp))
                AuthBtn("🔑 POŁĄCZ I ZALOGUJ", primary = true, enabled = !m.authBusy, onClick = {
                    m.login(apiKey.trim(), email.trim(), pass, cmdToken.trim(), rememberCreds)
                })
                Box(Modifier.height(8.dp))
                AuthBtn("🧪 TRYB SYMULACJI (OFFLINE)", primary = false, enabled = true, onClick = { m.useDemoFromAuth() })
            }
        }
    }
}

/** `.auth-btn` (+ `.primary` = gradient #0070ff → #00d4f5). */
@Composable
private fun AuthBtn(label: String, primary: Boolean, enabled: Boolean, onClick: () -> Unit) {
    val bg: Brush = if (primary) Brush.linearGradient(listOf(Color(0xFF0070FF), Pal.Cyan))
    else SolidColor(Pal.rgba(0, 212, 245, .08f))
    Box(
        Modifier.fillMaxWidth().height(42.dp).background(bg, RoundedCornerShape(10.dp))
            .border(BorderStroke(1.dp, if (primary) Color.Transparent else Pal.rgba(0, 212, 245, .3f)), RoundedCornerShape(10.dp))
            .clickable(enabled = enabled) { onClick() },
        contentAlignment = Alignment.Center
    ) {
        Text(
            label, fontSize = 13.sp,
            fontWeight = if (primary) FontWeight.ExtraBold else FontWeight.Bold,
            letterSpacing = 0.5.sp,
            color = if (primary) Color(0xFF03101C) else Pal.Cyan,
            modifier = Modifier.alpha(if (enabled) 1f else .5f)
        )
    }
}

