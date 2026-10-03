package pl.sterownikco.dev

import android.animation.ValueAnimator
import android.content.Context
import android.graphics.*
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import android.view.View
import android.view.animation.DecelerateInterpolator
import kotlin.math.*

/**
 * STEROWNIK CO — Premium telemetry chart.
 * Redesigned v1.0: Grafana-inspired, smooth bezier curves, outer glow,
 * gradient fills, floating tooltip, refined axes and grid.
 */
class ProfessionalTelemetryChartView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
) : View(context, attrs) {

    enum class Mode { SPLIT, COMMON, NORMALIZED, FROM_START, XY }
    enum class LineStyle { SMOOTH, STEPPED }

    data class Series(
        val id: String,
        val label: String,
        val channel: Int?,
        val servo: Boolean = false,
        val scale: Double = 0.1,
        val unit: String = "°C",
        val accent: Int,
    )

    var onSelectionChanged: ((String) -> Unit)? = null

    // --- Data ---
    private var rows: List<TelemetryRecord> = emptyList()
    private var allSeries: List<Series> = emptyList()
    private val selected = LinkedHashSet<String>()
    private var mode = Mode.COMMON
    private var lineStyle = LineStyle.SMOOTH
    private var realOnly = true
    private var anomalyFilter = true
    private var showBand = false
    private var fixedMin: Double? = null
    private var fixedMax: Double? = null

    // --- Viewport ---
    private var zoom = 1f
    private var offset = 1f
    private var zoomFocusX = 0.5f
    private var previousZoom = 1f
    private var crosshairX: Float? = null
    private var downX = 0f
    private var downAt = 0L
    private var moved = false
    private var lastTapAt = 0L

    // --- Animation ---
    private var animationProgress = 1f
    private var animator: ValueAnimator? = null

    // --- Alarm lines ---
    private var alarmLoLo: Double? = null
    private var alarmLo: Double? = null
    private var alarmHi: Double? = null
    private var alarmHiHi: Double? = null
    private var showAlarmLines = false

    // --- Paints ---
    private val bgPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val plotPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val gridPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val axisPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val axisText = Paint(Paint.ANTI_ALIAS_FLAG)
    private val titleText = Paint(Paint.ANTI_ALIAS_FLAG)
    private val crosshairPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val bandPaint = Paint(Paint.ANTI_ALIAS_FLAG)

    init {
        isClickable = true
        isFocusable = true
        axisText.color = 0xFF8EA6BA.toInt()
        axisText.textSize = dp(10f)
        titleText.color = 0xFF7E9AB0.toInt()
        titleText.textSize = dp(8.8f)
        crosshairPaint.color = 0xA06F94AF.toInt()
        crosshairPaint.strokeWidth = dp(1f)
        crosshairPaint.pathEffect = DashPathEffect(floatArrayOf(dp(5f), dp(5f)), 0f)
        bandPaint.color = 0x143CC8F0
    }

    private val scaleDetector = ScaleGestureDetector(context, object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
        override fun onScaleBegin(detector: ScaleGestureDetector): Boolean {
            zoomFocusX = ((detector.focusX - plotLeft()) / max(1f, plotRight() - plotLeft())).coerceIn(0f, 1f)
            previousZoom = zoom
            return true
        }
        override fun onScale(detector: ScaleGestureDetector): Boolean {
            val next = (zoom * detector.scaleFactor).coerceIn(1f, 24f)
            if (abs(next - zoom) > 0.001f) {
                val oldCount = max(2, (rows.size / zoom).roundToInt().coerceAtMost(rows.size))
                val oldMaxStart = max(0, rows.size - oldCount)
                val oldStart = oldMaxStart * (1f - offset)
                val anchorIndex = oldStart + (oldCount - 1) * zoomFocusX
                zoom = next
                val newCount = max(2, (rows.size / zoom).roundToInt().coerceAtMost(rows.size))
                val newMaxStart = max(0, rows.size - newCount)
                val newStart = (anchorIndex - (newCount - 1) * zoomFocusX).coerceIn(0f, newMaxStart.toFloat())
                offset = if (newMaxStart <= 0) 1f else 1f - (newStart / newMaxStart).coerceIn(0f, 1f)
                invalidate()
            }
            return true
        }
    })

    // --- Public API ---

    fun setCatalog(series: List<Series>) {
        allSeries = series
        if (selected.isEmpty()) {
            series.take(if (series.any { it.servo }) 2 else 4).forEach { selected += it.id }
        }
        invalidate()
    }

    fun setData(value: List<TelemetryRecord>, animate: Boolean = true) {
        rows = value.asSequence()
            .filter { it.ts > 0L }
            .sortedWith(compareBy<TelemetryRecord> { it.ts }.thenBy { it.seq })
            .fold(LinkedHashMap<Long, TelemetryRecord>()) { acc, row ->
                acc[row.ts] = row
                acc
            }.values.toList()
        zoom = 1f
        offset = 1f
        crosshairX = null
        if (animate) startReveal() else {
            animationProgress = 1f
            post { invalidate() }
        }
        post { invalidate() }
    }

    fun setSeriesEnabled(id: String, enabled: Boolean) {
        if (enabled) selected += id else selected -= id
        if (selected.isEmpty()) allSeries.firstOrNull()?.let { selected += it.id }
        invalidate()
    }

    fun isSeriesEnabled(id: String): Boolean = selected.contains(id)
    fun selectedSeries(): List<Series> = allSeries.filter { selected.contains(it.id) }
    fun snapshot(): List<TelemetryRecord> = rows
    fun currentMode(): Mode = mode
    fun currentZoom(): Float = zoom
    fun isRealOnly(): Boolean = realOnly
    fun isAnomalyFilter(): Boolean = anomalyFilter
    fun isSmooth(): Boolean = lineStyle == LineStyle.SMOOTH
    fun isBandVisible(): Boolean = showBand

    data class SummaryStats(val rangeText: String, val pointsText: String)

    fun readouts(): List<String> {
        if (rows.isEmpty()) return emptyList()
        return selectedSeries().mapNotNull { s ->
            val values = buildValues(s, rows).mapNotNull { it.second }
            values.lastOrNull()?.let { "${s.label}: ${fmt(it)}${if (s.servo) "%" else s.unit}" }
        }
    }

    fun summaryStats(): SummaryStats {
        if (rows.isEmpty() || selected.isEmpty()) return SummaryStats("—", "0")
        val active = selectedSeries()
        if (active.isEmpty()) return SummaryStats("—", rows.size.toString())
        val units = active.map { if (it.servo) "%" else it.unit }.distinct()
        if (units.size > 1) return SummaryStats("wiele jednostek", rows.size.toString())
        val all = active.flatMap { s -> buildValues(s, rows).mapNotNull { it.second } }
        if (all.isEmpty()) return SummaryStats("—", rows.size.toString())
        val lo = all.minOrNull() ?: 0.0
        val hi = all.maxOrNull() ?: 0.0
        return SummaryStats("${fmt(lo)}–${fmt(hi)} ${units.firstOrNull().orEmpty()}", rows.size.toString())
    }

    fun setMode(value: Mode) { mode = value; invalidate() }
    fun setLineStyle(value: LineStyle) { lineStyle = value; invalidate() }
    fun setRealOnly(value: Boolean) { realOnly = value; invalidate() }
    fun setAnomalyFilter(value: Boolean) { anomalyFilter = value; invalidate() }
    fun setFixedRange(min: Double?, max: Double?) { fixedMin = min; fixedMax = max; invalidate() }
    fun toggleBand() { showBand = !showBand; invalidate() }

    fun setAlarmLines(loLo: Double?, lo: Double?, hi: Double?, hiHi: Double?, visible: Boolean = true) {
        alarmLoLo = loLo; alarmLo = lo; alarmHi = hi; alarmHiHi = hiHi; showAlarmLines = visible
        invalidate()
    }

    fun clearAlarmLines() {
        alarmLoLo = null; alarmLo = null; alarmHi = null; alarmHiHi = null; showAlarmLines = false
        invalidate()
    }

    fun hasAlarmLines(): Boolean = showAlarmLines && listOf(alarmLoLo, alarmLo, alarmHi, alarmHiHi).any { it != null }

    fun zoomBy(factor: Float) {
        val target = (zoom * factor).coerceIn(1f, 24f)
        ValueAnimator.ofFloat(zoom, target).apply {
            duration = 220L
            interpolator = DecelerateInterpolator(1.5f)
            addUpdateListener { zoom = it.animatedValue as Float; invalidate() }
        }.start()
    }

    fun resetViewport(animated: Boolean = false) {
        if (!animated) {
            zoom = 1f; offset = 1f; crosshairX = null; invalidate(); return
        }
        ValueAnimator.ofFloat(zoom, 1f).apply {
            duration = 240L
            interpolator = DecelerateInterpolator(1.6f)
            addUpdateListener {
                zoom = it.animatedValue as Float
                offset += (1f - offset) * .16f
                invalidate()
            }
            addListener(object : android.animation.AnimatorListenerAdapter() {
                override fun onAnimationEnd(animation: android.animation.Animator) {
                    zoom = 1f; offset = 1f; crosshairX = null; invalidate()
                }
            })
            start()
        }
    }

    override fun onDetachedFromWindow() {
        animator?.cancel()
        animationProgress = 1f
        super.onDetachedFromWindow()
    }

    // --- Animation ---

    private fun startReveal() {
        animator?.cancel()
        animationProgress = 0f
        animator = ValueAnimator.ofFloat(0f, 1f).apply {
            duration = 620L
            addListener(object : android.animation.AnimatorListenerAdapter() {
                override fun onAnimationEnd(animation: android.animation.Animator) { animationProgress = 1f; invalidate() }
            })
            interpolator = DecelerateInterpolator(1.55f)
            addUpdateListener {
                animationProgress = it.animatedValue as Float
                invalidate()
            }
            start()
        }
    }

    // --- Touch ---

    override fun onTouchEvent(event: MotionEvent): Boolean {
        scaleDetector.onTouchEvent(event)
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                downX = event.x
                downAt = System.currentTimeMillis()
                moved = false
                crosshairX = plotLeft().let { max(it, min(event.x, plotRight())) }
                dispatchSelection()
                invalidate()
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                if (abs(event.x - downX) > dp(7f)) moved = true
                if (!scaleDetector.isInProgress && moved && zoom > 1.02f) {
                    val span = max(dp(1f), plotRight() - plotLeft())
                    offset = (offset - (event.x - downX) / span * .92f).coerceIn(0f, 1f)
                    downX = event.x
                }
                crosshairX = event.x.coerceIn(plotLeft(), plotRight())
                dispatchSelection()
                invalidate()
                return true
            }
            MotionEvent.ACTION_UP -> {
                val now = System.currentTimeMillis()
                if (!moved && now - downAt < 280L && now - lastTapAt < 380L) resetViewport(true)
                lastTapAt = now
                dispatchSelection()
                invalidate()
                return true
            }
            MotionEvent.ACTION_CANCEL -> return true
        }
        return true
    }

    // --- Drawing ---

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val w = width.toFloat(); val h = height.toFloat()
        if (w < dp(120f) || h < dp(160f)) return

        drawBackground(canvas, w, h)
        if (rows.size < 2 || selected.isEmpty()) {
            drawEmpty(canvas, if (rows.isEmpty()) "Brak danych dla wybranego zakresu" else "Wybierz serię do analizy")
            return
        }

        val (start, end) = visibleSlice()
        if (end - start < 2) {
            drawEmpty(canvas, "Powiększony zakres ma za mało punktów")
            return
        }
        val viewRows = rows.subList(start, end)
        val minTs = viewRows.first().ts
        val maxTs = viewRows.last().ts
        if (maxTs <= minTs) return

        if (mode == Mode.XY) drawXy(canvas, viewRows) else drawTimeSeries(canvas, viewRows, minTs, maxTs)
        drawAxes(canvas, minTs, maxTs)
        crosshairX?.let { drawCrosshair(canvas, viewRows, it, minTs, maxTs) }
        drawContextBar(canvas, start, end)
    }

    private fun drawBackground(canvas: Canvas, w: Float, h: Float) {
        // Rich gradient background
        val gradient = LinearGradient(0f, 0f, w, h, 0xFF0B1A2E.toInt(), 0xFF07131F.toInt(), Shader.TileMode.CLAMP)
        bgPaint.shader = gradient
        canvas.drawRoundRect(dp(2f), dp(2f), w-dp(2f), h-dp(2f), dp(18f), dp(18f), bgPaint)
        bgPaint.shader = null

        // Subtle top highlight
        val highlight = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            shader = LinearGradient(0f, dp(2f), 0f, dp(30f), 0x15FFFFFF, 0x00FFFFFF, Shader.TileMode.CLAMP)
        }
        canvas.drawRoundRect(dp(2f), dp(2f), w-dp(2f), dp(32f), dp(18f), dp(18f), highlight)

        // Border
        val border = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeWidth = dp(1f)
            color = 0x3A4A6A85
        }
        canvas.drawRoundRect(dp(2f), dp(2f), w-dp(2f), h-dp(2f), dp(18f), dp(18f), border)

        // Badge
        val cap = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFF6F8BA0.toInt()
            textSize = dp(7.8f)
            typeface = Typeface.create("sans-serif", Typeface.BOLD)
        }
        canvas.drawText(if (mode == Mode.XY) "KORELACJA" else if (selected.size <= 1) "1 SERIA" else "${selected.size} SERIE", dp(12f), dp(17f), cap)
        cap.textAlign = Paint.Align.RIGHT
        canvas.drawText(if (realOnly) "REAL" else "RAW", w-dp(12f), dp(17f), cap)
        cap.textAlign = Paint.Align.LEFT
    }

    private fun drawEmpty(canvas: Canvas, message: String) {
        val p = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFF8EA6BA.toInt()
            textSize = dp(12f)
            typeface = Typeface.create("sans-serif", Typeface.BOLD)
            textAlign = Paint.Align.CENTER
        }
        canvas.drawText(message, width/2f, height/2f, p)
        p.textAlign = Paint.Align.LEFT
    }

    private fun drawTimeSeries(canvas: Canvas, viewRows: List<TelemetryRecord>, minTs: Long, maxTs: Long) {
        val series = selectedSeries()
        val left = plotLeft(); val right = plotRight(); val top = plotTop(); val bottom = plotBottom()
        val prepared = series.map { it to buildValues(it, viewRows) }
        val finiteAll = prepared.flatMap { it.second.mapNotNull { p -> p.second } }
        if (finiteAll.isEmpty()) {
            drawEmpty(canvas, if (realOnly) "Brak świeżych danych" else "Brak danych")
            return
        }

        val common = fixedMin?.let { fm -> fixedMax?.let { fx -> fm to fx } } ?: rangeFor(finiteAll)
        val groups = prepared.groupBy { it.first.unit to it.first.servo }
        val useSplit = mode == Mode.SPLIT && groups.size > 1
        val leftSeries = if (useSplit) prepared.filter { it.first.unit == prepared.first().first.unit } else prepared
        val rightSeries = if (useSplit) prepared.filter { it.first.unit != prepared.first().first.unit } else emptyList()
        val leftVals = leftSeries.flatMap { it.second.mapNotNull { p -> p.second } }
        val rightVals = rightSeries.flatMap { it.second.mapNotNull { p -> p.second } }
        val leftRange = if (mode == Mode.NORMALIZED) 0.0 to 100.0 else if (mode == Mode.FROM_START) -100.0 to 100.0 else (fixedMin to fixedMax).takeUnless { it.first == null || it.second == null }?.let { it.first!! to it.second!! } ?: rangeFor(leftVals.ifEmpty { finiteAll })
        val rightRange = if (rightVals.isNotEmpty()) rangeFor(rightVals) else leftRange

        // Plot area with gradient
        val plotGradient = LinearGradient(left, top, right, bottom, 0xFF0E1F33.toInt(), 0xFF091724.toInt(), Shader.TileMode.CLAMP)
        plotPaint.shader = plotGradient
        canvas.drawRoundRect(left-dp(8f), top-dp(12f), right+dp(4f), bottom+dp(7f), dp(18f), dp(18f), plotPaint)
        plotPaint.shader = null

        // Day/night zones
        if (maxTs - minTs >= 36L * 3600L) {
            val dayPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL; color = 0x0B65AFC6 }
            val zone = java.time.ZoneId.of("Europe/Warsaw")
            var day = java.time.Instant.ofEpochSecond(minTs).atZone(zone).toLocalDate().atStartOfDay(zone).toEpochSecond()
            var flip = false
            while (day < maxTs) {
                val next = java.time.Instant.ofEpochSecond(day).atZone(zone).plusDays(1).toEpochSecond()
                val x1 = left + (right-left) * ((max(day,minTs)-minTs).toFloat()/max(1f,(maxTs-minTs).toFloat()))
                val x2 = left + (right-left) * ((min(next,maxTs)-minTs).toFloat()/max(1f,(maxTs-minTs).toFloat()))
                if (flip) canvas.drawRect(x1,top,x2,bottom,dayPaint)
                flip=!flip; day=next
            }
        }

        // Grid
        drawGrid(canvas, left, right, top, bottom)

        // Alarm lines
        if (showAlarmLines && mode != Mode.XY) {
            drawAlarmLine(canvas, alarmLoLo, "LoLo", 0xB0E06B82.toInt(), common.first, common.second, left, right, top, bottom)
            drawAlarmLine(canvas, alarmLo, "Lo", 0xB0C99B4A.toInt(), common.first, common.second, left, right, top, bottom)
            drawAlarmLine(canvas, alarmHi, "Hi", 0xB0C99B4A.toInt(), common.first, common.second, left, right, top, bottom)
            drawAlarmLine(canvas, alarmHiHi, "HiHi", 0xB0E06B82.toInt(), common.first, common.second, left, right, top, bottom)
        }

        // Gaps
        drawGaps(canvas, viewRows, minTs, maxTs, left, right, top, bottom)

        // Band
        if (showBand && prepared.size >= 2 && !useSplit && mode == Mode.COMMON) {
            drawEnvelopeBand(canvas, prepared, minTs, maxTs, left, right, top, bottom, common.first, common.second)
        }

        // Series
        prepared.forEach { (s, pts) ->
            if (pts.none { it.second != null }) return@forEach
            val range = if (useSplit && s in rightSeries.map { it.first }) rightRange else if (mode == Mode.NORMALIZED) 0.0 to 100.0 else if (mode == Mode.FROM_START) -100.0 to 100.0 else if (mode == Mode.COMMON || mode == Mode.SPLIT) common else rangeFor(pts.mapNotNull { it.second })
            drawSeriesLine(canvas, s, pts, minTs, maxTs, left, right, top, bottom, range.first, range.second)
        }

        // Latest markers
        drawLatestMarkers(canvas, prepared, minTs, maxTs, left, right, top, bottom, common)
    }

    private fun drawGrid(canvas: Canvas, left: Float, right: Float, top: Float, bottom: Float) {
        // Frame
        val frame = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeWidth = dp(1f)
            color = 0x4C46677D
        }
        canvas.drawRoundRect(left-dp(6f), top-dp(8f), right+dp(3f), bottom+dp(5f), dp(14f), dp(14f), frame)

        // Vertical lines
        gridPaint.strokeWidth = dp(.65f)
        for (i in 0..5) {
            val x = left + (right-left) * i/5f
            gridPaint.color = 0x2B5D7B8D
            canvas.drawLine(x, top, x, bottom, gridPaint)
        }

        // Horizontal lines
        for (i in 0..8) {
            val y = top + (bottom-top) * i/8f
            gridPaint.color = if (i % 2 == 0) 0x355B788A else 0x1E4C687A
            canvas.drawLine(left, y, right, y, gridPaint)
        }
    }

    private fun drawSeriesLine(canvas: Canvas, s: Series, pts: List<Pair<Long,Double?>>, minTs: Long, maxTs: Long, left: Float, right: Float, top: Float, bottom: Float, lo: Double, hi: Double) {
        val points = pts.mapNotNull { (ts, v) ->
            if (v == null) null
            else {
                val x = left + (right-left) * ((ts-minTs).toFloat()/max(1f,(maxTs-minTs).toFloat()))
                val y = bottom - (bottom-top) * (((v-lo)/max(1e-9,hi-lo)).toFloat().coerceIn(0f,1f))
                x to y
            }
        }
        if (points.size < 2) return

        // Build smooth path (Catmull-Rom to cubic bezier)
        val path = Path()
        path.moveTo(points[0].first, points[0].second)
        for (i in 0 until points.size - 1) {
            val p0 = if (i > 0) points[i-1] else points[0]
            val p1 = points[i]
            val p2 = points[i+1]
            val p3 = if (i + 2 < points.size) points[i+2] else points[1]

            val tension = 0.3f
            val cp1x = p1.first + (p2.first - p0.first) * tension
            val cp1y = p1.second + (p2.second - p0.second) * tension
            val cp2x = p2.first - (p3.first - p1.first) * tension
            val cp2y = p2.second - (p3.second - p1.second) * tension

            path.cubicTo(cp1x, cp1y, cp2x, cp2y, p2.first, p2.second)
        }

        // Gradient fill under line
        val fillPath = Path(path)
        fillPath.lineTo(points.last().first, bottom)
        fillPath.lineTo(points.first().first, bottom)
        fillPath.close()

        val fillGradient = LinearGradient(0f, top, 0f, bottom,
            Color.argb(55, Color.red(s.accent), Color.green(s.accent), Color.blue(s.accent)),
            Color.argb(5, Color.red(s.accent), Color.green(s.accent), Color.blue(s.accent)),
            Shader.TileMode.CLAMP)
        val fillPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.FILL
            shader = fillGradient
        }
        canvas.drawPath(fillPath, fillPaint)

        // Outer glow
        val glowPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeWidth = dp(6f)
            strokeCap = Paint.Cap.ROUND
            strokeJoin = Paint.Join.ROUND
            color = Color.argb(45, Color.red(s.accent), Color.green(s.accent), Color.blue(s.accent))
        }
        canvas.drawPath(path, glowPaint)

        // Main line
        val linePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeWidth = dp(2.2f)
            strokeCap = Paint.Cap.ROUND
            strokeJoin = Paint.Join.ROUND
            color = s.accent
        }
        canvas.drawPath(path, linePaint)
    }

    private fun drawLatestMarkers(canvas: Canvas, prepared: List<Pair<Series, List<Pair<Long, Double?>>>>, minTs: Long, maxTs: Long, left: Float, right: Float, top: Float, bottom: Float, common: Pair<Double,Double>) {
        if (maxTs <= minTs) return
        val latestTs = prepared.flatMap { it.second.filter { p -> p.second != null } }.maxOfOrNull { it.first } ?: return
        val lx = left + (right-left) * ((latestTs-minTs).toFloat()/max(1f,(maxTs-minTs).toFloat()))

        // Vertical dashed line
        val linePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0x4F7A93A8
            strokeWidth = dp(1f)
            pathEffect = DashPathEffect(floatArrayOf(dp(3f),dp(4f)),0f)
        }
        canvas.drawLine(lx,top,lx,bottom,linePaint)

        // "OSTATNI" label
        val labelPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xFFACC0CE.toInt()
            textSize = dp(7.4f)
            typeface = Typeface.DEFAULT_BOLD
            textAlign = Paint.Align.CENTER
        }
        canvas.drawText("OSTATNI", lx.coerceIn(left+dp(28f), right-dp(28f)), top+dp(10f), labelPaint)

        // Endpoint markers with labels
        prepared.take(5).forEachIndexed { idx, (series, pts) ->
            val last = pts.lastOrNull { it.second != null } ?: return@forEachIndexed
            val xx = left + (right-left) * ((last.first-minTs).toFloat()/max(1f,(maxTs-minTs).toFloat()))
            val yy = pointYForRange(series, last.second!!, top, bottom, common) ?: return@forEachIndexed

            // Halo
            val halo = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                color = Color.argb(52, Color.red(series.accent), Color.green(series.accent), Color.blue(series.accent))
            }
            canvas.drawCircle(xx, yy, dp(6f), halo)

            // Dot
            val dot = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = series.accent }
            canvas.drawCircle(xx, yy, dp(2.9f), dot)

            // Endpoint label
            if (prepared.size <= 4) {
                drawEndpointLabel(canvas, series, last.second!!, yy, right, top, bottom, idx)
            }
        }
    }

    private fun drawEndpointLabel(canvas: Canvas, series: Series, value: Double, y: Float, right: Float, top: Float, bottom: Float, slot: Int) {
        val w = dp(54f)
        val h = dp(18f)
        val x2 = right - dp(3f)
        val yy = (y - h/2f + (slot % 3) * dp(6f)).coerceIn(top, bottom-h)
        val x1 = x2 - w

        // Background
        val fill = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.argb(220, 10, 25, 39)
        }
        canvas.drawRoundRect(x1, yy, x2, yy+h, dp(8f), dp(8f), fill)

        // Border
        val stroke = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeWidth = dp(.8f)
            color = Color.argb(120, Color.red(series.accent), Color.green(series.accent), Color.blue(series.accent))
        }
        canvas.drawRoundRect(x1, yy, x2, yy+h, dp(8f), dp(8f), stroke)

        // Dot
        val dot = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = series.accent }
        canvas.drawCircle(x1+dp(7f), yy+h/2f, dp(2.0f), dot)

        // Text
        val tp = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.WHITE
            textSize = dp(7.1f)
            typeface = Typeface.create("sans-serif", Typeface.BOLD)
        }
        canvas.drawText(fmt(value) + if(series.servo) "%" else series.unit, x1+dp(12f), yy+dp(12.2f), tp)
    }

    private fun pointYForRange(series: Series, value: Double, top: Float, bottom: Float, common: Pair<Double,Double>): Float? {
        val v = transformValue(series, value, 0, listOf(0L to value))
        val range = when(mode) {
            Mode.NORMALIZED -> 0.0 to 100.0
            Mode.FROM_START -> -100.0 to 100.0
            else -> common
        }
        val (lo, hi) = range
        if (hi <= lo) return null
        val n = ((v - lo) / (hi - lo)).toFloat().coerceIn(0f, 1f)
        return bottom - (bottom - top) * n
    }

    private fun drawXy(canvas: Canvas, rows: List<TelemetryRecord>) {
        val series = selectedSeries().take(2)
        if (series.size < 2) { drawEmpty(canvas, "XY wymaga dwóch serii"); return }
        val a = buildValues(series[0], rows).filter { it.second != null }.associate { it.first to it.second!! }
        val b = buildValues(series[1], rows).filter { it.second != null }.associate { it.first to it.second!! }
        val pairs = a.keys.intersect(b.keys).sorted().map { it to (a[it]!! to b[it]!!) }
        if (pairs.size < 2) { drawEmpty(canvas, "Brak wspólnych punktów dla XY"); return }
        val xs = pairs.map { it.second.first }; val ys = pairs.map { it.second.second }
        val xr = rangeFor(xs); val yr = rangeFor(ys)
        val l = plotLeft(); val r = plotRight(); val t = plotTop(); val bot = plotBottom()

        val grid = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0x22546D80; strokeWidth = dp(.7f) }
        for (i in 0..4) {
            val yy = t + (bot-t) * i/4f; val xx = l + (r-l) * i/4f
            canvas.drawLine(l, yy, r, yy, grid); canvas.drawLine(xx, t, xx, bot, grid)
        }

        val glow = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.argb(35, Color.red(series[0].accent), Color.green(series[0].accent), Color.blue(series[0].accent))
        }
        val point = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = series[0].accent }

        pairs.forEach { (_, xy) ->
            val xx = l + (r-l) * ((xy.first-xr.first)/(xr.second-xr.first)).toFloat().coerceIn(0f,1f)
            val yy = bot - (bot-t) * ((xy.second-yr.first)/(yr.second-yr.first)).toFloat().coerceIn(0f,1f)
            canvas.drawCircle(xx, yy, dp(6f), glow); canvas.drawCircle(xx, yy, dp(2.7f), point)
        }

        titleText.color = 0xFF8EA6BA.toInt(); titleText.textSize = dp(8.8f)
        canvas.drawText("X  ${series[0].label}   ·   Y  ${series[1].label}", l, t-dp(15f), titleText)

        axisText.textSize = dp(8.7f); axisText.color = 0xFF8EA6BA.toInt()
        canvas.drawText(fmt(xr.first), l, t-dp(2f), axisText)
        val xrText = fmt(xr.second); canvas.drawText(xrText, r-axisText.measureText(xrText), t-dp(2f), axisText)
        val yrText = fmt(yr.second); canvas.drawText(yrText, dp(7f), t+dp(9f), axisText); canvas.drawText(fmt(yr.first), dp(7f), bot, axisText)
    }

    private fun drawAxes(canvas: Canvas, minTs: Long, maxTs: Long) {
        val left = plotLeft(); val right = plotRight(); val top = plotTop(); val bottom = plotBottom()
        val (start, end) = visibleSlice()
        if (end - start < 2) return
        val viewRows = rows.subList(start, end)

        axisPaint.color = 0x56738CA5
        axisPaint.strokeWidth = dp(1f)
        axisText.color = 0xFF8EA6BA.toInt()
        axisText.textSize = dp(10f)

        // Y axis (left) - value labels
        val finiteAll = selectedSeries().flatMap { s -> buildValues(s, viewRows).mapNotNull { it.second } }
        val range = fixedMin?.let { fm -> fixedMax?.let { fx -> fm to fx } } ?: rangeFor(finiteAll)
        val (lo, hi) = range

        for (i in 0..8) {
            val y = top + (bottom-top) * i/8f
            val v = hi - (hi-lo) * i/8.0
            val label = fmt(v)
            axisPaint.strokeWidth = dp(1f)
            canvas.drawLine(left-dp(6f), y, left, y, axisPaint)
            axisText.textAlign = Paint.Align.RIGHT
            canvas.drawText(label, left-dp(10f), y+dp(4f), axisText)
        }

        // X axis (bottom) - time labels
        val count = 5
        for (i in 0..count) {
            val ts = minTs + (maxTs-minTs) * i / count
            val x = left + (right-left) * i/count.toFloat()
            val label = fmtTs(ts)
            axisText.textAlign = Paint.Align.CENTER
            canvas.drawText(label, x, bottom+dp(20f), axisText)
        }
        axisText.textAlign = Paint.Align.LEFT
    }

    private fun drawCrosshair(canvas: Canvas, viewRows: List<TelemetryRecord>, x: Float, minTs: Long, maxTs: Long) {
        val left = plotLeft(); val right = plotRight(); val top = plotTop(); val bottom = plotBottom()

        // Vertical line
        val xx = x.coerceIn(left, right)
        canvas.drawLine(xx, top, xx, bottom, crosshairPaint)

        // Find nearest point
        val ratio = ((xx - left) / max(1f, right - left)).coerceIn(0f, 1f)
        val ts = minTs + ((maxTs-minTs) * ratio).toLong()
        val row = viewRows.minByOrNull { abs(it.ts - ts) } ?: return

        // Draw horizontal lines and dots for each series
        selectedSeries().take(5).forEach { s ->
            val pts = buildValues(s, viewRows)
            val nearest = pts.minByOrNull { abs(it.first - ts) } ?: return@forEach
            val v = nearest.second ?: return@forEach

            val range = if (mode == Mode.NORMALIZED) 0.0 to 100.0 else if (mode == Mode.FROM_START) -100.0 to 100.0 else (fixedMin to fixedMax).takeUnless { it.first == null || it.second == null }?.let { it.first!! to it.second!! } ?: rangeFor(selectedSeries().flatMap { buildValues(it, viewRows).mapNotNull { p -> p.second } })
            val (lo, hi) = range
            val yy = bottom - (bottom-top) * (((v-lo)/max(1e-9,hi-lo)).toFloat().coerceIn(0f,1f))

            // Horizontal dashed line
            val hLine = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                color = Color.argb(80, Color.red(s.accent), Color.green(s.accent), Color.blue(s.accent))
                strokeWidth = dp(1f)
                pathEffect = DashPathEffect(floatArrayOf(dp(4f), dp(4f)), 0f)
            }
            canvas.drawLine(left, yy, right, yy, hLine)

            // Dot
            val dot = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = s.accent }
            canvas.drawCircle(xx, yy, dp(4f), dot)
        }

        // Tooltip
        drawTooltip(canvas, row, x.coerceIn(left+dp(10f), right-dp(10f)), top)
    }

    private fun drawTooltip(canvas: Canvas, row: TelemetryRecord, x: Float, top: Float) {
        val dt = java.text.SimpleDateFormat("dd.MM HH:mm:ss", java.util.Locale.US).apply {
            timeZone = java.util.TimeZone.getTimeZone("Europe/Warsaw")
        }
        val timeStr = dt.format(java.util.Date(row.ts * 1000L))

        // Build lines
        val lines = mutableListOf<String>()
        lines.add(timeStr)
        selectedSeries().take(4).forEach { s ->
            val pts = buildValues(s, listOf(row))
            val v = pts.firstOrNull()?.second
            if (v != null) {
                lines.add("${s.label}: ${fmt(v)}${if (s.servo) "%" else s.unit}")
            }
        }

        // Calculate tooltip size
        val tooltipText = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.WHITE
            textSize = dp(9f)
            typeface = Typeface.create("sans-serif", Typeface.NORMAL)
        }
        val maxLineWidth = lines.maxOf { tooltipText.measureText(it) }
        val tooltipWidth = maxLineWidth + dp(16f)
        val tooltipHeight = lines.size * dp(18f) + dp(16f)

        val tooltipX = x.coerceIn(dp(10f), width.toFloat() - tooltipWidth - dp(10f))
        val tooltipY = (top - tooltipHeight - dp(10f)).coerceAtLeast(dp(10f))

        // Background
        val bg = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xF0121F31.toInt()
        }
        canvas.drawRoundRect(tooltipX, tooltipY, tooltipX + tooltipWidth, tooltipY + tooltipHeight, dp(8f), dp(8f), bg)

        // Border
        val border = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeWidth = dp(1f)
            color = 0x765F7890
        }
        canvas.drawRoundRect(tooltipX, tooltipY, tooltipX + tooltipWidth, tooltipY + tooltipHeight, dp(8f), dp(8f), border)

        // Text
        tooltipText.textAlign = Paint.Align.LEFT
        lines.forEachIndexed { idx, line ->
            val color = if (idx == 0) 0xFFACC0CE.toInt() else Color.WHITE
            tooltipText.color = color
            canvas.drawText(line, tooltipX + dp(8f), tooltipY + dp(14f) + idx * dp(18f), tooltipText)
        }
    }

    private fun drawContextBar(canvas: Canvas, start: Int, end: Int) {
        if (rows.size < 2 || zoom < 1.02f) return
        val left = plotLeft(); val right = plotRight()
        val trackY = height - dp(42f)
        val h = dp(26f)
        val a = left + (right-left) * (start.toFloat()/max(1,(rows.size-1)))
        val b = left + (right-left) * (end.toFloat()/max(1,(rows.size-1)))

        // Track background
        val trackBg = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0x181E2D3D
        }
        canvas.drawRoundRect(left, trackY, right, trackY+h, dp(6f), dp(6f), trackBg)

        // Active region
        val active = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xA52FD0EE.toInt()
        }
        canvas.drawRoundRect(a, trackY+dp(1f), b, trackY+h-dp(1f), dp(6f), dp(6f), active)

        // Edge
        val edge = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeWidth = dp(.8f)
            color = 0xB034CDE8.toInt()
        }
        canvas.drawRoundRect(a, trackY+dp(1f), b, trackY+h-dp(1f), dp(6f), dp(6f), edge)

        // Label
        titleText.textSize = dp(7.4f); titleText.color = 0xFF7B93A5.toInt(); titleText.typeface = Typeface.DEFAULT_BOLD
        canvas.drawText("POWIĘKSZENIE  ${zoom.toInt()}×", left, trackY-dp(4f), titleText)
        titleText.textAlign = Paint.Align.RIGHT
        titleText.textAlign = Paint.Align.LEFT
    }

    private fun drawGaps(canvas: Canvas, viewRows: List<TelemetryRecord>, minTs: Long, maxTs: Long, left: Float, right: Float, top: Float, bottom: Float) {
        if (viewRows.size < 2) return
        var gapStart: Long? = null
        for (i in 1 until viewRows.size) {
            val delta = viewRows[i].ts - viewRows[i-1].ts
            if (delta > GAP_BREAK_SECONDS) {
                if (gapStart == null) gapStart = viewRows[i-1].ts
            } else if (gapStart != null) {
                val x1 = left + (right-left) * ((gapStart-minTs).toFloat()/max(1f,(maxTs-minTs).toFloat()))
                val x2 = left + (right-left) * ((viewRows[i-1].ts-minTs).toFloat()/max(1f,(maxTs-minTs).toFloat()))
                val gapPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                    color = 0x1A2A3A4A
                }
                canvas.drawRect(x1, top, x2, bottom, gapPaint)
                gapStart = null
            }
        }
        if (gapStart != null) {
            val x1 = left + (right-left) * ((gapStart-minTs).toFloat()/max(1f,(maxTs-minTs).toFloat()))
            val gapPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0x1A2A3A4A }
            canvas.drawRect(x1, top, right, bottom, gapPaint)
        }
    }

    private fun drawAlarmLine(canvas: Canvas, value: Double?, label: String, color: Int, lo: Double, hi: Double, left: Float, right: Float, top: Float, bottom: Float) {
        if (value == null || !value.isFinite() || hi <= lo) return
        val n = ((value - lo) / (hi - lo)).toFloat().coerceIn(0f, 1f)
        val y = bottom - (bottom - top) * n
        val p = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeWidth = dp(1f)
            this.color = color
            pathEffect = DashPathEffect(floatArrayOf(dp(4f), dp(5f)), 0f)
        }
        canvas.drawLine(left, y, right, y, p)
        val t = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            this.color = color or 0xFF000000.toInt()
            textSize = dp(7.2f)
            typeface = Typeface.DEFAULT_BOLD
        }
        canvas.drawText("$label ${fmt(value)}", left + dp(4f), y - dp(3f), t)
    }

    private fun drawEnvelopeBand(canvas: Canvas, prepared: List<Pair<Series,List<Pair<Long,Double?>>>>, minTs: Long, maxTs: Long, left: Float, right: Float, top: Float, bottom: Float, lo: Double, hi: Double) {
        if (prepared.size < 2 || prepared.any { it.second.isEmpty() }) return
        val upper = Path(); val lower = Path(); var started = false
        val n = prepared.first().second.size
        for (i in 0 until n) {
            val vals = prepared.mapNotNull { it.second.getOrNull(i)?.second }
            if (vals.isEmpty()) continue
            val ts = prepared.first().second[i].first
            val x = left + (right-left) * ((ts-minTs).toFloat()/max(1f,(maxTs-minTs).toFloat()))
            val mx = vals.maxOrNull()!!; val mn = vals.minOrNull()!!
            val yu = bottom - (bottom-top) * (((mx-lo)/max(1e-9,hi-lo)).toFloat().coerceIn(0f,1f))
            val yl = bottom - (bottom-top) * (((mn-lo)/max(1e-9,hi-lo)).toFloat().coerceIn(0f,1f))
            if (!started) { upper.moveTo(x, yu); lower.moveTo(x, yl); started = true } else { upper.lineTo(x, yu); lower.lineTo(x, yl) }
        }
        if (!started) return
        val polygon = Path(upper)
        val rev = prepared.first().second.indices.reversed()
        var rs = false
        for (i in rev) {
            val vals = prepared.mapNotNull { it.second.getOrNull(i)?.second }
            if (vals.isEmpty()) continue
            val ts = prepared.first().second[i].first
            val x = left + (right-left) * ((ts-minTs).toFloat()/max(1f,(maxTs-minTs).toFloat()))
            val mn = vals.minOrNull()!!
            val y = bottom - (bottom-top) * (((mn-lo)/max(1e-9,hi-lo)).toFloat().coerceIn(0f,1f))
            if (!rs) { polygon.lineTo(x, y); rs = true } else polygon.lineTo(x, y)
        }
        polygon.close()
        val p = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.FILL
            shader = LinearGradient(0f, top, 0f, bottom, 0x223CC8F0, 0x043CC8F0, Shader.TileMode.CLAMP)
        }
        canvas.drawPath(polygon, p)
    }

    private fun dispatchSelection() {
        val x = crosshairX ?: return
        if (rows.size < 2 || selected.isEmpty()) return
        val (start, end) = visibleSlice()
        if (end - start < 2) return
        val vr = rows.subList(start, end)
        val minTs = vr.first().ts; val maxTs = vr.last().ts
        val ratio = ((x - plotLeft()) / (plotRight() - plotLeft())).coerceIn(0f, 1f)
        val ts = minTs + ((maxTs - minTs) * ratio).toLong()
        val row = vr.minByOrNull { abs(it.ts - ts) } ?: return
        val dt = java.text.SimpleDateFormat("dd.MM HH:mm:ss", java.util.Locale.US).apply {
            timeZone = java.util.TimeZone.getTimeZone("Europe/Warsaw")
        }
        val text = buildString {
            append(dt.format(java.util.Date(row.ts * 1000L)))
            selectedSeries().take(6).forEach { sr ->
                val v = if (sr.servo) {
                    val deg = if (sr.id == "servo_flap") row.k else row.s
                    if (!deg.isFinite()) null else if (sr.id == "servo_flap") deg/180.0*100.0 else deg/90.0*100.0
                } else {
                    sr.channel?.let { ch -> row.a.getOrNull(ch)?.takeIf { it.isFinite() }?.times(sr.scale) }
                }
                if (v != null) append("   •   ").append(sr.label).append(" ").append(fmt(v)).append(if (sr.servo) "%" else sr.unit)
            }
        }
        onSelectionChanged?.invoke(text)
    }

    private fun visibleSlice(): Pair<Int,Int> {
        val n = rows.size
        val count = max(2, (n/zoom).roundToInt().coerceAtMost(n))
        val maxStart = max(0, n-count)
        val start = (maxStart * (1f - offset)).roundToInt().coerceIn(0, maxStart)
        return start to min(n, start+count)
    }

    private fun buildValues(s: Series, source: List<TelemetryRecord>): List<Pair<Long,Double?>> = source.mapIndexed { idx, row ->
        var v: Double? = if (s.servo) {
            val deg = if (s.id == "servo_flap") row.k else row.s
            if (!deg.isFinite()) null else if (s.id == "servo_flap") deg/180.0*100.0 else deg/90.0*100.0
        } else {
            val q = s.channel?.let { quality(row.q, it) } ?: 0
            val simulated = s.channel?.let { (((row.sim) ushr it) and 1L) == 1L } == true
            if (realOnly && (q != 0 || simulated)) null else s.channel?.let { row.a.getOrNull(it)?.takeIf { v -> v.isFinite() }?.times(s.scale) }
        }
        if (anomalyFilter && v != null && s.channel != null && idx > 3) {
            val ch = s.channel!!
            val neigh = source.subList(max(0, idx-3), min(source.size, idx+4)).mapNotNull { it.a.getOrNull(ch)?.takeIf { v -> v.isFinite() }?.times(s.scale) }
            if (neigh.size >= 4) {
                val med = neigh.sorted()[neigh.size/2]
                val dev = abs(v!! - med)
                val span = max(1.0, (neigh.maxOrNull() ?: med) - (neigh.minOrNull() ?: med))
                if (dev > span * 4.0) v = null
            }
        }
        row.ts to v
    }

    private fun transformValue(s: Series, v: Double, index: Int, pts: List<Pair<Long,Double?>>): Double = when (mode) {
        Mode.NORMALIZED -> { val f = pts.mapNotNull { it.second }; val lo = f.minOrNull() ?: v; val hi = f.maxOrNull() ?: v; (v-lo)/max(1e-9,hi-lo)*100.0 }
        Mode.FROM_START -> { val start = pts.firstOrNull { it.second != null }?.second ?: v; (v-start)/max(1.0,abs(start))*100.0 }
        else -> v
    }

    // --- Helpers ---

    private fun plotLeft() = dp(46f)
    private fun plotRight() = width - dp(16f)
    private fun plotTop() = dp(42f)
    private fun plotBottom() = height - dp(78f)
    private fun quality(q: Long, channel: Int) = ((q ushr (channel*2)) and 3L).toInt()
    private fun rangeFor(values: List<Double>): Pair<Double,Double> {
        var lo = values.minOrNull() ?: 0.0
        var hi = values.maxOrNull() ?: 1.0
        if (lo == hi) { lo -= 1.0; hi += 1.0 }
        val pad = max(.5, (hi-lo) * .08)
        return lo-pad to hi+pad
    }
    private fun fmt(v: Double) = String.format(java.util.Locale.US, "%.1f", v)
    private fun fmtTs(ts: Long) = java.text.SimpleDateFormat("HH:mm", java.util.Locale.US).apply { timeZone = java.util.TimeZone.getTimeZone("Europe/Warsaw") }.format(java.util.Date(ts * 1000L))
    private fun dp(v: Float) = v * resources.displayMetrics.density

    private companion object {
        const val GAP_BREAK_SECONDS = 150L
    }
}
