package com.sterownikco.pro.ui.chart

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.runtime.Composable
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.input.pointer.PointerInputScope
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.clipRect
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
    val padLeft: Float, val padRight: Float
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
fun buildChartView(
    dataset: List<TelemPoint>,
    catalog: List<SeriesDef>,
    zoom: Float,
    offset: Float,
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
    val pumpOnly = cat.isNotEmpty() && cat.all { it.group == "pump" }
    val positionOnly = cat.isNotEmpty() && cat.all { it.group == "position" }
    if (pumpOnly) {
        minVal = 0.0; maxVal = 100.0; axisUnit = ""
    } else if (positionOnly || mode == "NORMALIZED") {
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
            primCat.any { it.group == "position" } -> { minVal = 0.0; maxVal = 100.0; axisUnit = "%" }
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
    return ChartView(rows, cat, values, minVal, maxVal, axisUnit, dual, secGroup, secMin, secMax, secUnit, secColor,
        padLeft, padRight)
}

/**
 * Jeden detektor gestow wykresu: tap = kursor, 2× tap = reset widoku,
 * poziomy drag z otwartym inspektorem = przesuwanie próbki, bez inspektora = pan,
 * szczypniecie = zoom z kotwica w srodku palcow. Pionowych dragów nie konsumujemy.
 */
private suspend fun PointerInputScope.chartGestures(
    padL: Float,
    plotW: Float,
    onTap: (Offset, Float, Float) -> Unit,
    onDoubleTap: () -> Unit,
    onHorizontalDrag: (Float, Float) -> Unit,
    onPinch: (Float, Float) -> Unit
) {
    val slop = viewConfiguration.touchSlop
    val dblTimeout = viewConfiguration.doubleTapTimeoutMillis
    var lastUpT = 0L
    var lastUpX = 0f
    awaitEachGesture {
        awaitFirstDown(requireUnconsumed = false)
        var mode = 0 // 0=tap? 1=horizontal drag 2=pinch 3=obcy (pion/scroll)
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
                    if (plotW > 0f && dx != 0f) onHorizontalDrag(p.position.x, dx)
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
    val inspectorActiveState = rememberUpdatedState(crossIdx != null)
    val onCrossState = rememberUpdatedState(onCross)
    val onPanState = rememberUpdatedState(onPan)
    val onPinchState = rememberUpdatedState(onPinch)
    val onResetViewState = rememberUpdatedState(onResetView)
    val padLKey = view.padLeft
    val padRKey = view.padRight
    Canvas(
        modifier = modifier.clipToBounds().pointerInput(padLKey, padRKey) {
            val padL = padLKey.dp.toPx()
            val plotW = (size.width - padL - padRKey.dp.toPx()).coerceAtLeast(0f)
            chartGestures(
                padL = padL,
                plotW = plotW,
                onTap = { pos, pl, pw ->
                    chartSampleIndexAtX(pos.x, pl, pw, viewState.value.rows.size)?.let { onCrossState.value(it) }
                },
                onDoubleTap = { onCrossState.value(null); onResetViewState.value() },
                onHorizontalDrag = { x, dx ->
                    if (inspectorActiveState.value) {
                        // Gdy inspektor jest otwarty, przesuwamy próbkę pod palcem zamiast panoramować wykres.
                        chartSampleIndexAtX(x, padL, plotW, viewState.value.rows.size)?.let { onCrossState.value(it) }
                    } else if (plotW > 0f && dx != 0f) {
                        onPanState.value(dx / plotW)
                    }
                },
                onPinch = { factor, anchor -> onPinchState.value(factor, anchor) }
            )
        }
    ) {
        val w = size.width; val h = size.height
        val padLeft = view.padLeft.dp.toPx(); val padRight = view.padRight.dp.toPx()
        val padTop = 16.dp.toPx(); val padBottom = 24.dp.toPx()
        val plotW = (w - padLeft - padRight).coerceAtLeast(0f)
        val plotH = (h - padTop - padBottom).coerceAtLeast(0f)
        val axis = TextStyle(fontSize = 8.5.sp, color = Pal.AxisText)
        fun measure(txt: String, st: TextStyle) = measurer.measure(AnnotatedString(txt), st, density = this, fontFamilyResolver = famRes)
        val axisSec = TextStyle(fontSize = 8.5.sp, color = hexColor(view.secColor))
        val pumpOnly = view.cat.isNotEmpty() && view.cat.all { it.group == "pump" }

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

        // siatka + lewa oś Y; pompa jest kategorią binarną, więc ma tylko poziomy PRACA/STOP.
        val ySteps = if (pumpOnly) 1 else 4
        for (i in 0..ySteps) {
            val yVal = view.min + (view.max - view.min) * (1 - i.toDouble() / ySteps)
            val yPos = padTop + (plotH / ySteps) * i
            drawLine(Pal.rgba(255, 255, 255, .06f), Offset(padLeft, yPos), Offset(w - padRight, yPos), 0.8.dp.toPx())
            val unitLabel = if (view.axisUnit == "°C") "°" else if (view.axisUnit.isNotEmpty()) " " + view.axisUnit else ""
            val axisLabel = if (pumpOnly) {
                when (i) { 0 -> "PRACA"; ySteps -> "STOP"; else -> "" }
            } else Math.round(yVal).toString() + unitLabel
            if (axisLabel.isNotEmpty()) {
                val tp = measure(axisLabel, axis)
                drawText(tp, topLeft = Offset(padLeft - 6.dp.toPx() - tp.size.width, yPos - tp.size.height / 2f))
            }
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
        // Wszystkie cztery poziomy odniesienia dla serii temperatur na wspólnej osi °C.
        if (alarmLines && mode == "COMMON" && view.axisUnit == "°C") {
            listOf(
                "lolo" to Pal.Violet,
                "lo" to Pal.Cyan,
                "hi" to Pal.Accent,
                "hihi" to Pal.Err
            ).forEach { (k, c) ->
                val v = alarmLevels[k] ?: return@forEach
                if (v <= view.max && v >= view.min) {
                    val y = padTop + plotH * (1 - ((v - view.min) / (view.max - view.min))).toFloat()
                    drawLine(c.copy(alpha = .6f), Offset(padLeft, y), Offset(w - padRight, y), 1f.dp.toPx(),
                        pathEffect = PathEffect.dashPathEffect(floatArrayOf(4.dp.toPx(), 4.dp.toPx())))
                }
            }
        }
        // Oś X: godzina zwykle; dzień/miesiąc tylko przy początku lub zmianie daty.
        val xSteps = min(5, view.rows.size)
        val spanMs = if (view.rows.size > 1) view.rows.last().ts - view.rows.first().ts else 0L
        var previousLabelTs: Long? = null
        for (i in 0 until xSteps) {
            val idx = ((view.rows.size - 1) * (i.toDouble() / max(1, xSteps - 1))).roundToInt()
            val row = view.rows.getOrNull(idx) ?: continue
            val xPos = padLeft + (plotW / max(1f, (view.rows.size - 1).toFloat())) * idx
            val lbl = fmtXLabel(row.ts, spanMs, previousLabelTs)
            val tp = measure(lbl, axis)
            drawText(tp, topLeft = Offset(xPos - tp.size.width / 2f, h - 8.dp.toPx() - tp.size.height / 2f))
            previousLabelTs = row.ts
        }

        // serie
        view.cat.forEach { s ->
            val vals = view.values[s.id] ?: return@forEach
            var lastTs = 0L
            val segs = ArrayList<ArrayList<SegPoint>>()
            var cur = ArrayList<SegPoint>()
            view.rows.forEachIndexed { idx, r ->
                val v = vals.getOrNull(idx)
                if (v != null && v.isFinite()) {
                    val x = padLeft + (idx.toFloat() / max(1f, (view.rows.size - 1).toFloat())) * plotW
                    val y = when {
                        s.group == "pump" -> padTop + plotH * (1 - (v / 100.0).toFloat())
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
                    cur.add(SegPoint(x, y, v, r.ts, idx, r.isSimulated(s.ch)))
                    lastTs = r.ts
                } else {
                    if (cur.isNotEmpty()) segs.add(cur); cur = ArrayList(); lastTs = 0L
                }
            }
            if (cur.isNotEmpty()) segs.add(cur)
            segs.forEach { seg ->
                if (seg.isEmpty()) return@forEach
                val accent = hexColor(s.accent)
                if (areaBand && s.group != "pump" && mode != "NORMALIZED" && seg.size >= 2) {
                    val path = Path()
                    path.moveTo(seg.first().x, padTop + plotH)
                    seg.forEach { path.lineTo(it.x, it.y) }
                    path.lineTo(seg.last().x, padTop + plotH)
                    path.close()
                    clipRect(padLeft, padTop, w - padRight, padTop + plotH) {
                        drawPath(path, Brush.verticalGradient(
                            listOf(accent.copy(alpha = .22f), accent.copy(alpha = 0f)),
                            startY = padTop, endY = padTop + plotH
                        ))
                    }
                }
                fun drawStrokeRun(run: List<SegPoint>, isSimulated: Boolean) {
                    if (run.isEmpty()) return
                    val line = Path()
                    val useSteps = lineStyle == "STEPPED" || s.group == "pump"
                    if (useSteps || run.size < 3) {
                        run.forEachIndexed { i, p ->
                            if (i == 0) line.moveTo(p.x, p.y)
                            else {
                                if (useSteps) line.lineTo(p.x, run[i - 1].y)
                                line.lineTo(p.x, p.y)
                            }
                        }
                    } else {
                        line.moveTo(run[0].x, run[0].y)
                        for (i in 0 until run.size - 1) {
                            val p0 = run[if (i == 0) 0 else i - 1]
                            val p1 = run[i]
                            val p2 = run[i + 1]
                            val p3 = run[min(i + 2, run.size - 1)]
                            line.cubicTo(
                                p1.x + (p2.x - p0.x) / 6f, p1.y + (p2.y - p0.y) / 6f,
                                p2.x - (p3.x - p1.x) / 6f, p2.y - (p3.y - p1.y) / 6f,
                                p2.x, p2.y
                            )
                        }
                    }
                    // Only edges touching a simulated sample get the dashed style;
                    // genuine-to-genuine edges retain the series' ordinary stroke.
                    val dash = if (isSimulated) PathEffect.dashPathEffect(
                        floatArrayOf(6.dp.toPx(), 4.dp.toPx())
                    ) else null
                    // Catmull–Rom bezier tangents can overshoot the mapped y-range.
                    clipRect(padLeft, padTop, w - padRight, padTop + plotH) {
                        drawPath(line, accent, style = Stroke(width = 2.dp.toPx(), pathEffect = dash))
                    }
                }

                if (seg.size > 1) {
                    // Group edges (not samples), so the transition edge is dashed
                    // while adjacent genuine sections stay continuous and solid.
                    var firstEdge = 0
                    while (firstEdge < seg.size - 1) {
                        val isSimulated = seg[firstEdge].simulated || seg[firstEdge + 1].simulated
                        var lastEdge = firstEdge
                        while (lastEdge + 1 < seg.size - 1 &&
                            (seg[lastEdge + 1].simulated || seg[lastEdge + 2].simulated) == isSimulated) {
                            lastEdge++
                        }
                        drawStrokeRun(seg.subList(firstEdge, lastEdge + 2), isSimulated)
                        firstEdge = lastEdge + 1
                    }
                } else {
                    // Keep an isolated simulated sample visible even without a line.
                    val point = seg.first()
                    clipRect(padLeft, padTop, w - padRight, padTop + plotH) {
                        if (point.simulated) {
                            drawCircle(Pal.CanvasBg, 3.5.dp.toPx(), center = Offset(point.x, point.y))
                            drawCircle(accent, 3.dp.toPx(), center = Offset(point.x, point.y),
                                style = Stroke(width = 1.5.dp.toPx(), pathEffect =
                                    PathEffect.dashPathEffect(floatArrayOf(2.dp.toPx(), 1.5.dp.toPx())))
                            )
                        } else {
                            drawCircle(accent, 3.dp.toPx(), center = Offset(point.x, point.y))
                        }
                    }
                }
            }
        }

        // kursor z dymkiem (`chartCrosshairIdx`)
        run {
            val idx = crossIdx
            if (idx != null && idx >= 0 && idx < view.rows.size) {
                val x = padLeft + (idx.toFloat() / max(1f, (view.rows.size - 1).toFloat())) * plotW
                clipRect(padLeft, padTop, w - padRight, padTop + plotH) {
                    drawLine(Pal.rgba(0, 212, 245, .7f), Offset(x, padTop), Offset(x, padTop + plotH), 1.2.dp.toPx(),
                        pathEffect = PathEffect.dashPathEffect(floatArrayOf(3.dp.toPx(), 3.dp.toPx())))
                    view.cat.forEach { s ->
                        val vals = view.values[s.id] ?: return@forEach
                        val v = vals.getOrNull(idx)
                        if (v != null && v.isFinite()) {
                            val y = when {
                                s.group == "pump" -> padTop + plotH * (1 - (v / 100.0).toFloat())
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
}

internal fun chartSampleIndexAtX(x: Float, padLeft: Float, plotWidth: Float, sampleCount: Int): Int? {
    if (sampleCount < 2 || !plotWidth.isFinite() || plotWidth <= 0f) return null
    val fraction = ((x - padLeft) / plotWidth).coerceIn(0f, 1f)
    return (fraction * (sampleCount - 1)).roundToInt()
}

private class SegPoint(val x: Float, val y: Float, val v: Double, val ts: Long, val idx: Int, val simulated: Boolean)

/**
 * Labels the date only when it establishes context or crosses a calendar boundary:
 * full date at the first tick/month change, day number at day changes, time otherwise.
 */
fun fmtXLabel(ts: Long, spanMs: Long, previousTs: Long? = null): String {
    val calendar = java.util.Calendar.getInstance().apply { timeInMillis = ts }
    val previous = previousTs?.let { java.util.Calendar.getInstance().apply { timeInMillis = it } }
    fun p(value: Int) = if (value < 10) "0$value" else value.toString()

    val day = p(calendar.get(java.util.Calendar.DAY_OF_MONTH))
    val month = p(calendar.get(java.util.Calendar.MONTH) + 1)
    val year = calendar.get(java.util.Calendar.YEAR)
    val hour = p(calendar.get(java.util.Calendar.HOUR_OF_DAY))
    val minute = p(calendar.get(java.util.Calendar.MINUTE))
    val second = p(calendar.get(java.util.Calendar.SECOND))

    val datePrefix = when {
        previous == null -> "$day.$month"
        previous != null && year != previous.get(java.util.Calendar.YEAR) -> "$day.$month.$year"
        previous != null && calendar.get(java.util.Calendar.MONTH) != previous.get(java.util.Calendar.MONTH) -> "$day.$month"
        previous != null && calendar.get(java.util.Calendar.DAY_OF_MONTH) != previous.get(java.util.Calendar.DAY_OF_MONTH) -> day
        else -> null
    }

    return when {
        spanMs < 2 * 3_600_000L -> listOfNotNull(datePrefix, "$hour:$minute:$second").joinToString(" ")
        spanMs < 48 * 3_600_000L -> listOfNotNull(datePrefix, "$hour:$minute").joinToString(" ")
        datePrefix != null -> datePrefix
        else -> "$hour:$minute"
    }
}
