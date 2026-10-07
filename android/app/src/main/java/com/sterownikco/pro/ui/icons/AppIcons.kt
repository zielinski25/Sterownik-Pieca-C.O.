package com.sterownikco.pro.ui.icons

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.sterownikco.pro.ui.svg.Svg
import com.sterownikco.pro.ui.svg.drawSvg

/* ══════════════════════════════════════════════════════════════════════════
   IKONY 1:1 z `ILU.nav` w Piec.html (rysunek 24×24, stroke 1.8, currentColor).
   Funkcja `wrap(body, sw)` z panelu przeniesiona bez zmian — dzięki temu
   każda ikona w aplikacji natywnej wygląda identycznie jak w HTML.
   ══════════════════════════════════════════════════════════════════════════ */

private const val NS = "http://www.w3.org/2000/svg"

private fun wrap(body: String, sw: Double = 1.8): String =
    "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"$sw\" " +
        "stroke-linecap=\"round\" stroke-linejoin=\"round\" xmlns=\"$NS\">$body</svg>"

object Icons {
    private val bodies = mapOf(
        "desktop" to wrap("<rect width=\"20\" height=\"14\" x=\"2\" y=\"3\" rx=\"2\"/><line x1=\"8\" x2=\"16\" y1=\"21\" y2=\"21\"/><line x1=\"12\" x2=\"12\" y1=\"17\" y2=\"21\"/>"),
        "phone" to wrap("<rect width=\"14\" height=\"20\" x=\"5\" y=\"2\" rx=\"2\"/><line x1=\"12\" x2=\"12.01\" y1=\"18\" y2=\"18\"/>"),
        "dashboard" to wrap("<rect x=\"3\" y=\"3\" width=\"7\" height=\"9\" rx=\"1.5\"/><rect x=\"14\" y=\"3\" width=\"7\" height=\"5\" rx=\"1.5\"/><rect x=\"14\" y=\"12\" width=\"7\" height=\"9\" rx=\"1.5\"/><rect x=\"3\" y=\"16\" width=\"7\" height=\"5\" rx=\"1.5\"/>"),
        "pulpit" to wrap("<rect x=\"3\" y=\"3\" width=\"7\" height=\"9\" rx=\"1.5\"/><rect x=\"14\" y=\"3\" width=\"7\" height=\"5\" rx=\"1.5\"/><rect x=\"14\" y=\"12\" width=\"7\" height=\"9\" rx=\"1.5\"/><rect x=\"3\" y=\"16\" width=\"7\" height=\"5\" rx=\"1.5\"/>"),
        "chart" to wrap("<path d=\"M3 3v18h18\"/><path d=\"m19 9-5 5-4-4-3 3\"/>"),
        "wykresy" to wrap("<path d=\"M3 3v18h18\"/><path d=\"m19 9-5 5-4-4-3 3\"/>"),
        "weather" to wrap("<path d=\"M17.5 19H9a7 7 0 1 1 6.71-9h1.79a4.5 4.5 0 1 1 0 9Z\"/>"),
        "pogoda" to wrap("<path d=\"M17.5 19H9a7 7 0 1 1 6.71-9h1.79a4.5 4.5 0 1 1 0 9Z\"/>"),
        "settings" to wrap("<circle cx=\"12\" cy=\"12\" r=\"3\"/><path d=\"M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 1 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 1 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 1 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 1 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1Z\"/>"),
        "ustawienia" to wrap("<circle cx=\"12\" cy=\"12\" r=\"3\"/><path d=\"M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 1 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 1 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 1 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 1 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1Z\"/>"),
        "sterowanie" to wrap("<circle cx=\"12\" cy=\"12\" r=\"3\"/><path d=\"M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 1 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 1 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 1 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 1 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1Z\"/>"),
        "pump" to wrap("<circle cx=\"12\" cy=\"12\" r=\"8.5\"/><path d=\"M12 12m-3 0a3 3 0 1 0 6 0a3 3 0 1 0-6 0\"/><path d=\"M12 3.5v3M12 17.5v3M3.5 12h3M17.5 12h3\"/>"),
        "servo" to wrap("<rect x=\"3\" y=\"6\" width=\"18\" height=\"12\" rx=\"2\"/><circle cx=\"12\" cy=\"12\" r=\"3.2\"/><line x1=\"12\" y1=\"6\" x2=\"12\" y2=\"8.8\"/><line x1=\"12\" y1=\"15.2\" x2=\"12\" y2=\"18\"/>"),
        "mixer" to wrap("<path d=\"M5 4h14l-2 15H7L5 4z\"/><line x1=\"12\" y1=\"2\" x2=\"12\" y2=\"15\"/><line x1=\"8.5\" y1=\"15\" x2=\"15.5\" y2=\"15\"/>"),
        "shield" to wrap("<path d=\"M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z\"/>"),
        "thermo" to wrap("<path d=\"M14 14.76V3.5a2.5 2.5 0 0 0-5 0v11.26a4.5 4.5 0 1 0 5 0z\"/>"),
        "outside" to wrap("<circle cx=\"12\" cy=\"12\" r=\"4\"/><path d=\"M12 2v2M12 20v2M4.93 4.93l1.41 1.41M17.66 17.66l1.41 1.41M2 12h2M20 12h2M6.34 17.66l-1.41 1.41M19.07 4.93l-1.41 1.41\"/>"),
        "panel" to wrap("<rect x=\"3\" y=\"4\" width=\"18\" height=\"16\" rx=\"2\"/><line x1=\"3\" y1=\"10\" x2=\"21\" y2=\"10\"/><line x1=\"3\" y1=\"15\" x2=\"21\" y2=\"15\"/><line x1=\"9\" y1=\"4\" x2=\"9\" y2=\"20\"/><line x1=\"15\" y1=\"4\" x2=\"15\" y2=\"20\"/>"),
        "clock" to wrap("<circle cx=\"12\" cy=\"12\" r=\"10\"/><polyline points=\"12 6 12 12 16 14\"/>"),
        "logs" to wrap("<path d=\"M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z\"/><polyline points=\"14 2 14 8 20 8\"/><line x1=\"16\" y1=\"13\" x2=\"8\" y2=\"13\"/><line x1=\"16\" y1=\"17\" x2=\"8\" y2=\"17\"/><line x1=\"10\" y1=\"9\" x2=\"8\" y2=\"9\"/>"),
        "terminal" to wrap("<polyline points=\"4 17 10 11 4 5\"/><line x1=\"12\" y1=\"19\" x2=\"20\" y2=\"19\"/>"),
        "upload" to wrap("<path d=\"M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4\"/><polyline points=\"17 8 12 3 7 8\"/><line x1=\"12\" y1=\"3\" x2=\"12\" y2=\"15\"/>"),
        "session" to wrap("<path d=\"M5 12.55a11 11 0 0 1 14.08 0\"/><path d=\"M1.42 9a16 16 0 0 1 21.16 0\"/><path d=\"M8.53 16.11a6 6 0 0 1 6.95 0\"/><line x1=\"12\" y1=\"20\" x2=\"12.01\" y2=\"20\"/>"),
        "wifi" to wrap("<path d=\"M5 12.55a11 11 0 0 1 14.08 0\"/><path d=\"M1.42 9a16 16 0 0 1 21.16 0\"/><path d=\"M8.53 16.11a6 6 0 0 1 6.95 0\"/><line x1=\"12\" y1=\"20\" x2=\"12.01\" y2=\"20\"/>"),
        "telegram" to wrap("<path d=\"m22 2-7 20-4-9-9-4Z\"/><path d=\"M22 2 11 13\"/>"),
        "location" to wrap("<path d=\"M12 2a8 8 0 0 0-8 8c0 5.25 8 12 8 12s8-6.75 8-12a8 8 0 0 0-8-8z\"/><circle cx=\"12\" cy=\"10\" r=\"3\"/>"),
        "next" to wrap("<polyline points=\"9 18 15 12 9 6\"/>"),
        "chevron" to wrap("<polyline points=\"9 18 15 12 9 6\"/>"),
        "back" to wrap("<polyline points=\"15 18 9 12 15 6\"/>"),
        "close" to wrap("<path d=\"M18 6L6 18M6 6l12 12\"/>"),
        "refresh" to wrap("<path d=\"M21.5 2v6h-6M21.34 15.57a10 10 0 1 1-.57-8.38l6.73-1.19\"/>"),
        "reset" to wrap("<path d=\"M3 12a9 9 0 1 0 9-9 9.75 9.75 0 0 0-6.74 2.74L3 8\"/><path d=\"M3 3v5h5\"/>"),
        "more" to wrap("<circle cx=\"12\" cy=\"12\" r=\"1.5\"/><circle cx=\"12\" cy=\"5\" r=\"1.5\"/><circle cx=\"12\" cy=\"19\" r=\"1.5\"/>"),
        "expand" to wrap("<polyline points=\"15 3 21 3 21 9\"/><polyline points=\"9 21 3 21 3 15\"/><line x1=\"21\" y1=\"3\" x2=\"14\" y2=\"10\"/><line x1=\"3\" y1=\"21\" x2=\"10\" y2=\"14\"/>"),
        "fullscreen" to wrap("<polyline points=\"15 3 21 3 21 9\"/><polyline points=\"9 21 3 21 3 15\"/><line x1=\"21\" y1=\"3\" x2=\"14\" y2=\"10\"/><line x1=\"3\" y1=\"21\" x2=\"10\" y2=\"14\"/>"),
        "check" to wrap("<polyline points=\"20 6 9 17 4 12\"/>"),
        "warn" to wrap("<path d=\"m21.73 18-8-14a2 2 0 0 0-3.48 0l-8 14A2 2 0 0 0 4 21h16a2 2 0 0 0 1.73-3Z\"/><line x1=\"12\" y1=\"9\" x2=\"12\" y2=\"13\"/><line x1=\"12\" y1=\"17\" x2=\"12.01\" y2=\"17\"/>"),
        "info" to wrap("<circle cx=\"12\" cy=\"12\" r=\"10\"/><line x1=\"12\" y1=\"16\" x2=\"12\" y2=\"12\"/><line x1=\"12\" y1=\"8\" x2=\"12.01\" y2=\"8\"/>"),
        "power" to wrap("<path d=\"M18.36 6.64a9 9 0 1 1-12.73 0M12 2v10\"/>"),
        "fire" to wrap("<path d=\"M8.5 14.5A2.5 2.5 0 0 0 11 12c0-1.38-.5-2-1-3-1.072-2.143-.224-4.054 2-6 .5 2.5 2 4.9 4 6.5 2 1.6 3 3.5 3 5.5a7 7 0 1 1-14 0c0-1.153.433-2.294 1-3a2.5 2.5 0 0 0 2.5 2.5z\"/>"),
        "droplet" to wrap("<path d=\"M12 2.69l5.66 5.66a8 8 0 1 1-11.31 0z\"/>"),
        "wind" to wrap("<path d=\"M17.7 7.7A2.5 2.5 0 1 1 19 12H2M14.7 3.7A2.5 2.5 0 1 1 16 8H2M19.7 15.7A2.5 2.5 0 1 1 21 20H2\"/>"),
        "sun" to wrap("<circle cx=\"12\" cy=\"12\" r=\"5\"/><line x1=\"12\" y1=\"1\" x2=\"12\" y2=\"3\"/><line x1=\"12\" y1=\"21\" x2=\"12\" y2=\"23\"/><line x1=\"4.22\" y1=\"4.22\" x2=\"5.64\" y2=\"5.64\"/><line x1=\"18.36\" y1=\"18.36\" x2=\"19.78\" y2=\"19.78\"/><line x1=\"1\" y1=\"12\" x2=\"3\" y2=\"12\"/><line x1=\"21\" y1=\"12\" x2=\"23\" y2=\"12\"/><line x1=\"4.22\" y1=\"19.78\" x2=\"5.64\" y2=\"18.36\"/><line x1=\"18.36\" y1=\"5.64\" x2=\"19.78\" y2=\"4.22\"/>"),
        "moon" to wrap("<path d=\"M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z\"/>"),
        "cloud" to wrap("<path d=\"M18 10h-1.26A8 8 0 1 0 9 20h9a5 5 0 0 0 0-10z\"/>"),
        "zap" to wrap("<polygon points=\"13 2 3 14 12 14 11 22 21 10 12 10 13 2\"/>"),
        "zoomIn" to wrap("<circle cx=\"11\" cy=\"11\" r=\"8\"/><line x1=\"21\" y1=\"21\" x2=\"16.65\" y2=\"16.65\"/><line x1=\"11\" y1=\"8\" x2=\"11\" y2=\"14\"/><line x1=\"8\" y1=\"11\" x2=\"14\" y2=\"11\"/>"),
        "zoomOut" to wrap("<circle cx=\"11\" cy=\"11\" r=\"8\"/><line x1=\"21\" y1=\"21\" x2=\"16.65\" y2=\"16.65\"/><line x1=\"8\" y1=\"11\" x2=\"14\" y2=\"11\"/>"),
        "spark" to wrap("<path d=\"M5 12.5l4.5 4.5L19 7.5\"/>", 2.6),
        "cross" to wrap("<path d=\"M6 6l12 12M18 6L6 18\"/>", 2.6)
    )

    /** Pełny mark-up SVG ikony (cached przez Svg.parse). */
    fun svg(name: String): String = bodies[name] ?: bodies.getValue("settings")

    fun has(name: String): Boolean = bodies.containsKey(name)
}

/** Ikona wektorowa 1:1 z panelu. `tint` odpowiada CSS `color` (currentColor). */
@Composable
fun AppIcon(
    name: String,
    modifier: Modifier = Modifier,
    size: Dp = 24.dp,
    tint: Color = Color.Unspecified
) {
    val src = Icons.svg(name)
    val parsed = remember(src) { Svg.parse(src) }
    Box(modifier = modifier.size(size)) {
        Canvas(Modifier.size(size)) { drawSvg(parsed, currentColor = tint) }
    }
}
