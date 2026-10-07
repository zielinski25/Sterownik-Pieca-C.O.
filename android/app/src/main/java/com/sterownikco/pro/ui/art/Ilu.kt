package com.sterownikco.pro.ui.art

import com.sterownikco.pro.ui.svg.Anim
import java.util.Locale
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.min

/* ══════════════════════════════════════════════════════════════════════════
   ILUSTRACJE KAFELKÓW — przeniesione 1:1 z obiektu `ILU` w Piec.html
   (sekcja „ANIMACJE WARSTW ILUSTRACJI SVG (ESP + KOTLIN)”).
   Każda funkcja zwraca ten sam mark-up SVG co panel HTML. Klasy CSS, które
   w panelu napędzały @keyframes, są tu kluczami mapy `anim()` — patrz dół pliku.
   Jedyna celowa zmiana nazw klas: modyfikatory (f1/s1/…) dostały prefiks
   kontekstu (sf1, zs1, hs1 …), bo w Compose jedna mapa animacji obsługuje
   wszystkie ilustracje naraz.
   ══════════════════════════════════════════════════════════════════════════ */

/** Stan potrzebny do narysowania ilustracji (lustrzane odbicie pól `S`). */
data class ArtState(
    val night: Boolean = false,
    val weatherCode: Int = 0,
    val isDay: Boolean = true,
    val tZewn: Float = 11.8f,
    val tOgrz: Float = 58.4f,
    val tBojler: Float = 48.6f,
    val tPanel: Float = 36.2f,
    val tPokoj: Float = 22.1f,
    val alarmOgrz: Boolean = false,
    val alarmPanel: Boolean = false,
    val pompa: Boolean = true,
    val pompa2: Boolean = false,
    val klapa: Float = 90f,
    val syberka: Float = 0f,
    val mieszadlo: Boolean = false,
    val dymAlarm: Boolean = false,
    val klapaAktywne: Boolean = false,
    val cisnienie: Float = 1013f,
    val wilgotnosc: Float = 54f,
    val godz: Int = 12,
    val min: Int = 0
) {
    val solar: Boolean get() = !night
}

object Ilu {

    private const val NS = "http://www.w3.org/2000/svg"

    /** ILU.defs — filtry i gradienty używane przez ilustracje. */
    private const val DEFS = "<defs>" +
        "<radialGradient id=\"gl-ember\" cx=\"50%\" cy=\"50%\" r=\"50%\"><stop offset=\"0%\" stop-color=\"#ff9f43\" stop-opacity=\".45\"/><stop offset=\"100%\" stop-color=\"#ff9f43\" stop-opacity=\"0\"/></radialGradient>" +
        "<radialGradient id=\"gl-sun\" cx=\"50%\" cy=\"50%\" r=\"50%\"><stop offset=\"0%\" stop-color=\"#ffd166\" stop-opacity=\".55\"/><stop offset=\"100%\" stop-color=\"#ffd166\" stop-opacity=\"0\"/></radialGradient>" +
        "<radialGradient id=\"gl-frost\" cx=\"50%\" cy=\"50%\" r=\"50%\"><stop offset=\"0%\" stop-color=\"#00d4f5\" stop-opacity=\".4\"/><stop offset=\"100%\" stop-color=\"#00d4f5\" stop-opacity=\"0\"/></radialGradient>" +
        "<radialGradient id=\"gl-err\" cx=\"50%\" cy=\"50%\" r=\"50%\"><stop offset=\"0%\" stop-color=\"#ff5f78\" stop-opacity=\".65\"/><stop offset=\"100%\" stop-color=\"#ff5f78\" stop-opacity=\"0\"/></radialGradient>" +
        "<radialGradient id=\"gl-ringcyan\" cx=\"50%\" cy=\"50%\" r=\"50%\"><stop offset=\"60%\" stop-color=\"#00d4f5\" stop-opacity=\"0\"/><stop offset=\"90%\" stop-color=\"#00d4f5\" stop-opacity=\".35\"/><stop offset=\"100%\" stop-color=\"#00d4f5\" stop-opacity=\"0\"/></radialGradient>" +
        "<linearGradient id=\"g-flame\" x1=\"0\" y1=\"1\" x2=\"0\" y2=\"0\"><stop offset=\"0%\" stop-color=\"#c2410c\"/><stop offset=\"50%\" stop-color=\"#ea580c\"/><stop offset=\"85%\" stop-color=\"#f97316\"/><stop offset=\"100%\" stop-color=\"#fde047\"/></linearGradient>" +
        "<linearGradient id=\"g-water\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\"><stop offset=\"0%\" stop-color=\"#38bdf8\"/><stop offset=\"100%\" stop-color=\"#0284c7\"/></linearGradient>" +
        "<linearGradient id=\"g-boiler\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\"><stop offset=\"0%\" stop-color=\"#ffb04a\"/><stop offset=\"100%\" stop-color=\"#ea580c\"/></linearGradient>" +
        "<linearGradient id=\"g-metal\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"1\"><stop offset=\"0%\" stop-color=\"#475569\"/><stop offset=\"50%\" stop-color=\"#1e293b\"/><stop offset=\"100%\" stop-color=\"#0f172a\"/></linearGradient>" +
        "</defs>"

    private fun svg1(body: String): String =
        "<svg viewBox=\"0 0 24 24\" fill=\"none\" xmlns=\"$NS\">" + DEFS + body + "</svg>"

    private fun n1(v: Double): String = String.format(Locale.US, "%.1f", v)
    private fun n2(v: Double): String = String.format(Locale.US, "%.2f", v)

    // ── 1. ZEWNĘTRZNA (dynamiczna pogoda meteo w kafelku) ──────────────────
    private const val SUN24 = "<circle cx=\"12\" cy=\"12\" r=\"4.2\" fill=\"#ffd166\" class=\"sun-pulse\"/>" +
        "<g class=\"sun-spin\" stroke=\"#ffd166\" stroke-width=\"1.2\" stroke-linecap=\"round\">" +
        "<line x1=\"12\" y1=\"3.5\" x2=\"12\" y2=\"5.5\"/><line x1=\"12\" y1=\"18.5\" x2=\"12\" y2=\"20.5\"/>" +
        "<line x1=\"3.5\" y1=\"12\" x2=\"5.5\" y2=\"12\"/><line x1=\"18.5\" y1=\"12\" x2=\"20.5\" y2=\"12\"/>" +
        "<line x1=\"6\" y1=\"6\" x2=\"7.5\" y2=\"7.5\"/><line x1=\"16.5\" y1=\"16.5\" x2=\"18\" y2=\"18\"/>" +
        "<line x1=\"6\" y1=\"18\" x2=\"7.5\" y2=\"16.5\"/><line x1=\"16.5\" y1=\"7.5\" x2=\"18\" y2=\"6\"/></g>"

    private const val MOON24 = "<path d=\"M15 7.5A5.5 5.5 0 1 1 8.5 14.5a6.5 6.5 0 0 0 6.5-7Z\" fill=\"#ffd166\"/>" +
        "<circle cx=\"6.5\" cy=\"6.5\" r=\".6\" fill=\"#fef3c7\" class=\"star zs1\"/>" +
        "<circle cx=\"17.5\" cy=\"16.5\" r=\".5\" fill=\"#fef3c7\" class=\"star zs2\"/>"

