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
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.unit.sp
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.core.WifiNet
import com.sterownikco.pro.core.DLOG_CATEGORIES
import com.sterownikco.pro.core.Prefs
import com.sterownikco.pro.ui.icons.AppIcon
import com.sterownikco.pro.ui.components.Note
import com.sterownikco.pro.ui.components.SectionHeader
import com.sterownikco.pro.ui.components.UstBtn
import com.sterownikco.pro.ui.theme.Pal
import com.sterownikco.pro.ui.theme.Txt

/* ══════════════════════════════════════════════════════════════════════════
   Arkusze systemowe: WI-FI, TELEGRAM, TERMINAL, LOGI, OTA, SESJA
   (port sekcji 10 A–D Piec.html: 5591-6790).
   ══════════════════════════════════════════════════════════════════════════ */

/** `.badge-on` / `.badge-off` / `.badge-known`. */
@Composable
private fun NetBadge(text: String, kind: String) {
    val (fg, bg, bd) = when (kind) {
        "on" -> Triple(Color(0xFF45D98B), Color(0x3345D98B), Color(0x4D45D98B))
        "known" -> Triple(Pal.Cyan, Pal.rgba(0, 212, 245, .15f), Pal.rgba(0, 212, 245, .3f))
        else -> Triple(Color(0xFF94A3B8), Color(0x2694A3B8), Color(0x4094A3B8))
    }
    Box(
        Modifier.background(bg, RoundedCornerShape(4.dp)).border(BorderStroke(1.dp, bd), RoundedCornerShape(4.dp))
            .padding(horizontal = 6.dp, vertical = 2.dp)
    ) {
        Text(text, fontSize = 10.sp, fontWeight = FontWeight.Bold, color = fg)
    }
}

/** `.wifi-sig` — 4 kreski 3×(4,7,10,14) px, `.on` → cyan + poświata. */
@Composable
fun WifiSig(rssi: Int, modifier: Modifier = Modifier) {
    val lvl = when {
        rssi >= -50 -> 4; rssi >= -60 -> 3; rssi >= -70 -> 2; rssi >= -80 -> 1; else -> 0
    }
    Row(modifier.height(14.dp), verticalAlignment = Alignment.Bottom, horizontalArrangement = Arrangement.spacedBy(2.dp)) {
        listOf(4, 7, 10, 14).forEachIndexed { i, h ->
            Box(
                Modifier.width(3.dp).height(h.dp).background(
                    if (i + 1 <= lvl) Pal.Cyan else Color(0x26FFFFFF), RoundedCornerShape(1.dp)
                )
            )
        }
    }
}

/** `.auth-field` + `.auth-input` (label 10 px dim bold nad polem). */
@Composable
fun SheetField(
    label: String,
    value: String,
    onValue: (String) -> Unit,
    placeholder: String = "",
    password: Boolean = false,
    trailing: (@Composable () -> Unit)? = null
) {
    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
        if (label.isNotEmpty()) Text(label, fontSize = 10.sp, fontWeight = FontWeight.Bold, color = Pal.TextDim)
        if (trailing != null) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                FieldBox(value, onValue, placeholder, password, Modifier.weight(1f))
                trailing()
            }
        } else FieldBox(value, onValue, placeholder, password, Modifier.fillMaxWidth())
    }
}

@Composable
private fun FieldBox(
    value: String,
    onValue: (String) -> Unit,
    placeholder: String,
    password: Boolean,
    modifier: Modifier
) {
    OutlinedTextField(
        value = value,
        onValueChange = onValue,
        modifier = modifier,
        singleLine = true,
        placeholder = if (placeholder.isNotEmpty()) {
            { Text(placeholder, style = Txt.inp.copy(fontSize = 12.sp), color = Pal.TextDim2) }
        } else null,
        textStyle = Txt.inp.copy(fontSize = 13.sp),
        shape = RoundedCornerShape(10.dp),
        visualTransformation = if (password) PasswordVisualTransformation() else VisualTransformation.None,
        keyboardOptions = KeyboardOptions(
            keyboardType = if (password) KeyboardType.Password else KeyboardType.Text,
            imeAction = ImeAction.Done
        ),
        colors = OutlinedTextFieldDefaults.colors(
            focusedBorderColor = Pal.Cyan, unfocusedBorderColor = Pal.rgba(0, 180, 220, .25f),
            unfocusedContainerColor = Pal.rgba(6, 13, 24, .7f), focusedContainerColor = Pal.rgba(6, 13, 24, .7f),
            focusedTextColor = Pal.White, unfocusedTextColor = Pal.White, cursorColor = Pal.Cyan
        ),
    )
}

/** `.terminal-btn` (7×12 px, radius 8, 11 px bold; `.primary` = gradient #0070ff→#00d4f5). */
@Composable
fun TermBtn(
    label: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    variant: String = "plain",
    enabled: Boolean = true
) {
    val primary = variant == "primary"
    val bg: Brush = when {
        primary -> Brush.linearGradient(listOf(Color(0xFF0070FF), Pal.Cyan))
        variant == "danger" -> SolidColor(Pal.rgba(255, 77, 109, .15f))
        else -> SolidColor(Pal.Surface2)
    }
    val bd = when (variant) {
        "danger" -> Pal.rgba(255, 77, 109, .4f); "primary" -> Color.Transparent; else -> Pal.Border
    }
    val fg = when (variant) { "primary" -> Color(0xFF03101C); "danger" -> Color(0xFFFF4D6D); else -> Pal.Text }
        .let { if (enabled) it else it.copy(alpha = .4f) }
    Box(
        modifier = modifier.background(bg, RoundedCornerShape(8.dp))
            .border(BorderStroke(1.dp, if (enabled) bd else Pal.Border), RoundedCornerShape(8.dp))
            .clickable(enabled = enabled) { onClick() }
            .padding(horizontal = 12.dp, vertical = 7.dp),
        contentAlignment = Alignment.Center
    ) { Text(label, fontSize = 11.sp, fontWeight = FontWeight.Bold, color = fg, maxLines = 1, overflow = TextOverflow.Ellipsis) }
}

