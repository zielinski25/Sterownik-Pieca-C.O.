package pl.sterownikco.dev

import android.content.Context
import android.graphics.*
import android.view.View
import kotlin.math.*

/**
 * STEROWNIK CO — Premium boiler illustration.
 * Redesigned v1.0: cleaner visual, better proportions, smooth animations.
 */
class BoilerHeroView(context: Context) : View(context) {

    private var temperature = 0.0
    private var overheating = false
    private var alarm = false
    private var time = 0f

    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val stroke = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeCap = Paint.Cap.ROUND
        strokeJoin = Paint.Join.ROUND
    }
    private val text = Paint(Paint.ANTI_ALIAS_FLAG)

    fun bind(temp: Double, overheat: Boolean, alarmState: Boolean) {
        temperature = temp
        overheating = overheat
        alarm = alarmState
        invalidate()
    }

    override fun onDraw(c: Canvas) {
        super.onDraw(c)
        val w = width.toFloat()
        val h = height.toFloat()
        if (w < dp(80f) || h < dp(80f)) return

        time = (System.currentTimeMillis() % 2000L) / 2000f

        val cx = w / 2f
        val cy = h / 2f
        val radius = min(w, h) * 0.4f

        // Outer glow
        val glowColor = if (alarm) Color.argb(60, 255, 80, 100) else if (overheating) Color.argb(50, 255, 160, 60) else Color.argb(40, 0, 212, 245)
        val glow = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            shader = RadialGradient(cx, cy, radius * 1.3f, glowColor, Color.TRANSPARENT, Shader.TileMode.CLAMP)
        }
        c.drawCircle(cx, cy, radius * 1.5f, glow)

        // Main circle with gradient
        val baseColor = if (alarm) 0xFF2A1520.toInt() else if (overheating) 0xFF2A2015.toInt() else 0xFF0F1E2E.toInt()
        fill.shader = RadialGradient(cx, cy * 0.8f, radius * 1.2f, baseColor, 0xFF0A1420.toInt(), Shader.TileMode.CLAMP)
        c.drawCircle(cx, cy, radius, fill)
        fill.shader = null

        // Border
        val borderColor = if (alarm) 0xFFFF6B81.toInt() else if (overheating) 0xFFFFB04A.toInt() else 0xFF00D4F5.toInt()
        stroke.color = borderColor
        stroke.strokeWidth = dp(2f)
        c.drawCircle(cx, cy, radius, stroke)

        // Inner ring
        stroke.color = Color.argb(40, Color.red(borderColor), Color.green(borderColor), Color.blue(borderColor))
        stroke.strokeWidth = dp(1f)
        c.drawCircle(cx, cy, radius * 0.75f, stroke)

        // Flame / energy indicator
        if (!alarm) {
            val flameColor = if (overheating) 0xFFFFB04A.toInt() else 0xFFFF9F43.toInt()
            val flameSize = radius * (0.3f + 0.1f * sin(time * PI * 2).toFloat())
            fill.color = Color.argb(80, Color.red(flameColor), Color.green(flameColor), Color.blue(flameColor))
            c.drawCircle(cx, cy + radius * 0.3f, flameSize, fill)

            fill.color = Color.argb(120, Color.red(flameColor), Color.green(flameColor), Color.blue(flameColor))
            c.drawCircle(cx, cy + radius * 0.3f, flameSize * 0.6f, fill)
        }

        // Temperature text
        text.color = if (alarm) 0xFFFF7B91.toInt() else Color.WHITE
        text.typeface = Typeface.create("sans-serif", Typeface.BOLD)
        text.textSize = dp(22f)
        text.textAlign = Paint.Align.CENTER
        val tempText = if (temperature.isFinite()) String.format(java.util.Locale.US, "%.1f°", temperature) else "—"
        c.drawText(tempText, cx, cy + dp(8f), text)

        // Subtitle
        text.textSize = dp(10f)
        text.color = 0xFF8EA6BA.toInt()
        text.typeface = Typeface.create("sans-serif", Typeface.NORMAL)
        val subtitle = when {
            alarm -> "ALARM"
            overheating -> "GORĄCY"
            temperature > 45 -> "PRACA"
            temperature.isFinite() -> "CZUWANIE"
            else -> "—"
        }
        c.drawText(subtitle, cx, cy + dp(26f), text)

        text.textAlign = Paint.Align.LEFT
        postInvalidateDelayed(50)
    }

    private fun dp(v: Float) = v * resources.displayMetrics.density
}
