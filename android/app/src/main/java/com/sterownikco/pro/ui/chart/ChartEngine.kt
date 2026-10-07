package com.sterownikco.pro.ui.chart

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.runtime.Composable
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.input.pointer.PointerInputScope
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.sterownikco.pro.core.ChartSeries
import com.sterownikco.pro.core.SeriesDef
import com.sterownikco.pro.core.TelemPoint
import com.sterownikco.pro.ui.theme.Pal
import kotlin.math.abs
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

/* ══════════════════════════════════════════════════════════════════════════
   SILNIK RYSOWANIA WYKRESU — port `updateChartCanvas()` (Piec.html 7289-7700).
   Skala, podwójna oś Y, filtry anomalia, pasma obszarowe, linie alarmowe,
   style SMOOTH/STEPPED, kursor z dymkiem — wszystko tak jak w oryginale.
   ══════════════════════════════════════════════════════════════════════════ */

fun hexColor(hex: String): Color {
    val h = hex.removePrefix("#")
    return try {
        if (h.length == 6) Color(0xFF000000L or h.toLong(16)) else Color(0xFF000000L or h.padEnd(6, '0').toLong(16))
    } catch (e: Exception) { Pal.Cyan }
}

fun rgbColor(rgb: String, alpha: Float): Color {
    val p = rgb.split(",").map { it.trim().toIntOrNull() ?: 0 }
    return Color(argb(p.getOrNull(0) ?: 0, p.getOrNull(1) ?: 0, p.getOrNull(2) ?: 0, alpha))
}

private fun argb(r: Int, g: Int, b: Int, a: Float): ULong {
    val ai = (a * 255).roundToInt().coerceIn(0, 255).toLong()
    val ri = r.coerceIn(0, 255).toLong(); val gi = g.coerceIn(0, 255).toLong(); val bi = b.coerceIn(0, 255).toLong()
    return ((ai shl 24) or (ri shl 16) or (gi shl 8) or bi).toULong()
}

/** Wynik przeliczenia okna widocznego + osi (część `updateChartCanvas` przed rysowaniem). */
class ChartView(
    val rows: List<TelemPoint>,
    val cat: List<SeriesDef>,
    val values: Map<String, List<Double?>>,
    val min: Double, val max: Double, val axisUnit: String,
    val dual: Boolean, val secGroup: String?, val secMin: Double, val secMax: Double,
    val secUnit: String, val secColor: String,
    val padLeft: Float, val padRight: Float,
    val anySim: Boolean,
    val simSeries: Set<String>
) {
    val span: String
        get() = if (rows.isEmpty()) "—" else {
            val a = rows.first().ts; val b = rows.last().ts
            val m = (b - a) / 60000.0
            if (m < 60) "${m.roundToInt()} min" else if (m < 1440) "${(m / 60 * 10).roundToInt() / 10.0} h"
            else "${(m / 1440).roundToInt()} dni"
        }
}

/**
 * `updateChartCanvas` — dobór widocznego okna (zoom/offset), serii, skali osi
 * lewej i prawej (podwójna oś dla kombinacji °C/%/hPa/ADC).
 */