    private const val CLOUD24 = "<g class=\"w-cloud\"><ellipse cx=\"13\" cy=\"15\" rx=\"6\" ry=\"3.2\" fill=\"#94a3b8\"/>" +
        "<circle cx=\"9.5\" cy=\"13.5\" r=\"2.8\" fill=\"#cbd5e1\"/><circle cx=\"14\" cy=\"12\" r=\"3.5\" fill=\"#e2e8f0\"/>" +
        "<circle cx=\"17.5\" cy=\"14\" r=\"2.4\" fill=\"#cbd5e1\"/></g>"

    private const val DCLOUD24 = "<g class=\"w-cloud-dark\"><ellipse cx=\"12.5\" cy=\"13.5\" rx=\"6.5\" ry=\"3.5\" fill=\"#475569\"/>" +
        "<circle cx=\"9\" cy=\"11.5\" r=\"3\" fill=\"#64748b\"/><circle cx=\"13.5\" cy=\"10\" r=\"3.8\" fill=\"#94a3b8\"/>" +
        "<circle cx=\"17\" cy=\"12\" r=\"2.6\" fill=\"#64748b\"/></g>"

    private const val RAIN24 = "<g class=\"w-rain\" stroke=\"#38bdf8\" stroke-width=\"1.3\" stroke-linecap=\"round\">" +
        "<line x1=\"9\" y1=\"17\" x2=\"8\" y2=\"20\" class=\"rain-drop d1\"/>" +
        "<line x1=\"13\" y1=\"17\" x2=\"12\" y2=\"20\" class=\"rain-drop d2\"/>" +
        "<line x1=\"17\" y1=\"17\" x2=\"16\" y2=\"20\" class=\"rain-drop d3\"/></g>"

    private const val SNOW24 = "<g class=\"w-snow\" fill=\"#f8fafc\">" +
        "<circle cx=\"9\" cy=\"18\" r=\".8\" class=\"snow-flake f1\"/>" +
        "<circle cx=\"13\" cy=\"19\" r=\".9\" class=\"snow-flake f2\"/>" +
        "<circle cx=\"17\" cy=\"18\" r=\".8\" class=\"snow-flake f3\"/></g>"

    private const val STORM24 = "<polygon points=\"14 14 11 18 13.5 18 12 22 17 17 14.5 17\" fill=\"#fde047\" stroke=\"#eab308\" stroke-width=\".6\" class=\"w-lightning\"/>"

    private const val FOG24 = "<g stroke=\"#94a3b8\" stroke-width=\"1.2\" stroke-linecap=\"round\" opacity=\".7\">" +
        "<line x1=\"6\" y1=\"15\" x2=\"18\" y2=\"15\"/><line x1=\"7\" y1=\"17.5\" x2=\"17\" y2=\"17.5\"/>" +
        "<line x1=\"8\" y1=\"20\" x2=\"16\" y2=\"20\"/></g>"

    fun zewn(s: ArtState): String {
        val code = s.weatherCode
        val isDay = s.isDay
        val body = when {
            code == 0 -> if (isDay) SUN24 else MOON24
            code == 1 || code == 2 -> (if (isDay) "<g transform=\"scale(0.85) translate(2,0)\">$SUN24</g>" else MOON24) + CLOUD24
            code == 3 -> DCLOUD24 + CLOUD24
            code == 45 || code == 48 -> CLOUD24 + FOG24
            (code in 51..67) || (code in 80..82) -> DCLOUD24 + RAIN24
            (code in 71..77) || (code in 85..86) -> DCLOUD24 + SNOW24
            code >= 95 -> DCLOUD24 + STORM24 + RAIN24
            else -> (if (isDay) SUN24 else MOON24) + CLOUD24
        }
        val glow = if (code in 71..77) "url(#gl-frost)" else if (isDay) "url(#gl-sun)" else "url(#gl-ringcyan)"
        return svg1("<circle cx=\"12\" cy=\"12\" r=\"9.5\" fill=\"$glow\" opacity=\".4\"/>" + body)
    }

    // ── 2. PIEC C.O. (kocioł z żywym ogniem i iskrami) ─────────────────────
    fun ogrz(s: ArtState): String {
        val t = s.tOgrz
        val isAlarm = s.alarmOgrz
        val flameOp = if (t <= 25f) ".2" else if (t <= 45f) ".6" else "1"
        val flameColor = if (isAlarm) "#ef4444" else if (t > 70f) "#ea580c" else "#fb923c"
        val iskra = if (t > 40f)
            "<circle class=\"iskra i1\" cx=\"10.8\" cy=\"11.2\" r=\".4\" fill=\"#fed7aa\"/>" +
                "<circle class=\"iskra i2\" cx=\"13.2\" cy=\"10.6\" r=\".35\" fill=\"#fde047\"/>" +
                "<circle class=\"iskra i3\" cx=\"12.1\" cy=\"9.8\" r=\".3\" fill=\"#ffedd5\"/>" else ""
        return svg1(
            "<circle class=\"piec-glow\" cx=\"12\" cy=\"13\" r=\"9.5\" fill=\"" + (if (isAlarm) "url(#gl-err)" else "url(#gl-ember)") + "\" opacity=\"" + (if (isAlarm) ".9" else flameOp) + "\"/>" +
                "<path d=\"M5.5 19.5h13a1 1 0 0 0 1-1v-12a1 1 0 0 0-1-1h-13a1 1 0 0 0-1 1v12a1 1 0 0 0 1 1Z\" fill=\"#131c2e\" stroke=\"#475569\" stroke-width=\"1.2\"/>" +
                "<rect x=\"7\" y=\"7.5\" width=\"10\" height=\"2.2\" rx=\".6\" fill=\"#334155\"/>" +
                "<rect x=\"7\" y=\"11\" width=\"10\" height=\"6.5\" rx=\"1\" fill=\"#080d19\" stroke=\"#334155\" stroke-width=\".8\"/>" +
                "<g class=\"flames-group\" style=\"opacity:" + flameOp + "\">" +
                "<path class=\"plomien-zew\" d=\"M12 16.5c-1.8 0-3.2-1.3-3.2-3.2 0-1.5 1-2.7 1.5-4.2.3 1 1 1.5 1 1.5-.3-1.5.6-3.2 1.7-3.7-.6 1.5 0 2.7.8 3.5.8.8 1.4 1.8 1.4 2.8 0 1.9-1.3 3.3-3.2 3.3Z\" fill=\"#c2410c\"/>" +
                "<path class=\"plomien-sr\" d=\"M12.1 15.6c-1.2 0-2.1-.9-2.1-2.1 0-1 .7-1.7 1-2.7.2.7.7 1 .7 1-.2-1 .4-2.1 1.1-2.4-.4 1 0 1.7.5 2.2.6.6.9 1.2.9 1.9 0 1.2-.9 2.1-2.1 2.1Z\" fill=\"$flameColor\"/>" +
                "<path class=\"plomien-wew\" d=\"M12.1 14.5c-.6 0-1.1-.4-1.1-1.1 0-.6.3-.9.5-1.5.1.3.4.6.4.6-.1-.6.2-1.1.6-1.3-.2.6 0 .9.3 1.2.3.3.5.6.5 1 0 .7-.5 1.1-1.2 1.1Z\" fill=\"#fde047\"/>" +
                "</g>" + iskra
        )
    }