/** `.wifi-row` (skaner / lista zapisanych). */
@Composable
private fun WifiRow(
    net: WifiNet,
    showSig: Boolean,
    active: Boolean = false,
    onClick: (() -> Unit)? = null,
    onDelete: (() -> Unit)? = null
) {
    Row(
        Modifier.fillMaxWidth()
            .background(if (active) Pal.rgba(0, 212, 245, .08f) else Color(0xB3111F35), RoundedCornerShape(10.dp))
            .border(BorderStroke(1.dp, if (active) Pal.rgba(0, 212, 245, .6f) else Pal.Border), RoundedCornerShape(10.dp))
            .then(if (onClick != null) Modifier.clickable { onClick() } else Modifier)
            .padding(10.dp, 12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Row(Modifier.weight(1f), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            com.sterownikco.pro.ui.icons.AppIcon(
                "wifi", size = 14.dp, tint = if (active) Pal.Cyan else Pal.TextDim
            )
            Text(net.ssid, fontSize = 13.sp, fontWeight = FontWeight.SemiBold, color = Pal.Text)
            if (active) NetBadge("aktywna", "on")
            if (net.known && onDelete == null) NetBadge("znana", "known")
            if (!net.hasPass) NetBadge(if (onDelete == null) "otwarta" else "bez hasła", "off")
        }
        if (net.rssi != 0) {
            Text("${net.rssi} dBm", style = Txt.monoSm)
            if (showSig) WifiSig(net.rssi)
        }
        if (onDelete != null) {
            Spacer(Modifier.width(6.dp))
            TermBtn("Usuń", variant = "danger", onClick = onDelete)
        }
    }
}

/** `showWifiSheet()` — karta stanu, skaner, dodawanie sieci, NVS. */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun WifiSheet(m: AppModel) {
    var ssid by remember { mutableStateOf("") }
    var pass by remember { mutableStateOf("") }
    LaunchedEffect(Unit) { m.wifiLoad() }

    val cur = m.wifiSaved.firstOrNull { it.active }
    val curRssi = cur?.rssi?.takeIf { it != 0 } ?: m.S.wifi_rssi
    Column(Modifier.fillMaxWidth().background(Pal.Surface2, RoundedCornerShape(12.dp))
        .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(12.dp)).padding(14.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Row(Modifier.weight(1f), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(
                    m.wifiActiveSsid(), fontSize = 15.sp, fontWeight = FontWeight.Bold, color = Pal.Text,
                    modifier = Modifier.weight(1f), maxLines = 1, overflow = TextOverflow.Ellipsis
                )
                NetBadge("POŁĄCZONO", "on")
            }
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Text("$curRssi dBm", style = Txt.monoSm)
                WifiSig(curRssi)
            }
        }
        FlowRow(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(14.dp),
            verticalArrangement = Arrangement.spacedBy(4.dp)
        ) {
            Text(
                "IP: ${if (m.S.ip.isEmpty()) "—" else m.S.ip}", style = Txt.cardDesc, color = Pal.Text,
                maxLines = 1, overflow = TextOverflow.Ellipsis
            )
            Text("Źródło: /api/wifi/list (centrala)", style = Txt.cardDesc,
                maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text("Tryb: STA (Klient)", style = Txt.cardDesc.copy(color = Pal.Cyan),
                maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text("MAC: 48:E7:29:B1:0A:F4", style = Txt.monoSm,
                maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
    }

    SectionHeader("Skaner sieci radiowych")
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        UstBtn(
            label = if (m.wifiBusy) "🔄 Skanowanie…" else "🔄 Skanuj sieci Wi-Fi",
            variant = "primary", enabled = !m.wifiBusy,
            onClick = { m.wifiScanStart() }, modifier = Modifier.weight(1f)
        )
        Text(m.wifiScanStatus, style = Txt.note)
    }
    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        when {
            m.wifiBusy -> Box(Modifier.fillMaxWidth().padding(16.dp), contentAlignment = Alignment.Center) {
                Text("🔍 Przeszukiwanie pasma 2.4 GHz…", style = Txt.note, color = Pal.Cyan)
            }
            m.wifiScanNote != null -> Text(
                m.wifiScanNote ?: "", style = Txt.note,
                color = if (m.wifiScanNote!!.startsWith("Centrala nieosiągalna") || m.wifiScanNote!!.startsWith("Nie udało się")) Pal.Err else Pal.Warn,
                modifier = Modifier.padding(8.dp)
            )
            m.wifiScan.isEmpty() -> Text(
                "Kliknij „Skanuj sieci Wi-Fi”, aby wyszukać nadajniki w zasięgu kotłowni.",
                style = Txt.note, modifier = Modifier.padding(8.dp)
            )
            else -> m.wifiScan.forEach { net ->
                WifiRow(net = net, showSig = true, onClick = { m.wifiPick(net); ssid = net.ssid })
            }
        }
    }

    SectionHeader("Dodaj nową sieć Wi-Fi")
    Column(
        Modifier.fillMaxWidth().background(Pal.Surface2, RoundedCornerShape(10.dp))
            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(10.dp)).padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        SheetField("NAZWA SIECI (SSID)", ssid, { ssid = it }, "Wpisz lub wybierz SSID z listy powyżej")
        SheetField("HASŁO WPA2 / WPA3", pass, { pass = it }, "Wpisz hasło do sieci Wi-Fi", password = true)
        UstBtn("➕ Połącz i zapisz w sterowniku", variant = "primary", modifier = Modifier.fillMaxWidth(), onClick = {
            m.wifiAdd(ssid, pass); ssid = ""; pass = ""
        })
    }

    SectionHeader("Zapisane sieci w pamięci NVS ESP32")
    var pending by remember { mutableStateOf<String?>(null) }
    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        if (m.wifiSaved.isEmpty() && m.wifiLoading) {
            Text("Pobieranie zapisanych sieci z centrali…", style = Txt.note, color = Pal.Cyan, modifier = Modifier.padding(8.dp))
        } else if (m.wifiSaved.isEmpty() && m.wifiSavedNote == null) {
            Text("Brak zapisanych sieci w pamięci sterownika.", style = Txt.note, modifier = Modifier.padding(8.dp))
        } else m.wifiSaved.forEach { net ->
            WifiRow(
                net = net, showSig = true, active = net.active,
                onDelete = { pending = net.ssid }
            )
        }
        m.wifiSavedNote?.let { Note(it) }
    }
    pending?.let { ssidDel ->
        Column(Modifier.fillMaxWidth().background(Pal.rgba(255, 77, 109, .10f), RoundedCornerShape(10.dp))
            .border(BorderStroke(1.dp, Pal.rgba(255, 77, 109, .4f)), RoundedCornerShape(10.dp)).padding(12.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Czy na pewno usunąć sieć \"$ssidDel\" z pamięci sterownika?", style = Txt.note, color = Pal.Text)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                UstBtn("Usuń", variant = "danger", modifier = Modifier.weight(1f), onClick = {
                    m.wifiDelete(ssidDel); pending = null
                })
                UstBtn("Anuluj", modifier = Modifier.weight(1f), onClick = { pending = null })
            }
        }
    }
}

