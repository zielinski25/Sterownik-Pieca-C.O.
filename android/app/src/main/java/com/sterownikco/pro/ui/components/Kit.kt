package com.sterownikco.pro.ui.components

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsFocusedAsState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.sterownikco.pro.ui.icons.AppIcon
import com.sterownikco.pro.ui.theme.Dimens
import com.sterownikco.pro.ui.theme.Pal
import com.sterownikco.pro.ui.theme.Txt

/* ══════════════════════════════════════════════════════════════════════════
   KIT — odpowiedniki pomocników DOM z Piec.html:
   `h('div','card')`, `sectionHeader`, `note`, `seg`, `ust-btn`, `row`,
   `numInput`, `toggleRow`, `slider`, `.pill`, `.badge`.
   ══════════════════════════════════════════════════════════════════════════ */

/** `.card` — karta z `--surface`, radius 16, border `--border`. */
@Composable
fun Card(
    modifier: Modifier = Modifier,
    bg: Color = Pal.Surface,
    contentPadding: androidx.compose.foundation.layout.PaddingValues = PaddingValues(12.dp, 11.dp),
    content: @Composable ColumnScope.() -> Unit
) {
    Column(
        modifier = modifier
            .fillMaxWidth()
            .background(bg, RoundedCornerShape(Dimens.radiusCard))
            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(Dimens.radiusCard))
            .padding(contentPadding),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) { content() }
}

@Composable
fun SectionHeader(text: String, modifier: Modifier = Modifier) {
    Row(modifier = modifier.fillMaxWidth().padding(top = 2.dp), verticalAlignment = Alignment.CenterVertically) {
        Text(text.uppercase(), style = Txt.secHead)
        Spacer(Modifier.size(8.dp))
        Box(Modifier.weight(1f).height(1.dp).background(Pal.Border))
    }
}

@Composable
fun Note(text: String, modifier: Modifier = Modifier) {
    Text(text, style = Txt.note, modifier = modifier.fillMaxWidth().background(
        Pal.Surface2, RoundedCornerShape(10.dp)).padding(10.dp, 9.dp))
}

@Composable
fun Pill(text: String, color: Color = Pal.Cyan, bg: Color = Pal.rgba(0, 212, 245, .12f), modifier: Modifier = Modifier) {
    Box(
        modifier = modifier.background(bg, RoundedCornerShape(999.dp)).padding(horizontal = 7.dp, vertical = 3.dp)
    ) { Text(text, style = Txt.pill, color = color) }
}

/** `.seg` — segmentowany wybór (44 px wys., aktywny = cyjan tło #00131F). */
@Composable
fun <T> Seg(options: List<Pair<T, String>>, current: T, columns: Int = options.size, onPick: (T) -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        options.chunked(columns).forEach { rowItems ->
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp), modifier = Modifier.fillMaxWidth()) {
                rowItems.forEach { (v, label) ->
                    val on = v == current
                    Box(
                        modifier = Modifier.weight(1f).height(36.dp)
                            .background(if (on) Pal.Cyan else Pal.Surface2, RoundedCornerShape(10.dp))
                            .border(BorderStroke(1.dp, if (on) Pal.Cyan else Pal.Border), RoundedCornerShape(10.dp))
                            .clickable { onPick(v) },
                        contentAlignment = Alignment.Center
                    ) {
                        Text(label, style = Txt.seg, color = if (on) Color(0xFF00131F) else Pal.TextDim, textAlign = TextAlign.Center)
                    }
                }
            }
        }
    }
}

/** `.ust-btn` — przycisk ustawień (42 px). `variant`: primary | danger | plain. */
@Composable
fun UstBtn(
    label: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    variant: String = "plain",
    enabled: Boolean = true
) {
    val bg = when (variant) {
        "primary" -> Pal.rgba(0, 212, 245, .16f)
        "danger" -> Pal.rgba(255, 95, 120, .16f)
        else -> Pal.Surface2
    }
    val bd = when (variant) {
        "primary" -> Pal.Cyan; "danger" -> Pal.Err; else -> Pal.BorderStrong
    }
    val fg = when (variant) {
        "primary" -> Pal.Cyan; "danger" -> Pal.Err; else -> Pal.Text
    }
    Box(
        modifier = modifier.height(42.dp).background(if (enabled) bg else bg.copy(alpha = bg.alpha * .4f), RoundedCornerShape(11.dp))
            .border(BorderStroke(1.dp, if (enabled) bd else Pal.Border), RoundedCornerShape(11.dp))
            .clickable(enabled = enabled) { onClick() }
            .padding(horizontal = 12.dp),
        contentAlignment = Alignment.Center
    ) { Text(label, style = Txt.btn, color = fg, textAlign = TextAlign.Center) }
}

