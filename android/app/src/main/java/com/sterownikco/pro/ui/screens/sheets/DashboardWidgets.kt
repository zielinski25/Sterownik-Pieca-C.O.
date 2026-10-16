package com.sterownikco.pro.ui.screens.sheets

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.core.TileDefs
import com.sterownikco.pro.ui.components.CheckRow
import com.sterownikco.pro.ui.components.Note
import com.sterownikco.pro.ui.components.SectionHeader
import com.sterownikco.pro.ui.components.UstBtn

@Composable
fun DashboardWidgetsSheet(m: AppModel) {
    Column {
        Note("Zaznacz kafelki, które mają być widoczne na pulpicie. Wybór jest zapisywany na tym urządzeniu.")

        SectionHeader("Pomiary")
        TileDefs.TILES.filter { it.grid == 1 }.forEach { widget ->
            CheckRow(
                name = widget.title,
                desc = widget.description,
                checked = widget.id in m.dashboardWidgets,
                onChange = { m.setDashboardWidgetEnabled(widget.id, it) }
            )
        }

        SectionHeader("Sterowanie i bezpieczeństwo")
        TileDefs.TILES.filter { it.grid == 2 }.forEach { widget ->
            CheckRow(
                name = widget.title,
                desc = widget.description,
                checked = widget.id in m.dashboardWidgets,
                onChange = { m.setDashboardWidgetEnabled(widget.id, it) }
            )
        }

        UstBtn(
            label = "Przywróć domyślne kafelki",
            onClick = { m.resetDashboardWidgets() },
            modifier = Modifier.fillMaxWidth()
        )
    }
}