/** `showTelegramSheet()` — przełącznik, formularz bota, podgląd dymka, katalog zdarzeń. */
@Composable
fun TelegramSheet(m: AppModel) {
    var token by remember { mutableStateOf(m.tgToken) }
    var chatId by remember { mutableStateOf(m.tgChatId) }
    var showToken by remember { mutableStateOf(false) }

    CheckLike(m, "Powiadomienia Telegram aktywne", "Powiadomienia push i zdalne komendy bota na smartfonie")
    SectionHeader("Konfiguracja bota Telegram")
    SheetField(
        label = "BOT TOKEN (z @BotFather)", value = token, onValue = { token = it },
        placeholder = "123456789:ABCdefGhIJKlmNoPQRsTUVwxyZ", password = !showToken,
        trailing = { TermBtn(if (showToken) "🙈" else "👁️", onClick = { showToken = !showToken }) }
    )
    SheetField("CHAT ID OPERATORA (lub grupy)", chatId, { chatId = it }, "np. 987654321 lub -1001234567890")
    UstBtn("💾 Zapisz konfigurację Telegram", variant = "primary", modifier = Modifier.fillMaxWidth(), onClick = {
            if (token.isBlank() || chatId.isBlank()) {
                m.showToast("Telegram", "Wypełnij Token i Chat ID", "warn")
            } else {
                m.tgToken = token; m.tgChatId = chatId
                val payload = org.json.JSONObject().put("token", token).put("chatId", chatId).put("enabled", m.tgEnabled)
                if (m.connected) {
                    m.send("tg_config $token $chatId ${if (m.tgEnabled) 1 else 0}")
                    m.showToast("Telegram", "Wysłano konfigurację do centrali (Firebase)", "ok")
                } else m.tgSave(payload, "Zapisano konfigurację w NVS centrali")
            }
        })
    UstBtn("📨 Wyślij testowy raport", modifier = Modifier.fillMaxWidth(), onClick = { m.tgSendTest() })

    SectionHeader("Podgląd generowanego raportu Telegram")
    Column(
        Modifier.fillMaxWidth().background(Pal.BubbleBg, RoundedCornerShape(12.dp))
            .border(BorderStroke(1.dp, Pal.rgba(42, 171, 238, .3f)), RoundedCornerShape(12.dp)).padding(14.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("✈️", fontSize = 14.sp)
            Text("@SterownikCO_Bot · Telegram Messenger", fontSize = 12.sp, fontWeight = FontWeight.Bold, color = Pal.Telegram)
        }
        Box(
            Modifier.fillMaxWidth().background(Color(0x0DFFFFFF), RoundedCornerShape(8.dp))
                .padding(0.dp)
        ) {
            Row(Modifier.fillMaxWidth()) {
                Box(Modifier.width(3.dp).fillMaxHeight().background(Pal.Telegram))
                Text(m.telegramReport(), Modifier.padding(start = 10.dp, top = 12.dp, bottom = 12.dp, end = 4.dp),
                    style = Txt.term.copy(color = Pal.Text, lineHeight = 18.sp))
            }
        }
    }

    SectionHeader("Wykaz zdarzeń wysyłanych na Telegram")
    listOf(
        "🚨" to ("Alarm przegrzania kotła" to "Natychmiastowy alert push > progAlarmTemp"),
        "💨" to ("Alarm zadymienia kotłowni" to "Wykrycie dymu ADC powyżej progu"),
        "🔄" to ("Raport restartu ESP32" to "Przyczyna rebootu, sloty schedulera, uptime"),
        "📦" to ("Nowe wydanie OTA GitHub" to "Powiadomienie o nowej wersji i komenda /update"),
        "📉" to ("Wykresy & Dziennik nastaw" to "Pliki hist_short.csv, hist_long.csv, log_ustawien"),
        "🕹️" to ("Zdalne komendy operatorskie" to "/pompa, /tryb, /klapa, /syberek, /coredumpy")
    ).forEach { (ico, pair) ->
        Row(
            Modifier.fillMaxWidth().background(Pal.Surface2, RoundedCornerShape(8.dp))
                .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(8.dp)).padding(8.dp, 12.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Text(ico, fontSize = 18.sp)
            Column {
                Text(pair.first, fontSize = 11.sp, fontWeight = FontWeight.Bold, color = Pal.Text)
                Text(pair.second, fontSize = 10.sp, color = Pal.TextDim)
            }
        }
    }
}