    // ── 3. BOJLER C.W.U. (zasobnik z wrzącą wodą i bąbelkami) ───────────────
    fun bojler(s: ArtState): String {
        val t = s.tBojler
        val pct = max(.05, min(.95, (t - 15) / 55.0))
        val hVal = n1(15 * pct)
        val yVal = n1(19 - 15 * pct)
        val col = if (t > 55) "#ea580c" else if (t > 35) "#f59e0b" else "#0284c7"
        val bubs = if (t > 30)
            "<circle fill=\"rgba(255,255,255,.75)\" class=\"bojler-babel bb1\" cx=\"9.5\" cy=\"15\" r=\".5\"/>" +
                "<circle fill=\"rgba(255,255,255,.75)\" class=\"bojler-babel bb2\" cx=\"12\" cy=\"16\" r=\".4\"/>" +
                "<circle fill=\"rgba(255,255,255,.75)\" class=\"bojler-babel bb3\" cx=\"14\" cy=\"14.5\" r=\".45\"/>" else ""
        return svg1(
            "<defs><clipPath id=\"bClip_v1\"><rect x=\"7\" y=\"4\" width=\"10\" height=\"15\" rx=\"3\"/></clipPath></defs>" +
                "<rect x=\"7\" y=\"4\" width=\"10\" height=\"15\" rx=\"3\" fill=\"#131c2e\" stroke=\"#475569\" stroke-width=\"1.2\"/>" +
                "<g clip-path=\"url(#bClip_v1)\">" +
                "<rect class=\"bojler-fill\" x=\"7\" y=\"$yVal\" width=\"10\" height=\"$hVal\" fill=\"$col\" opacity=\".85\"/>" +
                bubs + "</g>" +
                "<path d=\"M10 2.5h4v1.5h-4z\" fill=\"#475569\"/>" +
                "<path d=\"M8.5 19v2.5M15.5 19v2.5\" stroke=\"#475569\" stroke-width=\"1.2\" stroke-linecap=\"round\"/>" +
                "<path d=\"M9 6v10\" stroke=\"rgba(255,255,255,.18)\" stroke-width=\".8\" stroke-linecap=\"round\"/>"
        )
    }

    // ── 4. KOLEKTOR SŁONECZNY RUROWY ZE ZBIORNIKIEM WODY ─────────────────────
    fun panel(s: ArtState): String {
        val isNight = s.night
        val isAlarm = s.alarmPanel
        val tubeCol = if (isAlarm) "#ef4444" else "#0369a1"
        val flowCol = if (isAlarm) "#fbbf24" else "#38bdf8"
        val sb = StringBuilder()
        for (i in 0..3) {
            val x1 = 5 + i * 3.0
            val y1 = 10.0
            val x2 = 11 + i * 3.0
            val y2 = 19.0
            sb.append("<line x1=\"" + x1.toInt() + "\" y1=\"" + y1.toInt() + "\" x2=\"" + x2.toInt() + "\" y2=\"" + y2.toInt() + "\" stroke=\"" + tubeCol + "\" stroke-width=\"1.8\"/>")
            sb.append("<line x1=\"" + n1(x1 + .4) + "\" y1=\"9.8\" x2=\"" + n1(x2 + .4) + "\" y2=\"18.8\" stroke=\"#ffffff\" stroke-width=\".5\" opacity=\".85\"/>")
            if (!isNight) sb.append("<line class=\"solar-heat-flow sf" + (i + 1) + "\" x1=\"" + x2.toInt() + "\" y1=\"" + y2.toInt() + "\" x2=\"" + x1.toInt() + "\" y2=\"" + y1.toInt() + "\" stroke=\"" + flowCol + "\" stroke-width=\".8\" stroke-dasharray=\"2 3\"/>")
        }
        val sky = if (isNight) {
            "<g class=\"solar-night-group\"><path d=\"M20 4a2.5 2.5 0 1 1-2.5-2.5 2 2 0 0 0 2.5 2.5z\" fill=\"#94a3b8\"/>" +
                "<circle class=\"star zs1\" cx=\"16.5\" cy=\"2.5\" r=\".4\" fill=\"#e2e8f0\"/>" +
                "<circle class=\"star zs2\" cx=\"21.5\" cy=\"7.5\" r=\".45\" fill=\"#e2e8f0\"/></g>"
        } else {
            "<g class=\"solar-sun-wrap\"><circle class=\"sun-pulse\" cx=\"19.5\" cy=\"4.5\" r=\"2.2\" fill=\"" + (if (isAlarm) "#ef4444" else "#ffd32a") + "\"/>" +
                "<g class=\"sun-spin\"><path d=\"M19.5 1v1M19.5 7v1M16 4.5h1M22 4.5h1M17 2l.75.75M21.25 6.25l.75.75M17 7l.75-.75M21.25 2.75l.75-.75\" stroke=\"" + (if (isAlarm) "#ef4444" else "#ffd32a") + "\" stroke-width=\".75\" stroke-linecap=\"round\"/></g></g>"
        }
        val opac = if (isNight) "0.15" else if (isAlarm) "0.75" else "0.45"
        return svg1(
            "<circle class=\"panel-glow " + (if (isAlarm) "alarm-glow" else "sun-glow") + "\" cx=\"12\" cy=\"12\" r=\"9.5\" fill=\"" + (if (isAlarm) "url(#gl-err)" else "url(#gl-sun)") + "\" opacity=\"$opac\"/>" +
                sky +
                "<path d=\"M4 20.5l3-6.5M16 20.5l3-6.5M3 20.5h16\" stroke=\"#475569\" stroke-width=\".9\" stroke-linecap=\"round\"/>" +
                "<path d=\"M6 20.5l10-6.5\" stroke=\"#334155\" stroke-width=\".6\" stroke-linecap=\"round\"/>" +
                "<g stroke-linecap=\"round\">" + sb + "</g>" +
                "<path d=\"M10 19.5l11-.5\" stroke=\"#334155\" stroke-width=\"1.4\" stroke-linecap=\"round\"/>" +
                "<circle cx=\"11\" cy=\"19\" r=\".7\" fill=\"#64748b\"/><circle cx=\"14\" cy=\"19\" r=\".7\" fill=\"#64748b\"/><circle cx=\"17\" cy=\"19\" r=\".7\" fill=\"#64748b\"/><circle cx=\"20\" cy=\"19\" r=\".7\" fill=\"#64748b\"/>" +
                "<g class=\"tank-heat-pulse\">" +
                "<rect x=\"3\" y=\"5.5\" width=\"14\" height=\"4.5\" rx=\"2.25\" fill=\"" + (if (isAlarm) "#b91c1c" else "#0c1d33") + "\" stroke=\"" + (if (isAlarm) "#ef4444" else "#00d4f5") + "\" stroke-width=\".9\"/>" +
                "<path d=\"M5 6.8h10\" stroke=\"#ffffff\" stroke-width=\".6\" stroke-linecap=\"round\" opacity=\".9\"/>" +
                "<rect x=\"15\" y=\"6.5\" width=\"1.5\" height=\"2.5\" rx=\".75\" fill=\"#f59e0b\"/>" +
                "<circle cx=\"4.5\" cy=\"7.75\" r=\".7\" fill=\"#00d4f5\"/>" +
                "</g>"
        )
    }

