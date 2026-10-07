package com.sterownikco.pro.ui.svg

import androidx.compose.foundation.Canvas
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.withFrameNanos
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color

/* ══════════════════════════════════════════════════════════════════════════
   Odpowiednik wstrzykiwania <svg> do <div class="illu"> w Piec.html:
   komponent rysuje mark-up SVG (już sparsowany i zakejlowany) na Canvasie,
   a w pętli klatek podmienia mapę animacji (odpowiednik CSS @keyframes).
   ══════════════════════════════════════════════════════════════════════════ */

@Composable
fun SvgView(
    src: String?,
    modifier: Modifier = Modifier,
    tint: Color = Color.Unspecified,
    animated: Boolean = true,
    animFor: (Float) -> Map<String, Anim> = { NO_ANIM }
) {
    if (src.isNullOrEmpty()) return
    val parsed = remember(src) { Svg.parse(src) }
    var clock by remember { mutableFloatStateOf(0f) }
    if (animated) {
        LaunchedEffect(parsed) {
            val t0 = withFrameNanos { it }
            while (true) {
                val now = withFrameNanos { it }
                clock = (now - t0) / 1_000_000_000f
            }
        }
    }
    Canvas(modifier) {
        val anim = if (animated) animFor(clock) else NO_ANIM
        drawSvg(parsed, anim = anim, currentColor = tint)
    }
}
