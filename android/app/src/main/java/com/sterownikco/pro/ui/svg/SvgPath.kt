package com.sterownikco.pro.ui.svg

import androidx.compose.ui.graphics.Path
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.acos
import kotlin.math.ceil
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.sin
import kotlin.math.sqrt
import kotlin.math.tan

/**
 * Parser atrybutu `d` (SVG path) → androidx.compose.ui.graphics.Path.
 * Obsługuje: M/m L/l H/h V/v C/c S/s Q/q T/t A/a Z/z — pełen zestaw użyty w Piec.html.
 */
object SvgPath {

    private class Cursor {
        var x = 0f
        var y = 0f
        var startX = 0f
        var startY = 0f
        var lastCubicCx = 0f
        var lastCubicCy = 0f
        var lastQuadCx = 0f
        var lastQuadCy = 0f
        var prevCmd = ' '
    }

    fun parse(d: String): Path {
        val path = Path()
        val cmds = ArrayList<Pair<Char, List<Float>>>()
        var i = 0
        val n = d.length
        var cur = ArrayList<Float>()
        var cmd = ' '

        fun flush() {
            if (cmd != ' ') cmds.add(cmd to cur.toList())
            cur = ArrayList()
        }

        while (i < n) {
            val c = d[i]
            when {
                c.isLetter() && c != 'e' && c != 'E' -> { flush(); cmd = c }
                c == '-' || c == '+' || c == '.' || c.isDigit() -> {
                    val start = i
                    if (c == '-' || c == '+') i++
                    var seenDot = false
                    while (i < n) {
                        val cc = d[i]
                        if (cc.isDigit()) i++
                        else if (cc == '.' && !seenDot) { seenDot = true; i++ }
                        else if ((cc == 'e' || cc == 'E') && i + 1 < n &&
                            (d[i + 1].isDigit() || d[i + 1] == '-' || d[i + 1] == '+')
                        ) {
                            i += 2
                            if (i < n && (d[i] == '-' || d[i] == '+')) i++
                            while (i < n && d[i].isDigit()) i++
                            break
                        } else break
                    }
                    d.substring(start, i).toFloatOrNull()?.let { cur.add(it) }
                    continue
                }
            }
            i++
        }
        flush()

        val c0 = Cursor()
        for ((command, args) in cmds) {
            apply(path, c0, command, args)
        }
        return path
    }

    private fun apply(path: Path, st: Cursor, cmdIn: Char, a: List<Float>) {
        val rel = cmdIn.isLowerCase()
        val cmd = cmdIn.uppercaseChar()
        val step = when (cmd) {
            'M', 'L', 'T' -> 2
            'H', 'V' -> 1
            'C' -> 6
            'S', 'Q' -> 4
            'A' -> 7
            'Z' -> 0
            else -> 0
        }
        if (step == 0 && cmd != 'Z') return
        fun at(k: Int): Float = a.getOrElse(k) { 0f }
        var idx = 0
        var first = true
        while (idx <= a.size - step || (cmd == 'Z' && first)) {
            doOne(path, st, cmd, if (first) ' ' else ' ', rel, a, idx, first)
            idx += step
            first = false
            if (cmd == 'Z') break
        }
    }

    /** Wykonuje jedną instancję komendy; `implicit` oznacza powtórzenie M jako L. */
    private fun doOne(
        path: Path, st: Cursor, cmd: Char, implicit: Char, rel: Boolean,
        a: List<Float>, base: Int, first: Boolean
    ) {
        var idx = base
        fun at(): Float = a.getOrElse(idx) { 0f }.also { idx++ }
        fun px(v: Float) = if (rel) st.x + v else v
        fun py(v: Float) = if (rel) st.y + v else v
        when (cmd) {
            'M' -> {
                val x = px(at()); val y = py(at())
                st.x = x; st.y = y; st.startX = x; st.startY = y
                if (first) path.moveTo(x, y) else path.lineTo(x, y)
            }
            'L' -> { val x = px(at()); val y = py(at()); path.lineTo(x, y); st.x = x; st.y = y }
            'H' -> { val x = px(at()); path.lineTo(x, st.y); st.x = x }
            'V' -> { val y = py(at()); path.lineTo(st.x, y); st.y = y }
            'C' -> {
                val c1x = px(at()); val c1y = py(at())
                val c2x = px(at()); val c2y = py(at())
                val x = px(at()); val y = py(at())
                path.cubicTo(c1x, c1y, c2x, c2y, x, y)
                st.lastCubicCx = c2x; st.lastCubicCy = c2y; st.x = x; st.y = y
            }
            'S' -> {
                val c1x = 2 * st.x - st.lastCubicCx
                val c1y = 2 * st.y - st.lastCubicCy
                val c2x = px(at()); val c2y = py(at())
                val x = px(at()); val y = py(at())
                path.cubicTo(c1x, c1y, c2x, c2y, x, y)
                st.lastCubicCx = c2x; st.lastCubicCy = c2y; st.x = x; st.y = y
            }
            'Q' -> {
                val cx = px(at()); val cy = py(at())
                val x = px(at()); val y = py(at())
                path.quadraticBezierTo(cx, cy, x, y)
                st.lastQuadCx = cx; st.lastQuadCy = cy; st.x = x; st.y = y
            }
            'T' -> {
                val cx = 2 * st.x - st.lastQuadCx
                val cy = 2 * st.y - st.lastQuadCy
                val x = px(at()); val y = py(at())
                path.quadraticBezierTo(cx, cy, x, y)
                st.lastQuadCx = cx; st.lastQuadCy = cy; st.x = x; st.y = y
            }
            'A' -> {
                val rx = abs(at()); val ry = abs(at())
                val rot = at() * PI.toFloat() / 180f
                val laf = at() > 0.5f
                val sweep = at() > 0.5f
                val x = px(at()); val y = py(at())
                arcTo(path, st.x, st.y, rx, ry, rot, laf, sweep, x, y)
                st.x = x; st.y = y
            }
            'Z' -> { path.close(); st.x = st.startX; st.y = st.startY }
        }
        st.prevCmd = cmd
    }