    // ── 5. POMPA C.O. (wirnik obiegowy 360°) ────────────────────────────────
    fun pompa(s: ArtState, on: Boolean = s.pompa): String {
        val flow = if (on) " przeplyw-flow" else ""
        val wir = if (on) " wirnik-spin" else ""
        return svg1(
            "<circle cx=\"11\" cy=\"12\" r=\"8.2\" fill=\"url(#gl-ringcyan)\" opacity=\"" + (if (on) "1" else "0") + "\"/>" +
                "<circle cx=\"11\" cy=\"12\" r=\"8.2\" stroke=\"#475569\" stroke-width=\"1.3\" fill=\"#0b1324\"/>" +
                "<circle class=\"przeplyw" + flow + "\" cx=\"11\" cy=\"12\" r=\"6.6\" stroke=\"#38bdf8\" stroke-width=\"1.1\" stroke-linecap=\"round\" stroke-dasharray=\"3.5 3.5\" fill=\"none\" opacity=\"" + (if (on) "1" else ".18") + "\"/>" +
                "<g class=\"wirnik" + wir + "\">" +
                "<circle cx=\"11\" cy=\"12\" r=\"2.5\" fill=\"#38bdf8\"/>" +
                "<path d=\"M11 9.5c1.5-2.2 3.2-2.2 4.2-1.2 1 1 1 2.6-1.2 4.2\" stroke=\"#38bdf8\" stroke-width=\"1.2\" stroke-linecap=\"round\" fill=\"none\"/>" +
                "<path d=\"M13.5 12c2.2 1.5 2.2 3.2 1.2 4.2-1 1-2.6 1-4.2-1.2\" stroke=\"#38bdf8\" stroke-width=\"1.2\" stroke-linecap=\"round\" fill=\"none\"/>" +
                "<path d=\"M11 14.5c-1.5 2.2-3.2 2.2-4.2 1.2-1-1-1-2.6 1.2-4.2\" stroke=\"#38bdf8\" stroke-width=\"1.2\" stroke-linecap=\"round\" fill=\"none\"/>" +
                "<path d=\"M8.5 12c-2.2-1.5-2.2-3.2-1.2-4.2 1-1 2.6-1 4.2 1.2\" stroke=\"#38bdf8\" stroke-width=\"1.2\" stroke-linecap=\"round\" fill=\"none\"/>" +
                "</g>" +
                "<path d=\"M19.2 8v8M19.2 12h2.8\" stroke=\"#475569\" stroke-width=\"1.3\" stroke-linecap=\"round\"/>"
        )
    }

    // ── 6. KLAPA / MIARKOWNIK CIĄGU (serwo) ──────────────────────────────────
    fun serwo(s: ArtState): String {
        val deg = s.klapa.toDouble()
        val rot = n1(deg - 90)
        val wobble = if (s.klapaAktywne) " klapa-flutter" else ""
        return svg1(
            "<path d=\"M3 6.5h18v11H3z\" fill=\"#081423\" stroke=\"#475569\" stroke-width=\"1.2\" rx=\"2\"/>" +
                "<g class=\"serwo-przeplyw\" opacity=\"" + n2(max(.2, deg / 180.0)) + "\"><path d=\"M4 9.5h16M4 12h16M4 14.5h16\" stroke=\"#38bdf8\" stroke-width=\".9\" stroke-linecap=\"round\" stroke-dasharray=\"2 3\"/></g>" +
                "<circle cx=\"12\" cy=\"12\" r=\"8.2\" stroke=\"#64748b\" stroke-width=\"1.4\" fill=\"none\"/>" +
                "<g class=\"klapa-wobble" + wobble + "\"><g class=\"klapa-kat\" transform=\"rotate($rot 12 12)\">" +
                "<line x1=\"12\" y1=\"4.2\" x2=\"12\" y2=\"19.8\" stroke=\"#00d4f5\" stroke-width=\"2.2\" stroke-linecap=\"round\"/>" +
                "<ellipse cx=\"12\" cy=\"12\" rx=\"5.4\" ry=\"1.6\" fill=\"#00d4f5\" opacity=\".7\"/>" +
                "</g></g>" +
                "<circle cx=\"12\" cy=\"12\" r=\"2.2\" fill=\"#e2e8f0\" stroke=\"#081423\" stroke-width=\".8\"/>" +
                "<path d=\"M12 2v2.2M12 19.8V22\" stroke=\"#64748b\" stroke-width=\"1.4\" stroke-linecap=\"round\"/>"
        )
    }

    // ── 7. MIESZADŁO / PODAJNIK ──────────────────────────────────────────────
    fun mieszadlo(s: ArtState): String {
        val on = s.mieszadlo
        val col = if (on) "#4ade80" else "#475569"
        val cls = if (on) " mix-flow" else ""
        return svg1(
            "<path d=\"M4.5 4.5h15l-1.8 14h-11.4L4.5 4.5Z\" fill=\"#081423\" stroke=\"#64748b\" stroke-width=\"1.3\"/>" +
                "<g class=\"wir" + (if (on) " on" else "") + "\">" +
                "<path class=\"mix-swirl" + cls + "\" d=\"M7 8.5c2.6 1.2 7.4 1.2 10 0\" stroke=\"$col\" stroke-width=\"1.1\" stroke-linecap=\"round\" opacity=\".7\"/>" +
                "<path class=\"mix-swirl" + cls + "\" d=\"M7.5 12c2.4 1 6.6 1 9 0\" stroke=\"$col\" stroke-width=\"1.1\" stroke-linecap=\"round\" opacity=\".8\"/>" +
                "<path class=\"mix-swirl" + cls + "\" d=\"M8 15.5c2 .8 6 .8 8 0\" stroke=\"$col\" stroke-width=\"1.1\" stroke-linecap=\"round\" opacity=\".9\"/>" +
                "</g>" +
                "<line x1=\"12\" y1=\"2.5\" x2=\"12\" y2=\"16.5\" stroke=\"#94a3b8\" stroke-width=\"1.6\" stroke-linecap=\"round\"/>" +
                "<rect x=\"9.5\" y=\"16\" width=\"5\" height=\"2.5\" rx=\".6\" fill=\"" + (if (on) "#4ade80" else "#334155") + "\"/>" +
                "<circle cx=\"12\" cy=\"3\" r=\"1.8\" fill=\"" + (if (on) "#4ade80" else "#e2e8f0") + "\"/>"
        )
    }

    // ── 8. CZUJNIK DYMU ─────────────────────────────────────────────────────
    fun dym(s: ArtState): String {
        val alarm = s.dymAlarm
        val mark = if (alarm) "<path d=\"M12 8v5M12 15.5v.5\" stroke=\"#ff5f78\" stroke-width=\"2\" stroke-linecap=\"round\"/>" else ""
        return svg1(
            "<circle class=\"dym-glow\" cx=\"12\" cy=\"12\" r=\"9\" fill=\"" + (if (alarm) "url(#gl-err)" else "url(#gl-ember)") + "\" opacity=\"" + (if (alarm) ".85" else ".2") + "\"/>" +
                "<path d=\"M12 3L4 7v6c0 5 3.5 9.5 8 10.5 4.5-1 8-5.5 8-10.5V7l-8-4Z\" fill=\"#131c2e\" stroke=\"#475569\" stroke-width=\"1.2\"/>" +
                "<g class=\"dym-anim\">" +
                "<circle class=\"sm-p1\" cx=\"10\" cy=\"14\" r=\"2.2\" fill=\"#94a3b8\" opacity=\".6\"/>" +
                "<circle class=\"sm-p2\" cx=\"14\" cy=\"12\" r=\"2.8\" fill=\"#cbd5e1\" opacity=\".7\"/>" +
                "<circle class=\"sm-p3\" cx=\"11.5\" cy=\"9.5\" r=\"2\" fill=\"#e2e8f0\" opacity=\".8\"/></g>" +
                mark
        )
    }

