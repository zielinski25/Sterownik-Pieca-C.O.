package com.sterownikco.pro.ui.svg

import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.CornerRadius
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.TileMode
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import kotlin.math.max

/** Zastosowanie atrybutów stylu elementu do dziedziczonego stanu. */
internal fun applyStyle(node: SvgNode, st: St) {
    node.attr("fill")?.let { v ->
        st.hasFillAttr = true
        if (v == "none") { st.fill = null; st.fillGrad = null }
        else if (v.startsWith("url(")) {
            st.fillGrad = v.removePrefix("url(").removeSuffix(")").trim().removePrefix("#")
            st.fill = Color.White
        } else {
            st.fill = Svg.parseColor(v) ?: Svg.CURRENT
            st.fillGrad = null
        }
    }
    node.attr("stroke")?.let { v ->
        st.hasStrokeAttr = true
        if (v == "none") { st.stroke = null; st.strokeGrad = null }
        else if (v.startsWith("url(")) {
            st.strokeGrad = v.removePrefix("url(").removeSuffix(")").trim().removePrefix("#")
            st.stroke = Color.White
        } else {
            st.stroke = Svg.parseColor(v) ?: Svg.CURRENT
            st.strokeGrad = null
        }
    }
    node.attr("stroke-width")?.toFloatOrNull()?.let { st.sw = it }
    node.attr("stroke-linecap")?.let {
        st.cap = when (it) {
            "round" -> StrokeCap.Round
            "square" -> StrokeCap.Square
            else -> StrokeCap.Butt
        }
    }
    node.attr("stroke-linejoin")?.let {
        st.join = when (it) {
            "round" -> StrokeJoin.Round
            "bevel" -> StrokeJoin.Bevel
            else -> StrokeJoin.Miter
        }
    }
    node.attr("stroke-dasharray")?.let { d ->
        if (d.trim() == "none") st.dash = null
        else {
            val nums = d.trim().split(Regex("[\\s,]+")).mapNotNull { it.toFloatOrNull() }.filter { it > 0f }
            if (nums.isNotEmpty()) {
                st.dash = if (nums.size % 2 == 1) (nums + nums).toFloatArray() else nums.toFloatArray()
            }
        }
    }
    node.num("opacity", Float.NaN).takeIf { !it.isNaN() }?.let { st.alpha = it }
    node.attr("color")?.let { Svg.parseColor(it)?.let { c -> st.color = c } }

    val style = node.attr("style")
    if (style != null) {
        styleValue(style, "opacity")?.toFloatOrNull()?.let { st.alpha = it }
        styleValue(style, "stroke-width")?.toFloatOrNull()?.let { st.sw = it }
        styleValue(style, "fill")?.let { v ->
            if (v == "none") { st.fill = null; st.fillGrad = null }
            else if (v.startsWith("url(")) {
                st.fillGrad = v.removePrefix("url(").removeSuffix(")").trim().removePrefix("#")
                st.fill = Color.White
            } else Svg.parseColor(v)?.let { st.fill = it }
        }
        styleValue(style, "stroke")?.let { v ->
            if (v == "none") st.stroke = null
            else Svg.parseColor(v)?.let { st.stroke = it }
        }
    }
    // zamiana znacznika currentColor na realny kolor
    if (st.fill == Svg.CURRENT) st.fill = st.color
    if (st.stroke == Svg.CURRENT) st.stroke = st.color
}

internal fun styleValue(style: String, prop: String): String? {
    for (raw in style.split(';')) {
        val part = raw.trim()
        if (part.startsWith("$prop:")) return part.substringAfter(':').trim()
    }
    return null
}

/** `transform="translate(2,0) scale(0.85) rotate(30 12 12)"` → lista operacji. */
internal fun parseTransform(raw: String?): List<Array<String>> {
    if (raw.isNullOrBlank()) return emptyList()
    val out = ArrayList<Array<String>>()
    var i = 0
    val n = raw.length
    while (i < n) {
        val open = raw.indexOf('(', i)
        if (open < 0) break
        val name = raw.substring(i, open).trim()
        val close = raw.indexOf(')', open)
        if (close < 0) break
        val args = raw.substring(open + 1, close).trim().split(Regex("[\\s,]+"))
            .filter { it.isNotEmpty() }
            .map { it.removeSuffix("deg").removeSuffix("px").trim() }
        if (name.isNotEmpty()) out.add(arrayOf(name.lowercase(), *args.toTypedArray()))
        i = close + 1
    }
    return out
}

internal fun parseStyleTransform(style: String?): List<Array<String>> =
    style?.let { parseTransform(styleValue(it, "transform")) } ?: emptyList()

