package com.sterownikco.pro.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGesturesAfterLongPress
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.layout.positionInRoot
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.zIndex
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.core.buildTiles
import com.sterownikco.pro.ui.components.Banner
import com.sterownikco.pro.ui.components.Hero
import com.sterownikco.pro.ui.components.QCard
import com.sterownikco.pro.ui.components.Sect
import com.sterownikco.pro.ui.components.SysStrip
import com.sterownikco.pro.ui.components.Trend
import com.sterownikco.pro.ui.components.UstBtn
import com.sterownikco.pro.ui.components.anyAlarm
import com.sterownikco.pro.ui.icons.AppIcon
import com.sterownikco.pro.ui.theme.Dimens
import com.sterownikco.pro.ui.theme.Pal
import com.sterownikco.pro.ui.theme.Txt
import com.sterownikco.pro.ui.components.Tile

/* ══════════════════════════════════════════════════════════════════════════
   PULPIT — `#pg-pulpit` z Piec.html: sysstrip → banery → hero → quick →
   POMIARY (g1) → STEROWANIE (g2) → ANALIZA I SYSTEM → TREND.
   ══════════════════════════════════════════════════════════════════════════ */

@Composable
fun Dashboard(m: AppModel) {
    val rev = m.S.rev.intValue // subskrypcja zmian stanu
    val S = m.S
    val servoMode = m.servoModePresentation().mode
    val weatherCode = m.weather?.current?.code ?: -1
    val isDay = m.weather?.current?.isDay ?: (!S.night)
    val art = remember(rev, servoMode, weatherCode, isDay) { S.art(weatherCode, isDay, servoMode) }
    val tiles = remember(rev, servoMode, weatherCode, isDay, m.weather, m.weatherBusy) { buildTiles(m, weatherCode, isDay) }
    val orderIndex = m.dashboardWidgetOrder.withIndex().associate { it.value to it.index }
    val selectedTiles = tiles.filter { it.id in m.dashboardWidgets }
        .sortedBy { orderIndex[it.id] ?: Int.MAX_VALUE }
    val measurementTiles = selectedTiles.filter { it.grid == 1 }
    val controlTiles = selectedTiles.filter { it.grid == 2 }
    val to = S.valOf("ogrz", "t_ogrz")
    val tb = S.valOf("bojler", "t_bojler")
    val anyAl = anyAlarm(m)
    var setpointDialogOpen by remember { mutableStateOf(false) }

    Column(
        modifier = Modifier.fillMaxWidth().verticalScroll(rememberScrollState())
            .padding(start = Dimens.pagePadH, end = Dimens.pagePadH, top = Dimens.pagePad, bottom = 16.dp),
        verticalArrangement = Arrangement.spacedBy(Dimens.gap)
    ) {
        SysStrip(
            sub = m.sysStripSub(),
            state = if (!m.isFbFresh) "OFFLINE / NIEAKTUALNE" else if (!S.online) "BRAK TELEMETRII" else if (anyAl) "ALARM" else if (S.sym.isNotEmpty()) "SYMULACJA STEROWNIKA" else if (!to.isFinite()) "OCZEKIWANIE" else if (to > 45) "GRZANIE" else "CZUWANIE",
            stateCls = if (!m.isFbFresh || !S.online) "warn" else if (anyAl) "err" else if (S.sym.isNotEmpty()) "warn" else "live",
            onClick = { m.openSheet("sesja") }
        )
        if (!m.isFbFresh) {
            Banner(
                text = if (m.connected) "Odczyt sterownika jest nieaktualny — oczekiwanie na Firebase" else "Brak połączenia z bazą Firebase — połącz na żywo",
                kind = "warn",
                action = "🔑 POŁĄCZ",
                onAction = { m.authOpen = true }
            )
        }
        if (anyAl && S.online) {
            Banner(
                text = when {
                    S.dym_alarm -> if (S.symAktywna("dym"))
                        "ALARM DYMU — wejście symulowane, alarm sterownika aktywny; sprawdź kotłownię"
                    else "ALARM DYMU — sprawdź kotłownię"
                    S.alarm_ogrzewanie -> "PRZEGRZANIE PIECA — " + S.fmt1(to) + " °C"
                    else -> "PRZEGRZANIE PANELU — " + S.fmt1(tb) + " °C"
                },
                kind = "alarm",
                onAction = null
            )
        }
        Hero(
            m,
            art,
            alarm = S.hasData("alarm_ogrzewanie") && S.alarm_ogrzewanie,
            heroSub = when {
                !m.isFbFresh -> "ostatni status sterownika — dane nieaktualne"
                S.symAktywna("ogrz") -> "WARTOŚĆ SYMULOWANA PRZEZ STEROWNIK · używana przez jego logikę"
                !S.hasData("t_ogrz") -> "oczekiwanie na rzeczywisty odczyt pieca C.O."
                else -> "temperatura pieca C.O. · " + (if (to > 70) "grzanie na maks." else "odczyt odebrany")
            },
            onSetpointClick = { setpointDialogOpen = true }
        )
        if (setpointDialogOpen) {
            BoilerSetpointDialog(m, onDismiss = { setpointDialogOpen = false })
        }
        // Dashboard observes S.rev, but AppModel itself keeps the same identity. With
        // Compose strong-skipping, the child could be skipped and retain the initial
        // hidden state after Firebase changed the availability flag. Re-key it on each
        // accepted status revision so the visibility gate is evaluated again.
        key(rev) { ServoIgnitionButton(m) }

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            val pumpKnown = S.hasData("pompa")
            val pumpMode = m.pumpModePresentation().mode
            val pumpModeKnown = pumpMode != null
            QCard(
                title = "POMPA", icon = "pump",
                value = if (!pumpKnown) "—" else if (S.pompa) "WŁĄCZONA" else "WYŁĄCZONA",
                sub = when {
                    !pumpKnown -> "oczekiwanie na stan"
                    !pumpModeKnown -> "tryb pracy: brak odczytu"
                    pumpMode == 1 -> "⏱️ Czasowy"
                    pumpMode == 2 -> "🌡️ Temperaturowy"
                    else -> "⚡ Auto"
                },
                state = if (pumpKnown && S.pompa) "ok" else if (!pumpKnown) "warn" else "",
                onClick = { m.openSheet("pompa") }
            )
            val servoKnown = servoMode != null
            val flapKnown = S.hasData("klapa") && S.klapa in 0..180
            val flapLabel = if (flapKnown) "${Math.round(S.klapa * 100.0 / 180.0)}%" else "—"
            QCard(
                title = "SERWO", icon = "servo",
                value = if (!servoKnown) "—" else when (servoMode) {
                    1 -> "AUTO ($flapLabel)"
                    2 -> "RĘCZNY ($flapLabel)"
                    3 -> "BEZPIECZNY"
                    else -> "—"
                },
                sub = if (flapKnown) "klapa ${S.klapa}°" else "brak prawidłowego odczytu pozycji",
                state = if (!servoKnown) "warn" else when (servoMode) { 1 -> "ok"; 2 -> "warn"; 3 -> "err"; else -> "" },
                onClick = { m.openSheet("serwo") }
            )
            val mixerKnown = S.hasData("mieszadlo")
            val mixerEnabledKnown = S.hasData("mieszadloWlaczony")
            QCard(
                title = "MIESZADŁO", icon = "mixer",
                value = if (!mixerKnown) "—" else if (S.mieszadlo) "WŁĄCZONE" else "WYŁĄCZONE",
                sub = when {
                    !mixerKnown -> "oczekiwanie na stan"
                    S.hasData("rozpalanie") && S.rozpalanie -> "override: rozpalanie"
                    mixerEnabledKnown && !S.mieszadloWlaczony -> "automatyka wyłączona"
                    mixerEnabledKnown && S.mieszadloWlaczony -> "automatyka aktywna"
                    else -> "stan pracy odebrany"
                },
                state = if (mixerKnown && S.mieszadlo) "ok" else if (!mixerKnown) "warn" else "",
                onClick = { m.openSheet("mieszadlo") }
            )
        }

        if (measurementTiles.isNotEmpty()) {
            Sect("Pomiary")
            TileGrid(measurementTiles, m, art)
        }
        if (controlTiles.isNotEmpty()) {
            Sect("Sterowanie · bezpieczeństwo")
            TileGrid(controlTiles, m, art)
        }
        Sect("Analiza i system")

        AnalysisCard(
            accent = Pal.Cyan, icon = "chart", title = "WYKRESY",
            sub = "historia • 6 H / 24 H / 7 DNI / 30 DNI",
            onClick = { m.page = 1 }
        )
        val rtcStatusKnown = S.hasData("rtc_ok")
        val rtcDateKnown = S.hasAllData("dzien", "miesiac", "rok") &&
            S.dzien in 1..31 && S.miesiac in 1..12 && S.rok in 2000..2100
        AnalysisCard(
            accent = Color(0xFFB7C4D4), icon = "clock",
            title = String.format(java.util.Locale.US, "%02d:%02d",
                java.util.Calendar.getInstance().get(java.util.Calendar.HOUR_OF_DAY),
                java.util.Calendar.getInstance().get(java.util.Calendar.MINUTE)),
            sub = when {
                !rtcStatusKnown -> "oczekiwanie na odczyt RTC"
                !S.rtc_ok -> "RTC niegotowy"
                rtcDateKnown -> "${S.dzien}.${S.miesiac}.${S.rok}"
                else -> "RTC gotowy · brak daty"
            },
            onClick = { m.openSheet("czas") }
        )

        Trend(S.hist.toList())
    }
}