    // ── 9. POKÓJ / TERMOSTAT ────────────────────────────────────────────────
    fun pokoj(s: ArtState): String {
        val t = s.tPokoj
        val dot = if (t > 23) "#f59e0b" else if (t < 19) "#38bdf8" else "#ffd166"
        return svg1(
            "<path d=\"M3.5 10.5L12 4l8.5 6.5v8.5a1 1 0 0 1-1 1h-15a1 1 0 0 1-1-1v-8.5Z\" fill=\"#131c2e\" stroke=\"#475569\" stroke-width=\"1.2\"/>" +
                "<rect x=\"9\" y=\"12\" width=\"6\" height=\"8\" rx=\".5\" fill=\"#1e293b\" stroke=\"#38bdf8\" stroke-width=\".9\"/>" +
                "<line x1=\"12\" y1=\"12\" x2=\"12\" y2=\"20\" stroke=\"#38bdf8\" stroke-width=\".7\"/>" +
                "<line x1=\"9\" y1=\"16\" x2=\"15\" y2=\"16\" stroke=\"#38bdf8\" stroke-width=\".7\"/>" +
                "<circle cx=\"12\" cy=\"7.8\" r=\"1.3\" fill=\"$dot\"/>"
        )
    }

    // ── 10. CIŚNIENIE (barometr) ────────────────────────────────────────────
    fun cisnienie(s: ArtState): String {
        val p = s.cisnienie.toDouble()
        val deg = max(-60.0, min(60.0, (p - 1005) * 4))
        return svg1(
            "<circle cx=\"12\" cy=\"12\" r=\"9\" fill=\"#131c2e\" stroke=\"#475569\" stroke-width=\"1.2\"/>" +
                "<circle cx=\"12\" cy=\"12\" r=\"7.2\" stroke=\"#64748b\" stroke-width=\".6\" stroke-dasharray=\"1.2 2\"/>" +
                "<path d=\"M7 14a6 6 0 0 1 10 0\" stroke=\"#38bdf8\" stroke-width=\"1\" fill=\"none\" stroke-linecap=\"round\"/>" +
                "<g class=\"barom-igla\" transform=\"rotate(" + n1(deg) + " 12 12)\">" +
                "<line x1=\"12\" y1=\"12\" x2=\"12\" y2=\"5.8\" stroke=\"#ff5f78\" stroke-width=\"1.3\" stroke-linecap=\"round\"/>" +
                "<circle cx=\"12\" cy=\"12\" r=\"1.6\" fill=\"#f8fafc\"/></g>"
        )
    }

    // ── 11. WILGOTNOŚĆ ──────────────────────────────────────────────────────
    fun wilgotnosc(s: ArtState): String = svg1(
        "<path d=\"M12 3.5C12 3.5 6 10.5 6 14.5a6 6 0 0 0 12 0c0-4-6-11-6-11Z\" fill=\"#131c2e\" stroke=\"#475569\" stroke-width=\"1.2\"/>" +
            "<path d=\"M12 6.5c-2.8 3.8-4.2 6.2-4.2 8a4.2 4.2 0 0 0 8.4 0c0-1.8-1.4-4.2-4.2-8Z\" fill=\"url(#g-water)\" opacity=\".85\"/>" +
            "<ellipse cx=\"10\" cy=\"14\" rx=\"1.3\" ry=\"2.2\" fill=\"rgba(255,255,255,.35)\" transform=\"rotate(-25 10 14)\"/>"
    )

    /** Pozostałe ikony systemowe kafelków. */
    fun wykresy(s: ArtState): String = svg1(
        "<path d=\"M4 19.5h16\" stroke=\"#475569\" stroke-width=\"1.2\" stroke-linecap=\"round\"/>" +
            "<path d=\"M5.5 15.5l4-5 3.5 3.5 5.5-7\" stroke=\"#00d4f5\" stroke-width=\"1.8\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>" +
            "<circle cx=\"18.5\" cy=\"7\" r=\"1.5\" fill=\"#00d4f5\"/>"
    )

    fun czas(s: ArtState): String {
        val hDeg = (s.godz % 12 + s.min / 60.0) * 30.0
        val mDeg = s.min * 6.0
        return svg1(
            "<circle cx=\"12\" cy=\"12\" r=\"8.8\" stroke=\"#475569\" stroke-width=\"1.3\" fill=\"#131c2e\"/>" +
                "<circle cx=\"12\" cy=\"12\" r=\"1.5\" fill=\"#f8fafc\"/>" +
                "<g class=\"h-hand\" transform=\"rotate(" + n1(hDeg) + " 12 12)\"><line x1=\"12\" y1=\"12\" x2=\"12\" y2=\"7.5\" stroke=\"#f8fafc\" stroke-width=\"1.4\" stroke-linecap=\"round\"/></g>" +
                "<g class=\"m-hand\" transform=\"rotate(" + n1(mDeg) + " 12 12)\"><line x1=\"12\" y1=\"12\" x2=\"12\" y2=\"5.5\" stroke=\"#00d4f5\" stroke-width=\"1.1\" stroke-linecap=\"round\"/></g>"
        )
    }

    /** Dispatcher — dobór ilustracji do kafelka (jak `ILU[id]` w panelu). */
    fun svgFor(id: String, s: ArtState): String? = when (id) {
        "zewn" -> zewn(s)
        "ogrz" -> ogrz(s)
        "bojler" -> bojler(s)
        "panel" -> panel(s)
        "pompa" -> pompa(s, s.pompa)
        "pompa2" -> pompa(s, s.pompa2)
        "serwo" -> serwo(s)
        "mieszadlo" -> mieszadlo(s)
        "dym" -> dym(s)
        "pokoj" -> pokoj(s)
        "cisnienie" -> cisnienie(s)
        "wilgotnosc" -> wilgotnosc(s)
        "wykresy" -> wykresy(s)
        "czas" -> czas(s)
        else -> null
    }

    /** Klucz treści (odpowiednik `ILU[id].key`) — do odświeżania obrazu. */
    fun keyFor(id: String, s: ArtState): String = when (id) {
        "zewn" -> "w_" + s.weatherCode + "_" + (if (s.night) "n" else "d")
        "ogrz" -> "ogrz_" + (if (s.alarmOgrz) "a" else "o") + "_" + (if (s.tOgrz > 40) "hot" else "cold")
        "bojler" -> "boj_" + Math.round(if (s.tBojler > 0f) s.tBojler / 5f else 10.0f)
        "panel" -> "sol_" + (if (s.night) "n" else "d") + (if (s.alarmPanel) "a" else "o")
        "pompa" -> "pmp_" + (if (s.pompa) "on" else "off")
        "pompa2" -> "pmp2_" + (if (s.pompa2) "on" else "off")
        "serwo" -> "srv_" + Math.round(s.klapa)
        "mieszadlo" -> "msh_" + (if (s.mieszadlo) "on" else "off")
        "dym" -> "dym_" + (if (s.dymAlarm) "a" else "o")
        "pokoj" -> "pok_" + Math.round(s.tPokoj)
        "cisnienie" -> "baro_" + Math.round(s.cisnienie / 2)
        "wilgotnosc" -> "hum"
        "wykresy" -> "chk"
        "czas" -> "clk_" + s.godz + "_" + s.min
        else -> id
    }

