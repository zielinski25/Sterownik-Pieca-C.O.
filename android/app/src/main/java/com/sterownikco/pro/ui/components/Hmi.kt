package com.sterownikco.pro.ui.components

import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.sterownikco.pro.core.TileSpec
import com.sterownikco.pro.ui.art.ArtState
import com.sterownikco.pro.ui.art.Ilu
import com.sterownikco.pro.ui.icons.AppIcon
import com.sterownikco.pro.ui.svg.SvgView
import com.sterownikco.pro.ui.theme.Dimens
import com.sterownikco.pro.ui.theme.Pal
import com.sterownikco.pro.ui.theme.Txt

/* ══════════════════════════════════════════════════════════════════════════
   KOMPONENTY PULPITU — wymiary i stany 1:1 z klasami CSS:
   .tile / .well / .badge / .hero / .qcard / .sect / .trend / .nav / .chip
   ══════════════════════════════════════════════════════════════════════════ */

internal fun tileAccent(cls: String): Color = when (cls) {
    "c-flame" -> Pal.Cyan
    "c-flame2" -> Pal.CFlame2
    "c-ember" -> Pal.CEmber
    "c-slonce" -> Pal.CSlonce
    "c-ok" -> Pal.COk
    "c-fiolet" -> Pal.CFiolet
    else -> Pal.TextDim2
}

private fun badgeColors(cls: String): Pair<Color, Color> = when (cls) {
    "live" -> Pal.Live to Pal.rgba(74, 222, 128, .12f)
    "alarm" -> Pal.Err to Pal.rgba(255, 95, 120, .14f)
    "sim" -> Pal.Cyan to Pal.rgba(0, 212, 245, .18f)
    "stale" -> Pal.rgba(127, 147, 163, .95f) to Pal.rgba(127, 147, 163, .12f)
    "active" -> Pal.Accent to Pal.rgba(255, 159, 67, .14f)
    else -> Color(0xFF96ABBB) to Pal.rgba(33, 53, 70, .25f)
}

