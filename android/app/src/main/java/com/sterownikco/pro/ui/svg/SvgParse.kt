package com.sterownikco.pro.ui.svg

import android.util.LruCache
import android.util.Xml
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.graphics.Color
import org.xmlpull.v1.XmlPullParser
import java.io.StringReader

/**
 * Parser mark-upu SVG użytego w Piec.html (podzbiór: g/path/rect/circle/ellipse/
 * line/polyline/polygon/text/defs z gradientami i clipPath).
 */
object Svg {

    private val cache = LruCache<String, ParsedSvg>(600)

    fun parse(src: String): ParsedSvg {
        cache.get(src)?.let { return it }
        val p = build(src)
        cache.put(src, p)
        return p
    }

    private fun build(src: String): ParsedSvg {
        val all = readXml(src)
        val rootSvg = all.firstOrNull { it.tag == "svg" }
        val vb = rootSvg?.attr("viewBox")?.let { v ->
            val parts = v.trim().split(Regex("[\\s,]+")).mapNotNull { it.toFloatOrNull() }
            if (parts.size == 4) Rect(parts[0], parts[1], parts[0] + parts[2], parts[1] + parts[3])
            else Rect(0f, 0f, 24f, 24f)
        } ?: Rect(0f, 0f, 24f, 24f)

        val grads = HashMap<String, SvgGrad>()
        val clips = HashMap<String, List<SvgNode>>()
        val body = ArrayList<SvgNode>()
        val children = rootSvg?.children ?: all
        walk(children, grads, clips, body)
        return ParsedSvg(vb, body, grads, clips)
    }

    private fun walk(
        nodes: List<SvgNode>,
        grads: MutableMap<String, SvgGrad>,
        clips: MutableMap<String, List<SvgNode>>,
        body: MutableList<SvgNode>
    ) {
        for (node in nodes) {
            when (node.tag) {
                "defs" -> walk(node.children, grads, clips, body)
                "linearGradient", "radialGradient" -> readGrad(node)?.let { grads[it.first] = it.second }
                "clipPath" -> node.attr("id")?.let { clips[it] = node.children }
                "style", "script", "title", "filter", "use" -> Unit
                "svg" -> walk(node.children, grads, clips, body)
                else -> body.add(node)
            }
        }
    }

    private fun readGrad(node: SvgNode): Pair<String, SvgGrad>? {
        val id = node.attr("id") ?: return null
        val stops = node.children.filter { it.tag == "stop" }.map { s ->
            val raw = s.attr("offset")?.trim() ?: "0"
            val frac = if (raw.endsWith("%")) (raw.dropLast(1).toFloatOrNull() ?: 0f) / 100f
            else raw.toFloatOrNull() ?: 0f
            val col = parseColor(s.attr("stop-color") ?: "#ffffff") ?: Color.White
            val o = s.num("stop-opacity", 1f)
            frac to col.copy(alpha = col.alpha * (if (o.isNaN()) 1f else o))
        }
        val grad = if (node.tag == "linearGradient") SvgGrad(
            false,
            pct(node.attr("x1")), pct(node.attr("y1")), pct(node.attr("x2")), pct(node.attr("y2")),
            0f, 0f, 0f, stops
        ) else SvgGrad(
            true, 0f, 0f, 0f, 0f,
            pct(node.attr("cx")), pct(node.attr("cy")), pct(node.attr("r")), stops
        )
        return id to grad
    }

    private fun pct(v: String?): Float {
        val s = v?.trim() ?: return 0f
        return if (s.endsWith("%")) (s.dropLast(1).toFloatOrNull() ?: 0f) / 100f else s.toFloatOrNull() ?: 0f
    }