@Suppress("UNUSED_PARAMETER")
fun buildChartView(
    dataset: List<TelemPoint>,
    catalog: List<SeriesDef>,
    zoom: Float,
    offset: Float,
    focus: Int,
    mode: String,
    glitch: Boolean
): ChartView {
    val cat = catalog.filter { it.on }
    val total = dataset.size
    val visibleCount = max(4, Math.round(total / max(zoom, 0.01f)).toInt())
    val maxStart = max(0, total - visibleCount)
    val startIdx = max(0, min(maxStart, Math.round(maxStart * (1 - offset)).toInt()))
    val endIdx = min(total, startIdx + visibleCount)
    val rows = dataset.subList(max(0, startIdx), max(0, endIdx))

    val hasTemp = cat.any { it.group == "temp" }
    val hasServo = cat.any { it.group == "position" }
    val hasRh = cat.any { it.group == "rh" }
    val hasPress = cat.any { it.group == "pressure" }
    val hasSmoke = cat.any { it.group == "smoke" }
    var dual = false; var secGroup: String? = null; var secUnit = "%"; var secMin = 0.0; var secMax = 100.0; var secColor = "#ff6fc7"
    when {
        (hasTemp || hasRh) && hasServo -> { dual = true; secGroup = "position"; secUnit = "%"; secMin = 0.0; secMax = 100.0; secColor = "#ff6fc7" }
        hasTemp && hasRh -> { dual = true; secGroup = "rh"; secUnit = "%"; secMin = 0.0; secMax = 100.0; secColor = "#00d4f5" }
        (hasTemp || hasRh) && hasPress -> { dual = true; secGroup = "pressure"; secUnit = "hPa"; secColor = "#45d98b" }
        (hasTemp || hasRh || hasPress || hasServo) && hasSmoke -> { dual = true; secGroup = "smoke"; secUnit = "ADC"; secColor = "#f06292" }
        hasPress && hasServo -> { dual = true; secGroup = "position"; secUnit = "%"; secMin = 0.0; secMax = 100.0; secColor = "#ff6fc7" }
    }
    val primCat = if (dual) cat.filter { it.group != secGroup } else cat
    val primHasLong = primCat.any { it.group == "pressure" || it.group == "smoke" }
    val secHasLong = dual && (secGroup == "pressure" || secGroup == "smoke")
    val padLeft = if (primHasLong) 56f else 40f
    val padRight = if (dual) (if (secHasLong) 56f else 38f) else 14f

    val values = HashMap<String, List<Double?>>()
    cat.forEach { s -> values[s.id] = ChartSeries.filterGlitches(rows, s.ch, s.group, glitch) }

    var minVal = Double.POSITIVE_INFINITY; var maxVal = Double.NEGATIVE_INFINITY; var axisUnit = "°C"
    if (focus == 1 || mode == "NORMALIZED") {
        minVal = 0.0; maxVal = 100.0; axisUnit = "%"
    } else {
        fun rangeFor(group: String): Pair<Double, Double>? {
            var lo = Double.POSITIVE_INFINITY; var hi = Double.NEGATIVE_INFINITY
            primCat.filter { it.group == group }.forEach { s ->
                (values[s.id] ?: emptyList()).forEach { v ->
                    if (v != null && v.isFinite()) { lo = min(lo, v); hi = max(hi, v) }
                }
            }
            return if (lo == Double.POSITIVE_INFINITY) null else lo to hi
        }
        when {
            primCat.any { it.group == "temp" } -> {
                axisUnit = "°C"
                var (lo, hi) = rangeFor("temp") ?: (20.0 to 70.0)
                val spanV = max(6.0, hi - lo)
                minVal = floor(lo - spanV * 0.08); maxVal = ceil(hi + spanV * 0.08)
            }
            primCat.any { it.group == "rh" } -> { minVal = 0.0; maxVal = 100.0; axisUnit = "%" }
            primCat.any { it.group == "pressure" } -> {
                axisUnit = "hPa"
                var (lo, hi) = rangeFor("pressure") ?: (990.0 to 1030.0)
                val spanV = max(4.0, hi - lo)
                minVal = floor(lo - spanV * 0.1); maxVal = ceil(hi + spanV * 0.1)
            }
            primCat.any { it.group == "smoke" } -> {
                axisUnit = "ADC"
                var (lo, hi) = rangeFor("smoke") ?: (0.0 to 500.0)
                val spanV = max(50.0, hi - lo)
                minVal = max(0.0, floor(lo - spanV * 0.08)); maxVal = ceil(hi + spanV * 0.12)
            }
            else -> { minVal = 0.0; maxVal = 100.0; axisUnit = "" }
        }
    }
    if (dual) {
        when (secGroup) {
            "pressure", "smoke" -> {
                var lo = Double.POSITIVE_INFINITY; var hi = Double.NEGATIVE_INFINITY
                cat.filter { it.group == secGroup }.forEach { s ->
                    (values[s.id] ?: emptyList()).forEach { v ->
                        if (v != null && v.isFinite()) { lo = min(lo, v); hi = max(hi, v) }
                    }
                }
                if (lo == Double.POSITIVE_INFINITY) { lo = if (secGroup == "pressure") 990.0 else 0.0; hi = if (secGroup == "pressure") 1030.0 else 500.0 }
                val sp = if (secGroup == "pressure") max(4.0, hi - lo) else max(50.0, hi - lo)
                secMin = if (secGroup == "pressure") floor(lo - sp * 0.1) else max(0.0, floor(lo - sp * 0.08))
                secMax = ceil(hi + if (secGroup == "pressure") sp * 0.1 else sp * 0.12)
            }
            else -> { secMin = 0.0; secMax = 100.0 }
        }
    }
    val simSeries = cat.filter { s -> rows.any { r -> r.sim and (1L shl s.qCh) != 0L } }.map { it.id }.toSet()
    val anySim = simSeries.isNotEmpty()
    return ChartView(rows, cat, values, minVal, maxVal, axisUnit, dual, secGroup, secMin, secMax, secUnit, secColor,
        padLeft, padRight, anySim, simSeries)
}

