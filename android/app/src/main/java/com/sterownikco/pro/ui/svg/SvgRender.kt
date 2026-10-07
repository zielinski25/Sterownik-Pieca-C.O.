package com.sterownikco.pro.ui.svg

import android.graphics.Paint
import android.graphics.Typeface
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.drawIntoCanvas
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.drawscope.Stroke


/* ══════════════════════════════════════════════════════════════════════════
   Renderer drzewa SVG na Canvas Compose:
   • dziedziczenie stylów (fill/stroke/stroke-width/linecap/linejoin/dasharray/opacity)
   • gradienty z <defs> (url(#id)), clip-path, transform + transform-origin
   • „currentColor" — kolor ikony podany przez wywołującego (odpowiednik CSS color)
   • animacje warstw: mapa klas CSS → Anim (odpowiedniki @keyframes z Piec.html)
   ══════════════════════════════════════════════════════════════════════════ */

internal class St {
    var fill: Color? = null
    var fillGrad: String? = null
    var stroke: Color? = null
    var strokeGrad: String? = null
    var sw: Float = 1f
    var cap: StrokeCap = StrokeCap.Butt
    var join: StrokeJoin = StrokeJoin.Miter
    var dash: FloatArray? = null
    var alpha: Float = 1f
    var color: Color = Color.White
    var hasFillAttr: Boolean = false
    var hasStrokeAttr: Boolean = false

    fun clone(): St {
        val s = St()
        s.fill = fill; s.fillGrad = fillGrad; s.stroke = stroke; s.strokeGrad = strokeGrad
        s.sw = sw; s.cap = cap; s.join = join; s.dash = dash; s.alpha = alpha; s.color = color
        s.hasFillAttr = hasFillAttr; s.hasStrokeAttr = hasStrokeAttr
        return s
    }
}

/** Rysuje cały dokument SVG dopasowany do bieżącego rozmiaru (preserveAspectRatio: meet). */
fun DrawScope.drawSvg(
    parsed: ParsedSvg,
    anim: Map<String, Anim> = NO_ANIM,
    currentColor: Color = Color.Unspecified,
    extraAlpha: Float = 1f
) {
    val vb = parsed.vb
    val vbW = (vb.right - vb.left).let { if (it <= 0f) 24f else it }
    val vbH = (vb.bottom - vb.top).let { if (it <= 0f) 24f else it }
    if (vbW <= 0f || vbH <= 0f) return
    val s = minOf(size.width / vbW, size.height / vbH)
    if (s <= 0f || s.isNaN() || s.isInfinite()) return
    val offX = (size.width - vbW * s) / 2f - vb.left * s
    val offY = (size.height - vbH * s) / 2f - vb.top * s
    val root = St().apply {
        if (currentColor != Color.Unspecified) color = currentColor
    }
    tTranslate(offX, offY) {
        tScale(s, s, 0f, 0f) {
            svgNodes(parsed.root, parsed, root, anim, extraAlpha)
        }
    }
}

private fun DrawScope.svgNodes(
    nodes: List<SvgNode>,
    parsed: ParsedSvg,
    parent: St,
    anim: Map<String, Anim>,
    alphaMul: Float
) {
    for (n in nodes) svgNode(n, parsed, parent, anim, alphaMul)
}

private fun DrawScope.svgNode(
    node: SvgNode,
    parsed: ParsedSvg,
    parent: St,
    anim: Map<String, Anim>,
    alphaMul: Float
) {
    val st = parent.clone()
    applyStyle(node, st)

    var a = Anim()
    for (cls in node.classes) anim[cls]?.let { a = it }
    if (a.hidden) return

    val alpha = st.alpha * a.alpha * alphaMul
    val body: DrawScope.() -> Unit = {
        when (node.tag) {
            "g", "svg", "a", "clipPath" ->
                svgNodes(node.children, parsed, st, anim, alpha)
            "text" -> {
                drawShape(node, st, parsed, a, alpha)
                drawSvgText(node, st, alpha)
            }
            else -> drawShape(node, st, parsed, a, alpha)
        }
    }

    val painted: DrawScope.() -> Unit = {
        val clipRef = node.attr("clip-path")?.trim()?.removePrefix("url(")?.removeSuffix(")")?.trim()
        val clipNodes = clipRef?.removePrefix("#")?.let { parsed.clips[it] }
        if (clipNodes != null) {
            val cp = Path()
            for (cn in clipNodes) shapePath(cn)?.let { cp.addPath(it, Offset.Zero) }
            tClip(cp) { body() }
        } else body()
    }

    val ops = parseTransform(node.attr("transform")) + parseStyleTransform(node.attr("style"))
    val origin = parseOrigin(node.attr("style")?.let { styleValue(it, "transform-origin") })
    val px = if (origin != Offset.Zero) origin.x else a.pivotX
    val py = if (origin != Offset.Zero) origin.y else a.pivotY

    var inner: DrawScope.() -> Unit = painted
    if (a.sx != 1f || a.sy != 1f) {
        val sx = a.sx; val sy = a.sy
        val prev = inner
        inner = { tTranslate(px, py) { tScale(sx, sy, px, py) { tTranslate(-px, -py) { prev() } } } }
    }
    if (a.rotDeg != 0f) {
        val r = a.rotDeg
        val prev = inner
        inner = { tRotate(r, px, py) { prev() } }
    }
    if (a.dx != 0f || a.dy != 0f) {
        val dx = a.dx; val dy = a.dy
        val prev = inner
        inner = { tTranslate(dx, dy) { prev() } }
    }
    inner = wrapTransforms(ops, inner)
    inner()
}

