package pl.sterownikco.dev

import android.graphics.Canvas
import android.graphics.Color
import android.graphics.DashPathEffect
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RadialGradient
import android.graphics.RectF
import android.graphics.Shader
import java.util.Calendar
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin

/**
 * STEROWNIK CO — ilustracje kafelków (port 1:1 ilustracji SVG z lokalnego panelu WWW ESP,
 * `ICO` / `BOJLER_SVG` w main_centrala.cpp, plus kilka "smaczków" których ESP nie ma).
 *
 * Każda ilustracja jest rysowana w przestrzeni 24×24 jednostek (jak viewBox SVG na ESP),
 * warstwa po warstwie, z własną animacją każdej warstwy (promienie słońca, płomienie,
 * banieczki w bojlerze, wirnik pompy itd.). Stan ilustracji (poziom wody w bojlerze,
 * wychylenie igły manometru, obroty pompy/mieszadła, trzepot klapy serwa, para nad
 * bojlerem) zależy od realnych odczytów przekazanych w [State] — dokładnie jak
 * `odswiezKafelki()` na ESP.
 */
object TileArt {

    class State {
        /** Znormalizowany odczyt 0..1 (bojler: (t−15)/55, ciśnienie: (p−970)/70, klapa: %/100). */
        var fraction = 0f
        /** Surowy odczyt / tryb (bojler: °C, serwo: tryb 1/2/3, zewn: °C). NaN gdy brak. */
        var aux = Float.NaN
        /** Urządzenie pracuje (pompa/mieszadło włączone, czujnik dymu zasilany). */
        var active = false
        var alarm = false
        /** Czujnik fizycznie wyłączony (dym przy zimnym piecu) — ilustracja szara. */
        var disabled = false
        var simulated = false
    }

    // ───────────────────────── kolory z ESP ─────────────────────────
    private val SUN = hex("#ffd94d")
    private val CLOUD_A = hex("#cbd5e1")
    private val CLOUD_B = hex("#e2e8f0")
    private val FLAME_OUT = hex("#c2410c")
    private val FLAME_MID = hex("#fb923c")
    private val FLAME_IN = hex("#fde047")
    private val TANK = hex("#64748b")
    private val WATER_COLD = hex("#0070ff")
    private val WATER_HOT = hex("#ffa54f")
    private val STEAM = hex("#e2e8f0")
    private val PANEL_BG = hex("#1e293b")
    private val PANEL_BORDER = hex("#475569")
    private val PANEL_GRID = hex("#fbbf24")
    private val ENERGY = hex("#fde047")
    private val HOUSE = hex("#94a3b8")
    private val WINDOW = hex("#fbbf24")
    private val DOOR = hex("#64748b")
    private val GAUGE_TRACK = hex("#334155")
    private val GAUGE_OK = hex("#4ade80")
    private val GAUGE_ERR = hex("#f87171")
    private val NEEDLE = hex("#f1f5f9")
    private val DROP_TOP = hex("#7dd3fc")
    private val DROP_BOTTOM = hex("#0284c7")
    private val RIPPLE = hex("#38bdf8")
    private val BLADES = intArrayOf(hex("#22d3ee"), hex("#0ea5e9"), hex("#0284c7"), hex("#0891b2"))
    private val HUB = hex("#e0f2fe")
    private val FLAP_FILL = hex("#94a3b8")
    private val FLAP_STROKE = hex("#cbd5e1")
    private val FLAP_MANUAL = hex("#fcd34d")
    private val FLAP_SAFE = hex("#fca5a5")
    private val SHAFT = hex("#475569")
    private val PADDLE_A = hex("#a16207")
    private val PADDLE_B = hex("#854d0e")
    private val PADDLE_HUB = hex("#e7e5e4")
    private val SMOKE_A = hex("#94a3b8")
    private val SMOKE_B = hex("#cbd5e1")
    private val SMOKE_ALARM_A = hex("#fca5a5")
    private val SMOKE_ALARM_B = hex("#f87171")
    private val BAR_1 = hex("#00d4f5")
    private val BAR_2 = hex("#ff9f43")
    private val BAR_3 = hex("#ffd32a")
    private val CLOCK_FACE = hex("#64748b")
    private val CLOCK_HAND = hex("#cbd5e1")
    private val CLOCK_SECOND = hex("#f87171")
    private val MOON = hex("#e2e8f0")
    private val STAR = hex("#fef3c7")

