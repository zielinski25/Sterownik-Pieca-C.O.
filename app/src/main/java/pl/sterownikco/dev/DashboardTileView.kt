package pl.sterownikco.dev

import android.content.Context
import android.graphics.*
import android.view.MotionEvent
import android.view.View
import kotlin.math.*

/**
 * STEROWNIK CO — Premium telemetry tile.
 * Redesigned v1.0: cleaner layout, better proportions, subtle animations.
 */
class DashboardTileView @JvmOverloads constructor(
    context: Context,
    attrs: android.util.AttributeSet? = null
) : View(context, attrs) {

    enum class Kind { OUTSIDE, HEATING, BOILER, PANEL, ROOM, PRESSURE, HUMIDITY, PUMP, SERVO, MIXER, SMOKE, CHARTS, CLOCK }

    private var kind = Kind.OUTSIDE
    private var title = ""
    private var value = "—"
    private var state = "LIVE"
    private var accent = 0xFFFF9F43.toInt()
    private var active = false
    private var alarm = false
    private var fraction = 0f
    private var pressed = false
    private var flash = 0f
    private var lastValue = ""
    private var stale = false
    private var simulated = false
    private var simMinutes = 0
    private var animating = true
    private var attached = false

    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val stroke = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeCap = Paint.Cap.ROUND
        strokeJoin = Paint.Join.ROUND
    }
    private val text = Paint(Paint.ANTI_ALIAS_FLAG)
    private val valuePaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val small = Paint(Paint.ANTI_ALIAS_FLAG)

    init {
        isClickable = true
        isFocusable = true
        setOnTouchListener { _, e ->
            when (e.actionMasked) {
                MotionEvent.ACTION_DOWN -> { pressed = true; invalidate(); true }
                MotionEvent.ACTION_UP -> { pressed = false; invalidate(); performClick(); false }
                MotionEvent.ACTION_CANCEL -> { pressed = false; invalidate(); false }
                else -> true
            }
        }
    }

    override fun performClick(): Boolean { super.performClick(); return true }

    // Fix: Battery drain — stop animations when view is not visible
    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        attached = true
        if (active && !stale) postInvalidateDelayed(50)
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()
        attached = false
    }

    fun stopAnimations() { animating = false }
    fun startAnimations() {
        animating = true
        if (attached && (active && !stale || flash > 0.01f)) invalidate()
    }

    fun bind(
        kind: Kind,
        title: String,
        value: String,
        state: String = "LIVE",
        accent: Int,
        active: Boolean = false,
        alarm: Boolean = false,
        fraction: Float = 0f,
        simulated: Boolean = false,
        simMinutes: Int = 0
    ) {
        this.kind = kind
        this.title = title
        this.value = value
        this.state = state
        this.accent = accent
        this.active = active
        this.alarm = alarm
        this.fraction = fraction.coerceIn(0f, 1f)
        this.simulated = simulated
        this.simMinutes = simMinutes
        this.stale = false
        if (value != lastValue) flash = 1f
        lastValue = value
        invalidate()
    }

    fun pulse() { flash = 1f; invalidate() }

    fun setStale(value: Boolean) {
        stale = value
        if (value) pressed = false
        invalidate()
    }

    override fun onDraw(c: Canvas) {
        super.onDraw(c)
        val w = width.toFloat()
        val h = height.toFloat()
        if (w < dp(120f) || h < dp(90f)) return

        val radius = dp(18f)
        val left = dp(3f); val top = dp(3f); val right = w-dp(3f); val bottom = h-dp(3f)

        // Rich gradient background
        fill.shader = LinearGradient(0f, top, w, bottom, 0xFF102437.toInt(), 0xFF07131F.toInt(), Shader.TileMode.CLAMP)
        c.drawRoundRect(left, top, right, bottom, radius, radius, fill)
        fill.shader = null

        // Accent glow (subtle)
        fill.shader = RadialGradient(w*.95f, h*.08f, w*.85f,
            Color.argb(if (alarm) 64 else if (active) 50 else 22, Color.red(accent), Color.green(accent), Color.blue(accent)),
            Color.TRANSPARENT, Shader.TileMode.CLAMP)
        c.drawRoundRect(left, top, right, bottom, radius, radius, fill)
        fill.shader = null

        // Border
        stroke.strokeWidth = dp(if (alarm) 1.8f else if (pressed) 1.6f else if (active) 1.25f else 0.85f)
        stroke.color = when {
            simulated -> 0xFFEAB308.toInt()  // Yellow border for simulation
            alarm -> 0xFFFF627B.toInt()
            stale -> 0x6A7F93A3
            pressed -> Color.argb(185, Color.red(accent), Color.green(accent), Color.blue(accent))
            active -> Color.argb(150, Color.red(accent), Color.green(accent), Color.blue(accent))
            else -> 0x3C4B6477
        }
        c.drawRoundRect(left, top, right, bottom, radius, radius, stroke)

        // Illustration area
        val ix = dp(43f); val iy = dp(42f)
        val wellL = ix-dp(27f); val wellT = iy-dp(27f); val wellR = ix+dp(27f); val wellB = iy+dp(27f)
        fill.shader = LinearGradient(wellL, wellT, wellR, wellB,
            Color.argb(if (alarm) 44 else if (active) 38 else 28, Color.red(accent), Color.green(accent), Color.blue(accent)),
            0x15101D2B, Shader.TileMode.CLAMP)
        c.drawRoundRect(wellL, wellT, wellR, wellB, dp(16f), dp(16f), fill)
        fill.shader = null

        // Arc indicator
        stroke.strokeWidth = dp(2.1f)
        stroke.color = Color.argb(50, Color.red(accent), Color.green(accent), Color.blue(accent))
        c.drawArc(ix-dp(30f), iy-dp(30f), ix+dp(30f), iy+dp(30f), -105f, 210f, false, stroke)
        if (fraction > 0f) {
            stroke.strokeWidth = dp(2.1f)
            stroke.color = Color.argb(220, Color.red(accent), Color.green(accent), Color.blue(accent))
            c.drawArc(ix-dp(30f), iy-dp(30f), ix+dp(30f), iy+dp(30f), -105f, 210f*fraction, false, stroke)
        }

        // State badge
        val badgeText = if (simulated) "SYM ${simMinutes}min" else if (stale) "STALE" else state.uppercase().take(12)
        small.typeface = Typeface.create("sans-serif", Typeface.BOLD)
        small.textSize = dp(8.0f)
        val bw = max(dp(62f), min(dp(108f), small.measureText(badgeText) + dp(28f)))
        fill.color = when { simulated -> 0x2EEAB308.toInt(); stale -> 0x1C7F93A3; alarm -> 0x2E4B1420; active -> Color.argb(32, Color.red(accent), Color.green(accent), Color.blue(accent)); else -> 0x17213546 }
        c.drawRoundRect(right-bw-dp(10f), dp(11f), right-dp(10f), dp(31f), dp(10f), dp(10f), fill)
        fill.color = when { simulated -> 0xFFFFD166.toInt(); stale -> 0xFF8298A9.toInt(); alarm -> 0xFFFF7087.toInt(); active -> accent; else -> 0xFF71879A.toInt() }
        c.drawCircle(right-bw+dp(1f), dp(21f), dp(2.1f), fill)
        small.color = when { simulated -> 0xFFFFE082.toInt(); stale -> 0xFF9DB0BE.toInt(); alarm -> 0xFFFF9EAC.toInt(); active -> accent; else -> 0xFF96ABBB.toInt() }
        small.typeface = Typeface.create("sans-serif", Typeface.BOLD)
        small.textSize = dp(8.0f)
        small.textAlign = Paint.Align.CENTER
        c.drawText(badgeText, right-bw/2f-dp(4f), dp(25f), small)
        small.textAlign = Paint.Align.LEFT

        // Title
        text.color = 0xFF92A9BC.toInt()
        text.typeface = Typeface.create("sans-serif", Typeface.BOLD)
        text.textSize = dp(9.0f)
        text.letterSpacing = .055f
        c.drawText(title.uppercase(), dp(13f), h-dp(41f), text)

        // Value
        valuePaint.color = if (simulated) 0xFFFFE082.toInt() else if (alarm) 0xFFFF7B91.toInt() else 0xFFF4F8FB.toInt()
        valuePaint.typeface = Typeface.create("sans-serif", Typeface.BOLD)
        valuePaint.textSize = when {
            value.length > 15 -> dp(15.2f)
            value.length > 11 -> dp(18f)
            else -> dp(21.5f)
        }
        while (valuePaint.measureText(value) > w-dp(28f) && valuePaint.textSize > dp(12.8f)) valuePaint.textSize -= dp(.6f)
        c.drawText(value, dp(13f), h-dp(14f), valuePaint)

        // Bottom progress bar
        fill.color = 0x2330475D
        c.drawRoundRect(dp(13f), h-dp(5f), w-dp(13f), h-dp(2.8f), dp(2f), dp(2f), fill)
        if (fraction > 0f) {
            fill.color = Color.argb(if (alarm) 240 else 205, Color.red(accent), Color.green(accent), Color.blue(accent))
            c.drawRoundRect(dp(13f), h-dp(5f), dp(13f)+(w-dp(26f))*fraction, h-dp(2.8f), dp(2f), dp(2f), fill)
        }

        // Active pulse — only animate when attached (fix: battery drain)
        if (active && !stale && attached && animating) {
            val t = (System.currentTimeMillis() % 1600L) / 1600f
            fill.color = Color.argb((55f * (.25f + .75f * sin(t * PI.toFloat() * 2f))).toInt(), Color.red(accent), Color.green(accent), Color.blue(accent))
            c.drawCircle(w-dp(17f), h-dp(4f), dp(2.8f), fill)
            postInvalidateDelayed(50)
        }
        if (flash > 0.01f && attached && animating) {
            fill.color = Color.argb((flash*18f).toInt(), Color.red(accent), Color.green(accent), Color.blue(accent))
            c.drawRoundRect(dp(2f), dp(2f), w-dp(2f), h-dp(2f), radius, radius, fill)
            flash *= .82f
            postInvalidateDelayed(32)
        }
    }

    private fun dp(v: Float) = v * resources.displayMetrics.density
}