    // ── HERO KOTŁOWNI (112×112) ─────────────────────────────────────────────
    fun heroBoiler(s: ArtState): String {
        val tb = s.tBojler.toDouble()
        val to = s.tOgrz.toDouble()
        val pct = max(.05, min(.95, (tb - 15) / 55))
        val fy = n1(58 - 48 * pct)
        val fh = n1(48 * pct)
        val flameOp = if (to > 30) "1" else ".25"
        val glow = if (s.alarmOgrz || s.dymAlarm) "url(#gl-err)" else "url(#gl-ember)"
        return "<svg viewBox=\"0 0 112 112\" fill=\"none\" xmlns=\"$NS\">" + DEFS +
            "<defs>" +
            "<clipPath id=\"heroBojlerClip\"><rect x=\"34\" y=\"10\" width=\"44\" height=\"48\" rx=\"10\"/></clipPath>" +
            "<linearGradient id=\"heroBojlerGrad\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\"><stop offset=\"0%\" stop-color=\"#ffb04a\"/><stop offset=\"100%\" stop-color=\"#ea580c\"/></linearGradient>" +
            "</defs>" +
            "<circle class=\"hero-glow\" cx=\"56\" cy=\"62\" r=\"46\" fill=\"$glow\" opacity=\".6\"/>" +
            "<g class=\"hero-bojler-group\">" +
            "<rect x=\"34\" y=\"10\" width=\"44\" height=\"48\" rx=\"10\" fill=\"#0f172a\" stroke=\"#475569\" stroke-width=\"2.2\"/>" +
            "<g clip-path=\"url(#heroBojlerClip)\">" +
            "<rect class=\"hero-fill\" x=\"34\" y=\"$fy\" width=\"44\" height=\"$fh\" fill=\"url(#heroBojlerGrad)\" opacity=\".85\"/>" +
            "<circle fill=\"rgba(255,255,255,.75)\" class=\"bojler-babel hb1\" cx=\"44\" cy=\"48\" r=\"1.5\"/>" +
            "<circle fill=\"rgba(255,255,255,.75)\" class=\"bojler-babel hb2\" cx=\"56\" cy=\"52\" r=\"1.3\"/>" +
            "<circle fill=\"rgba(255,255,255,.75)\" class=\"bojler-babel hb3\" cx=\"66\" cy=\"46\" r=\"1.4\"/>" +
            "</g>" +
            "<path d=\"M42 14v40\" stroke=\"rgba(255,255,255,.14)\" stroke-width=\"1.8\" stroke-linecap=\"round\"/>" +
            "<path d=\"M50 5h12v5H50z\" fill=\"#334155\" stroke=\"#475569\" stroke-width=\"1.2\"/>" +
            "<text class=\"hero-lbl-bojler\" x=\"56\" y=\"23\" text-anchor=\"middle\" font-size=\"6.5\" font-weight=\"700\" fill=\"#94a3b8\" letter-spacing=\"0.8\">BOJLER</text>" +
            "<text class=\"hero-t-bojler\" x=\"56\" y=\"41\" text-anchor=\"middle\" font-size=\"13\" font-weight=\"800\" fill=\"#ffffff\">" + String.format(Locale.US, "%.1f", tb) + "°C</text>" +
            "</g>" +
            "<g class=\"hero-piec-group\">" +
            "<rect x=\"28\" y=\"58\" width=\"56\" height=\"48\" rx=\"7\" fill=\"#080e1a\" stroke=\"#334155\" stroke-width=\"2\"/>" +
            "<rect x=\"32\" y=\"64\" width=\"48\" height=\"38\" rx=\"5\" fill=\"#030712\" stroke=\"#1e293b\" stroke-width=\"1.2\"/>" +
            "<path d=\"M38 106v5M74 106v5\" stroke=\"#475569\" stroke-width=\"2.4\" stroke-linecap=\"round\"/>" +
            "<g class=\"hero-flames\" opacity=\"$flameOp\">" +
            "<path class=\"plomien-zew hp1\" d=\"M56 100c-8 0-14-5.5-14-13.5 0-6.5 4.5-11.5 6.8-18 1.4 4.5 4.5 6.5 4.5 6.5-1.4-6.5 2.5-13.5 7.2-15.8-2.5 6.5 0 11.5 3.5 15 3.5 3.5 6 7.8 6 12 0 8.2-5.7 13.8-14 13.8Z\" fill=\"#c2410c\"/>" +
            "<path class=\"plomien-sr hp2\" d=\"M56.3 97c-5.5 0-9.5-4-9.5-9.5 0-4.5 3-7.8 4.6-12.2.9 3 3 4.6 3 4.6-.9-4.6 1.7-9.5 4.9-11-1.7 4.6 0 7.8 2.4 10.1 2.5 2.5 4.1 5.5 4.1 8.5 0 5.5-4 9.5-9.5 9.5Z\" fill=\"#fb923c\"/>" +
            "<path class=\"plomien-wew hp3\" d=\"M56.4 93c-2.8 0-5-2.2-5-5.2 0-2.6 1.6-4.2 2.5-6.8.5 1.6 1.6 2.6 1.6 2.6-.5-2.6 1-5.2 2.6-6.2-1 2.6 0 4.2 1.3 5.7 1.4 1.4 2.4 3 2.4 4.7 0 3-2.4 5.2-5.4 5.2Z\" fill=\"#fde047\"/>" +
            "</g>" +
            "<circle class=\"hero-spark hs1\" cx=\"48\" cy=\"72\" r=\".9\" fill=\"#fde047\"/>" +
            "<circle class=\"hero-spark hs2\" cx=\"64\" cy=\"70\" r=\".8\" fill=\"#fdba74\"/>" +
            "<circle class=\"hero-spark hs3\" cx=\"54\" cy=\"65\" r=\".7\" fill=\"#fef08a\"/>" +
            "<g class=\"hero-piec-badge\">" +
            "<rect x=\"35\" y=\"80\" width=\"42\" height=\"19\" rx=\"5\" fill=\"rgba(8,14,26,0.88)\" stroke=\"#f97316\" stroke-width=\"1.2\"/>" +
            "<text class=\"hero-lbl-piec\" x=\"56\" y=\"87.5\" text-anchor=\"middle\" font-size=\"5.5\" font-weight=\"800\" fill=\"#fdba74\" letter-spacing=\"0.6\">PIEC C.O.</text>" +
            "<text class=\"hero-t-piec\" x=\"56\" y=\"96\" text-anchor=\"middle\" font-size=\"9.5\" font-weight=\"900\" fill=\"#ffffff\">" + String.format(Locale.US, "%.1f", to) + "°C</text>" +
            "</g>" +
            "</g></svg>"
    }

