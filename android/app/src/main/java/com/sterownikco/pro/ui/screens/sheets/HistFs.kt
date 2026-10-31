package com.sterownikco.pro.ui.screens.sheets

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.ui.components.CheckRowEcho
import com.sterownikco.pro.ui.components.Note
import com.sterownikco.pro.ui.components.SectionHeader

/**
 * [0.31.64] „Zapis historii na flash" — przełącznik dopisywania
 * `hist_short.csv` i `hist_long.csv` na LittleFS centrali (firmware v3.57.4+).
 *
 * Po co: historia długa rośnie do ~1,8 MB (8000 próbek), krótka do ~130 kB.
 * Kto nie potrzebuje archiwum na flashu, wyłącza zapis i oszczędza pamięć
 * oraz cykle zapisu. **Wyłączenie nie dotyka Firebase** — telemetria leci do
 * chmury osobną ścieżką, a bufory w RAM (wykresy, `/api/history`) działają dalej.
 *
 * Stan pochodzi ze statusu (`hist_fs`). Wzorzec 1:1 z „Buforem offline":
 * po kliknięciu `CheckRowEcho` pokazuje stan docelowy, świeży status go
 * potwierdza albo echo wraca.
 */
@Composable
fun HistFsSheet(m: AppModel) {
    val S = m.S
    S.rev.intValue // subskrypcja zmian statusu
    val known = S.hasData("hist_fs")
    val on = S.histFs == 1

    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        SectionHeader("ZAPIS HISTORII NA FLASH")
        Note(
            "Centrala trzyma dwie historie na pamięci flash: krótką (24 h, ~130 kB) " +
                "i długą (archiwum, do ~1,8 MB). Ten przełącznik decyduje, czy mają " +
                "rosnąć. Wyłączenie nie zmienia wysyłki do Firebase ani wykresów " +
                "czytanych z pamięci centrali."
        )
        CheckRowEcho(
            name = "Zapisuj historię na flash",
            desc = when {
                !known -> "Ta wersja firmware nie zgłasza tego ustawienia (potrzebna 3.57.4)"
                on -> "Włączony · pliki rosną na flashu centrali"
                else -> "Wyłączony · flash nie rośnie, RAM i Firebase bez zmian"
            },
            device = on,
            known = known
        ) { v -> m.setHistFs(v) }

        SectionHeader("CZYSZCZENIE PLIKÓW")
        Button(
            onClick = { m.clearHistFiles() },
            modifier = Modifier.fillMaxWidth(),
            enabled = known
        ) {
            Text("Wyczyść oba pliki historii", modifier = Modifier.padding(vertical = 4.dp))
        }
        Note(
            "Pliki zostaną usunięte przy najbliższym restarcie centrali. Wtedy nic już " +
                "do nich nie pisze, więc nie ma ryzyka przerwanego zapisu."
        )

        SectionHeader("CO SIĘ NIE ZMIENIA")
        Note(
            "Firebase: próbki co minutę lecą do chmury niezależnie od tego ustawienia. " +
                "Pamięć RAM: wykresy „6 h / 24 h” i archiwum czytane przez /api/history " +
                "działają do restartu centrali. Zniknie tylko trwały zapis na flashu."
        )
    }
}