private fun wrapTransforms(ops: List<Array<String>>, inner: DrawScope.() -> Unit): DrawScope.() -> Unit {
    if (ops.isEmpty()) return inner
    val head = ops.first()
    val rest = wrapTransforms(ops.drop(1), inner)
    val t = head[0]
    val a = head[1].toFloatOrNull() ?: 0f
    val b = head.getOrNull(2)?.toFloatOrNull() ?: 0f
    return when (t) {
        "translate" -> {
            if (b != 0f) ({ tTranslate(a, b) { rest() } }) else ({ tTranslate(a, 0f) { rest() } })
        }
        "scale" -> ({ tScale(a, if (b != 0f) b else a, 0f, 0f) { rest() } })
        "rotate" -> {
            if (head.size >= 4) {
                val cx = head[2].toFloatOrNull() ?: 0f
                val cy = head[3].toFloatOrNull() ?: 0f
                ({ tRotate(a, cx, cy) { rest() } })
            } else ({ tRotate(a, 0f, 0f) { rest() } })
        }
        "skewX", "skewY" -> rest
        else -> rest
    }
}

private fun DrawScope.drawShape(
    node: SvgNode,
    st: St,
    parsed: ParsedSvg,
    a: Anim,
    alpha: Float
) {
    val path = shapePath(node) ?: return
    val dash = st.dash?.let { PathEffect.dashPathEffect(it, a.dashOffset) }
    if (st.fill != null && node.tag != "line" && node.tag != "polyline") {
        val brush = brushFor(parsed, st.fillGrad, st.fill!!, path)
        drawPath(path, brush, alpha = alpha.coerceIn(0f, 1f))
    }
    if (st.stroke != null && st.sw > 0f) {
        val brush = brushFor(parsed, st.strokeGrad, st.stroke!!, path)
        drawPath(
            path, brush, alpha = alpha.coerceIn(0f, 1f),
            style = androidx.compose.ui.graphics.drawscope.Stroke(
                width = st.sw, cap = st.cap, join = st.join, pathEffect = dash
            )
        )
    }
}

private fun DrawScope.drawSvgText(node: SvgNode, st: St, alpha: Float) {
    val txt = node.text
    if (txt.isEmpty()) return
    val fs = node.num("font-size", 8f).let { if (it.isNaN()) 8f else it }
    val weight = node.attr("font-weight")?.toIntOrNull() ?: 400
    val col = st.fill ?: st.color
    val anchor = node.attr("text-anchor") ?: "start"
    val ls = node.num("letter-spacing", 0f)
    drawIntoCanvas { c ->
        val p = Paint(Paint.ANTI_ALIAS_FLAG)
        p.textSize = if (fs.isNaN()) 8f else fs
        p.color = android.graphics.Color.argb(
            (col.alpha * alpha.coerceIn(0f, 1f) * 255).toInt(),
            (col.red * 255).toInt(), (col.green * 255).toInt(), (col.blue * 255).toInt()
        )
        p.typeface = if (weight >= 600) Typeface.create(Typeface.DEFAULT, Typeface.BOLD) else Typeface.DEFAULT
        if (!ls.isNaN() && ls > 0f) p.letterSpacing = ls / p.textSize
        var x = node.num("x", 0f)
        if (x.isNaN()) x = 0f
        val y = node.num("y", 0f).let { if (it.isNaN()) 0f else it }
        if (anchor == "middle" || anchor == "center") x -= p.measureText(txt) / 2f
        if (anchor == "end" || anchor == "right") x -= p.measureText(txt)
        c.nativeCanvas.drawText(txt, x, y, p)
    }
}

/* ── transformacje przez canvas (bez rozszerzeń DrawScope, które wymagają
      importów — dlatego używamy bezpośrednio Canvas (save/restore)) ── */

private inline fun DrawScope.tTranslate(dx: Float, dy: Float, block: DrawScope.() -> Unit) {
    val c = drawContext.canvas
    c.save(); c.translate(dx, dy); block(); c.restore()
}

private inline fun DrawScope.tScale(sx: Float, sy: Float, px: Float, py: Float, block: DrawScope.() -> Unit) {
    val c = drawContext.canvas
    c.save(); c.translate(px, py); c.scale(sx, sy); c.translate(-px, -py); block(); c.restore()
}

private inline fun DrawScope.tRotate(deg: Float, px: Float, py: Float, block: DrawScope.() -> Unit) {
    val c = drawContext.canvas
    c.save(); c.translate(px, py); c.rotate(deg); c.translate(-px, -py); block(); c.restore()
}

private inline fun DrawScope.tClip(path: Path, block: DrawScope.() -> Unit) {
    val c = drawContext.canvas
    c.save(); c.clipPath(path); block(); c.restore()
}