@Composable
private fun CheckLike(m: AppModel, name: String, desc: String) {
    com.sterownikco.pro.ui.components.CheckRow(name, desc, m.tgEnabled) { v ->
        m.tgEnabled = v
        m.tgSave(org.json.JSONObject().put("enabled", v), if (v) "Włączono powiadomienia Telegram" else "Wyłączono powiadomienia Telegram")
    }
}

/** `.diag-card` (lbl / val / sub) — używane w logach i sesji. */
@Composable
fun DiagCard(lbl: String, value: String, sub: String) {
    Column(
        Modifier.fillMaxWidth().background(Pal.Surface2, RoundedCornerShape(12.dp))
            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(12.dp)).padding(12.dp, 14.dp)
    ) {
        Text(lbl, style = Txt.diagLbl)
        Text(value, style = Txt.diagVal, modifier = Modifier.padding(top = 4.dp))
        Text(sub, style = Txt.diagSub, modifier = Modifier.padding(top = 4.dp))
    }
}

/** selektor `select` z paska narzędzi terminala. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun Sel(label: String, options: List<Pair<String, String>>, current: String, modifier: Modifier = Modifier, onPick: (String) -> Unit) {
    var open by remember { mutableStateOf(false) }
    Column(modifier, verticalArrangement = Arrangement.spacedBy(3.dp)) {
        Text(label, fontSize = 10.sp, fontWeight = FontWeight.Bold, color = Pal.TextDim)
        Box(Modifier.fillMaxWidth().background(Color(0xFF091524), RoundedCornerShape(8.dp))
            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(8.dp))
            .clickable { open = true }.padding(horizontal = 10.dp, vertical = 6.dp)) {
            Text(options.firstOrNull { it.first == current }?.second ?: current, fontSize = 12.sp, color = Pal.Text,
                maxLines = 1, overflow = TextOverflow.Ellipsis)
            DropdownMenu(expanded = open, onDismissRequest = { open = false }) {
                options.forEach { (v, lbl) ->
                    DropdownMenuItem(text = { Text(lbl, fontSize = 12.sp, color = Pal.Text) }, onClick = {
                        open = false; onPick(v)
                    })
                }
            }
        }
    }
}

/** `showTerminalSheet()` — 21 kategorii DLOG, konsola, wiersz poleceń. */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun TerminalSheet(m: AppModel) {
    var cmd by remember { mutableStateOf("") }
    LaunchedEffect(Unit) { m.terminalSeed() }
    val listState = rememberLazyListState()
    LaunchedEffect(m.terminalDisplay.size, m.termPaused) {
        if (!m.termPaused && m.terminalDisplay.isNotEmpty()) listState.scrollToItem(m.terminalDisplay.size - 1)
    }

    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        // 1. nagłówek + status
        Row(Modifier.fillMaxWidth().padding(bottom = 8.dp)
            .border(BorderStroke(width = 1.dp, color = Pal.Border), RoundedCornerShape(bottomStart = 0.dp, bottomEnd = 0.dp)),
            verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text("💻 Terminal Zdalny ESP32", fontSize = 15.sp, fontWeight = FontWeight.ExtraBold, color = Pal.Text)
                Text("sesja ${m.termSession} · /piec/telemetry/diagnostics", style = Txt.cardDesc, modifier = Modifier.padding(top = 2.dp))
            }
            Row(Modifier.background(Color(0x40000000), RoundedCornerShape(8.dp))
                .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(8.dp)).padding(horizontal = 10.dp, vertical = 4.dp),
                verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                Box(Modifier.size(9.dp).background(if (m.termOpened) Color(0xFF45D98B) else Color(0xFF64748B), CircleShape))
                Text(if (m.termOpened) "ON" else "OFF", style = Txt.monoSm.copy(color = Pal.TextDim, fontWeight = FontWeight.ExtraBold))
            }
        }

        // 2. pasek akcji + selektory, pionowo: dwie kolumny obok siebie na 360 dp
        // sciskaly przyciski ("Wyczyść" lamal sie na "Wycz/ysc", selektory ucinaly).
        Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                TermBtn("▶ Włącz Remote", modifier = Modifier.weight(1f), variant = "primary", enabled = !m.termOpened,
                    onClick = { m.terminalRemoteOn() })
                TermBtn("■ Wyłącz", modifier = Modifier.weight(1f), variant = "danger", enabled = m.termOpened,
                    onClick = { m.terminalRemoteOff() })
            }
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                TermBtn(if (m.termPaused) "▶ Wznów" else "Ⅱ Pauza", modifier = Modifier.weight(1f),
                    onClick = { m.terminalTogglePause() })
                TermBtn("⌫ Wyczyść", modifier = Modifier.weight(1f), onClick = { m.terminalClear() })
            }
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Sel("SESJA", listOf(
                    "20261005_120000" to "20261005_120000 · najnowsza",
                    "20261005_100000" to "20261005_100000 · boot #14",
                    "20261004_180000" to "20261004_180000 · boot #13"
                ), m.termSession, Modifier.weight(1f)) { m.termSession = it }
                Sel("POZIOM DLOG", listOf(
                    "TRACE" to "TRACE (wszystko)", "DEBUG" to "DEBUG", "INFO" to "INFO",
                    "WARN" to "WARN", "ERR" to "ERR (tylko błędy)"
                ), m.termLevel, Modifier.weight(1f)) { m.terminalSetLevel(it) }
            }
        }

        // 3. filtr 21 kategorii
        Column(Modifier.fillMaxWidth().background(Color(0x99091524), RoundedCornerShape(10.dp))
            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(10.dp)).padding(10.dp, 12.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)) {
            // Etykieta nad przyciskami: w jednym wierszu dostawala ~60 dp
            // i lamala sie na sylaby ("FILTR/21/KATEG/ORII/DLOG/(ESP3/2)").
            Text("FILTR 21 KATEGORII DLOG (ESP32)", fontSize = 11.sp, fontWeight = FontWeight.ExtraBold, color = Pal.Cyan, letterSpacing = 0.5.sp)
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                TermBtn("✓ Wszystkie DLOG", modifier = Modifier.weight(1f), onClick = { m.terminalSetAllCategories(true) })
                TermBtn("□ Wyłącz wszystkie", modifier = Modifier.weight(1f), onClick = { m.terminalSetAllCategories(false) })
            }
            FlowRow(horizontalArrangement = Arrangement.spacedBy(5.dp), verticalArrangement = Arrangement.spacedBy(5.dp)) {
                DLOG_CATEGORIES.forEach { cat ->
                    val on = m.termCats[cat] != false
                    Box(Modifier.background(if (on) Pal.rgba(0, 212, 245, .12f) else Color(0xCC111F35), RoundedCornerShape(6.dp))
                        .border(BorderStroke(1.dp, if (on) Pal.rgba(0, 212, 245, .4f) else Color(0x14FFFFFF)), RoundedCornerShape(6.dp))
                        .clickable { m.terminalSetCategory(cat, !on) }
                        .padding(horizontal = 8.dp, vertical = 3.dp)) {
                        Text((if (on) "☑ " else "☐ ") + cat, fontSize = 10.sp, fontFamily = Txt.mono,
                            fontWeight = FontWeight.SemiBold, color = if (on) Pal.Cyan else Pal.TextDim)
                    }
                }
            }
        }

        // 4. metryki (FlowRow: 6 kostek nie miesci sie w jednym wierszu 360 dp)
        val sec = if (m.termAutoOffAt > 0) ((m.termAutoOffAt - System.currentTimeMillis()) / 1000).coerceAtLeast(0) else -1L
        FlowRow(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            listOf(
                "SEQ: " + (if (m.termSeq >= 0) m.termSeq.toString() else "—"),
                m.terminalFmtBytes(m.termBytes),
                "${m.termLines} linii",
                "Luki: ${m.termGaps}",
                "LIVE: " + (if (m.termOpened) "aktywny" else "—"),
                "Auto-OFF: " + if (sec >= 0) String.format(java.util.Locale.US, "%d:%02d", sec / 60, sec % 60) else "—"
            ).forEach {
                Box(Modifier.background(Color(0x99111F35), RoundedCornerShape(6.dp))
                    .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(6.dp)).padding(horizontal = 8.dp, vertical = 4.dp)) {
                    Text(it, fontSize = 10.sp, fontFamily = Txt.mono, fontWeight = FontWeight.Bold, color = Pal.TextDim)
                }
            }
        }

        // 5. konsola + pasek komend + podpowiedzi
        Column(Modifier.fillMaxWidth().background(Color(0xFF040911), RoundedCornerShape(12.dp))
            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(12.dp))) {
            LazyColumn(state = listState, modifier = Modifier.fillMaxWidth().height(300.dp).padding(14.dp)) {
                items(m.terminalDisplay.toList()) { line ->
                    val cls = m.terminalLineClass(line)
                    val c = when (cls) {
                        "t-err" -> Color(0xFFFF6B81); "t-warn" -> Color(0xFFFBBF24)
                        "t-debug" -> Color(0xFF4ADE80); "t-trace" -> Color(0xFF94A3B8)
                        else -> Color(0xFF67E8F9)
                    }
                    Row {
                        Box(Modifier.background(Color(0x14FFFFFF), RoundedCornerShape(4.dp)).padding(horizontal = 4.dp)) {
                            Text("[" + m.terminalCategory(line) + "]", fontSize = 10.sp, fontFamily = Txt.mono, color = c)
                        }
                        Text(line, fontSize = 11.5.sp, fontFamily = Txt.mono,
                            lineHeight = 17.sp, color = c, fontWeight = if (cls == "t-err") FontWeight.Bold else FontWeight.Normal)
                    }
                }
            }
            Row(
                Modifier.fillMaxWidth().background(Color(0xFF0A1728), RoundedCornerShape(bottomStart = 12.dp, bottomEnd = 12.dp))
                    .padding(horizontal = 12.dp, vertical = 8.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                OutlinedTextField(
                    value = cmd, onValueChange = { cmd = it }, modifier = Modifier.weight(1f), singleLine = true,
                    placeholder = { Text("Wpisz komendę np. diag remote on, klapa 50, pompa_wl, status, update_panel…",
                        style = Txt.inp.copy(fontSize = 11.sp), color = Pal.TextDim2) },
                    textStyle = Txt.inp.copy(fontSize = 12.sp),
                    shape = RoundedCornerShape(8.dp),
                    keyboardOptions = KeyboardOptions(imeAction = ImeAction.Send),
                    keyboardActions = KeyboardActions(onSend = { m.terminalExec(cmd); cmd = "" }),
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedBorderColor = Pal.Cyan, unfocusedBorderColor = Pal.Border,
                        unfocusedContainerColor = Color(0xFF040911), focusedContainerColor = Color(0xFF040911),
                        focusedTextColor = Pal.White, unfocusedTextColor = Pal.White, cursorColor = Pal.Cyan
                    ),
                )
                TermBtn("Wyślij", variant = "primary", onClick = { m.terminalExec(cmd); cmd = "" })
            }
            FlowRow(Modifier.fillMaxWidth().background(Color(0xFF0A1728)).padding(horizontal = 12.dp, vertical = 10.dp),
                horizontalArrangement = Arrangement.spacedBy(5.dp), verticalArrangement = Arrangement.spacedBy(5.dp)) {
                listOf("diag remote on", "diag remote off", "pompa_wl", "pompa_wyl", "pompa_auto", "klapa 50", "syberek 50", "status", "diag status", "update_panel")
                    .forEach { c ->
                        Box(Modifier.background(Color(0x0DFFFFFF), RoundedCornerShape(5.dp))
                            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(5.dp))
                            .clickable { cmd = c; m.terminalExec(c); cmd = "" }
                            .padding(horizontal = 7.dp, vertical = 2.dp)) {
                            Text(c, fontSize = 10.sp, fontFamily = Txt.mono, color = Pal.TextDim)
                        }
                    }
                }
        }
        Text(m.termStatus, style = Txt.note.copy(fontSize = 11.sp),
            color = when (m.termStatusKind) { "ok" -> Color(0xFF45D98B); "warn" -> Pal.Warn; "err" -> Color(0xFFFF6B81); else -> Pal.TextDim })
    }
}

