package com.sterownikco.pro.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.ui.components.SetCard
import com.sterownikco.pro.ui.theme.Dimens
import com.sterownikco.pro.ui.theme.Pal
import com.sterownikco.pro.ui.theme.Txt

/* ══════════════════════════════════════════════════════════════════════════
   `#pg-ustawienia` (10 kart) i `#pg-wiecej` (4 karty) — kolejność, tytuły i
   opisy exactly jak w markupie Piec.html:3393-3425. `data-menu` → `openMenu`.
   ══════════════════════════════════════════════════════════════════════════ */

/** `.page-head` — tytuł + lead (po prawej opcjonalna akcja). */
@Composable
fun PageHead(title: String, lead: String, action: (@Composable () -> Unit)? = null) {
    Row(
        Modifier.fillMaxWidth().padding(start = 2.dp, end = 2.dp, top = 2.dp, bottom = 4.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(Modifier.weight(1f)) {
            Text(title, style = Txt.h2)
            Text(lead, style = Txt.lead, modifier = Modifier.padding(top = 2.dp))
        }
        if (action != null) action()
    }
}

private val Slonce = Color(0xFFFFD32A)

@Composable
private fun MenuCard(m: AppModel, menu: String, icon: String, tint: Color, title: String, desc: String) {
    SetCard(icon = icon, title = title, desc = desc, tint = tint, onClick = {
        if (menu == "firebase") m.authOpen = true else m.openSheet(menu)
    })
}

@Composable
fun SettingsPage(m: AppModel) {
    Column(
        Modifier.fillMaxWidth().verticalScroll(rememberScrollState())
            .padding(start = Dimens.pagePadH, end = Dimens.pagePadH, top = Dimens.pagePad, bottom = 16.dp),
        verticalArrangement = Arrangement.spacedBy(Dimens.gap)
    ) {
        PageHead("USTAWIENIA STEROWNIKA", "Sterowanie natywne · komendy /piec/cmd · potwierdzenie ACK")
        MenuCard(m, "alarmy", "warn", Pal.Err, "Alarmy dymu i przegrzania",
            "Dźwięk, drzemka, czuwanie w tle, uprawnienia")
        MenuCard(m, "firebase", "shield", Pal.Cyan, "Połączenie z bazą Firebase",
            "Logowanie kontem Firebase, token komend, stan połączenia")
        MenuCard(m, "wifi", "wifi", Pal.Cyan, "Sieci Wi-Fi & Łączność",
            "Skanowanie sieci, siła RSSI, konfiguracja SSID/hasło, lista sieci")
        MenuCard(m, "telegram", "telegram", Pal.Telegram, "Powiadomienia Telegram",
            "Bot powiadomień, Chat ID, testowy raport, alerty CO i dymu")
        MenuCard(m, "pompa", "pump", Pal.Cyan, "Pompa C.O.",
            "Strategia (Trociniak czasowy / Kopciuch temp.) · wymuszenia · antystop")
        MenuCard(m, "serwo", "servo", Pal.Cyan, "Serwo klapy & syberka",
            "Tryb Auto / Ręczny / Bezpieczna · automatyka histerezy")
        MenuCard(m, "mieszadlo", "mixer", Pal.Cyan, "Mieszadło",
            "Włączone · wymuszenie ręczne · cykl pracy")
        MenuCard(m, "dym", "shield", Pal.Err, "Czujnik dymu",
            "Próg alarmu · aktywacja temperaturowa · symulacja ADC")
        MenuCard(m, "ogrz", "thermo", Pal.Accent, "Przegrzanie kotła & panelu",
            "Wspólny próg bezpieczeństwa · histereza · symulacja")
        MenuCard(m, "czujniki", "outside", Slonce, "Czujniki instalacji (10 czujników)",
            "Zewnętrzna, Bojler, Pokój, Piec, Panel, Powrót, Podajnik, Ciśnienie, Wilgotność, Dym")
        MenuCard(m, "czas", "clock", Pal.TextDim, "Data i czas zegara RTC",
            "Synchronizacja zegara sterownika z czasem telefonu")
    }
}

@Composable
fun MorePage(m: AppModel) {
    Column(
        Modifier.fillMaxWidth().verticalScroll(rememberScrollState())
            .padding(start = Dimens.pagePadH, end = Dimens.pagePadH, top = Dimens.pagePad, bottom = 16.dp),
        verticalArrangement = Arrangement.spacedBy(Dimens.gap)
    ) {
        PageHead("SERWIS & DIAGNOSTYKA", "Narzędzia operatorskie, zdalny terminal 21 DLOG i aktualizacje OTA")
        MenuCard(m, "terminal", "terminal", Pal.Cyan, "Terminal diagnostyczny (21 DLOG)",
            "Remote LIVE streaming, wybór sesji, filtry 21 kategorii DLOG, poziomy TRACE-ERR i komendy /piec/cmd")
        MenuCard(m, "logi", "logs", Slonce, "Logi telemetryczne",
            "Podgląd zdarzeń sterownika na żywo · filtracja · eksport")
        MenuCard(m, "ota", "upload", Pal.Live, "Aktualizacja firmware (OTA & GitHub)",
            "Weryfikacja wersji GitHub Releases, aktualizacja Centrali (.bin) i Panelu")
        MenuCard(m, "sesja", "session", Pal.Violet, "Sesja operatora & Firebase",
            "Konto Firebase, diagnostyka sygnału RSSI i szyfrowanie")
    }
}
