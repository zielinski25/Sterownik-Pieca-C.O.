package pl.sterownikco.dev

import android.content.Context
import android.graphics.*
import android.view.View
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin

/**
 * STEROWNIK CO — Premium native icons.
 * Redesigned v1.0: cleaner shapes, better proportions, consistent style.
 */
class NativeIconView(context: Context) : View(context) {
    enum class Icon { DASHBOARD, CHART, SETTINGS, MORE, WEATHER, LOGS, TERMINAL, PUMP, SERVO, MIXER, CLOCK, THERMOMETER, SHIELD, REFRESH, RESET, EXPAND, INFO, CLOSE, NEXT, OUTSIDE, UPLOAD, WIFI }

    var icon: Icon = Icon.DASHBOARD
    var tint: Int = Color.WHITE
    var active: Boolean = false

    private val stroke = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeCap = Paint.Cap.ROUND
        strokeJoin = Paint.Join.ROUND
    }
    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)

    init { importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO }

    override fun onDraw(c: Canvas) {
        super.onDraw(c)
        val d = resources.displayMetrics.density
        val cx = width / 2f
        val cy = height / 2f
        val base = 42f * d
        val scale = minOf(width, height).coerceAtLeast(1) / base
        c.save()
        c.translate(cx, cy)
        c.scale(scale, scale)

        // Active halo
        if (active) {
            val halo = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                style = Paint.Style.FILL
                color = Color.argb(24, Color.red(tint), Color.green(tint), Color.blue(tint))
            }
            c.drawCircle(0f, 0f, 17 * d, halo)
        }

        stroke.color = tint
        stroke.strokeWidth = (if (active) 2.5f else 2.1f) * d
        fill.color = tint

        when (icon) {
            Icon.DASHBOARD -> {
                c.drawRoundRect(-15*d, -15*d, -1*d, -1*d, 3*d, 3*d, stroke)
                c.drawRoundRect(1*d, -15*d, 15*d, -1*d, 3*d, 3*d, stroke)
                c.drawRoundRect(-15*d, 1*d, -1*d, 15*d, 3*d, 3*d, stroke)
                c.drawRoundRect(1*d, 1*d, 15*d, 15*d, 3*d, 3*d, stroke)
            }
            Icon.CHART -> {
                val p = Path()
                p.moveTo(-16*d, 12*d); p.lineTo(-16*d, -14*d); p.lineTo(16*d, -14*d)
                c.drawPath(p, stroke)
                val q = Path()
                q.moveTo(-12*d, 7*d)
                q.cubicTo(-7*d, -1*d, -3*d, 5*d, 2*d, -4*d)
                q.cubicTo(7*d, -13*d, 11*d, -3*d, 14*d, -8*d)
                c.drawPath(q, stroke)
                c.drawCircle(14*d, -8*d, 2.2f*d, fill)
            }
            Icon.SETTINGS -> {
                c.drawCircle(0f, 0f, 7*d, stroke)
                repeat(8) { i ->
                    val a = i * PI / 4.0
                    c.drawLine(cos(a).toFloat()*10*d, sin(a).toFloat()*10*d, cos(a).toFloat()*15*d, sin(a).toFloat()*15*d, stroke)
                }
                c.drawCircle(0f, 0f, 2.2f*d, fill)
            }
            Icon.MORE -> { repeat(3) { i -> c.drawCircle((i-1)*8*d, 0f, 2.6f*d, fill) } }
            Icon.PUMP -> {
                c.drawCircle(0f, 0f, 3*d, fill)
                repeat(4) { i -> c.save(); c.rotate(i*90f); c.drawOval(-2.5f*d, -14*d, 3.5f*d, -1*d, fill); c.restore() }
                c.drawCircle(0f, 0f, 15*d, stroke)
            }
            Icon.SERVO -> {
                c.drawCircle(0f, 0f, 10*d, stroke)
                c.drawLine(-15*d, 0f, 15*d, 0f, stroke)
                c.drawCircle(9*d, -6*d, 2.6f*d, fill)
            }
            Icon.MIXER -> {
                c.drawCircle(0f, 0f, 3*d, fill)
                repeat(3) { i -> c.save(); c.rotate(i*120f); c.drawRoundRect(-2*d, -15*d, 3*d, -2*d, 2*d, 2*d, fill); c.restore() }
            }
            Icon.CLOCK -> {
                c.drawCircle(0f, 0f, 14*d, stroke)
                c.drawLine(0f, 0f, 0f, -8*d, stroke)
                c.drawLine(0f, 0f, 7*d, 4*d, stroke)
                c.drawCircle(0f, 0f, 2*d, fill)
            }
            Icon.THERMOMETER -> {
                c.drawRoundRect(-4*d, -16*d, 4*d, 6*d, 4*d, 4*d, stroke)
                c.drawCircle(0f, 10*d, 8*d, stroke)
                c.drawLine(0f, -8*d, 0f, 10*d, stroke)
                c.drawCircle(0f, 10*d, 4*d, fill)
            }
            Icon.SHIELD -> {
                val p = Path()
                p.moveTo(0f, -16*d); p.lineTo(13*d, -11*d); p.lineTo(10*d, 5*d)
                p.quadTo(0f, 16*d, 0f, 16*d); p.quadTo(0f, 16*d, -10*d, 5*d)
                p.lineTo(-13*d, -11*d); p.close()
                c.drawPath(p, stroke)
                c.drawLine(-5*d, 0f, -1*d, 4*d, stroke)
                c.drawLine(-1*d, 4*d, 7*d, -5*d, stroke)
            }
            Icon.REFRESH -> {
                c.drawArc(-13*d, -13*d, 13*d, 13*d, 40f, 285f, false, stroke)
                val p = Path(); p.moveTo(11*d, -10*d); p.lineTo(15*d, -9*d); p.lineTo(13*d, -5*d)
                c.drawPath(p, fill)
            }
            Icon.RESET -> {
                c.drawArc(-13*d, -13*d, 13*d, 13*d, -50f, 290f, false, stroke)
                val p = Path(); p.moveTo(-10*d, -11*d); p.lineTo(-15*d, -10*d); p.lineTo(-12*d, -6*d)
                c.drawPath(p, fill)
            }
            Icon.EXPAND -> {
                c.drawLine(-12*d, -4*d, -12*d, -12*d, stroke); c.drawLine(-12*d, -12*d, -4*d, -12*d, stroke)
                c.drawLine(12*d, 4*d, 12*d, 12*d, stroke); c.drawLine(12*d, 12*d, 4*d, 12*d, stroke)
                c.drawLine(4*d, -12*d, 12*d, -12*d, stroke); c.drawLine(12*d, -12*d, 12*d, -4*d, stroke)
                c.drawLine(-4*d, 12*d, -12*d, 12*d, stroke); c.drawLine(-12*d, 12*d, -12*d, 4*d, stroke)
            }
            Icon.INFO -> {
                c.drawCircle(0f, 0f, 15*d, stroke)
                c.drawCircle(0f, -7*d, 1.8f*d, fill)
                c.drawLine(0f, -1*d, 0f, 9*d, stroke)
            }
            Icon.CLOSE -> {
                c.drawLine(-10*d, -10*d, 10*d, 10*d, stroke); c.drawLine(10*d, -10*d, -10*d, 10*d, stroke)
            }
            Icon.NEXT -> {
                c.drawLine(-8*d, -10*d, 2*d, 0f, stroke); c.drawLine(2*d, 0f, -8*d, 10*d, stroke)
                c.drawLine(0f, 0f, 12*d, 0f, stroke)
            }
            Icon.OUTSIDE -> {
                c.drawCircle(-3*d, -4*d, 6*d, stroke)
                repeat(8) { i ->
                    val a = i * PI / 4.0
                    c.drawLine((-3*d+cos(a).toFloat()*10*d), (-4*d+sin(a).toFloat()*10*d), (-3*d+cos(a).toFloat()*14*d), (-4*d+sin(a).toFloat()*14*d), stroke)
                }
                c.drawArc(0*d, 2*d, 16*d, 12*d, 190f, 160f, false, stroke)
                c.drawLine(6*d, 2*d, 13*d, 2*d, stroke)
            }
            Icon.WEATHER -> {
                c.drawCircle(-5*d, -5*d, 5*d, stroke)
                repeat(8) { i ->
                    val a = i * PI / 4.0
                    c.drawLine((-5*d+cos(a).toFloat()*8*d), (-5*d+sin(a).toFloat()*8*d), (-5*d+cos(a).toFloat()*11*d), (-5*d+sin(a).toFloat()*11*d), stroke)
                }
                c.drawOval(-1*d, 2*d, 15*d, 11*d, stroke)
                c.drawCircle(4*d, 2*d, 4*d, stroke)
                c.drawCircle(10*d, 3*d, 4*d, stroke)
            }
            Icon.LOGS -> {
                c.drawCircle(-12*d, -8*d, 1.8f*d, fill); c.drawLine(-7*d, -8*d, 13*d, -8*d, stroke)
                c.drawCircle(-12*d, 0f, 1.8f*d, fill); c.drawLine(-7*d, 0f, 13*d, 0f, stroke)
                c.drawCircle(-12*d, 8*d, 1.8f*d, fill); c.drawLine(-7*d, 8*d, 8*d, 8*d, stroke)
            }
            Icon.TERMINAL -> {
                c.drawRoundRect(-15*d, -12*d, 15*d, 13*d, 4*d, 4*d, stroke)
                c.drawLine(-9*d, -5*d, -3*d, 0f, stroke); c.drawLine(-3*d, 0f, -9*d, 5*d, stroke)
                c.drawLine(1*d, 6*d, 9*d, 6*d, stroke)
            }
            Icon.UPLOAD -> {
                c.drawRect(-12*d, 3*d, 12*d, 13*d, stroke)
                c.drawLine(0f, 12*d, 0f, -9*d, stroke)
                c.drawLine(0f, -9*d, -6*d, -3*d, stroke)
                c.drawLine(0f, -9*d, 6*d, -3*d, stroke)
            }
            Icon.WIFI -> {
                c.drawArc(-16*d, -14*d, 16*d, 14*d, 218f, 104f, false, stroke)
                c.drawArc(-11*d, -9*d, 11*d, 10*d, 220f, 100f, false, stroke)
                c.drawArc(-6*d, -4*d, 6*d, 5*d, 224f, 92f, false, stroke)
                c.drawCircle(0f, 11*d, 2.4f*d, fill)
            }
        }
        c.restore()
    }
}