/** `showLogsSheet()` — diagnostyka + filtr + strumień `systemLogs`. */
@Composable
fun LogsSheet(m: AppModel) {
    DiagCard(
        lbl = "DIAGNOSTYKA APLIKACJI",
        value = "Połączenie: " + (if (m.S.online) "LIVE" else "OFFLINE"),
        sub = "IP: ${m.S.ip} · Bufor telemetrii: ${m.chartPoints} próbek · RSSI: ${m.S.wifi_rssi} dBm"
    )
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        listOf("all" to "Wszystkie", "system" to "System", "cmd" to "Komendy", "warn" to "Ostrzeżenia").forEach { (k, lbl) ->
            val on = m.logFilter == k
            Box(Modifier.background(if (on) Pal.rgba(0, 212, 245, .15f) else Pal.Surface2, RoundedCornerShape(8.dp))
                .border(BorderStroke(1.dp, if (on) Pal.Cyan else Pal.Border), RoundedCornerShape(8.dp))
                .clickable { m.logFilter = k }.padding(horizontal = 10.dp, vertical = 6.dp)) {
                Text(lbl, fontSize = 11.sp, fontWeight = FontWeight.Bold, color = if (on) Pal.Cyan else Pal.TextDim)
            }
        }
        OutlinedTextField(
            value = m.logSearch, onValueChange = { m.logSearch = it }, modifier = Modifier.weight(1f).height(38.dp),
            singleLine = true, placeholder = { Text("Filtruj…", style = Txt.inp.copy(fontSize = 11.sp), color = Pal.TextDim2) },
            textStyle = Txt.inp.copy(fontSize = 11.sp), shape = RoundedCornerShape(8.dp),
            colors = OutlinedTextFieldDefaults.colors(
                focusedBorderColor = Pal.Cyan, unfocusedBorderColor = Pal.Border,
                focusedTextColor = Pal.White, unfocusedTextColor = Pal.White, cursorColor = Pal.Cyan
            ),
        )
    }
    val rows = m.logsFiltered()
    Column(
        Modifier.fillMaxWidth().height(260.dp).background(Pal.rgba(6, 13, 24, .7f), RoundedCornerShape(10.dp))
            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(10.dp)).padding(10.dp, 8.dp)
    ) {
        if (rows.isEmpty()) Text("Brak wpisów w buforze logów.", style = Txt.note)
        else LazyColumn(Modifier.fillMaxWidth()) {
            items(rows) { l ->
                val c = when (l.type) { "err" -> Pal.Err; "warn" -> Pal.Warn; "ok" -> Pal.Live; else -> Pal.TextDim }
                Row(Modifier.fillMaxWidth().padding(vertical = 3.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(l.ts, style = Txt.monoSm)
                    Text(l.tag, style = Txt.monoSm.copy(color = c, fontWeight = FontWeight.Bold))
                    Text(l.msg, style = Txt.note.copy(fontSize = 10.5.sp, lineHeight = 14.sp), color = Pal.Text, modifier = Modifier.weight(1f))
                }
            }
        }
    }
    val clipboard = LocalClipboardManager.current
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        UstBtn("Kopiuj logi", modifier = Modifier.weight(1f), onClick = {
            val txt = m.logs.joinToString("\n") { "[${it.ts}] [${it.tag}] ${it.msg}" }
            clipboard.setText(androidx.compose.ui.text.AnnotatedString(txt))
            m.showToast("logi", "Skopiowano logi do schowka", "ok")
        })
        UstBtn("Wyczyść", variant = "danger", modifier = Modifier.weight(1f), onClick = { m.clearLogs() })
    }
}