@Composable
fun Tile(spec: TileSpec, art: ArtState, onClick: () -> Unit) {
    val displayValue = (if (spec.sim) "~" else "") + spec.value
    val accent = tileAccent(spec.cls)
    val alarm = spec.state == "err"
    val tr = rememberInfiniteTransition(label = "tile")
    val pulse by tr.animateFloat(0f, 1f, animationSpec = infiniteRepeatable(tween(1500), RepeatMode.Reverse), label = "pulse")
    val borderColor = when {
        spec.state == "err" -> Pal.rgba(255, 77, 109, .65f)
        spec.state == "warn" -> Pal.rgba(251, 191, 36, .5f)
        spec.state == "ok" -> Pal.rgba(74, 222, 128, .4f)
        spec.state == "dis" -> Pal.rgba(148, 163, 184, .3f)
        spec.sim -> Pal.rgba(0, 212, 245, .55f)
        spec.stale -> Pal.TileStale
        else -> Pal.TileBorder
    }
    val bgBrush = if (spec.state == "err" && alarm)
        Brush.linearGradient(listOf(Pal.rgba(255, 77, 109, .05f + .2f * pulse), Color(0xFF07131F)))
    else if (spec.sim) Brush.linearGradient(listOf(Pal.rgba(0, 212, 245, .08f), Pal.Surface))
    else Brush.linearGradient(listOf(Color(0xFF102437), Color(0xFF07131F)))
    val valueColor = when {
        spec.state == "err" -> Pal.ErrValue
        spec.state == "warn" -> Pal.WarnValue
        spec.state == "ok" -> Pal.Live
        spec.state == "dis" -> Pal.DisValue
        spec.stale -> Pal.StaleValue
        spec.sim -> Pal.White
        spec.vcol.isNotEmpty() -> Color(android.graphics.Color.parseColor(spec.vcol))
        else -> Pal.TileValue
    }
    Box(
        modifier = Modifier.fillMaxWidth().height(Dimens.tileH).clip(RoundedCornerShape(Dimens.radiusTile))
            .background(bgBrush).border(1.dp, borderColor, RoundedCornerShape(Dimens.radiusTile))
            .clickable { onClick() }
    ) {
        // poświata akcentu (kopiada .tile::before)
        Box(
            modifier = Modifier.align(Alignment.TopEnd).size(width = 96.dp, height = 96.dp).background(
                Brush.radialGradient(listOf(accent.copy(alpha = .18f), Color.Transparent)))
        )
        // .well — studnia ilustracji SVG
        Box(
            modifier = Modifier.align(Alignment.TopStart).padding(10.dp).size(44.dp)
                .background(
                    if (spec.state == "ok") Pal.rgba(74, 222, 128, .15f)
                    else if (spec.state == "dis") Pal.rgba(148, 163, 184, .1f)
                    else if (spec.sim) Pal.rgba(0, 212, 245, .12f) else Color(0x0DFFFFFF),
                    RoundedCornerShape(12.dp)
                )
                .border(1.dp, Color(0x14FFFFFF), RoundedCornerShape(12.dp)),
            contentAlignment = Alignment.Center
        ) {
            val src = remember(spec.artKey, art) { Ilu.svgFor(spec.artKey, art) }
            SvgView(
                src = src, modifier = Modifier.size(32.dp), tint = accent,
                animFor = { t -> Ilu.anim(t, art) }
            )
        }
        // plakietka
        val (bfg, bbg) = badgeColors(if (spec.sim) "sim" else spec.badgeCls)
        Row(
            modifier = Modifier.align(Alignment.TopEnd).padding(top = 10.dp, end = 8.dp).height(18.dp)
                .background(bbg, RoundedCornerShape(9.dp)).border(1.dp, bfg.copy(alpha = .3f), RoundedCornerShape(9.dp))
                .padding(horizontal = 7.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(4.dp)
        ) {
            Box(Modifier.size(4.dp).background(bfg, RoundedCornerShape(50)))
            Text(spec.badge, style = Txt.badge, color = bfg, maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
        // tytuł / wartość / opis
        Text(
            spec.title.uppercase(), style = Txt.tileTitle,
            modifier = Modifier.align(Alignment.TopStart).padding(top = 60.dp, start = 10.dp, end = 60.dp),
            maxLines = 1, overflow = TextOverflow.Ellipsis
        )
        Row(
            modifier = Modifier.align(Alignment.TopStart).padding(top = if (displayValue.length > 9) 76.dp else 72.dp, start = 10.dp, end = 10.dp),
            verticalAlignment = Alignment.Bottom
        ) {
            Text(
                displayValue,
                style = if (displayValue.length > 9) Txt.tileValueSm else Txt.tileValue,
                color = valueColor, maxLines = 1, overflow = TextOverflow.Ellipsis
            )
            if (spec.unit.isNotEmpty()) {
                Text(" " + spec.unit, style = Txt.tileUnit, color = Pal.TextDim, modifier = Modifier.padding(bottom = 2.dp))
            }
        }
        Text(
            spec.desc, style = Txt.tileDesc,
            modifier = Modifier.align(Alignment.TopStart).padding(top = 98.dp, start = 10.dp, end = 10.dp),
            maxLines = 1, overflow = TextOverflow.Ellipsis
        )
        // pasek postępu
        Box(
            modifier = Modifier.align(Alignment.BottomStart).padding(start = 10.dp, end = 10.dp, bottom = 4.dp)
                .fillMaxWidth().height(2.5.dp).background(Pal.rgba(48, 71, 93, .2f), RoundedCornerShape(2.dp))
        ) {
            Box(
                modifier = Modifier.fillMaxWidth(spec.frac.coerceIn(0.0, 1.0).toFloat())
                    .height(2.5.dp)
                    .background(if (spec.cls == "c-none") Pal.TextDim2 else accent, RoundedCornerShape(2.dp))
            )
        }
    }
}