/** `.row` — wiersz etykieta + kontrolka (nazwa + opis pod spodem). */
@Composable
fun RowLabel(name: String, desc: String?, trailing: @Composable () -> Unit) {
    Row(
        modifier = Modifier.fillMaxWidth().padding(vertical = 5.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Column(Modifier.weight(1f)) {
            Text(name, style = Txt.rowLabel)
            if (!desc.isNullOrEmpty()) Text(desc, style = Txt.rowDesc)
        }
        trailing()
    }
}

/** `.inp` + przycisk „Ustaw” (numInput w panelu). */
@Composable
fun NumRow(
    name: String,
    desc: String?,
    value: Int,
    min: Int,
    max: Int,
    onCommit: (Int) -> Unit
) {
    var text by remember(value) { mutableStateOf(value.toString()) }
    fun commit() {
        val v = text.toIntOrNull()
        if (v == null || v < min || v > max) onCommit(-1) else onCommit(v)
    }
    RowLabel(name, desc) {
        // OutlinedTextField ma minimalna wysokosc kontenera 56 dp — przy 38 dp
        // ucinal cyfry od gory (Serwo/Pompa). BasicTextField z wlasna ramka.
        val inter = remember { MutableInteractionSource() }
        val focused by inter.collectIsFocusedAsState()
        BasicTextField(
            value = text,
            onValueChange = { t -> text = t.filter { it.isDigit() || it == '-' } },
            modifier = Modifier.width(76.dp).height(38.dp),
            singleLine = true,
            textStyle = Txt.inp.copy(fontSize = 13.sp, textAlign = TextAlign.Center),
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number, imeAction = ImeAction.Done),
            keyboardActions = KeyboardActions(onDone = { commit() }),
            interactionSource = inter,
            cursorBrush = SolidColor(Pal.Cyan),
            decorationBox = { inner ->
                Box(
                    Modifier.background(Pal.Surface2, RoundedCornerShape(9.dp))
                        .border(BorderStroke(1.dp, if (focused) Pal.Cyan else Pal.BorderStrong), RoundedCornerShape(9.dp))
                        .padding(horizontal = 6.dp),
                    contentAlignment = Alignment.Center
                ) { inner() }
            }
        )
        UstBtn("Ustaw", onClick = { commit() }, variant = "plain")
    }
}

/** `.chk` — pole zaznaczenia w stylu panelu. */
@Composable
fun CheckRow(name: String, desc: String?, checked: Boolean, onChange: (Boolean) -> Unit) {
    RowLabel(name, desc) {
        Box(
            modifier = Modifier.size(24.dp).background(
                if (checked) Pal.Cyan else Pal.Surface2, RoundedCornerShape(7.dp)
            ).border(BorderStroke(1.dp, if (checked) Pal.Cyan else Pal.BorderStrong), RoundedCornerShape(7.dp))
                .clickable { onChange(!checked) },
            contentAlignment = Alignment.Center
        ) {
            if (checked) AppIcon("check", size = 15.dp, tint = Color(0xFF00131F))
        }
    }
}

/** `.slider` — suwak z etykietą i wartością (panel: `<div class="h"><span>…</span><b></b></div>`). */
@Composable
fun SliderRow(
    name: String,
    value: Float,
    min: Float,
    max: Float,
    step: Float?,
    fmt: (Float) -> String,
    onValue: (Float) -> Unit,
    onCommit: ((Float) -> Unit)? = null
) {
    var v by remember(value) { mutableFloatStateOf(value) }
    Column(Modifier.fillMaxWidth().padding(vertical = 2.dp)) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Text(name, style = Txt.rowLabel, modifier = Modifier.weight(1f))
            Text(fmt(v), style = Txt.monoVal, color = Pal.White)
        }
        Slider(
            value = v,
            onValueChange = { n -> v = if (step != null && step > 0f) (Math.round(n / step) * step) else n; onValue(v) },
            valueRange = min..max,
            colors = SliderDefaults.colors(thumbColor = Pal.Cyan, activeTrackColor = Pal.Cyan, inactiveTrackColor = Pal.Surface3)
        )
        if (onCommit != null) {
            UstBtn("Zastosuj", onClick = { onCommit(v) }, variant = "primary", modifier = Modifier.fillMaxWidth())
        }
    }
}

/** Nagłówek karty ustawień z ikoną (`.setcard`: 36 px ikona, tytuł 11.5, opis 8.8). */
@Composable
fun SetCard(
    icon: String,
    title: String,
    desc: String,
    tint: Color = Pal.Cyan,
    modifier: Modifier = Modifier,
    onClick: (() -> Unit)? = null,
    trailing: (@Composable () -> Unit)? = null
) {
    Row(
        modifier = modifier.fillMaxWidth()
            .background(Pal.Surface, RoundedCornerShape(Dimens.radiusCard))
            .border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(Dimens.radiusCard))
            .then(if (onClick != null) Modifier.clickable { onClick() } else Modifier)
            .padding(11.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        Box(
            modifier = Modifier.size(36.dp).background(
                Brush.linearGradient(listOf(Pal.rgba(0, 212, 245, .16f), Color.Transparent)),
                RoundedCornerShape(11.dp)
            ).border(BorderStroke(1.dp, Pal.Border), RoundedCornerShape(11.dp)),
            contentAlignment = Alignment.Center
        ) { AppIcon(icon, size = 19.dp, tint = tint) }
        Column(Modifier.weight(1f)) {
            Text(title, style = Txt.cardTitle)
            Text(desc, style = Txt.cardDesc)
        }
        if (trailing != null) trailing() else AppIcon("next", size = 16.dp, tint = Pal.TextDim2)
    }
}

/** `.btnrow` z `manualRow(prefix)` — wymuszenie ręczne + powrót do Auto. */
@Composable
fun ManualRow(prefix: String, onSend: (String) -> Unit, modifier: Modifier = Modifier) {
    Row(modifier = modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        listOf("Wymuś WŁ." to prefix + "_wl", "Wymuś WYŁ." to prefix + "_wyl", "Auto" to prefix + "_auto")
            .forEach { (label, cmd) ->
                Box(Modifier.weight(1f)) {
                    UstBtn(label = label, onClick = { onSend(cmd) }, modifier = Modifier.fillMaxWidth())
                }
            }
    }
}