/** `showOtaSheet()` — wersje, GitHub Releases, komponenty, ręczny .bin. */
@Composable
fun OtaSheet(m: AppModel) {
    LaunchedEffect(Unit) { if (m.ghRelease == null && !m.ghChecking) m.checkGithubRelease() }
    val rel = m.ghRelease
    val curTag = m.fwLabel()
    val relTag = rel?.optString("tag", "—") ?: (if (m.fwKnown()) curTag else "—")
    val upToDate = m.fwKnown() && rel != null && m.isSameTag(relTag, curTag)

    SectionHeader("Zainstalowane oprogramowanie (Bieżący soft)")
    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        listOf(
            ("CENTRALA ESP32" to curTag) to m.fwMeta(),
            ("PANEL STEROWNIKA" to "—") to "Firmware nie publikuje wersji panelu w /piec/status",
            ("APLIKACJA WEB PRO" to m.verLabel(m.S.appVersion)) to "Interfejs HTML/CSS/JS PRO",
            ("PAMIĘĆ FLASH" to "—") to "Brak danych z urządzenia"
        ).forEach { (pair, sub) ->
            Column(Modifier.fillMaxWidth().background(Pal.Surface2, RoundedCornerShape(10.dp))
                .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(10.dp)).padding(10.dp, 12.dp),
                verticalArrangement = Arrangement.spacedBy(3.dp)) {
                Text(pair.first, fontSize = 10.sp, fontWeight = FontWeight.Bold, color = Pal.TextDim)
                Text(pair.second, style = Txt.monoVal.copy(fontSize = 14.sp, color = Pal.Cyan))
                Text(sub, fontSize = 9.sp, color = Pal.TextDim)
            }
        }
    }

    SectionHeader("Wydania GitHub Releases (zielinski25/Sterownik-Pieca-C.O.)")
    Column(
        Modifier.fillMaxWidth().background(if (upToDate) Pal.Surface2 else Color(0x0D45D98B), RoundedCornerShape(12.dp))
            .border(BorderStroke(1.dp, if (upToDate) Pal.Border else Color(0x8045D98B)), RoundedCornerShape(12.dp))
            .padding(14.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        if (m.ghChecking) {
            Box(Modifier.fillMaxWidth().padding(16.dp), contentAlignment = Alignment.Center) {
                Text("🔍 Łączenie z GitHub API i sprawdzanie najnowszych wydań…", style = Txt.note, color = Pal.Cyan)
            }
        } else {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Row(Modifier.weight(1f), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    AppIcon("upload", size = 16.dp, tint = Pal.Cyan)
                    Text("Wydanie na GitHub: $relTag", style = Txt.monoVal.copy(fontSize = 14.sp, color = Pal.White))
                }
                NetBadge(
                    when {
                        !m.fwKnown() -> "❔ WERSJA CENTRALI NIEZNANA"
                        upToDate -> "✓ NAJNOWSZA WERSJA"
                        else -> "⚡ DOSTĘPNA AKTUALIZACJA"
                    },
                    if (upToDate) "on" else "known"
                )
            }
            Text(
                if (rel != null) "Tytuł: ${rel.optString("name")} · Data wydania: ${rel.optString("publishedAt")}"
                else "Sprawdź stan wydań repozytorium GitHub.",
                style = Txt.note.copy(fontSize = 11.sp)
            )
            Text("LISTA ZMIAN / RELEASE NOTES:", fontSize = 10.sp, fontWeight = FontWeight.Bold, color = Pal.Cyan)
            Box(
                Modifier.fillMaxWidth().background(Pal.TerminalBg, RoundedCornerShape(8.dp))
                    .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(8.dp)).padding(10.dp)
            ) {
                Text(rel?.optString("body", "")?.takeIf { it.isNotEmpty() }
                    ?: "Brak danych. Kliknij poniższy przycisk, aby odpytać GitHub Releases.",
                    style = Txt.term.copy(color = Color(0xFFCBD5E1), lineHeight = 16.sp))
            }
            Text("Pliki binarne w wydaniu: firmware.bin (Centrala ESP32) · firmware_panel.bin (Panel LCD)",
                style = Txt.note.copy(fontSize = 10.sp))
        }
    }
    UstBtn("🔍 Sprawdź dostępność nowej wersji na GitHub", modifier = Modifier.fillMaxWidth(), onClick = {
        m.checkGithubRelease { m.showToast("OTA", "Sprawdzono wydania na GitHub Releases", "ok") }
    })

    SectionHeader("Wybór komponentu instalacji do aktualizacji")
    var busy by remember { mutableStateOf(false) }
    listOf(
        Triple("⚡", "Centrala główna ESP32", "Aktualizuje oprogramowanie sterownika kotłowni. Pobiera plik firmware.bin z GitHub Releases i zapisuje w partycji OTA. Po wgraniu nastąpi automatyczny restart (10-15s)."),
        Triple("📱", "Panel pokojowy LCD", "Pobiera plik firmware_panel.bin z GitHub i buforuje go w centrali. Panel dotykowy zaktualizuje się automatycznie bezprzewodowo przy kolejnym połączeniu.")
    ).forEach { (ico, name, desc) ->
        Column(Modifier.fillMaxWidth().background(Pal.Surface2, RoundedCornerShape(10.dp))
            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(10.dp)).padding(12.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(ico, fontSize = 18.sp); Text(name, fontSize = 13.sp, fontWeight = FontWeight.Bold, color = Pal.White)
            }
            Text(desc, style = Txt.note.copy(fontSize = 11.sp, lineHeight = 15.sp))
            if (name.startsWith("Centrala")) {
                UstBtn("⚡ Aktualizuj Centralę (firmware.bin)", variant = "primary", enabled = !busy,
                    modifier = Modifier.fillMaxWidth(), onClick = { m.runOtaProcedure("Centrala ESP32", "firmware.bin", "update") })
            } else {
                UstBtn("📱 Przygotuj firmware Panelu", enabled = !busy,
                    modifier = Modifier.fillMaxWidth(), onClick = { m.runOtaProcedure("Panel Sterownika", "firmware_panel.bin", "update_panel") })
            }
        }
    }

    if (m.otaProgressShown) {
        Column(Modifier.fillMaxWidth().padding(vertical = 6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                Text(m.otaProgressLabel, style = Txt.note.copy(fontSize = 11.sp), color = Pal.TextDim, modifier = Modifier.weight(1f))
                Text(m.otaProgressPct, style = Txt.monoVal.copy(color = Pal.Cyan))
            }
            Box(Modifier.fillMaxWidth().height(10.dp).background(Color(0x14FFFFFF), RoundedCornerShape(5.dp))
                .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(5.dp))) {
                val frac = m.otaProgressPct.filter { it.isDigit() }.toIntOrNull()?.let { it / 100f } ?: 1f
                Box(Modifier.fillMaxWidth(frac.coerceIn(0f, 1f)).height(10.dp)
                    .background(Brush.horizontalGradient(listOf(Color(0xFF0070FF), Pal.Cyan)), RoundedCornerShape(5.dp)))
            }
        }
    }

    SectionHeader("Ręczne wgranie pliku binarnego (.bin z dysku)")
    var binPath by remember { mutableStateOf("") }
    var otaPass by remember { mutableStateOf("") }
    Column(Modifier.fillMaxWidth().background(Pal.Surface2, RoundedCornerShape(12.dp))
        .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(12.dp)).padding(14.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(
            "Bezpośrednie wgranie skompilowanego pliku firmware.bin do pamięci Flash sterownika przez interfejs HTTP:",
            style = Txt.note.copy(fontSize = 11.sp)
        )
        SheetField("PLIK .BIN", binPath, { binPath = it }, "/storage/emulated/0/Download/firmware.bin")
        SheetField("HASŁO OTA", otaPass, { otaPass = it }, "Wpisz hasło autoryzacji OTA", password = true)
        UstBtn("Wgraj wybrany plik (.bin) do ESP32", variant = "primary", modifier = Modifier.fillMaxWidth(), onClick = {
            when {
                binPath.isBlank() -> m.showToast("OTA", "Wybierz plik .bin z dysku", "warn")
                otaPass.isBlank() -> m.showToast("OTA", "Podaj hasło zabezpieczenia OTA", "warn")
                else -> m.runOtaProcedure("Plik lokalny (${binPath.substringAfterLast('/')})", binPath.substringAfterLast('/'), "ota_upload")
            }
        })
    }
}

/** `showSessionSheet()` — stan Firebase + akcje logowania. */
@Composable
fun SessionSheet(m: AppModel) {
    DiagCard(
        lbl = "POŁĄCZENIE Z FIREBASE",
        value = when {
            m.connected -> "🟢 Połączono LIVE"
            else -> "🔴 Brak sesji"
        },
        sub = "Konto: " + (m.prefs.get(Prefs.K_EMAIL) ?: "Brak") + " · IP: ${m.S.ip} · RSSI: ${m.S.wifi_rssi} dBm"
    )
    Note("Panel łączy się z bazą danych Realtime Database przy użyciu autoryzacji Firebase UserAuth. Dane potrzebne do obsługi komend są zarządzane wewnętrznie.")
    UstBtn("🔑 Połącz / Zmień dane Firebase", variant = "primary", modifier = Modifier.fillMaxWidth(), onClick = {
        m.openSheet(null); m.authOpen = true
    })
    if (m.connected) {
        UstBtn("Rozłącz i wyloguj", variant = "danger", modifier = Modifier.fillMaxWidth(), onClick = {
            m.logout()
            m.showToast("sesja", "Wylogowano operatora", "ok")
            m.openSheet(null)
        })
    }
}