/**
 * Jeden detektor gestow wykresu: tap = kursor, 2× tap = reset widoku,
 * poziomy drag = pan osi czasu, szczypniecie = zoom z kotwica w srodku palcow.
 * Pionowych przeciagniec NIE konsumujemy, zeby strona dalej sie przewijala.
 */
private suspend fun PointerInputScope.chartGestures(
    padL: Float,
    plotW: Float,
    onTap: (Offset, Float, Float) -> Unit,
    onDoubleTap: () -> Unit,
    onPan: (Float) -> Unit,
    onPinch: (Float, Float) -> Unit
) {
    val slop = viewConfiguration.touchSlop
    val dblTimeout = viewConfiguration.doubleTapTimeoutMillis
    var lastUpT = 0L
    var lastUpX = 0f
    awaitEachGesture {
        awaitFirstDown(requireUnconsumed = false)
        var mode = 0 // 0=tap? 1=pan 2=pinch 3=obcy (pion/scroll)
        var downX = 0f
        var downY = 0f
        var lastX = 0f
        var d0 = 0f
        var haveDown = false
        do {
            val ev = awaitPointerEvent()
            val pressed = ev.changes.filter { it.pressed }
            if (pressed.isEmpty()) {
                if (haveDown && mode == 0) {
                    val now = System.currentTimeMillis()
                    if (now - lastUpT < dblTimeout && abs(downX - lastUpX) < slop * 2) {
                        onDoubleTap()
                        lastUpT = 0L
                    } else {
                        onTap(Offset(downX, downY), padL, plotW)
                        lastUpT = now
                        lastUpX = downX
                    }
                } else lastUpT = 0L
                break
            }
            if (!haveDown) {
                haveDown = true
                downX = pressed[0].position.x
                downY = pressed[0].position.y
                lastX = downX
            }
            if (pressed.size >= 2) {
                val a = pressed[0].position
                val b = pressed[1].position
                val d = (a - b).getDistance()
                if (mode != 2) { mode = 2; d0 = d }
                else if (d0 > 0f && d > 0f && plotW > 0f) {
                    val cx = (a.x + b.x) / 2f
                    onPinch(d / d0, ((cx - padL) / plotW).coerceIn(0f, 1f))
                    d0 = d
                }
                pressed.forEach { it.consume() }
            } else if (mode == 2 || mode == 3) {
                // Puszczono palec w trakcie pincha — koniec gestu, bez tapa.
                if (mode == 2) mode = 3
                pressed.forEach { it.consume() }
            } else {
                val p = pressed[0]
                if (mode == 0 && (abs(p.position.x - downX) > slop || abs(p.position.y - downY) > slop)) {
                    mode = if (abs(p.position.x - downX) > abs(p.position.y - downY)) 1 else 3
                    lastX = p.position.x
                }
                if (mode == 1) {
                    val dx = p.position.x - lastX
                    lastX = p.position.x
                    if (plotW > 0f && dx != 0f) onPan(dx / plotW)
                    p.consume()
                }
                // mode 3 (pion): nie konsumujemy — verticalScroll strony przejmuje
            }
        } while (true)
    }
}