    private fun readXml(src: String): List<SvgNode> {
        val wrapped = "<root>$src</root>"
        val parser = Xml.newPullParser()
        parser.setFeature(XmlPullParser.FEATURE_PROCESS_NAMESPACES, false)
        parser.setInput(StringReader(wrapped))
        val stack = ArrayList<Pair<String, HashMap<String, String>>>()
        val kids = ArrayList<MutableList<SvgNode>>()
        val texts = ArrayList<StringBuilder>()
        val out = ArrayList<SvgNode>()
        kids.add(out)
        texts.add(StringBuilder())

        var ev = parser.eventType
        while (ev != XmlPullParser.END_DOCUMENT) {
            when (ev) {
                XmlPullParser.START_TAG -> {
                    val attrs = HashMap<String, String>()
                    for (a in 0 until parser.attributeCount) {
                        attrs[parser.getAttributeName(a)] = parser.getAttributeValue(a) ?: ""
                    }
                    stack.add(parser.name to attrs)
                    kids.add(ArrayList())
                    texts.add(StringBuilder())
                }
                XmlPullParser.TEXT -> {
                    val t = parser.text?.trim()
                    if (!t.isNullOrEmpty() && texts.isNotEmpty()) texts[texts.size - 1].append(t).append(' ')
                }
                XmlPullParser.END_TAG -> {
                    if (stack.isNotEmpty()) {
                        val (tag, attrs) = stack.removeAt(stack.size - 1)
                        val ch = kids.removeAt(kids.size - 1)
                        val txt = texts.removeAt(texts.size - 1).toString().trim()
                        val node = SvgNode(tag, attrs, ch, txt)
                        kids[kids.size - 1].add(node)
                    }
                }
            }
            ev = parser.next()
        }
        return out
    }

    // ───────────────────────────── kolory ─────────────────────────────
    /** Znacznik „currentColor" — podstawiamy kolor przekazany przez rysującego. */
    val CURRENT = Color(0xFFFF01FF)

    fun parseColor(raw: String?): Color? {
        val s = raw?.trim()?.lowercase() ?: return null
        if (s.isEmpty() || s == "none" || s == "transparent") return null
        if (s == "currentcolor") return CURRENT
        if (s.startsWith("#")) {
            val hx = s.substring(1)
            return try {
                when (hx.length) {
                    3 -> Color(
                        Integer.parseInt(hx[0].toString(), 16) / 15f,
                        Integer.parseInt(hx[1].toString(), 16) / 15f,
                        Integer.parseInt(hx[2].toString(), 16) / 15f
                    )
                    4 -> Color(
                        Integer.parseInt(hx[0].toString(), 16) / 15f,
                        Integer.parseInt(hx[1].toString(), 16) / 15f,
                        Integer.parseInt(hx[2].toString(), 16) / 15f,
                        Integer.parseInt(hx[3].toString(), 16) / 15f
                    )
                    6 -> Color(0xFF000000L or Integer.parseInt(hx, 16).toLong())
                    8 -> {
                        val v = java.lang.Long.parseLong(hx, 16)
                        // SVG #rrggbbaa -> ARGB
                        Color((v and 0xFFFFFFL) shl 8 or (v ushr 24))
                    }
                    else -> null
                }
            } catch (e: NumberFormatException) {
                null
            }
        }
        if (s.startsWith("rgb(") || s.startsWith("rgba(")) {
            val nums = s.substringAfter('(').substringBeforeLast(')').split(',').map { it.trim() }
            if (nums.size < 3) return null
            val r = nums[0].removeSuffix("%").toFloatOrNull() ?: 0f
            val g = nums[1].removeSuffix("%").toFloatOrNull() ?: 0f
            val b = nums[2].removeSuffix("%").toFloatOrNull() ?: 0f
            val a = if (nums.size > 3) nums[3].toFloatOrNull() ?: 1f else 1f
            return Color(r / 255f, g / 255f, b / 255f, a)
        }
        return NAMED[s]
    }

    private val NAMED = mapOf(
        "white" to Color(0xFFFFFFFFL), "black" to Color(0xFF000000L),
        "red" to Color(0xFFFF0000L), "orange" to Color(0xFFFFA500L),
        "yellow" to Color(0xFFFFFF00L), "green" to Color(0xFF008000L),
        "lime" to Color(0xFF00FF00L), "cyan" to Color(0xFF00FFFFL),
        "blue" to Color(0xFF0000FFL), "purple" to Color(0xFF800080L),
        "magenta" to Color(0xFFFF00FFL), "gray" to Color(0xFF808080L),
        "grey" to Color(0xFF808080L), "silver" to Color(0xFFC0C0C0L),
        "gold" to Color(0xFFFFD700L), "skyblue" to Color(0xFF87CEEBL),
        "tomato" to Color(0xFFFF6347L)
    )
}