    // ── HERO POGODY (96×96) ─────────────────────────────────────────────────
    private const val SUN96 = "<circle cx=\"34\" cy=\"34\" r=\"14\" fill=\"#ffd94d\" class=\"w-sun\"/>" +
        "<g class=\"w-rays\" stroke=\"#ffd94d\" stroke-width=\"2.2\" stroke-linecap=\"round\">" +
        "<line x1=\"34\" y1=\"10\" x2=\"34\" y2=\"16\"/><line x1=\"34\" y1=\"52\" x2=\"34\" y2=\"58\"/>" +
        "<line x1=\"10\" y1=\"34\" x2=\"16\" y2=\"34\"/><line x1=\"52\" y1=\"34\" x2=\"58\" y2=\"34\"/>" +
        "<line x1=\"17\" y1=\"17\" x2=\"21.5\" y2=\"21.5\"/><line x1=\"46.5\" y1=\"46.5\" x2=\"51\" y2=\"51\"/>" +
        "<line x1=\"17\" y1=\"51\" x2=\"21.5\" y2=\"46.5\"/><line x1=\"46.5\" y1=\"21.5\" x2=\"51\" y2=\"17\"/></g>"

    private const val MOON96 = "<path d=\"M38 22a16 16 0 1 0 16 20 14 14 0 0 1-16-20Z\" fill=\"#e2e8f0\" class=\"w-moon\"/>" +
        "<circle cx=\"20\" cy=\"20\" r=\"1.5\" fill=\"#fef3c7\" class=\"star st1\"/>" +
        "<circle cx=\"56\" cy=\"18\" r=\"1.2\" fill=\"#fef3c7\" class=\"star st2\"/>" +
        "<circle cx=\"16\" cy=\"46\" r=\"1\" fill=\"#fef3c7\" class=\"star st3\"/>"

    private const val CLOUD96 = "<g class=\"w-cloud\"><ellipse cx=\"50\" cy=\"58\" rx=\"22\" ry=\"11\" fill=\"#cbd5e1\"/>" +
        "<circle cx=\"38\" cy=\"52\" r=\"9.5\" fill=\"#e2e8f0\"/><circle cx=\"54\" cy=\"47\" r=\"12\" fill=\"#e2e8f0\"/>" +
        "<circle cx=\"67\" cy=\"53\" r=\"8\" fill=\"#e2e8f0\"/></g>"

    private const val DCLOUD96 = "<g class=\"w-cloud-dark\"><ellipse cx=\"48\" cy=\"54\" rx=\"24\" ry=\"12\" fill=\"#64748b\"/>" +
        "<circle cx=\"35\" cy=\"47\" r=\"10\" fill=\"#94a3b8\"/><circle cx=\"52\" cy=\"42\" r=\"13\" fill=\"#94a3b8\"/>" +
        "<circle cx=\"66\" cy=\"49\" r=\"9\" fill=\"#94a3b8\"/></g>"

    private const val RAIN96 = "<g class=\"w-rain\" stroke=\"#38bdf8\" stroke-width=\"2.2\" stroke-linecap=\"round\">" +
        "<line x1=\"38\" y1=\"72\" x2=\"34\" y2=\"84\" class=\"rain-drop d1\"/>" +
        "<line x1=\"48\" y1=\"72\" x2=\"44\" y2=\"84\" class=\"rain-drop d2\"/>" +
        "<line x1=\"58\" y1=\"72\" x2=\"54\" y2=\"84\" class=\"rain-drop d3\"/>" +
        "<line x1=\"68\" y1=\"72\" x2=\"64\" y2=\"84\" class=\"rain-drop d4\"/></g>"

    private const val SNOW96 = "<g class=\"w-snow\" fill=\"#f8fafc\">" +
        "<circle cx=\"36\" cy=\"76\" r=\"2.2\" class=\"snow-flake f1\"/>" +
        "<circle cx=\"48\" cy=\"80\" r=\"2.5\" class=\"snow-flake f2\"/>" +
        "<circle cx=\"60\" cy=\"75\" r=\"2.2\" class=\"snow-flake f3\"/>" +
        "<circle cx=\"70\" cy=\"81\" r=\"2\" class=\"snow-flake f4\"/></g>"

    private const val STORM96 = "<polygon points=\"52 64 43 78 50 78 45 92 60 75 52 75\" fill=\"#fde047\" stroke=\"#eab308\" stroke-width=\"1.2\" class=\"w-lightning\"/>"

    private const val FOG96 = "<g stroke=\"#94a3b8\" stroke-width=\"2.5\" stroke-linecap=\"round\" opacity=\".7\">" +
        "<line x1=\"24\" y1=\"62\" x2=\"76\" y2=\"62\"/><line x1=\"28\" y1=\"70\" x2=\"72\" y2=\"70\"/>" +
        "<line x1=\"32\" y1=\"78\" x2=\"68\" y2=\"78\"/></g>"

    fun weatherHero(code: Int, isDay: Boolean): String {
        val body = when {
            code == 0 -> if (isDay) SUN96 else MOON96
            code == 1 || code == 2 -> (if (isDay) SUN96 else MOON96) + CLOUD96
            code == 3 -> "<g transform=\"translate(-10,-8) scale(.9)\">" + DCLOUD96 + "</g>" + CLOUD96
            code == 45 || code == 48 -> CLOUD96 + FOG96
            code in 51..67 -> DCLOUD96 + RAIN96
            code in 71..77 -> DCLOUD96 + SNOW96
            code in 80..86 -> (if (isDay) SUN96 else MOON96) + DCLOUD96 + RAIN96
            code >= 95 -> DCLOUD96 + STORM96 + RAIN96
            else -> (if (isDay) SUN96 else MOON96) + CLOUD96
        }
        return "<svg viewBox=\"0 0 96 96\" fill=\"none\" xmlns=\"$NS\">" + DEFS + body + "</svg>"
    }

    // ── Animacje warstw = @keyframes z CSS panelu ───────────────────────────
    private fun cyc(t: Float, dur: Double, delay: Double = 0.0): Float =
        (((t + delay) % dur) / dur).toFloat()

    /** 0 → 1 → 0 z wygładzeniem ease-in-out (CSS: 0% / 50% / 100%). */
    private fun pulse(p: Float): Float = .5f - .5f * cos(p * 2.0 * PI).toFloat()

    private fun lerp(a: Float, b: Float, k: Float): Float = a + (b - a) * k

