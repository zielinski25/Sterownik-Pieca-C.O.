package com.sterownikco.pro.ui.svg

import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.graphics.Color

/* ══════════════════════════════════════════════════════════════════════════
   Model drzewa SVG — grafika przeniesiona 1:1 z Piec.html (obiekt ILU.*).
   Panel HTML buduje ilustracje kafelków, hero kocioł+bojler, pogodę i ikony
   jako gotowe mark-upy SVG; natywna aplikacja parsuje TE SAME mark-upy.
   ══════════════════════════════════════════════════════════════════════════ */

class SvgNode(
    val tag: String,
    val attrs: Map<String, String>,
    val children: List<SvgNode>,
    val text: String
) {
    fun attr(name: String): String? = attrs[name]?.takeIf { it.isNotEmpty() }

    fun num(name: String, def: Float = Float.NaN): Float = attr(name)?.toFloatOrNull() ?: def

    val classes: List<String>
        get() = attr("class")?.split(' ')?.filter { it.isNotBlank() } ?: emptyList()
}

/** Gradient z <defs> — współrzędne w jednostkach bbox (0..1). */
class SvgGrad(
    val radial: Boolean,
    val x1: Float, val y1: Float, val x2: Float, val y2: Float,
    val cx: Float, val cy: Float, val r: Float,
    val stops: List<Pair<Float, Color>>
)

class ParsedSvg(
    val vb: Rect,
    val root: List<SvgNode>,
    val grads: Map<String, SvgGrad>,
    val clips: Map<String, List<SvgNode>>
)

/** Animacja warstwy (odpowiednik @keyframes): obrót, skala, przesunięcie, alpha, offset kreskowania. */
data class Anim(
    val rotDeg: Float = 0f,
    val pivotX: Float = 0f,
    val pivotY: Float = 0f,
    val sx: Float = 1f,
    val sy: Float = 1f,
    val dx: Float = 0f,
    val dy: Float = 0f,
    val alpha: Float = 1f,
    val dashOffset: Float = 0f,
    val hidden: Boolean = false
)

val NO_ANIM: Map<String, Anim> = emptyMap()