    /** Łuk eliptyczny (endpoint → center) aproksymowany odcinkami. */
    private fun arcTo(
        path: Path,
        x0: Float, y0: Float,
        rxIn: Float, ryIn: Float,
        phi: Float, laf: Boolean, sf: Boolean,
        x1: Float, y1: Float
    ) {
        if (rxIn == 0f || ryIn == 0f) { path.lineTo(x1, y1); return }
        var rx = rxIn; var ry = ryIn
        val dx = (x0 - x1) / 2f
        val dy = (y0 - y1) / 2f
        val x1p = cos(phi) * dx + sin(phi) * dy
        val y1p = -sin(phi) * dx + cos(phi) * dy
        var lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry)
        if (lambda > 1f) {
            val s = sqrt(lambda)
            rx *= s; ry *= s
        }
        val sign = if (laf != sf) 1f else -1f
        val num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p
        val den = rx * rx * y1p * y1p + ry * ry * x1p * x1p
        val co = sign * sqrt(max(num, 0f) / den)
        val cxp = co * rx * y1p / ry
        val cyp = -co * ry * x1p / rx
        val cx = cos(phi) * cxp - sin(phi) * cyp + (x0 + x1) / 2f
        val cy = sin(phi) * cxp + cos(phi) * cyp + (y0 + y1) / 2f

        fun angle(ux: Float, uy: Float, vx: Float, vy: Float): Float {
            val dot = ux * vx + uy * vy
            val len = sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy))
            var a = acos((dot / max(len, 1e-6f)).coerceIn(-1f, 1f))
            if (ux * vy - uy * vx < 0f) a = -a
            return a
        }

        val th1 = angle(1f, 0f, (x1p - cxp) / rx, (y1p - cyp) / ry)
        var dTheta = angle(
            (x1p - cxp) / rx, (y1p - cyp) / ry,
            (-x1p - cxp) / rx, (-y1p - cyp) / ry
        )
        if (!sf && dTheta > 0f) dTheta -= 2f * PI.toFloat()
        if (sf && dTheta < 0f) dTheta += 2f * PI.toFloat()

        val segs = max(ceil(abs(dTheta) / (PI.toFloat() / 2f)), 1f).toInt()
        val delta = dTheta / segs
        val t = 4f / 3f * tan(delta / 4f)
        var th = th1
        var px = cx + rx * cos(phi) * cos(th) - ry * sin(phi) * sin(th)
        var py = cy + rx * sin(phi) * cos(th) + ry * cos(phi) * sin(th)
        path.lineTo(px, py)
        for (k in 0 until segs) {
            val th2 = th + delta
            val x2 = cx + rx * cos(phi) * cos(th2) - ry * sin(phi) * sin(th2)
            val y2 = cy + rx * sin(phi) * cos(th2) + ry * cos(phi) * sin(th2)
            val dx1 = -rx * cos(phi) * sin(th) - ry * sin(phi) * cos(th)
            val dy1 = -rx * sin(phi) * sin(th) + ry * cos(phi) * cos(th)
            val dx2 = -rx * cos(phi) * sin(th2) - ry * sin(phi) * cos(th2)
            val dy2 = -rx * sin(phi) * sin(th2) + ry * cos(phi) * cos(th2)
            path.cubicTo(px + t * dx1, py + t * dy1, x2 - t * dx2, y2 - t * dy2, x2, y2)
            px = x2; py = y2; th = th2
        }
    }

}