    // ───────────────────────── ścieżki SVG z ESP ─────────────────────────
    private val SUN_RAYS = svg("M9.5 3v1.8M9.5 15.2v1M3 9.5h1.8M15.2 9.5h1M5 5l1.3 1.3M12.7 12.7l1 1M14 5l-1.3 1.3M6.3 12.7l-1 1")
    private val FLAME_OUTER = svg("M12 21c-3.3 0-5.8-2.3-5.8-5.6 0-2.7 1.8-4.6 2.7-7.3.5 1.8 1.8 2.7 1.8 2.7-.5-2.7 1-5.5 2.9-6.4-1 2.7 0 4.6 1.4 6 1.4 1.4 2.4 3.2 2.4 4.9 0 3.3-2.3 5.7-5.4 5.7Z")
    private val FLAME_MIDDLE = svg("M12.2 19.5c-2.1 0-3.7-1.5-3.7-3.7 0-1.8 1.2-3 1.8-4.8.3 1.2 1.2 1.8 1.2 1.8-.3-1.8.7-3.7 1.9-4.3-.7 1.8 0 3 .9 3.9 1 1 1.6 2.1 1.6 3.3 0 2.1-1.5 3.8-3.7 3.8Z")
    private val FLAME_INNER = svg("M12.3 17.6c-1 0-1.9-.8-1.9-2 0-1 .6-1.6.9-2.6.15.6.6 1 .6 1-.15-1 .4-2 1-2.4-.4 1 0 1.6.5 2.2.5.5.9 1.1.9 1.8 0 1.2-.9 2-2 2Z")
    private val TANK_PATH = svg("M12 3c3.5 0 6 1.3 6 3v11c0 1.7-2.5 3-6 3s-6-1.3-6-3V6c0-1.7 2.5-3 6-3Z")
    private val STEAM_PATHS = arrayOf(
        svg("M9 3.6c-.8-.9-.8-1.8 0-2.6"),
        svg("M12.5 3c-.8-.9-.8-1.8 0-2.6"),
        svg("M16 3.6c-.8-.9-.8-1.8 0-2.6")
    )
    private val PANEL_RAYS = svg("M17.5 1.8v1M17.5 8.2v1M14 5.5h1M20 5.5h1M15 3l.9.9M19.1 7.1l.9.9M20 3l-.9.9M15.9 7.1l-.9.9")
    private val PANEL_GRID_PATH = svg("M3 13.6h13.5M3 17h13.5M7.9 9.5v9.5M12.4 9.5v9.5")
    private val HOUSE_ROOF = svg("M4 11 12 4l8 7")
    private val HOUSE_WALLS = svg("M6 10v9h12v-9")
    private val HOUSE_DOOR = svg("M10 19v-3.2h4V19")
    private val BLADE_PATHS = arrayOf(
        svg("M12 10.4c0-3 1.6-5.2 3.6-5.2 1.6 0 2.4 1.3 1.6 2.7-.9 1.5-3 2.5-5.2 2.5Z"),
        svg("M13.6 12c3 0 5.2 1.6 5.2 3.6 0 1.6-1.3 2.4-2.7 1.6-1.5-.9-2.5-3-2.5-5.2Z"),
        svg("M12 13.6c0 3-1.6 5.2-3.6 5.2-1.6 0-2.4-1.3-1.6-2.7.9-1.5 3-2.5 5.2-2.5Z"),
        svg("M10.4 12c-3 0-5.2-1.6-5.2-3.6 0-1.6 1.3-2.4 2.7-1.6 1.5.9 2.5 3 2.5 5.2Z")
    )
    private val PADDLES = arrayOf(
        svg("M12 12 12 5.3 15 7.2Z"),
        svg("M12 12 18 14.3 16.2 17Z"),
        svg("M12 12 6.5 16.8 4.6 15.1Z")
    )
    private val SMOKE_PATHS = arrayOf(
        svg("M7 20c1-1.4 1-2.6 0-4s-1-2.6 0-4 1-2.6 0-4"),
        svg("M12 20.5c1-1.4 1-2.6 0-4s-1-2.6 0-4 1-2.6 0-4 1-2.6 0-4"),
        svg("M17 20c1-1.4 1-2.6 0-4s-1-2.6 0-4 1-2.6 0-4")
    )
    private val CLOCK_TICKS = svg("M12 4v1.3M12 18.7V20M4 12h1.3M18.7 12H20")
    private val DROP_PATH = Path().apply {
        moveTo(12f, 3.2f)
        cubicTo(12f, 3.2f, 18f, 10f, 18f, 14.3f)
        arcTo(RectF(6f, 8.3f, 18f, 20.3f), 0f, 180f)
        cubicTo(6f, 10f, 12f, 3.2f, 12f, 3.2f)
        close()
    }
    private val MOON_PATH = Path().apply {
        val full = Path().apply { addCircle(9.5f, 9.5f, 3.4f, Path.Direction.CW) }
        val bite = Path().apply { addCircle(11.1f, 8.3f, 2.9f, Path.Direction.CW) }
        op(full, bite, Path.Op.DIFFERENCE)
    }

    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val stroke = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeCap = Paint.Cap.ROUND
        strokeJoin = Paint.Join.ROUND
    }
    private val rect = RectF()
    private var dropShader: Shader? = null
    private var surfaceShader: Shader? = null
    private var panelAlarmShader: Shader? = null
    private val wavePath = Path()
    private val shaderCache = HashMap<Long, Shader>()
    private val calendar: Calendar = Calendar.getInstance()

    /**
     * Miękka poświata (gradient radialny) — shader jest cache'owany (klucz: pozycja/promień/kolor),
     * a zmienna w czasie jasność sterowana przez alpha Paint-a, żeby nie alokować obiektów co klatkę.
     * [inverted] = przezroczysty środek, kolor na obwodzie (pierścień świetlny).
     */
    private fun glow(c: Canvas, x: Float, y: Float, r: Float, color: Int, alpha: Float, inverted: Boolean = false) {
        val key = (color.toLong() and 0xFFFFFFFFL) or
            (((x * 10f).toLong() and 0xFFL) shl 32) or (((y * 10f).toLong() and 0xFFL) shl 40) or
            (((r * 10f).toLong() and 0xFFL) shl 48) or ((if (inverted) 1L else 0L) shl 56)
        val sh = shaderCache.getOrPut(key) {
            if (inverted) RadialGradient(x, y, r, Color.TRANSPARENT, color, Shader.TileMode.CLAMP)
            else RadialGradient(x, y, r, color, Color.TRANSPARENT, Shader.TileMode.CLAMP)
        }
        fill.shader = sh
        fill.color = Color.WHITE
        fill.alpha = (alpha.coerceIn(0f, 1f) * 255f + .5f).toInt()
        c.drawCircle(x, y, r, fill)
        fill.shader = null
        fill.alpha = 255
    }

    /**
     * Rysuje ilustrację [kind] wyśrodkowaną w ([cx],[cy]) i mieszczącą się w kwadracie [size] px.
     * [t] — czas w sekundach (ciągły, do animacji), [s] — stan z odczytów.
     */
    fun draw(c: Canvas, kind: DashboardTileView.Kind, cx: Float, cy: Float, size: Float, t: Float, s: State) {
        val u = size / 24f
        c.save()
        c.translate(cx - 12f * u, cy - 12f * u)
        c.scale(u, u)
        when (kind) {
            DashboardTileView.Kind.OUTSIDE -> outside(c, t, s)
            DashboardTileView.Kind.HEATING -> heating(c, t, s)
            DashboardTileView.Kind.BOILER -> boiler(c, t, s)
            DashboardTileView.Kind.PANEL -> panel(c, t, s)
            DashboardTileView.Kind.ROOM -> room(c, t)
            DashboardTileView.Kind.PRESSURE -> pressure(c, s)
            DashboardTileView.Kind.HUMIDITY -> humidity(c, t)
            DashboardTileView.Kind.PUMP -> pump(c, t, s)
            DashboardTileView.Kind.SERVO -> servo(c, t, s)
            DashboardTileView.Kind.MIXER -> mixer(c, t, s)
            DashboardTileView.Kind.SMOKE -> smoke(c, t, s)
            DashboardTileView.Kind.CHARTS -> charts(c, t)
            DashboardTileView.Kind.CLOCK -> clock(c)
        }
        c.restore()
    }

    /** Czy ilustracja ma warstwy animowane w danym stanie (żeby nie odświeżać bez potrzeby). */
    fun isAnimated(kind: DashboardTileView.Kind, s: State): Boolean = when (kind) {
        DashboardTileView.Kind.PRESSURE -> false
        DashboardTileView.Kind.SERVO -> isServoAuto(s)
        else -> true
    }

    // ───────────────────────── ZEWN: słońce + chmurka (noc: księżyc + gwiazdy) ─────────────────────────
    private fun outside(c: Canvas, t: Float, s: State) {
        calendar.timeInMillis = System.currentTimeMillis()
        val hour = calendar.get(Calendar.HOUR_OF_DAY)
        val night = hour < 6 || hour >= 21
        if (!night) {
            // poświata słońca
            glow(c, 9.5f, 9.5f, 6.5f, SUN, .32f)
            c.save()
            c.rotate(360f * cyc(t, 16f), 9.5f, 9.5f)
            strokePath(c, SUN_RAYS, SUN, 1.3f)
            c.restore()
            fillCircle(c, 9.5f, 9.5f, 3.3f, SUN)
        } else {
            glow(c, 9.5f, 9.5f, 6.5f, MOON, .18f)
            fillPath(c, MOON_PATH, MOON)
            star(c, 3.6f, 4.6f, 1.1f, .45f + .55f * sway(cyc(t, 2.3f)))
            star(c, 16.4f, 4.2f, .85f, .45f + .55f * sway(cyc(t, 3.1f, 1.2f)))
            star(c, 5.2f, 13.4f, .6f, .35f + .65f * sway(cyc(t, 2.7f, .6f)))
        }
        // chmurka dryfująca (−1.4 px w 5 s); mróz → delikatnie błękitna
        val frost = s.aux.isFinite() && s.aux < 0f
        val cloudA = if (frost) mix(CLOUD_A, hex("#93c5fd"), .45f) else CLOUD_A
        val cloudB = if (frost) mix(CLOUD_B, hex("#bfdbfe"), .45f) else CLOUD_B
        c.save()
        c.translate(-1.4f * sway(cyc(t, 5f)), 0f)
        fillEllipse(c, 14.5f, 16.5f, 6f, 3f, cloudA)
        fillCircle(c, 11.3f, 15f, 2.4f, cloudB)
        fillCircle(c, 15.5f, 13.8f, 2.9f, cloudB)
        c.restore()
    }

    private fun star(c: Canvas, x: Float, y: Float, r: Float, alpha: Float) {
        stroke.pathEffect = null
        stroke.color = withAlpha(STAR, alpha)
        stroke.strokeWidth = .45f
        c.drawLine(x - r, y, x + r, y, stroke)
        c.drawLine(x, y - r, x, y + r, stroke)
    }

    // ───────────────────────── OGRZ: 3 płomienie, każdy w innym rytmie ─────────────────────────
    private fun heating(c: Canvas, t: Float, s: State) {
        val speed = if (s.alarm) .55f else 1f
        val glowColor = if (s.alarm) GAUGE_ERR else FLAME_MID
        glow(c, 12f, 17f, 9f, glowColor, .22f + .12f * sway(cyc(t, 1.9f * speed)))
        flame(c, FLAME_OUTER, FLAME_OUT, 21f, 1.9f * speed, 0f, t)
        flame(c, FLAME_MIDDLE, FLAME_MID, 19.5f, 1.3f * speed, .12f, t)
        flame(c, FLAME_INNER, FLAME_IN, 17.6f, .85f * speed, .25f, t)
    }

    private fun flame(c: Canvas, p: Path, color: Int, originY: Float, period: Float, delay: Float, t: Float) {
        val k = sway(cyc(t, period, delay))
        c.save()
        c.translate(12f, originY)
        c.scale(1f, 1f + .08f * k)
        c.rotate(-2f + 4f * k)
        c.translate(-12f, -originY)
        fillPath(c, p, color)
        c.restore()
    }

    // ───────────────────────── BOJLER: poziom + kolor wody, banieczki, para > 45 °C ─────────────────────────
    private fun boiler(c: Canvas, t: Float, s: State) {
        val pct = s.fraction.coerceIn(.05f, .95f)
        val water = mix(WATER_COLD, WATER_HOT, pct)
        val surface = 20f - 15f * pct
        c.save()
        c.clipPath(TANK_PATH)
        // woda z lekko falującą powierzchnią
        val wave = wavePath
        wave.reset()
        wave.moveTo(5f, 21f)
        wave.lineTo(5f, surface)
        var x = 5f
        while (x <= 19f) {
            wave.lineTo(x, surface + .32f * sin((x * 1.1f + t * 2.6f).toDouble()).toFloat())
            x += 1f
        }
        wave.lineTo(19f, 21f)
        wave.close()
        fillPath(c, wave, water, .84f)
        // jaśniejsza warstwa przy powierzchni (shader stały, przesuwany translacją)
        if (surfaceShader == null) surfaceShader = LinearGradient(0f, 0f, 0f, 4f, withAlpha(Color.WHITE, .16f), Color.TRANSPARENT, Shader.TileMode.CLAMP)
        c.save()
        c.translate(0f, surface)
        fill.shader = surfaceShader
        fill.color = Color.WHITE
        c.drawRect(5f, 0f, 19f, 4f, fill)
        fill.shader = null
        c.restore()
        // banieczki unoszące się 2.4 s (ease-in)
        bubble(c, 9f, 17f, .7f, 0f, t, surface)
        bubble(c, 13.2f, 18f, .55f, .8f, t, surface)
        bubble(c, 15f, 16f, .6f, 1.6f, t, surface)
        c.restore()
        strokePath(c, TANK_PATH, TANK, 1.6f)
        // odblask na szkle zbiornika
        stroke.pathEffect = null
        stroke.color = withAlpha(Color.WHITE, .14f)
        stroke.strokeWidth = .8f
        c.drawLine(7.6f, 6.8f, 7.6f, 16.2f, stroke)
        // para — tylko gdy woda naprawdę gorąca
        val temp = if (s.aux.isFinite()) s.aux else 15f + 55f * s.fraction
        if (temp > 45f) {
            for (i in STEAM_PATHS.indices) {
                val ph = cyc(t, 2.6f, .6f * i)
                val e = easeOut(ph)
                val alpha = if (ph < .3f) ph / .3f * .85f else .85f * (1f - (ph - .3f) / .7f)
                c.save()
                c.translate(0f, -3.5f * e)
                strokePath(c, STEAM_PATHS[i], STEAM, 1f, alpha)
                c.restore()
            }
        }
    }

    private fun bubble(c: Canvas, x: Float, y: Float, r: Float, delay: Float, t: Float, surface: Float) {
        val ph = cyc(t, 2.4f, delay)
        val yy = y - 15f * ph * ph
        if (yy < surface + r) return
        val alpha = if (ph < .15f) ph / .15f * .9f else .9f * (1f - (ph - .15f) / .85f)
        fillCircle(c, x, yy, r, withAlpha(Color.WHITE, .65f * alpha))
    }

    // ───────────────────────── PANEL: słońce + ogniwa + impulsy energii ─────────────────────────
    private fun panel(c: Canvas, t: Float, s: State) {
        c.save()
        c.rotate(360f * cyc(t, 16f), 17.5f, 5.5f)
        strokePath(c, PANEL_RAYS, SUN, 1.2f)
        c.restore()
        fillCircle(c, 17.5f, 5.5f, 2f, SUN)
        rect.set(3f, 9.5f, 16.5f, 19f)
        fill.color = PANEL_BG
        c.drawRoundRect(rect, 1f, 1f, fill)
        if (s.alarm) {
            if (panelAlarmShader == null) panelAlarmShader = LinearGradient(3f, 9.5f, 16.5f, 19f, withAlpha(GAUGE_ERR, .35f), withAlpha(GAUGE_ERR, .08f), Shader.TileMode.CLAMP)
            fill.shader = panelAlarmShader
            fill.color = Color.WHITE
            c.drawRoundRect(rect, 1f, 1f, fill)
            fill.shader = null
        }
        stroke.pathEffect = null
        stroke.color = PANEL_BORDER
        stroke.strokeWidth = .6f
        c.drawRoundRect(rect, 1f, 1f, stroke)
        strokePath(c, PANEL_GRID_PATH, PANEL_GRID, .5f, .8f, Paint.Cap.BUTT)
        energy(c, 9.7f, 13f, .55f, 0f, t)
        energy(c, 13.5f, 15.5f, .5f, .9f, t)
    }

    private fun energy(c: Canvas, x: Float, y: Float, r: Float, delay: Float, t: Float) {
        val ph = cyc(t, 1.8f, delay)
        val alpha = if (ph < .3f) ph / .3f else 1f - (ph - .3f) / .7f
        fillCircle(c, x, y + 4f * ph * ph, r, withAlpha(ENERGY, alpha))
    }

    // ───────────────────────── POKÓJ: domek z ciepłym oknem ─────────────────────────
    private fun room(c: Canvas, t: Float) {
        val k = .5f + .5f * sway(cyc(t, 3f))
        glow(c, 12f, 14f, 5f, WINDOW, .28f * k)
        strokePath(c, HOUSE_ROOF, HOUSE, 1.6f)
        strokePath(c, HOUSE_WALLS, HOUSE, 1.6f)
        rect.set(9.3f, 12f, 14.7f, 16.2f)
        fill.color = withAlpha(WINDOW, k)
        c.drawRoundRect(rect, .4f, .4f, fill)
        strokePath(c, HOUSE_DOOR, DOOR, 1.4f, 1f, Paint.Cap.BUTT)
    }

    // ───────────────────────── CIŚNIENIE: manometr 270° (łuk + igła z odczytu) ─────────────────────────
    private fun pressure(c: Canvas, s: State) {
        val fr = s.fraction.coerceIn(.03f, .97f)
        rect.set(4f, 4f, 20f, 20f)
        stroke.pathEffect = null
        stroke.strokeWidth = 2.2f
        stroke.color = GAUGE_TRACK
        c.drawArc(rect, 135f, 270f, false, stroke)
        stroke.color = mix(GAUGE_OK, GAUGE_ERR, fr)
        c.drawArc(rect, 135f, 270f * fr, false, stroke)
        // podziałka
        stroke.strokeWidth = .7f
        stroke.color = withAlpha(NEEDLE, .35f)
        for (i in 0..4) {
            val a = Math.toRadians((135f + 67.5f * i).toDouble())
            val ca = cos(a).toFloat(); val sa = sin(a).toFloat()
            c.drawLine(12f + 5.4f * ca, 12f + 5.4f * sa, 12f + 6.4f * ca, 12f + 6.4f * sa, stroke)
        }
        // igła
        val a = Math.toRadians((135f + 270f * fr).toDouble())
        stroke.strokeWidth = 1.4f
        stroke.color = NEEDLE
        c.drawLine(12f, 12f, 12f + 6.7f * cos(a).toFloat(), 12f + 6.7f * sin(a).toFloat(), stroke)
        fillCircle(c, 12f, 12f, 1.5f, NEEDLE)
    }

    // ───────────────────────── WILGOTNOŚĆ: kropla + rozchodzące się fale ─────────────────────────
    private fun humidity(c: Canvas, t: Float) {
        if (dropShader == null) dropShader = LinearGradient(0f, 3.2f, 0f, 20.3f, DROP_TOP, DROP_BOTTOM, Shader.TileMode.CLAMP)
        fill.shader = dropShader
        fill.color = Color.WHITE
        c.drawPath(DROP_PATH, fill)
        fill.shader = null
        // odblask
        c.save()
        c.rotate(-18f, 9.9f, 12.6f)
        fillEllipse(c, 9.9f, 12.6f, .8f, 1.7f, withAlpha(Color.WHITE, .32f))
        c.restore()
        ripple(c, 3f, 1f, .8f, 0f, t)
        ripple(c, 5f, 1.6f, .6f, .7f, t)
    }

    private fun ripple(c: Canvas, rx: Float, ry: Float, width: Float, delay: Float, t: Float) {
        val e = easeOut(cyc(t, 2.2f, delay))
        val sc = .7f + .6f * e
        c.save()
        c.translate(12f, 21.5f)
        c.scale(sc, sc)
        stroke.pathEffect = null
        stroke.strokeWidth = width
        stroke.color = withAlpha(RIPPLE, .7f * (1f - e))
        rect.set(-rx, -ry, rx, ry)
        c.drawOval(rect, stroke)
        c.restore()
    }

    // ───────────────────────── POMPA: 4 łopatki (obrót gdy pracuje) + pierścień przepływu ─────────────────────────
    private fun pump(c: Canvas, t: Float, s: State) {
        if (s.active) glow(c, 12f, 12f, 11f, RIPPLE, .16f, inverted = true)
        stroke.strokeWidth = .9f
        stroke.color = withAlpha(RIPPLE, if (s.active) .55f else .28f)
        stroke.pathEffect = DashPathEffect(floatArrayOf(3f, 5f), if (s.active) 8f * (1f - cyc(t, 1.1f)) else 0f)
        c.drawCircle(12f, 12f, 9.3f, stroke)
        stroke.pathEffect = null
        c.save()
        if (s.active) c.rotate(360f * cyc(t, 1.6f), 12f, 12f)
        for (i in BLADE_PATHS.indices) fillPath(c, BLADE_PATHS[i], BLADES[i])
        c.restore()
        fillCircle(c, 12f, 12f, 1.7f, HUB)
    }

    // ───────────────────────── SERWO: przepustnica (trzepot w AUTO, kąt z odczytu w RĘCZNY/BEZPIECZNY) ─────────────────────────
    private fun isServoAuto(s: State): Boolean = !(s.aux == 2f || s.aux == 3f)

    private fun servo(c: Canvas, t: Float, s: State) {
        val auto = isServoAuto(s)
        stroke.pathEffect = null
        stroke.strokeWidth = 1.1f
        stroke.color = GAUGE_TRACK
        c.drawCircle(12f, 12f, 9.4f, stroke)
        val sy = if (auto) 1f - .68f * sway(cyc(t, 2.6f)) else (1f - .8f * s.fraction.coerceIn(0f, 1f)).coerceAtLeast(.14f)
        val col = when (s.aux) { 2f -> FLAP_MANUAL; 3f -> FLAP_SAFE; else -> FLAP_STROKE }
        c.save()
        c.translate(12f, 12f)
        c.scale(1f, sy)
        c.translate(-12f, -12f)
        fillEllipse(c, 12f, 12f, 8.6f, 3.1f, withAlpha(if (auto) FLAP_FILL else col, .3f))
        stroke.strokeWidth = 1.5f
        stroke.color = col
        rect.set(3.4f, 8.9f, 20.6f, 15.1f)
        c.drawOval(rect, stroke)
        c.restore()
        fillCircle(c, 12f, 12f, 1.6f, STEAM)
    }

    // ───────────────────────── MIESZADŁO: wał + 3 łopaty (obrót gdy pracuje) ─────────────────────────
    private fun mixer(c: Canvas, t: Float, s: State) {
        if (s.active) glow(c, 12f, 12f, 10f, GAUGE_OK, .14f, inverted = true)
        stroke.pathEffect = null
        stroke.strokeWidth = 1.5f
        stroke.color = SHAFT
        c.drawLine(12f, 2.5f, 12f, 21.5f, stroke)
        c.save()
        if (s.active) c.rotate(360f * cyc(t, 1.6f), 12f, 12f)
        fillPath(c, PADDLES[0], PADDLE_A)
        fillPath(c, PADDLES[1], PADDLE_B)
        fillPath(c, PADDLES[2], PADDLE_A)
        c.restore()
        fillCircle(c, 12f, 12f, 1.4f, PADDLE_HUB)
    }

    // ───────────────────────── DYM: 3 wstęgi unoszące się z zanikaniem ─────────────────────────
    private fun smoke(c: Canvas, t: Float, s: State) {
        val period = if (s.alarm) 1.7f else 3.2f
        val a = if (s.alarm) SMOKE_ALARM_A else SMOKE_A
        val b = if (s.alarm) SMOKE_ALARM_B else SMOKE_B
        if (s.alarm) glow(c, 12f, 14f, 10f, GAUGE_ERR, .18f + .14f * sway(cyc(t, 1.5f)))
        for (i in SMOKE_PATHS.indices) {
            val ph = cyc(t, period, period * .344f * i)
            val e = easeOut(ph)
            val base = if (i == 1) 1f else .8f
            val alpha = base * (if (ph < .2f) ph / .2f else 1f - (ph - .2f) / .8f)
            c.save()
            c.translate(1.6f * e, -3f * e)
            strokePath(c, SMOKE_PATHS[i], if (i == 1) b else a, 1.6f, alpha)
            c.restore()
        }
    }

    // ───────────────────────── WYKRESY: 3 "żywe" słupki ─────────────────────────
    private fun charts(c: Canvas, t: Float) {
        stroke.pathEffect = null
        stroke.strokeWidth = .8f
        stroke.color = GAUGE_TRACK
        c.drawLine(3f, 20.7f, 21f, 20.7f, stroke)
        bar(c, 3.5f, 6f, BAR_1, 0f, t)
        bar(c, 10.3f, 12f, BAR_2, .3f, t)
        bar(c, 17.1f, 9f, BAR_3, .6f, t)
    }

    private fun bar(c: Canvas, x: Float, h: Float, color: Int, delay: Float, t: Float) {
        val hh = h * (1f - .5f * sway(cyc(t, 2f, delay)))
        rect.set(x, 20f - hh, x + 3.4f, 20f)
        fill.color = color
        c.drawRoundRect(rect, .6f, .6f, fill)
    }

    // ───────────────────────── CZAS: zegar z realnymi wskazówkami ─────────────────────────
    private fun clock(c: Canvas) {
        calendar.timeInMillis = System.currentTimeMillis()
        val h = calendar.get(Calendar.HOUR_OF_DAY) % 12
        val m = calendar.get(Calendar.MINUTE)
        val sec = calendar.get(Calendar.SECOND) + calendar.get(Calendar.MILLISECOND) / 1000f
        stroke.pathEffect = null
        stroke.strokeWidth = 1.4f
        stroke.color = CLOCK_FACE
        c.drawCircle(12f, 12f, 9f, stroke)
        strokePath(c, CLOCK_TICKS, CLOCK_FACE, 1.2f)
        hand(c, (h + m / 60f) * 30f, 4.2f, 1.5f, CLOCK_HAND)
        hand(c, (m + sec / 60f) * 6f, 6.2f, 1.3f, CLOCK_HAND)
        hand(c, sec * 6f, 6.8f, .8f, CLOCK_SECOND)
        fillCircle(c, 12f, 12f, 1.1f, NEEDLE)
    }

    private fun hand(c: Canvas, angleDeg: Float, length: Float, width: Float, color: Int) {
        c.save()
        c.rotate(angleDeg, 12f, 12f)
        stroke.strokeWidth = width
        stroke.color = color
        c.drawLine(12f, 12f, 12f, 12f - length, stroke)
        c.restore()
    }

    // ───────────────────────── prymitywy ─────────────────────────
    private fun fillPath(c: Canvas, p: Path, color: Int, alpha: Float = 1f) {
        fill.shader = null
        fill.color = if (alpha >= 1f) color else withAlpha(color, alpha * Color.alpha(color) / 255f)
        c.drawPath(p, fill)
    }

    private fun strokePath(c: Canvas, p: Path, color: Int, width: Float, alpha: Float = 1f, cap: Paint.Cap = Paint.Cap.ROUND) {
        stroke.pathEffect = null
        stroke.strokeCap = cap
        stroke.strokeWidth = width
        stroke.color = if (alpha >= 1f) color else withAlpha(color, alpha * Color.alpha(color) / 255f)
        c.drawPath(p, stroke)
        stroke.strokeCap = Paint.Cap.ROUND
    }

    private fun fillCircle(c: Canvas, x: Float, y: Float, r: Float, color: Int) {
        fill.shader = null
        fill.color = color
        c.drawCircle(x, y, r, fill)
    }

    private fun fillEllipse(c: Canvas, x: Float, y: Float, rx: Float, ry: Float, color: Int) {
        fill.shader = null
        fill.color = color
        rect.set(x - rx, y - ry, x + rx, y + ry)
        c.drawOval(rect, fill)
    }

    /** Faza 0..1 cyklu o okresie [period] s, opóźnionego o [delay] s. */
    private fun cyc(t: Float, period: Float, delay: Float = 0f): Float {
        val x = (t - delay) / period
        return x - kotlin.math.floor(x)
    }

    /** 0 → 1 → 0 (ease-in-out), odpowiednik keyframes 0%/50%/100%. */
    private fun sway(phase: Float): Float = .5f - .5f * cos(2.0 * PI * phase).toFloat()

    private fun easeOut(x: Float): Float = 1f - (1f - x) * (1f - x)

    private fun hex(s: String): Int = Color.parseColor(s)

    private fun withAlpha(color: Int, a: Float): Int =
        (color and 0x00FFFFFF) or ((a.coerceIn(0f, 1f) * 255f + .5f).toInt() shl 24)

    private fun mix(c1: Int, c2: Int, f: Float): Int {
        val k = f.coerceIn(0f, 1f)
        return Color.rgb(
            (Color.red(c1) + (Color.red(c2) - Color.red(c1)) * k).toInt(),
            (Color.green(c1) + (Color.green(c2) - Color.green(c1)) * k).toInt(),
            (Color.blue(c1) + (Color.blue(c2) - Color.blue(c1)) * k).toInt()
        )
    }

    // ───────────────────────── mini-parser ścieżek SVG (M L H V C S Q Z, wersje względne, powtórzenia) ─────────────────────────
    // UWAGA: regex tworzony lokalnie — svg() jest wołane podczas inicjalizacji obiektu (pola ścieżek wyżej),
    // więc nie może zależeć od pola zadeklarowanego poniżej (byłoby jeszcze null → crash przy starcie).
    private fun svg(d: String): Path {
        val p = Path()
        val tokenRe = Regex("[MmLlHhVvCcSsQqZz]|-?(?:\\d+\\.?\\d*|\\.\\d+)(?:[eE]-?\\d+)?")
        val tk = tokenRe.findAll(d).map { it.value }.toList()
        var i = 0
        var cmd = 'M'
        var prev = ' '
        var cx = 0f; var cy = 0f; var sx = 0f; var sy = 0f; var lcx = 0f; var lcy = 0f
        fun num(): Float = tk[i++].toFloat()
        while (i < tk.size) {
            val s = tk[i]
            if (s.length == 1 && s[0].isLetter()) {
                cmd = s[0]; i++
                if (cmd == 'Z' || cmd == 'z') { p.close(); cx = sx; cy = sy; prev = 'Z'; continue }
            }
            when (cmd) {
                'M' -> { cx = num(); cy = num(); p.moveTo(cx, cy); sx = cx; sy = cy; cmd = 'L' }
                'm' -> { cx += num(); cy += num(); p.moveTo(cx, cy); sx = cx; sy = cy; cmd = 'l' }
                'L' -> { cx = num(); cy = num(); p.lineTo(cx, cy) }
                'l' -> { cx += num(); cy += num(); p.lineTo(cx, cy) }
                'H' -> { cx = num(); p.lineTo(cx, cy) }
                'h' -> { cx += num(); p.lineTo(cx, cy) }
                'V' -> { cy = num(); p.lineTo(cx, cy) }
                'v' -> { cy += num(); p.lineTo(cx, cy) }
                'C' -> { val x1 = num(); val y1 = num(); val x2 = num(); val y2 = num(); val x = num(); val y = num(); p.cubicTo(x1, y1, x2, y2, x, y); lcx = x2; lcy = y2; cx = x; cy = y }
                'c' -> { val x1 = cx + num(); val y1 = cy + num(); val x2 = cx + num(); val y2 = cy + num(); val x = cx + num(); val y = cy + num(); p.cubicTo(x1, y1, x2, y2, x, y); lcx = x2; lcy = y2; cx = x; cy = y }
                'S', 's' -> {
                    val rel = cmd == 's'
                    val smooth = prev == 'C' || prev == 'c' || prev == 'S' || prev == 's'
                    val x1 = if (smooth) 2f * cx - lcx else cx
                    val y1 = if (smooth) 2f * cy - lcy else cy
                    val x2 = (if (rel) cx else 0f) + num(); val y2 = (if (rel) cy else 0f) + num()
                    val x = (if (rel) cx else 0f) + num(); val y = (if (rel) cy else 0f) + num()
                    p.cubicTo(x1, y1, x2, y2, x, y); lcx = x2; lcy = y2; cx = x; cy = y
                }
                'Q', 'q' -> {
                    val rel = cmd == 'q'
                    val x1 = (if (rel) cx else 0f) + num(); val y1 = (if (rel) cy else 0f) + num()
                    val x = (if (rel) cx else 0f) + num(); val y = (if (rel) cy else 0f) + num()
                    p.quadTo(x1, y1, x, y); lcx = x1; lcy = y1; cx = x; cy = y
                }
                else -> i++
            }
            prev = cmd
        }
        return p
    }
}