@Composable
private fun ServoIgnitionButton(m: AppModel) {
    val s = m.S
    if (!s.hasData("rozpalanie_serwa_dostepne") || !s.rozpalanie_serwa_dostepne) return

    val alarmsKnown = s.hasAllData("dym_alarm", "alarm_ogrzewanie", "alarm_panel")
    val safetyAlarm = anyAlarm(m)
    val enabled = m.isFbFresh && s.online && s.hasData("t_ogrz") && alarmsKnown && !safetyAlarm
    val active = s.hasData("rozpalanie_serwa_aktywne") && s.rozpalanie_serwa_aktywne
    val blockedBySmoke = s.hasData("rozpalanie_serwa_blokada_dymu") && s.rozpalanie_serwa_blokada_dymu

    Column(verticalArrangement = Arrangement.spacedBy(5.dp)) {
        UstBtn(
            label = if (active) "🔥 Rozpalanie aktywne" else "🔥 Rozpalanie",
            variant = "primary",
            modifier = Modifier.fillMaxWidth(),
            enabled = enabled,
            onClick = { m.send("rozpalanie_serwa") }
        )
        Text(
            text = when {
                !m.isFbFresh || !alarmsKnown -> "Oczekiwanie na świeży status i potwierdzenie braku alarmów bezpieczeństwa."
                safetyAlarm -> "Rozpalanie zablokowane podczas aktywnego alarmu bezpieczeństwa."
                blockedBySmoke -> "Po alarmie dymu wymagane jest ponowne kliknięcie; klapa nie otworzy się automatycznie."
                active -> "Aktywne do przekroczenia 30°C. Kliknięcie odnawia pięciominutowy nadzór dymu."
                else -> "Serwa są przygotowywane sekwencyjnie: najpierw syberek, potem klapa."
            },
            style = Txt.cardDesc,
            color = if (active) Pal.Cyan else Pal.TextDim
        )
    }
}