internal fun parseOrigin(raw: String?): Offset {
    if (raw.isNullOrBlank()) return Offset.Zero
    val nums = raw.split(Regex("[\\s,]+")).mapNotNull { it.trim().removeSuffix("px").toFloatOrNull() }
    return when {
        nums.size >= 2 -> Offset(nums[0], nums[1])
        nums.size == 1 -> Offset(nums[0], nums[0])
        else -> Offset.Zero
    }
}

/** Ścieżka kształtu wg tagu elementu. */
internal fun shapePath(node: SvgNode): Path? {
    val p = Path()
    when (node.tag) {
        "path" -> {
            val d = node.attr("d") ?: return null
            return SvgPath.parse(d)
        }
        "rect" -> {
            val x = node.num("x", 0f).zz(); val y = node.num("y", 0f).zz()
            val w = node.num("width", 0f).zz(); val h = node.num("height", 0f).zz()
            if (w <= 0f || h <= 0f) return p
            val rx = node.num("rx", Float.NaN).let { if (it.isNaN()) 0f else minOf(it, w / 2f, h / 2f) }
            val ry = node.num("ry", rx).let { if (it.isNaN()) rx else minOf(it, w / 2f, h / 2f) }
            if (rx > 0f || ry > 0f) p.addRoundRect(Rect(x, y, x + w, y + h), CornerRadius(rx, if (ry > 0f) ry else rx))
            else p.addRect(x, y, x + w, y + h)
        }
        "circle" -> {
            val cx = node.num("cx", 0f).zz(); val cy = node.num("cy", 0f).zz()
            val r = node.num("r", 0f).zz()
            if (r <= 0f) return null
            p.addOval(Rect(cx - r, cy - r, cx + r, cy + r))
        }
        "ellipse" -> {
            val cx = node.num("cx", 0f).zz(); val cy = node.num("cy", 0f).zz()
            val rx = node.num("rx", 0f).zz(); val ry = node.num("ry", rx).let { if (it <= 0f) rx else it }
            if (rx <= 0f || ry <= 0f) return null
            p.addOval(Rect(cx - rx, cy - ry, cx + rx, cy + ry))
        }
        "line" -> {
            p.moveTo(node.num("x1", 0f).zz(), node.num("y1", 0f).zz())
            p.lineTo(node.num("x2", 0f).zz(), node.num("y2", 0f).zz())
        }
        "polyline", "polygon" -> {
            val pts = parsePoints(node.attr("points") ?: return null)
            if (pts.size < 4) return null
            p.moveTo(pts[0], pts[1])
            var i = 2
            while (i + 1 < pts.size) { p.lineTo(pts[i], pts[i + 1]); i += 2 }
            if (node.tag == "polygon") p.close()
        }
        else -> return null
    }
    return p
}

private fun parsePoints(raw: String): FloatArray {
    val nums = raw.trim().split(Regex("[\\s,]+")).mapNotNull { it.toFloatOrNull() }
    return FloatArray(nums.size) { nums[it] }
}

private fun Float.zz(): Float = if (isNaN()) 0f else this

/** Brush dla wypełnienia: gradient (bbox) lub lity kolor. */
internal fun brushFor(parsed: ParsedSvg, gradId: String?, fallback: Color, path: Path): Brush {
    if (gradId == null) return SolidColor(fallback)
    val grad = parsed.grads[gradId] ?: return SolidColor(fallback)
    val stops = grad.stops.sortedBy { it.first }
    if (stops.isEmpty()) return SolidColor(fallback)
    val pairs = ArrayList<Pair<Color, Float>>()
    if (stops.first().first > 0f) pairs.add(stops.first().second to 0f)
    stops.forEach { pairs.add(it.second to it.first.coerceIn(0f, 1f)) }
    if (stops.last().first < 1f) pairs.add(stops.last().second to 1f)
    val b = path.getBounds()
    val w = max(b.width, 0.001f)
    val h = max(b.height, 0.001f)
    return if (grad.radial) {
        Brush.radialGradient(
            *pairs.toTypedArray(),
            center = Offset(b.left + grad.cx * w, b.top + grad.cy * h),
            radius = max(grad.r * (w + h) / 2f, 0.5f),
            tileMode = TileMode.Clamp
        )
    } else {
        Brush.linearGradient(
            *pairs.toTypedArray(),
            start = Offset(b.left + grad.x1 * w, b.top + grad.y1 * h),
            end = Offset(b.left + grad.x2 * w, b.top + grad.y2 * h),
            tileMode = TileMode.Clamp
        )
    }
}
