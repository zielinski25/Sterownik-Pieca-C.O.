package com.sterownikco.pro.ui.screens.sheets

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.ui.components.CheckRowEcho
import com.sterownikco.pro.ui.components.Note
import com.sterownikco.pro.ui.components.SectionHeader
import com.sterownikco.pro.ui.theme.Pal

/**
 * [0.31.22] „Bufor offline" — przełącznik runtime Spool G2 w centrali.
 *
 * Centrala próbkuje telemetrię raz na minutę. Gdy nie ma internetu, próbki
 * czekają w RAM (24 h). Ten bufor zapisuje je na flash, więc przeżywają
 * restart i zanik prądu, a po powrocie sieci lecą do Firebase.
 *
 * Stan pochodzi ze statusu (`spool_rt`). Po kliknięciu CheckRowEcho pokazuje od
 * razu stan docelowy (echo), a świeży status go potwierdza — albo cofa echo po
 * 12 s. Bez tego kwadracik reagował dopiero po 8 s, gdy przyszedł kolejny
 * status, mimo że ACK z centrali był już w telefonie.
 */
@Composable
fun OfflineBufferSheet(m: AppModel) {
    val S = m.S
    S.rev.intValue // subskrypcja zmian statusu
    val known = S.hasData("spool_rt")
    val on = S.spoolRt == 1

    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        SectionHeader("BUFOR OFFLINE")
        Note(
            "Gdy centrala nie ma internetu, próbki telemetrii trafiają na jej pamięć flash. " +
                "Po powrocie sieci lecą do Firebase — nic nie ginie."
        )
        CheckRowEcho(
            name = "Bufor offline",
            desc = when {
                !known -> "Ta wersja firmware nie zgłasza tego ustawienia"
                on -> "Włączony · ustawienie zapisane w centrali"
                else -> "Wyłączony · próbki czekają tylko w pamięci RAM"
            },
            device = on,
            known = known
        ) { v -> m.setOfflineBuffer(v) }

        if (known) {
            SectionHeader("LICZNIKI OD STARTU CENTRALI")
            CounterRow("Strony zapisane na flash", S.spoolPages.toString())
            CounterRow("Próbki zapisane", S.spoolSpill.toString())
            CounterRow("Próbki odtworzone w Firebase", S.spoolReplay.toString())
            Note(
                "Próbka to jedna minuta. 768 próbek tworzy jedną stronę na flashu — " +
                    "raz na ~13 godzin bez internetu."
            )
        } else {
            Note("Zaktualizuj centralę do v3.54.0 — dopiero ta wersja ma przełącznik i liczniki.")
        }

        Note(
            "Stan jest zapisywany w pamięci NVS centrali, więc przetrwa restart i zanik prądu. " +
                "Zapisuje się tylko przy kliknięciu — nie przy każdej próbce."
        )
    }
}

@Composable
private fun CounterRow(name: String, value: String) {
    Row(
        Modifier.fillMaxWidth().padding(vertical = 2.dp),
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Text(name, fontSize = 11.sp, color = Pal.TextDim)
        Text(value, fontSize = 12.sp, fontWeight = FontWeight.Bold, color = Pal.Text)
    }
}