@Composable
private fun BoilerSetpointDialog(m: AppModel, onDismiss: () -> Unit) {
    var value by remember {
        mutableStateOf(if (m.S.hasData("tempZadServo")) m.S.tempZadServo.toString() else "")
    }
    var error by remember { mutableStateOf<String?>(null) }

    fun submit() {
        val degrees = value.toIntOrNull()
        if (degrees == null || degrees !in 0..100) {
            error = "Wpisz temperaturę od 0 do 100 °C."
            return
        }
        m.commitNum("Temperatura zadana pieca", "tempZadServo", 0, 100, degrees)
        onDismiss()
    }

    Dialog(onDismissRequest = onDismiss) {
        Column(
            Modifier.fillMaxWidth()
                .background(Pal.Surface2, RoundedCornerShape(16.dp))
                .border(1.dp, Pal.BorderStrong, RoundedCornerShape(16.dp))
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            Text("Temperatura pieca", style = Txt.sheetTitle)
            OutlinedTextField(
                value = value,
                onValueChange = { input ->
                    value = input.filter { it.isDigit() }.take(3)
                    error = null
                },
                modifier = Modifier.fillMaxWidth(),
                singleLine = true,
                label = { Text("Temperatura zadana (°C)") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number, imeAction = ImeAction.Done),
                keyboardActions = KeyboardActions(onDone = { submit() }),
                colors = OutlinedTextFieldDefaults.colors(
                    focusedBorderColor = Pal.Cyan,
                    unfocusedBorderColor = Pal.Border,
                    focusedContainerColor = Color(0xFF040911),
                    unfocusedContainerColor = Color(0xFF040911),
                    focusedTextColor = Pal.White,
                    unfocusedTextColor = Pal.White,
                    cursorColor = Pal.Cyan
                )
            )
            error?.let { Text(it, style = Txt.note.copy(fontSize = 11.sp), color = Pal.Err) }
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                UstBtn("Anuluj", onClick = onDismiss, modifier = Modifier.weight(1f))
                UstBtn("Ustaw", onClick = { submit() }, modifier = Modifier.weight(1f), variant = "primary")
            }
        }
    }
}

