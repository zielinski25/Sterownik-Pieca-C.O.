package com.sterownikco.pro.ui.theme

import androidx.compose.ui.graphics.Color

/**
 * Paleta 1:1 zmiennych CSS `:root` z panelu Piec.html (Sterownik Pieca C.O. — Centrala PRO).
 * Każda stała odpowiada tokenowi z arkusza stylów (`--cyan`, `--surface2`, …).
 */
object Pal {
    /** rgba() z CSS — alpha w zakresu 0..1 */
    fun rgba(r: Int, g: Int, b: Int, a: Float): Color =
        Color(((a.coerceIn(0f, 1f) * 255).toInt() shl 24) or (r shl 16) or (g shl 8) or b)

    // ── Tła i powierzchnie ──
    val Stage = Color(0xFF02060C)      // body background (poza ramką telefonu)
    val Bg = Color(0xFF060D18)         // --bg
    val Top = Color(0xFF071421)        // --top  (pasek górny)
    val Top2 = Color(0xFF0A1928)       // gradient koniec paska górnego
    val Nav = Color(0xFF081421)        // --nav  (nawigacja dolna)
    val Nav2 = Color(0xFF0B1725)
    val Surface = Color(0xFF0C1829)    // --surface
    val Surface2 = Color(0xFF111F35)   // --surface2
    val Surface3 = Color(0xFF162942)   // --surface3
    val Hero = Color(0xFF0B2032)       // --hero
    val HeroA = Color(0xFF0F2438)       // gradient hero start
    val HeroB = Color(0xFF07131F)       // gradient hero end
    val CanvasBg = Color(0xFF08101A)   // tło wykresów
    val BubbleBg = Color(0xFF0F2238)   // tło dymka Telegram
    val DemoBg = Color(0xFF0B1526)     // tło szuflady DEMO
    val TerminalBg = Color(0xFF060E18)

    // ── Obramowania ──
    val Border = rgba(80, 109, 132, .22f)        // --border
    val BorderStrong = rgba(94, 130, 155, .35f)  // --border-strong
    val TopBorder = rgba(58, 94, 121, .33f)
    val NavBorder = rgba(59, 86, 108, .27f)
    val TileBorder = rgba(75, 100, 119, .28f)

    // ── Tekst ──
    val Text = Color(0xFFE6F0F7)        // --text
    val TextDim = Color(0xFF8EA6BA)     // --text-dim
    val TextDim2 = Color(0xFF5A7A99)    // --text-dim2
    val White = Color(0xFFFFFFFF)
    val TileValue = Color(0xFFF4F8FB)
    val TileTitle = Color(0xFF92A9BC)
    val AxisText = Color(0xFF6E8B9F)

    // ── Akcenty semantyczne ──
    val Cyan = Color(0xFF00D4F5)       // --cyan
    val Accent = Color(0xFFFF9F43)     // --accent (pomarańcz)
    val Live = Color(0xFF4ADE80)       // --live (zieleń "OK")
    val Warn = Color(0xFFFBBF24)       // --warn
    val Err = Color(0xFFFF5F78)        // --err
    val Blue = Color(0xFF55D7FF)       // --blue
    val Orange = Color(0xFFFFB04A)     // --orange
    val Yellow = Color(0xFFFFD166)     // --yellow
    val Violet = Color(0xFFA855F7)     // --violet
    val Pink = Color(0xFFFF6FC7)       // --pink
    val Green = Color(0xFF45D98B)      // --green
    val Telegram = Color(0xFF2AABEE)

    val CyanGlow = rgba(0, 212, 245, .25f)
    val AccentGlow = rgba(255, 159, 67, .25f)
    val LiveGlow = rgba(74, 222, 128, .25f)
    val ErrGlow = rgba(255, 95, 120, .30f)
    val VioletGlow = rgba(168, 85, 247, .25f)

    // ── Kolory kafelków ( --c-* ) ──
    val CFlame = Color(0xFF00D4F5)
    val CFlame2 = Color(0xFF38BDF8)
    val CEmber = Color(0xFFFF9F43)
    val CSlonce = Color(0xFFFFD32A)
    val COk = Color(0xFF4ADE80)
    val CFiolet = Color(0xFFA855F7)

    // ── Stany kafelka ──
    val TileOk = rgba(74, 222, 128, .40f)
    val TileWarn = rgba(251, 191, 36, .50f)
    val TileErr = rgba(255, 77, 109, .65f)
    val TileDis = rgba(148, 163, 184, .30f)
    val TileStale = rgba(127, 147, 163, .42f)
    val TileSim = rgba(0, 212, 245, .55f)

    val WarnValue = Color(0xFFFFE082)
    val ErrValue = Color(0xFFFF7B91)
    val StaleValue = Color(0xFF9DB0BE)
    val DisValue = Color(0xFF94A3B8)

    // ── Ikony/art (SVG z Piec.html) ──
    val ArtShell = Color(0xFF131C2E)
    val ArtShellStroke = Color(0xFF475569)
    val ArtDark = Color(0xFF080D19)
    val ArtBar = Color(0xFF334155)
    val ArtSteel = Color(0xFF64748B)
    val ArtLight = Color(0xFFE2E8F0)
    val ArtFlameOuter = Color(0xFFC2410C)
    val ArtFlameMid = Color(0xFFEA580C)
    val ArtFlameSoft = Color(0xFFFB923C)
    val ArtFlameHot = Color(0xFFF97316)
    val ArtFlameCore = Color(0xFFFDE047)
    val ArtSpark = Color(0xFFFED7AA)
    val ArtSpark2 = Color(0xFFFFEDD5)
    val ArtWater = Color(0xFF0284C7)
    val ArtWaterLight = Color(0xFF38BDF8)
    val ArtCloud = Color(0xFF94A3B8)
    val ArtCloudLight = Color(0xFFCBD5E1)
    val ArtCloudWhite = Color(0xFFF8FAFC)
    val ArtSun = Color(0xFFFFD166)
    val ArtSunBright = Color(0xFFFFD94D)
    val Star = Color(0xFFFEF3C7)
    val ArtSnow = Color(0xFFF8FAFC)
    val ArtBolt = Color(0xFFEAB308)
    val ArtDeep = Color(0xFF030712)
    val ArtRed = Color(0xFFEF4444)
    val ArtRedDeep = Color(0xFFB91C1C)

    // ── Terminal / logi ──
    val TermInfo = Color(0xFF9FB6C7)
    val TermDebug = Color(0xFF7DD3FC)
    val TermTrace = Color(0xFF94A3B8)
    val TermWarn = Color(0xFFFBBF24)
    val TermErr = Color(0xFFFF6B81)
    val TermOk = Color(0xFF45D98B)
    val Muted = Color(0xFF64748B)
}
