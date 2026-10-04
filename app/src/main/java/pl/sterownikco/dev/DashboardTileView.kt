package pl.sterownikco.dev

import android.content.Context
import android.graphics.*
import android.view.MotionEvent
import android.view.View
import kotlin.math.*

/**
 * STEROWNIK CO — Premium telemetry tile.
 * v1.3: ilustracje jak na lokalnym panelu WWW ESP (TileArt) — wielokolorowe, animowane
 * warstwa po warstwie i sterowane realnym stanem (poziom bojlera, igła manometru, obroty pompy…).
 * Kontener ilustracji ma stany jak `.kaf-ikona` na ESP: zwykły / warn (symulacja) / err (alarm,
 * pulsowanie + "ping") / ok (urządzenie pracuje, zielona poświata) / dis (czujnik wyłączony, szarość).
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
    private var disabled = false
    private var fraction = 0f
    private var aux = Float.NaN
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
    private val grayLayer = Paint().apply {
        colorFilter = ColorMatrixColorFilter(ColorMatrix().apply { setSaturation(0f) })
        alpha = 140
    }
    private val artState = TileArt.State()
    private val visibleRect = Rect()
    private var simPulse = 0f

    // cache shaderów tła (zależą tylko od rozmiaru i stanu) — bez alokacji co klatkę
    private var bgShader: Shader? = null
    private var bgKey = 0L
    private var glowShader: Shader? = null
    private var glowKey = 0L

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
        postInvalidateDelayed(50)
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()
        attached = false
    }

    override fun onVisibilityChanged(changedView: View, visibility: Int) {
        super.onVisibilityChanged(changedView, visibility)
        // Powrót na zakładkę "Pulpit" (GONE → VISIBLE) — wznów pętlę animacji ilustracji.
        if (visibility == VISIBLE && attached && animating) invalidate()
    }

    fun stopAnimations() { animating = false }
    fun startAnimations() {
        animating = true
        if (attached) invalidate()
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
        simMinutes: Int = 0,
        aux: Float = Float.NaN,
        disabled: Boolean = false
    ) {
        this.kind = kind
        this.title = title
        this.value = value
        this.state = state
        this.accent = accent
        this.active = active
        this.alarm = alarm
        this.disabled = disabled
        this.fraction = fraction.coerceIn(0f, 1f)
        this.aux = aux
        this.simulated = simulated
        this.simMinutes = simMinutes
        this.simPulse = 0f
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
        val nowMs = System.currentTimeMillis()
        val t = ((nowMs % 3_600_000L) / 1000.0).toFloat()

        // Rich gradient background
        val bgStart = if (simulated) 0xFF3A2E0A.toInt() else 0xFF102437.toInt()
        val bgEnd = if (simulated) 0xFF1F1A05.toInt() else 0xFF07131F.toInt()
        val bk = (w.toLong() shl 32) xor (h.toLong() shl 8) xor (if (simulated) 1L else 0L)
        if (bgShader == null || bk != bgKey) { bgShader = LinearGradient(0f, top, w, bottom, bgStart, bgEnd, Shader.TileMode.CLAMP); bgKey = bk }
        fill.shader = bgShader
        fill.color = Color.WHITE
        c.drawRoundRect(left, top, right, bottom, radius, radius, fill)
        fill.shader = null

        // Accent glow (subtle)
        val glowAlpha = if (alarm) 64 else if (active) 50 else if (simulated) 45 else 22
        val glowColor = Color.argb(glowAlpha,
            if (simulated) 234 else Color.red(accent),
            if (simulated) 179 else Color.green(accent),
            if (simulated) 8 else Color.blue(accent))
        val gk = (w.toLong() shl 32) xor (h.toLong() shl 8) xor (glowColor.toLong() and 0xFFFFFFFFL)
        if (glowShader == null || gk != glowKey) { glowShader = RadialGradient(w*.95f, h*.08f, w*.85f, glowColor, Color.TRANSPARENT, Shader.TileMode.CLAMP); glowKey = gk }
        fill.shader = glowShader
        fill.color = Color.WHITE
        c.drawRoundRect(left, top, right, bottom, radius, radius, fill)
        fill.shader = null

        // Border
        stroke.strokeWidth = dp(if (alarm) 1.8f else if (simulated) 2.2f else if (pressed) 1.6f else if (active) 1.25f else 0.85f)
        stroke.color = when {
            simulated -> {
                val alpha = (200 + 55 * sin(simPulse)).toInt().coerceIn(0, 255)
                Color.argb(alpha, 234, 179, 8)  // Pulsing yellow border
            }
            alarm -> 0xFFFF627B.toInt()
            stale -> 0x6A7F93A3
            pressed -> Color.argb(185, Color.red(accent), Color.green(accent), Color.blue(accent))
            active -> Color.argb(150, Color.red(accent), Color.green(accent), Color.blue(accent))
            else -> 0x3C4B6477
        }
        c.drawRoundRect(left, top, right, bottom, radius, radius, stroke)

        // ── Ilustracja: kontener jak `.kaf-ikona` na ESP + stany warn/err/ok/dis ──
        val ix = dp(43f); val iy = dp(42f)
        val wellL = ix-dp(27f); val wellT = iy-dp(27f); val wellR = ix+dp(27f); val wellB = iy+dp(27f)
        val wellR14 = dp(15f)
        val okState = active && (kind == Kind.PUMP || kind == Kind.MIXER)
        val errPulse = .5f + .5f * sin(t * 2f * PI.toFloat() / 1.5f)
        fill.color = when {
            alarm -> Color.argb((70 + 40 * errPulse).toInt(), 255, 95, 120)
            simulated -> 0x2EFBBF24
            okState -> 0x264ADE80
            disabled -> 0x08FFFFFF
            else -> 0x0CFFFFFF
        }
        c.drawRoundRect(wellL, wellT, wellR, wellB, wellR14, wellR14, fill)
        if (!alarm && !simulated && !okState && !disabled) {
            // delikatny odcień koloru kafelka (ESP: `.kafelek` ma kolorową poświatę, ikona neutralną)
            fill.color = Color.argb(if (active) 30 else 18, Color.red(accent), Color.green(accent), Color.blue(accent))
            c.drawRoundRect(wellL, wellT, wellR, wellB, wellR14, wellR14, fill)
        }
        if (okState) {
            // zielona poświata wokół (ESP: `.kaf-ikona.ok` box-shadow)
            stroke.strokeWidth = dp(5f)
            stroke.color = 0x1A4ADE80
            c.drawRoundRect(wellL-dp(2.5f), wellT-dp(2.5f), wellR+dp(2.5f), wellB+dp(2.5f), wellR14+dp(2.5f), wellR14+dp(2.5f), stroke)
        }
        if (alarm) {
            // "ping": rozchodzący się pierścień (ESP: `.kaf-ikona.err::after`)
            val ph = ((nowMs % 1500L) / 1500f)
            val grow = dp(8f) * ph
            stroke.strokeWidth = dp(1.6f)
            stroke.color = Color.argb((190 * (1f - ph)).toInt(), 255, 107, 129)
            c.drawRoundRect(wellL-grow, wellT-grow, wellR+grow, wellB+grow, wellR14+grow, wellR14+grow, stroke)
        }
        stroke.strokeWidth = dp(1f)
        stroke.color = when {
            alarm -> 0x8CFF6B81.toInt()
            simulated -> 0x80FBBF24.toInt()
            okState -> 0x734ADE80
            else -> 0x12FFFFFF
        }
        c.drawRoundRect(wellL, wellT, wellR, wellB, wellR14, wellR14, stroke)

        artState.fraction = fraction
        artState.aux = aux
        artState.active = active
        artState.alarm = alarm
        artState.disabled = disabled
        artState.simulated = simulated
        if (disabled || stale) {
            // ESP `.dis`: grayscale + przygaszenie (tu także dla danych nieaktualnych)
            val layer = c.saveLayer(wellL, wellT, wellR, wellB, grayLayer)
            TileArt.draw(c, kind, ix, iy, dp(36f), t, artState)
            c.restoreToCount(layer)
        } else {
            TileArt.draw(c, kind, ix, iy, dp(36f), t, artState)
        }

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
        small.textSize = if (simulated) dp(8.5f) else dp(8.0f)
        val bw = max(dp(62f), min(dp(108f), small.measureText(badgeText) + dp(28f)))
        fill.color = when { simulated -> 0x40EAB308.toInt(); stale -> 0x1C7F93A3; alarm -> 0x2E4B1420; active -> Color.argb(32, Color.red(accent), Color.green(accent), Color.blue(accent)); else -> 0x17213546 }
        c.drawRoundRect(right-bw-dp(10f), dp(11f), right-dp(10f), dp(31f), dp(10f), dp(10f), fill)
        fill.color = when { simulated -> 0xFFFFD166.toInt(); stale -> 0xFF8298A9.toInt(); alarm -> 0xFFFF7087.toInt(); active -> accent; else -> 0xFF71879A.toInt() }
        c.drawCircle(right-bw+dp(1f), dp(21f), if (simulated) dp(2.8f) else dp(2.1f), fill)
        small.color = when { simulated -> 0xFFFFE082.toInt(); stale -> 0xFF9DB0BE.toInt(); alarm -> 0xFFFF9EAC.toInt(); active -> accent; else -> 0xFF96ABBB.toInt() }
        small.typeface = Typeface.create("sans-serif", Typeface.BOLD)
        small.textSize = if (simulated) dp(8.5f) else dp(8.0f)
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
        valuePaint.color = if (simulated) 0xFFFFE082.toInt() else if (alarm) 0xFFFF7B91.toInt() else if (disabled) 0xFF8EA6BA.toInt() else 0xFFF4F8FB.toInt()
        if (simulated) {
            valuePaint.setShadowLayer(dp(4f), 0f, 0f, 0x60EAB308.toInt())
        } else {
            valuePaint.clearShadowLayer()
        }
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

        // Active pulse
        if (active && !stale) {
            val tt = (nowMs % 1600L) / 1600f
            fill.color = Color.argb((55f * (.25f + .75f * sin(tt * PI.toFloat() * 2f))).toInt(), Color.red(accent), Color.green(accent), Color.blue(accent))
            c.drawCircle(w-dp(17f), h-dp(4f), dp(2.8f), fill)
        }

        // Simulation pulse animation
        if (simulated) {
            simPulse += 0.05f
            if (simPulse > PI.toFloat() * 2f) simPulse -= PI.toFloat() * 2f
        }
        if (flash > 0.01f) {
            fill.color = Color.argb((flash*18f).toInt(), Color.red(accent), Color.green(accent), Color.blue(accent))
            c.drawRoundRect(dp(2f), dp(2f), w-dp(2f), h-dp(2f), radius, radius, fill)
            flash *= .82f
        }

        // ── Pętla animacji (fix: battery drain) ──
        // Klatka co 50 ms tylko gdy kafelek jest naprawdę widoczny na ekranie; gdy przewinięty poza
        // ekran — rzadkie "nasłuchiwanie" (700 ms), żeby wznowić po powrocie. Zatrzymana całkiem,
        // gdy aplikacja w tle (stopAnimations) lub kafelek odłączony od okna.
        if (attached && animating && isShown) {
            val onScreen = getGlobalVisibleRect(visibleRect)
            val needsFrames = TileArt.isAnimated(kind, artState) || alarm || simulated || (active && !stale) || flash > 0.01f
            if (onScreen && needsFrames) postInvalidateDelayed(50) else postInvalidateDelayed(700)
        }
    }

    private fun dp(v: Float) = v * resources.displayMetrics.density
}