@Composable
private fun TileGrid(list: List<com.sterownikco.pro.core.TileSpec>, m: AppModel, art: com.sterownikco.pro.ui.art.ArtState) {
    val positions = remember { mutableStateMapOf<String, Rect>() }
    val ids = list.map { it.id }
    var draggingId by remember { mutableStateOf<String?>(null) }
    var dragOffset by remember { mutableStateOf(Offset.Zero) }
    var dragStartBounds by remember { mutableStateOf<Rect?>(null) }
    val minReorderDistance = with(LocalDensity.current) { 24.dp.toPx() }

    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        list.chunked(2).forEach { row ->
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                row.forEach { spec ->
                    Box(Modifier.weight(1f).zIndex(if (draggingId == spec.id) 1f else 0f)) {
                        Tile(
                            spec = spec,
                            art = art,
                            onClick = {
                                val target = m.menuForTile(spec.id)
                                if (target != null) m.openSheet(target)
                                else if (spec.id == "weather") { m.page = 2; m.refreshWeatherTab() }
                            },
                            modifier = Modifier
                                .onGloballyPositioned { coordinates ->
                                    if (draggingId != spec.id) {
                                        val topLeft = coordinates.positionInRoot()
                                        val bounds = Rect(
                                            topLeft.x,
                                            topLeft.y,
                                            topLeft.x + coordinates.size.width.toFloat(),
                                            topLeft.y + coordinates.size.height.toFloat()
                                        )
                                        if (positions[spec.id] != bounds) positions[spec.id] = bounds
                                    }
                                }
                                .graphicsLayer {
                                    val active = draggingId == spec.id
                                    translationX = if (active) dragOffset.x else 0f
                                    translationY = if (active) dragOffset.y else 0f
                                    scaleX = if (active) 1.035f else 1f
                                    scaleY = if (active) 1.035f else 1f
                                    shadowElevation = if (active) 16.dp.toPx() else 0f
                                },
                            reorderModifier = Modifier.pointerInput(spec.id, ids, minReorderDistance) {
                                detectDragGesturesAfterLongPress(
                                    onDragStart = { _ ->
                                        draggingId = spec.id
                                        dragOffset = Offset.Zero
                                        dragStartBounds = positions[spec.id]
                                    },
                                    onDrag = { change, amount ->
                                        change.consume()
                                        if (draggingId == spec.id) dragOffset += amount
                                    },
                                    onDragEnd = {
                                        val origin = dragStartBounds
                                        val offset = dragOffset
                                        if (origin != null && offset.getDistance() >= minReorderDistance) {
                                            val dropCenter = origin.center + offset
                                            val target = ids.asSequence()
                                                .filter { it != spec.id }
                                                .mapNotNull { id -> positions[id]?.let { id to it } }
                                                .minByOrNull { (_, bounds) -> bounds.distanceTo(dropCenter) }
                                            val maxDropDistance = maxOf(origin.width, origin.height) * .6f
                                            if (target != null && target.second.distanceTo(dropCenter) <= maxDropDistance) {
                                                m.moveDashboardWidget(spec.id, target.first)
                                            }
                                        }
                                        draggingId = null
                                        dragOffset = Offset.Zero
                                        dragStartBounds = null
                                    },
                                    onDragCancel = {
                                        draggingId = null
                                        dragOffset = Offset.Zero
                                        dragStartBounds = null
                                    }
                                )
                            }
                        )
                    }
                }
                if (row.size == 1) Spacer(Modifier.weight(1f))
            }
        }
    }
}

private fun Rect.distanceTo(point: Offset): Float {
    val dx = when {
        point.x < left -> left - point.x
        point.x > right -> point.x - right
        else -> 0f
    }
    val dy = when {
        point.y < top -> top - point.y
        point.y > bottom -> point.y - bottom
        else -> 0f
    }
    return kotlin.math.sqrt(dx * dx + dy * dy)
}

/** `.acard` — karta analizy (74 px, ikona + tytuł + opis + strzałka). */
@Composable
fun AnalysisCard(accent: Color, icon: String, title: String, sub: String, onClick: () -> Unit) {
    Row(
        modifier = Modifier.fillMaxWidth().height(Dimens.analysisH)
            .background(Pal.Surface, RoundedCornerShape(Dimens.radiusTile))
            .border(1.dp, Pal.Border, RoundedCornerShape(Dimens.radiusTile))
            .clickable { onClick() }.padding(start = 12.dp, end = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        Box(
            modifier = Modifier.size(34.dp).background(accent.copy(alpha = .12f), RoundedCornerShape(11.dp)),
            contentAlignment = Alignment.Center
        ) { AppIcon(icon, size = 18.dp, tint = accent) }
        Column(Modifier.weight(1f)) {
            Text(title, style = Txt.cardTitle, maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text(sub, style = Txt.cardDesc, maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
        AppIcon("next", size = 16.dp, tint = Pal.TextDim2)
    }
}
