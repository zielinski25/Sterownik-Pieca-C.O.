package pl.sterownikco.dev

import android.content.Context
import android.graphics.*
import android.view.View
import kotlin.math.*

/**
 * STEROWNIK CO — Premium trend overview.
 * Redesigned v1.0: smoother curves, better colors, glow effects.
 */
class DashboardOverviewView(context: Context) : View(context) {

    private val all = mutableListOf<TrendPoint>()
    private val maxPoints = 48

    data class TrendPoint(val outside: Float, val heating: Float, val boiler: Float, val pump: Boolean)

    private val outsidePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xFF55D7FF.toInt(); strokeWidth = dp(2.0f); style = Paint.Style.STROKE; strokeCap = Paint.Cap.ROUND; strokeJoin = Paint.Join.ROUND }
    private val heatingPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xFFFF5E67.toInt(); strokeWidth = dp(2.0f); style = Paint.Style.STROKE; strokeCap = Paint.Cap.ROUND; strokeJoin = Paint.Join.ROUND }
    private val boilerPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xFFFFB04A.toInt(); strokeWidth = dp(2.0f); style = Paint.Style.STROKE; strokeCap = Paint.Cap.ROUND; strokeJoin = Paint.Join.ROUND }
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xFFE8F1F8.toInt(); textSize = dp(9.4f); typeface = Typeface.DEFAULT_BOLD }
    private val valuePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xFF91A4B8.toInt(); textSize = dp(9.4f) }

    init {
        isClickable = false
    }

    fun submit(outside: Double, heating: Double, boiler: Double, pump: Boolean) {
        if (!outside.isFinite() && !heating.isFinite() && !boiler.isFinite()) return
        all.add(TrendPoint(
            if (outside.isFinite()) outside.toFloat() else 0f,
            if (heating.isFinite()) heating.toFloat() else 0f,
            if (boiler.isFinite()) boiler.toFloat() else 0f,
            pump
        ))
        while (all.size > maxPoints) all.removeAt(0)
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val l = dp(12f); val r = width - dp(12f); val t = dp(12f); val b = height - dp(18f)
        if (all.size < 2 || r - l < dp(20f)) return

        val allValues = all.flatMap { listOf(it.outside, it.heating, it.boiler) }
        val minV = allValues.minOrNull() ?: 0f
        val maxV = allValues.maxOrNull() ?: 1f
        val range = max(1f, maxV - minV)

        fun draw(values: (TrendPoint) -> Float, color: Int) {
            val points = mutableListOf<Pair<Float, Float>>()
            for (i in 0 until all.size) {
                val x = l + (r - l) * (i.toFloat() / max(1, all.size - 1).toFloat())
                val y = b - (b - t) * ((values(all[i]) - minV) / range).coerceIn(0f, 1f)
                points += x to y
            }
            if (points.size < 2) return

            // Build smooth path
            val path = Path().apply { moveTo(points.first().first, points.first().second) }
            for (i in 1 until points.size) {
                val prev = points[i-1]; val cur = points[i]
                val dx = (cur.first - prev.first) * .42f
                path.cubicTo(prev.first + dx, prev.second, cur.first - dx, cur.second, cur.first, cur.second)
            }

            // Gradient fill
            val fillPath = Path(path)
            fillPath.lineTo(points.last().first, b)
            fillPath.lineTo(points.first().first, b)
            fillPath.close()

            val fillGrad = LinearGradient(0f, t, 0f, b,
                Color.argb(35, Color.red(color), Color.green(color), Color.blue(color)),
                Color.argb(3, Color.red(color), Color.green(color), Color.blue(color)),
                Shader.TileMode.CLAMP)
            val fillPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL; shader = fillGrad }
            canvas.drawPath(fillPath, fillPaint)

            // Glow
            val glow = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.STROKE; strokeWidth = dp(5f); strokeCap = Paint.Cap.ROUND; strokeJoin = Paint.Join.ROUND
                this.color = Color.argb(30, Color.red(color), Color.green(color), Color.blue(color))
            }
            canvas.drawPath(path, glow)

            // Line
            val line = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.STROKE; strokeWidth = dp(2.0f); strokeCap = Paint.Cap.ROUND; strokeJoin = Paint.Join.ROUND
                this.color = color
            }
            canvas.drawPath(path, line)

            // End dot
            val end = points.last()
            val halo = Paint(Paint.ANTI_ALIAS_FLAG).apply { this.color = Color.argb(36, Color.red(color), Color.green(color), Color.blue(color)) }
            canvas.drawCircle(end.first, end.second, dp(6f), halo)
            canvas.drawCircle(end.first, end.second, dp(2.5f), line)
        }

        draw({ it.outside }, outsidePaint.color)
        draw({ it.heating }, heatingPaint.color)
        draw({ it.boiler }, boilerPaint.color)

        // Labels
        val last = all.last()
        valuePaint.textSize = dp(9.4f)
        canvas.drawText("Zew. ${one(last.outside)}°", l, height - dp(10f), valuePaint)
        valuePaint.color = heatingPaint.color
        canvas.drawText("Piec ${one(last.heating)}°", l + dp(84f), height - dp(10f), valuePaint)
        valuePaint.color = boilerPaint.color
        canvas.drawText("Bojler ${one(last.boiler)}°", l + dp(170f), height - dp(10f), valuePaint)
        valuePaint.color = 0xFFE8F1F8.toInt()

        if (last.pump) {
            val p = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xFF4ADE80.toInt(); strokeWidth = dp(1.6f); style = Paint.Style.STROKE }
            canvas.drawCircle(r - dp(18f), t + dp(1f), dp(4f), p)
            textPaint.color = 0xFF4ADE80.toInt()
            textPaint.textSize = dp(8.2f)
            textPaint.typeface = Typeface.DEFAULT_BOLD
            canvas.drawText("POMPA", r - dp(72f), t + dp(4f), textPaint)
            textPaint.color = 0xFF91A4B8.toInt()
            textPaint.typeface = Typeface.DEFAULT
        }
    }

    private fun one(v: Float) = String.format(java.util.Locale.US, "%.1f", v)
    private fun dp(v: Float) = v * resources.displayMetrics.density
}
