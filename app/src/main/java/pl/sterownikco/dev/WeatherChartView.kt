package pl.sterownikco.dev

import android.content.Context
import android.graphics.*
import android.view.View
import kotlin.math.min

/**
 * STEROWNIK CO — Simple weather chart for hourly forecast visualization.
 * Draws a line chart with optional filled area underneath.
 * Used in the Weather tab for 24h temperature/wind/cloud charts.
 */
class WeatherChartView @JvmOverloads constructor(
    context: Context,
    attrs: android.util.AttributeSet? = null
) : View(context, attrs) {

    data class Point(val x: Long, val y: Float)  // x=time, y=value

    private val linePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 0f // set per-instance
        strokeCap = Paint.Cap.ROUND
        strokeJoin = Paint.Join.ROUND
    }
    private val areaPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val gridPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 0.5f
        color = 0x1AFFFFFF.toInt()
    }
    private val labelPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = 0xFF8EA6BA.toInt()
        textSize = 0f // set per-instance
        textAlign = Paint.Align.CENTER
    }
    private val dotPaint = Paint(Paint.ANTI_ALIAS_FLAG)

    private var points: List<Point> = emptyList()
    private var minVal: Float = 0f
    private var maxVal: Float = 100f
    private var lineColor: Int = 0xFF00D4F5.toInt()
    private var areaColor: Int = 0x1800D4F5.toInt()
    private var chartHeight: Int = 130
    private var yUnit: String = ""
    private var xLabels: List<String> = emptyList() // hourly labels

    fun setData(pts: List<Float>, lineColor: Int, yUnit: String = "", minOverride: Float? = null, maxOverride: Float? = null) {
        points = pts.mapIndexed { i, v -> Point(i.toLong(), v) }
        this.lineColor = lineColor
        this.areaColor = Color.argb(24, Color.red(lineColor), Color.green(lineColor), Color.blue(lineColor))
        this.yUnit = yUnit
        val actualMin = minOverride ?: if (pts.isNotEmpty()) pts.min() else 0f
        val actualMax = maxOverride ?: if (pts.isNotEmpty()) pts.max() else 100f
        val pad = (actualMax - actualMin) * 0.15f
        minVal = actualMin - pad
        maxVal = actualMax + pad
        if (maxVal == minVal) { maxVal += 10f; minVal -= 10f }
        invalidate()
    }

    fun setXLabels(labels: List<String>) {
        xLabels = labels
        invalidate()
    }

    fun setHeight(h: Int) { chartHeight = h; invalidate() }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(dp(chartHeight.toFloat()).toInt(), MeasureSpec.AT_MOST))
    }

    override fun onDraw(c: Canvas) {
        super.onDraw(c)
        val w = width.toFloat()
        val h = height.toFloat()
        if (points.isEmpty() || w < 50f) return

        val top = dp(10f)
        val bottom = h - dp(38f)
        val left = dp(42f)  // Increased from 8f to make room for Y-axis labels
        val right = w - dp(8f)

        // Grid lines
        val gridLines = 4
        for (i in 0..gridLines) {
            val y = top + (bottom - top) * i / gridLines
            c.drawLine(left, y, right, y, gridPaint)
            val value = maxVal - (maxVal - minVal) * i / gridLines
            labelPaint.textSize = dp(7.5f)
            labelPaint.textAlign = Paint.Align.RIGHT
            val label = String.format("%.0f%s", value, yUnit)
            c.drawText(label, left - dp(4f), y + dp(2.5f), labelPaint)
        }

        // Draw area
        val path = Path()
        for (i in points.indices) {
            val x = left + (right - left) * i / maxOf(1, points.size - 1)
            val y = bottom - (bottom - top) * (points[i].y - minVal) / (maxVal - minVal)
            if (i == 0) path.moveTo(x, y) else path.lineTo(x, y)
        }
        path.lineTo(right, bottom)
        path.lineTo(left, bottom)
        path.close()
        areaPaint.color = areaColor
        c.drawPath(path, areaPaint)

        // Draw line
        val linePath = Path()
        for (i in points.indices) {
            val x = left + (right - left) * i / maxOf(1, points.size - 1)
            val y = bottom - (bottom - top) * (points[i].y - minVal) / (maxVal - minVal)
            if (i == 0) linePath.moveTo(x, y) else linePath.lineTo(x, y)
        }
        linePaint.color = lineColor
        linePaint.strokeWidth = dp(1.8f)
        c.drawPath(linePath, linePaint)

        // Draw dots
        dotPaint.color = lineColor
        for (i in points.indices) {
            val x = left + (right - left) * i / maxOf(1, points.size - 1)
            val y = bottom - (bottom - top) * (points[i].y - minVal) / (maxVal - minVal)
            c.drawCircle(x, y, dp(2.2f), dotPaint)
        }

        // X-axis labels
        if (xLabels.isNotEmpty()) {
            labelPaint.textSize = dp(7.0f)
            labelPaint.textAlign = Paint.Align.CENTER
            val step = maxOf(1, points.size / 6)
            for (i in xLabels.indices step step) {
                if (i < points.size) {
                    val x = left + (right - left) * i / maxOf(1, points.size - 1)
                    c.drawText(xLabels[i], x, h - dp(6f), labelPaint)
                }
            }
        }
    }

    private fun dp(v: Float) = v * resources.displayMetrics.density
}
