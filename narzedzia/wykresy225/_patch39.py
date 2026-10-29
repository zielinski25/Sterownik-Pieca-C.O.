import io, sys

p = "android/app/src/main/java/com/sterownikco/pro/ui/chart/ChartEngine.kt"
s = io.open(p, encoding="utf-8").read()

old = """        // [v0.31.34 GAP-MARK] Przerwa > 6 min jest zaznaczona i podpisana.
        // Puste miejsce ma byc widoczne jako BRAK PROBEK, a nie jako blad rysowania.
        run {
            val markColor = Pal.rgba(255, 138, 101, .55f)
            var prevTs = view.rows.first().ts
            for (i in 1 until view.rows.size) {
                val ts = view.rows[i].ts
                if (ts - prevTs > 360_000L) {
                    val xa = timeAxis.xOf(prevTs)
                    val xb = timeAxis.xOf(ts)
                    val dash = PathEffect.dashPathEffect(floatArrayOf(4.dp.toPx(), 4.dp.toPx()))
                    drawLine(markColor, Offset(xa, padTop), Offset(xa, padTop + plotH), 1.dp.toPx(), pathEffect = dash)
                    drawLine(markColor, Offset(xb, padTop), Offset(xb, padTop + plotH), 1.dp.toPx(), pathEffect = dash)
                    val gapMin = (ts - prevTs) / 60_000L
                    val txt = if (gapMin >= 60L) "luka " + (gapMin / 60L) + " h " + (gapMin % 60L) + " min"
                    else "luka " + gapMin + " min"
                    val tp = measure(txt, TextStyle(fontSize = 8.5.sp, color = markColor))
                    if (xb - xa > tp.size.width + 6.dp.toPx()) {
                        drawText(tp, topLeft = Offset((xa + xb - tp.size.width) / 2f, padTop + 4.dp.toPx()))
                    }
                }
                prevTs = ts
            }
        }"""

new = """        // [v0.31.34 GAP-MARK] Przerwa > 6 min jest zaznaczona i podpisana.
        // Puste miejsce ma byc widoczne jako BRAK PROBEK, a nie jako blad rysowania.
        // [0.31.39] Podpis nie miescil sie i znikal bez sladu. Przy zakresie 7 dni
        // luka 77 min to ~4 px szerokosci, a tekst "luka 1 h 17 min" ma ~70 px —
        // warunek `xb - xa > szerokosc tekstu` byl spelniony tylko przy duzym zoomie,
        // wiec na zrzucie z 29.10 00:56 byly dwie pary kresek i ZERO napisow.
        // Teraz: (1) cala luka dostaje delikatne tlo, widoczne przy kazdym zoomie,
        // (2) podpis trafia do srodka, gdy sie miesci, a gdy nie — obok kreski,
        // (3) etykiety nie nachodza na siebie (kazda pamieta swoja prawa krawedz).
        run {
            val markColor = Pal.rgba(255, 138, 101, .55f)
            val bandColor = Pal.rgba(255, 138, 101, .10f)
            val gapTextStyle = TextStyle(fontSize = 8.5.sp, color = markColor)
            var prevTs = view.rows.first().ts
            var lastLabelRight = -1f
            for (i in 1 until view.rows.size) {
                val ts = view.rows[i].ts
                if (ts - prevTs > 360_000L) {
                    val xa = timeAxis.xOf(prevTs)
                    val xb = timeAxis.xOf(ts)
                    // Tlo luki — od razu widac, ze to brak probek, a nie plotno.
                    if (xb > xa) drawRect(bandColor, Offset(xa, padTop), Size(xb - xa, plotH))
                    val dash = PathEffect.dashPathEffect(floatArrayOf(4.dp.toPx(), 4.dp.toPx()))
                    drawLine(markColor, Offset(xa, padTop), Offset(xa, padTop + plotH), 1.dp.toPx(), pathEffect = dash)
                    drawLine(markColor, Offset(xb, padTop), Offset(xb, padTop + plotH), 1.dp.toPx(), pathEffect = dash)
                    val gapMin = (ts - prevTs) / 60_000L
                    val txt = if (gapMin >= 60L) "luka " + (gapMin / 60L) + " h " + (gapMin % 60L) + " min"
                    else "luka " + gapMin + " min"
                    val tp = measure(txt, gapTextStyle)
                    val labelW = tp.size.width
                    val labelY = padTop + 4.dp.toPx()
                    val rightmost = w - padRight - labelW
                    // 1) srodek luki, gdy tekst sie miesci
                    var lx = if (xb - xa > labelW + 6.dp.toPx()) (xa + xb - labelW) / 2f else Float.NaN
                    // 2) inaczej po prawej stronie kreski konczacej luke
                    if (lx.isNaN()) lx = xb + 4.dp.toPx()
                    // 3) nie wychodz poza pole wykresu
                    if (lx > rightmost) lx = rightmost
                    if (lx < padLeft) lx = padLeft
                    // 4) nie nachodz na poprzedni podpis — wtedy nad nim
                    var ly = labelY
                    if (lx < lastLabelRight + 4.dp.toPx()) {
                        ly = labelY + tp.size.height + 1.dp.toPx()
                    }
                    drawText(tp, topLeft = Offset(lx, ly))
                    lastLabelRight = lx + labelW
                }
                prevTs = ts
            }
        }"""

if s.count(old) != 1:
    print("BLAD: oczekiwano 1 wystapienia, jest", s.count(old))
    sys.exit(1)
s = s.replace(old, new)
io.open(p, "w", encoding="utf-8").write(s)
print("ChartEngine.kt OK")

# wersja
p4 = "android/app/build.gradle.kts"
g = io.open(p4, encoding="utf-8").read()
assert g.count("versionCode = 31038") == 1 and g.count('versionName = "0.31.38"') == 1
g = g.replace("versionCode = 31038", "versionCode = 31039").replace('versionName = "0.31.38"', 'versionName = "0.31.39"')
io.open(p4, "w", encoding="utf-8").write(g)
print("wersja 0.31.39")
