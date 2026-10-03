package pl.sterownikco.dev

import android.content.Context
import android.graphics.*
import android.view.View
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin

/**
 * STEROWNIK CO — Premium weather illustration.
 * Animated sun/cloud/rain/snow based on weather code.
 */
class WeatherView(context: Context) : View(context) {

    var weatherCode = 0
    var isDay = true
    var temperature = Double.NaN

    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val stroke = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeCap = Paint.Cap.ROUND
        strokeJoin = Paint.Join.ROUND
    }
    private var time = 0f

    init {
        isClickable = false
    }

    fun bind(code: Int, day: Boolean, temp: Double) {
        weatherCode = code
        isDay = day
        temperature = temp
        invalidate()
    }

    override fun onDraw(c: Canvas) {
        super.onDraw(c)
        val w = width.toFloat()
        val h = height.toFloat()
        if (w < dp(60f) || h < dp(60f)) return

        time = (System.currentTimeMillis() % 2000L) / 2000f
        val cx = w / 2f
        val cy = h / 2f

        when {
            weatherCode == 0 -> drawSun(c, cx, cy, w)
            weatherCode in 1..2 -> drawPartlyCloudy(c, cx, cy, w)
            weatherCode == 3 -> drawCloudy(c, cx, cy, w)
            weatherCode in 45..48 -> drawFog(c, cx, cy, w)
            weatherCode in 51..57 -> drawDrizzle(c, cx, cy, w)
            weatherCode in 61..67 -> drawRain(c, cx, cy, w)
            weatherCode in 71..77 -> drawSnow(c, cx, cy, w)
            weatherCode in 80..86 -> drawShowers(c, cx, cy, w)
            weatherCode >= 95 -> drawThunderstorm(c, cx, cy, w)
            else -> drawSun(c, cx, cy, w)
        }
    }

    private fun drawSun(c: Canvas, cx: Float, cy: Float, w: Float) {
        val r = w * 0.3f
        // Glow
        val glow = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            shader = RadialGradient(cx, cy, r * 1.5f, 0x40FFD166.toInt(), Color.TRANSPARENT, Shader.TileMode.CLAMP)
        }
        c.drawCircle(cx, cy, r * 1.5f, glow)
        // Sun body
        fill.color = 0xFFFFD166.toInt()
        c.drawCircle(cx, cy, r * 0.6f, fill)
        // Rays — all math in Float (PI.toFloat())
        stroke.color = 0xFFFFD166.toInt()
        stroke.strokeWidth = dp(2.5f)
        for (i in 0 until 8) {
            val a = (i * 45 + time * 360) * PI.toFloat() / 180f
            val x1 = cx + cos(a) * r * 0.75f
            val y1 = cy + sin(a) * r * 0.75f
            val x2 = cx + cos(a) * r * 1.1f
            val y2 = cy + sin(a) * r * 1.1f
            c.drawLine(x1, y1, x2, y2, stroke)
        }
    }

    private fun drawCloud(c: Canvas, x: Float, y: Float, w: Float, color: Int) {
        val r = w * 0.15f
        fill.color = color
        c.drawCircle(x, y, r, fill)
        c.drawCircle(x + r * 0.9f, y - r * 0.3f, r * 1.1f, fill)
        c.drawCircle(x + r * 1.8f, y, r * 0.9f, fill)
        c.drawCircle(x + r * 0.5f, y + r * 0.4f, r * 0.8f, fill)
    }

    private fun drawPartlyCloudy(c: Canvas, cx: Float, cy: Float, w: Float) {
        drawSun(c, cx - w * 0.15f, cy - w * 0.1f, w * 0.7f)
        drawCloud(c, cx + w * 0.1f, cy + w * 0.05f, w, 0xFFC8D6E5.toInt())
    }

    private fun drawCloudy(c: Canvas, cx: Float, cy: Float, w: Float) {
        drawCloud(c, cx - w * 0.1f, cy - w * 0.1f, w, 0xFF8395A7.toInt())
        drawCloud(c, cx + w * 0.1f, cy + w * 0.05f, w * 0.9f, 0xFFC8D6E5.toInt())
    }

    private fun drawFog(c: Canvas, cx: Float, cy: Float, w: Float) {
        stroke.color = 0xFFC8D6E5.toInt()
        stroke.strokeWidth = dp(3f)
        for (i in 0 until 4) {
            val y = cy - w * 0.2f + i * w * 0.12f
            // All math in Float — PI.toFloat()
            val offset = sin(time * PI.toFloat() * 2f + i) * dp(3f)
            c.drawLine(cx - w * 0.3f + offset, y, cx + w * 0.3f + offset, y, stroke)
        }
    }

    private fun drawRainDrops(c: Canvas, cx: Float, cy: Float, w: Float, count: Int) {
        stroke.color = 0xFF54A0FF.toInt()
        stroke.strokeWidth = dp(2f)
        for (i in 0 until count) {
            val x = cx - w * 0.25f + (i * w * 0.5f / count)
            val dropY = cy + w * 0.1f + (time * w * 0.4f + i * w * 0.1f) % (w * 0.4f)
            c.drawLine(x, dropY, x - dp(2f), dropY + dp(8f), stroke)
        }
    }

    private fun drawDrizzle(c: Canvas, cx: Float, cy: Float, w: Float) {
        drawCloud(c, cx, cy - w * 0.1f, w, 0xFFC8D6E5.toInt())
        drawRainDrops(c, cx, cy, w, 4)
    }

    private fun drawRain(c: Canvas, cx: Float, cy: Float, w: Float) {
        drawCloud(c, cx, cy - w * 0.15f, w, 0xFF8395A7.toInt())
        drawRainDrops(c, cx, cy, w, 7)
    }

    private fun drawSnow(c: Canvas, cx: Float, cy: Float, w: Float) {
        drawCloud(c, cx, cy - w * 0.15f, w, 0xFFDFE6E9.toInt())
        fill.color = Color.WHITE
        for (i in 0 until 6) {
            val x = cx - w * 0.25f + (i * w * 0.5f / 6)
            val dropY = cy + w * 0.1f + (time * w * 0.3f + i * w * 0.08f) % (w * 0.35f)
            c.drawCircle(x, dropY, dp(2.5f), fill)
        }
    }

    private fun drawShowers(c: Canvas, cx: Float, cy: Float, w: Float) {
        drawRain(c, cx, cy, w)
    }

    private fun drawThunderstorm(c: Canvas, cx: Float, cy: Float, w: Float) {
        drawCloud(c, cx, cy - w * 0.15f, w, 0xFF576574.toInt())
        drawRainDrops(c, cx, cy, w, 7)
        // Lightning bolt
        if (time > 0.5f) {
            fill.color = 0xFFFFD166.toInt()
            val bolt = Path()
            bolt.moveTo(cx - dp(2f), cy + w * 0.05f)
            bolt.lineTo(cx + dp(4f), cy + w * 0.15f)
            bolt.lineTo(cx, cy + w * 0.15f)
            bolt.lineTo(cx + dp(3f), cy + w * 0.3f)
            bolt.lineTo(cx - dp(3f), cy + w * 0.18f)
            bolt.lineTo(cx + dp(1f), cy + w * 0.18f)
            bolt.close()
            c.drawPath(bolt, fill)
        }
    }

    private fun dp(v: Float) = v * resources.displayMetrics.density
}