/** Rysowanie — odpowiednik `updateChartCanvas` na Compose Canvasie. */
@Composable
fun ChartCanvas(
    view: ChartView,
    modifier: Modifier = Modifier,
    areaBand: Boolean = true,
    alarmLines: Boolean = false,
    alarmLevels: Map<String, Double> = emptyMap(),
    mode: String = "COMMON",
    lineStyle: String = "SMOOTH",
    crossIdx: Int? = null,
    onCross: (Int?) -> Unit = {},
    onPan: (Float) -> Unit = {},
    onPinch: (Float, Float) -> Unit = { _, _ -> },
    onResetView: () -> Unit = {}
) {
    val measurer = rememberTextMeasurer()
    val famRes = androidx.compose.ui.platform.LocalFontFamilyResolver.current
    // Gesty czytaja SWIEZY view bez restartu detektora: kluczem sa tylko pady
    // (zoom/offset przebudowuja view co klatke — pointerInput(view) rwalo by gest).
    val viewState = rememberUpdatedState(view)
    val padLKey = view.padLeft
    val padRKey = view.padRight
    Canvas(
        modifier = modifier.pointerInput(padLKey, padRKey) {
            val padL = padLKey.dp.toPx()
            val plotW = size.width - padL - padRKey.dp.toPx()
            chartGestures(
                padL = padL,
                plotW = plotW,
                onTap = { pos, pl, pw ->
                    val n = viewState.value.rows.size
                    if (n > 1) onCross((((pos.x - pl) / max(1f, pw)) * (n - 1)).roundToInt().coerceIn(0, n - 1))
                },
                onDoubleTap = { onCross(null); onResetView() },
                onPan = onPan,
                onPinch = onPinch
            )
        }
    ) {
        val w = size.width; val h = size.height
        val padLeft = view.padLeft.dp.toPx(); val padRight = view.padRight.dp.toPx()
        val padTop = 16.dp.toPx(); val padBottom = 24.dp.toPx()
        val plotW = w - padLeft - padRight
        val plotH = h - padTop - padBottom
        val axis = TextStyle(fontSize = 8.5.sp, color = Pal.AxisText)
        fun measure(txt: String, st: TextStyle) = measurer.measure(AnnotatedString(txt), st, density = this, fontFamilyResolver = famRes)
        val axisSec = TextStyle(fontSize = 8.5.sp, color = hexColor(view.secColor))

        // FIX-BRAK-DANYCH: uczciwy komunikat jak w HTML (zamiast pustego boxa)
        val hasPts = view.values.values.any { vs -> vs.any { v -> v != null && v.isFinite() } }
        if (view.rows.size < 2 || view.cat.isEmpty() || !hasPts) {
            val tp = measure(
                "Brak danych dla wybranych serii w tym oknie",
                TextStyle(fontSize = 12.sp, fontWeight = FontWeight.SemiBold, color = Pal.rgba(148, 163, 184, .95f))
            )
            drawText(tp, topLeft = Offset((w - tp.size.width) / 2f, padTop + plotH / 2f - tp.size.height / 2f))
            return@Canvas
        }

        // siatka + lewa oś Y
        val ySteps = 4
        for (i in 0..ySteps) {
            val yVal = view.min + (view.max - view.min) * (1 - i.toDouble() / ySteps)
            val yPos = padTop + (plotH / ySteps) * i
            drawLine(Pal.rgba(255, 255, 255, .06f), Offset(padLeft, yPos), Offset(w - padRight, yPos), 0.8.dp.toPx())
            val unitLabel = if (view.axisUnit == "°C") "°" else if (view.axisUnit.isNotEmpty()) " " + view.axisUnit else ""
            val tp = measure(Math.round(yVal).toString() + unitLabel, axis)
            drawText(tp, topLeft = Offset(padLeft - 6.dp.toPx() - tp.size.width, yPos - tp.size.height / 2f))
        }
        // prawa oś Y
        if (view.dual) {
            for (i in 0..ySteps) {
                val sVal = Math.round(view.secMin + (view.secMax - view.secMin) * (1 - i.toDouble() / ySteps))
                val yPos = padTop + (plotH / ySteps) * i
                val tp = measure(sVal.toString() + (if (view.secUnit.isNotEmpty()) " " + view.secUnit else ""), axisSec)
                drawText(tp, topLeft = Offset(w - padRight + 5.dp.toPx(), yPos - tp.size.height / 2f))
            }
        }
        // linie alarmowe (hi/hihi) — jak w oryginale tylko dla temperatur i osi °C
        if (alarmLines && mode == "COMMON" && view.axisUnit == "°C") {
            listOf("hihi" to Pal.Err, "hi" to Pal.Accent).forEach { (k, c) ->
                val v = alarmLevels[k] ?: return@forEach
                if (v <= view.max && v >= view.min) {
                    val y = padTop + plotH * (1 - ((v - view.min) / (view.max - view.min))).toFloat()
                    drawLine(c.copy(alpha = .6f), Offset(padLeft, y), Offset(w - padRight, y), 1f.dp.toPx(),
                        pathEffect = PathEffect.dashPathEffect(floatArrayOf(4.dp.toPx(), 4.dp.toPx())))
                }
            }
        }
        // oś X — format zależny od szerokości okna (FIX-OX-OS)
        val xSteps = min(5, view.rows.size)
        val spanMs = if (view.rows.size > 1) view.rows.last().ts - view.rows.first().ts else 0L
        for (i in 0 until xSteps) {
            val idx = ((view.rows.size - 1) * (i.toDouble() / max(1, xSteps - 1))).roundToInt()
            val row = view.rows.getOrNull(idx) ?: continue
            val xPos = padLeft + (plotW / max(1f, (view.rows.size - 1).toFloat())) * idx
            val lbl = fmtXLabel(row.ts, spanMs)
            val tp = measure(lbl, axis)
            drawText(tp, topLeft = Offset(xPos - tp.size.width / 2f, h - 8.dp.toPx() - tp.size.height / 2f))
        }

        // serie
        view.cat.forEach { s ->
            val vals = view.values[s.id] ?: return@forEach
            val pts = ArrayList<SegPoint>()
            var lastTs = 0L
            val segs = ArrayList<ArrayList<SegPoint>>()
            var cur = ArrayList<SegPoint>()
            view.rows.forEachIndexed { idx, r ->
                val v = vals.getOrNull(idx)
                if (v != null && v.isFinite()) {
                    val x = padLeft + (idx.toFloat() / max(1f, (view.rows.size - 1).toFloat())) * plotW
                    val y = when {
                        view.dual && s.group == view.secGroup ->
                            padTop + plotH * (1 - ((v - view.secMin) / max(1.0, view.secMax - view.secMin)).toFloat())
                        mode == "NORMALIZED" -> {
                            val all = vals.filterNotNull()
                            val lo = if (all.isNotEmpty()) all.min() else 0.0
                            val hi = if (all.isNotEmpty()) all.max() else 100.0
                            val nv = if (hi > lo) (v - lo) / (hi - lo) else 0.5
                            padTop + plotH * (1 - nv.toFloat())
                        }
                        else -> padTop + plotH * (1 - ((v - view.min) / max(1e-9, view.max - view.min)).toFloat())
                    }
                    if (lastTs != 0L && r.ts - lastTs > 360_000L) { if (cur.isNotEmpty()) segs.add(cur); cur = ArrayList() }
                    val isSim = r.sim and (1L shl s.qCh) != 0L
                    cur.add(SegPoint(x, y, v, r.ts, idx, isSim))
                    lastTs = r.ts
                } else {
                    if (cur.isNotEmpty()) segs.add(cur); cur = ArrayList(); lastTs = 0L
                }
            }
            if (cur.isNotEmpty()) segs.add(cur)
            val seriesSim = view.simSeries.contains(s.id)
            segs.forEach { seg ->
                if (seg.isEmpty()) return@forEach
                val segSim = seriesSim || seg.any { it.isSim }
                val accent = hexColor(s.accent)
                if (areaBand && mode != "NORMALIZED" && seg.size >= 2) {
                    val path = Path()
                    path.moveTo(seg.first().x, padTop + plotH)
                    seg.forEach { path.lineTo(it.x, it.y) }
                    path.lineTo(seg.last().x, padTop + plotH)
                    path.close()
                    drawPath(path, Brush.verticalGradient(
                        listOf(accent.copy(alpha = if (segSim) .10f else .22f), accent.copy(alpha = 0f)),
                        startY = padTop, endY = padTop + plotH
                    ))
                }
                val line = Path()
                if (lineStyle == "STEPPED" || seg.size < 3) {
                    seg.forEachIndexed { i, p ->
                        if (i == 0) line.moveTo(p.x, p.y)
                        else {
                            if (lineStyle == "STEPPED") line.lineTo(p.x, seg[i - 1].y)
                            line.lineTo(p.x, p.y)
                        }
                    }
                } else {
                    line.moveTo(seg[0].x, seg[0].y)
                    for (i in 0 until seg.size - 1) {
                        val p0 = seg[if (i == 0) 0 else i - 1]
                        val p1 = seg[i]
                        val p2 = seg[i + 1]
                        val p3 = seg[min(i + 2, seg.size - 1)]
                        line.cubicTo(
                            p1.x + (p2.x - p0.x) / 6f, p1.y + (p2.y - p0.y) / 6f,
                            p2.x - (p3.x - p1.x) / 6f, p2.y - (p3.y - p1.y) / 6f,
                            p2.x, p2.y
                        )
                    }
                }
                drawPath(line, accent, style = Stroke(
                    width = (if (segSim) 1.8f else 2.0f).dp.toPx(),
                    pathEffect = if (segSim) PathEffect.dashPathEffect(floatArrayOf(6.dp.toPx(), 3.5.dp.toPx())) else null
                ))
                if (seg.size == 1) drawCircle(accent, 3.dp.toPx(), center = Offset(seg[0].x, seg[0].y))
            }
        }

        // kursor z dymkiem (`chartCrosshairIdx`)
        run {
            val idx = crossIdx
            if (idx != null && idx >= 0 && idx < view.rows.size) {
                val x = padLeft + (idx.toFloat() / max(1f, (view.rows.size - 1).toFloat())) * plotW
                drawLine(Pal.rgba(0, 212, 245, .7f), Offset(x, padTop), Offset(x, padTop + plotH), 1.2.dp.toPx(),
                    pathEffect = PathEffect.dashPathEffect(floatArrayOf(3.dp.toPx(), 3.dp.toPx())))
                view.cat.forEach { s ->
                    val vals = view.values[s.id] ?: return@forEach
                    val v = vals.getOrNull(idx)
                    if (v != null && v.isFinite()) {
                        val y = when {
                            view.dual && s.group == view.secGroup ->
                                padTop + plotH * (1 - ((v - view.secMin) / max(1.0, view.secMax - view.secMin)).toFloat())
                            mode == "NORMALIZED" -> {
                                val all = vals.filterNotNull()
                                val lo = if (all.isNotEmpty()) all.min() else 0.0
                                val hi = if (all.isNotEmpty()) all.max() else 100.0
                                val nv = if (hi > lo) (v - lo) / (hi - lo) else 0.5
                                padTop + plotH * (1 - nv.toFloat())
                            }
                            else -> padTop + plotH * (1 - ((v - view.min) / max(1e-9, view.max - view.min)).toFloat())
                        }
                        drawCircle(Color.White, 4.dp.toPx(), center = Offset(x, y))
                        drawCircle(hexColor(s.accent), 3.dp.toPx(), center = Offset(x, y))
                    }
                }
            }
        }
    }
}

private class SegPoint(val x: Float, val y: Float, val v: Double, val ts: Long, val idx: Int, val isSim: Boolean)

/** `pad2(...) + …` — dobór formatu osi X do szerokości okna. */
fun fmtXLabel(ts: Long, spanMs: Long): String {
    val c = java.util.Calendar.getInstance().apply { timeInMillis = ts }
    fun p(v: Int) = if (v < 10) "0$v" else "$v"
    return when {
        spanMs < 2 * 3_600_000L -> p(c.get(java.util.Calendar.HOUR_OF_DAY)) + ":" + p(c.get(java.util.Calendar.MINUTE)) + ":" + p(c.get(java.util.Calendar.SECOND))
        spanMs < 48 * 3_600_000L -> p(c.get(java.util.Calendar.DAY_OF_MONTH)) + "." + p(c.get(java.util.Calendar.MONTH) + 1) +
            " " + p(c.get(java.util.Calendar.HOUR_OF_DAY)) + ":" + p(c.get(java.util.Calendar.MINUTE))
        else -> p(c.get(java.util.Calendar.DAY_OF_MONTH)) + "." + p(c.get(java.util.Calendar.MONTH) + 1)
    }
}