    /**
     * Klatka animacji dla danej chwili `t` (sekundy). Klucz = klasa CSS elementu;
     * `drawSvg` stosuje ją do każdego elementu o tej klasie.
     */
    fun anim(t: Float, s: ArtState): Map<String, Anim> {
        val m = HashMap<String, Anim>()

        // Słońce / promienie / gwiazdy
        m["sun-spin"] = Anim(rotDeg = 360f * (t / 14f) % 360f, pivotX = 19.5f, pivotY = 4.5f)
        m["sun-pulse"] = Anim(sx = 1f + .12f * pulse(cyc(t, 2.8)), sy = 1f + .12f * pulse(cyc(t, 2.8)), pivotX = 19.5f, pivotY = 4.5f)
        m["w-sun"] = Anim(sx = 1f + .12f * pulse(cyc(t, 3.0)), sy = 1f + .12f * pulse(cyc(t, 3.0)), pivotX = 34f, pivotY = 34f)
        m["w-rays"] = Anim(rotDeg = 360f * (t / 18f) % 360f, pivotX = 34f, pivotY = 34f)
        m["star"] = Anim(alpha = lerp(.35f, 1f, pulse(cyc(t, 2.3))))
        m["zs1"] = m.getValue("star")
        m["st1"] = Anim(alpha = lerp(.35f, 1f, pulse(cyc(t, 2.1))))
        m["zs2"] = Anim(alpha = lerp(.35f, 1f, pulse(cyc(t, 3.1, 1.2))))
        m["st2"] = m["zs2"]!!
        m["st3"] = Anim(alpha = lerp(.35f, 1f, pulse(cyc(t, 2.7, .6))))

        // Kolektor słoneczny: przepływ energii w rurach próżniowych
        for (i in 1..4) {
            val p = cyc(t, 1.5, (i - 1) * .35)
            m["sf$i"] = Anim(dashOffset = lerp(10f, 0f, p), alpha = lerp(.35f, 1f, pulse(p)))
        }

        // Płomienie w palenisku (24×24) i w hero (112×112)
        fun flames(prefix: String, px: Float, py: Float) {
            val k1 = pulse(cyc(t, 1.6))
            m["$prefix-zew"] = Anim(sy = 1f + .12f * k1, sx = 1f - .04f * k1, rotDeg = lerp(-2f, 3f, k1), pivotX = px, pivotY = py)
            val k2 = pulse(cyc(t, 1.1, .1))
            m["$prefix-sr"] = Anim(sy = 1f + .18f * k2, sx = 1f - .08f * k2, rotDeg = lerp(2f, -3f, k2), pivotX = px, pivotY = py - 1.5f)
            val k3 = pulse(cyc(t, .75, .2))
            m["$prefix-wew"] = Anim(sy = 1f + .22f * k3, rotDeg = 4f * k3, pivotX = px, pivotY = py - 3.4f)
        }
        flames("plomien", 12f, 21f)
        m["hp1"] = Anim(sy = 1f + .12f * pulse(cyc(t, 1.6)), sx = 1f - .04f * pulse(cyc(t, 1.6)), rotDeg = lerp(-2f, 3f, pulse(cyc(t, 1.6))), pivotX = 56f, pivotY = 100f)
        m["hp2"] = Anim(sy = 1f + .18f * pulse(cyc(t, 1.1, .1)), sx = 1f - .08f * pulse(cyc(t, 1.1, .1)), rotDeg = lerp(2f, -3f, pulse(cyc(t, 1.1, .1))), pivotX = 56.3f, pivotY = 97f)
        m["hp3"] = Anim(sy = 1f + .22f * pulse(cyc(t, .75, .2)), rotDeg = 4f * pulse(cyc(t, .75, .2)), pivotX = 56.4f, pivotY = 93f)

        // Iskrzenie
        fun iskra(key: String, dur: Double, delay: Double) {
            val p = cyc(t, dur, delay)
            val a = when {
                p < .2f -> p / .2f
                p < .8f -> lerp(1f, .8f, (p - .2f) / .6f)
                else -> lerp(.8f, 0f, (p - .8f) / .2f)
            }
            m[key] = Anim(dx = 3f * p, dy = -12f * p, sx = lerp(.5f, 1.2f, p), sy = lerp(.5f, 1.2f, p), alpha = a, pivotX = if (key.startsWith("hs")) 56f else 12f, pivotY = if (key.startsWith("hs")) 70f else 11f)
        }
        iskra("i1", 2.1, 0.0); iskra("i2", 2.5, .7); iskra("i3", 1.9, 1.3)
        iskra("hs1", 2.1, 0.0); iskra("hs2", 2.5, .7); iskra("hs3", 1.9, 1.3)

        // Bąbelki w bojlerze (24×24) i w zasobniku hero (112×112)
        fun babel(key: String, dur: Double, delay: Double) {
            val p = cyc(t, dur, delay)
            val a = when {
                p < .2f -> p / .2f * .95f
                p < .8f -> .95f - (p - .2f) / .6f * .05f
                else -> lerp(.9f, 0f, (p - .8f) / .2f)
            }
            m[key] = Anim(dy = -24f * p, sx = lerp(.8f, 1.2f, p), sy = lerp(.8f, 1.2f, p), alpha = a, pivotX = 12f, pivotY = 15f)
        }
        babel("bb1", 2.2, 0.0); babel("bb2", 2.2, .7); babel("bb3", 2.2, 1.4)
        babel("hb1", 2.6, 0.0); babel("hb2", 2.4, .8); babel("hb3", 2.8, 1.6)

        // Pompa / mieszadło
        m["wirnik-spin"] = Anim(rotDeg = 360f * (t / 1.1f) % 360f, pivotX = 11f, pivotY = 12f)
        m["przeplyw-flow"] = Anim(dashOffset = -14f * (t / 1.3f % 1f))
        m["mix-flow"] = Anim(dashOffset = -14f * (t / 1.4f % 1f))

        // Klapa
        if (s.klapaAktywne) {
            val k = pulse(cyc(t, 2.6))
            m["klapa-flutter"] = Anim(sy = lerp(1f, .4f, k), pivotX = 12f, pivotY = 12f)
        }

        // Dym
        for (i in 1..3) {
            val p = cyc(t, 2.4, (i - 1) * .8)
            val k = if (p < .5f) p / .5f else 1f - (p - .5f) / .5f
            m["sm-p$i"] = Anim(
                dy = if (p < .5f) -4f * k else -8f * (p - .5f) / .5f - 4f,
                sx = lerp(.8f, 1.3f, p), sy = lerp(.8f, 1.3f, p),
                alpha = if (p < .5f) lerp(.2f, .9f, k) else lerp(.9f, .2f, (p - .5f) / .5f),
                pivotX = 12f, pivotY = 12f
            )
        }

        // Alarm
        val ka = pulse(cyc(t, 1.2))
        m["alarm-glow"] = Anim(sx = lerp(.95f, 1.08f, ka), sy = lerp(.95f, 1.08f, ka), alpha = lerp(.9f, .4f, ka), pivotX = 12f, pivotY = 12f)

        // Pogoda
        val kc = cyc(t, 5.0)
        m["w-cloud"] = Anim(dx = 4f * pulse(kc))
        m["w-cloud-dark"] = Anim(dx = 4f * pulse(kc))
        for (i in 1..4) {
            val p = cyc(t, .75, (i - 1) * .2)
            m["d$i"] = Anim(dy = lerp(-8f, 12f, p), alpha = if (p < .3f) p / .3f else lerp(1f, 0f, (p - .3f) / .7f))
            val q = cyc(t, 1.8, (i - 1) * .45)
            m["f$i"] = Anim(
                dy = lerp(-6f, 10f, q), dx = lerp(0f, 2f, q),
                alpha = if (q < .3f) q / .3f else lerp(1f, 0f, (q - .3f) / .7f)
            )
        }
        val lb = cyc(t, 3.5)
        m["w-lightning"] = Anim(alpha = when {
            lb < .92f || lb > .96f -> 0f
            lb in .935f..0.945f -> .3f
            else -> 1f
        })
        return m
    }
}
