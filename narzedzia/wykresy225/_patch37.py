import io, sys

p = "android/app/src/main/java/com/sterownikco/pro/core/SolarAnalytics.kt"
s = io.open(p, encoding="utf-8").read()

def rep(old, new, cnt=1):
    global s
    n = s.count(old)
    if n != cnt:
        print("BLAD: oczekiwano %d wystapien, jest %d" % (cnt, n))
        print(old[:120])
        sys.exit(1)
    s = s.replace(old, new)

# ── 1. recomputeToday: z powrotem tPanel (woda w zbiorniku slonecznym na dachu)
rep(
"""                // [0.31.36] Liczymy ze ZBIORNIKA (tBojler), nie z kolektora (tPanel).
                // Podpis pod tabelą i karta na pulpicie mówią wprost: „Zysk = o ile °C
                // ogrzała się woda w zbiorniku 100 l”. Tymczasem kod brał temperaturę
                // PANELU, więc 2026-10-28 pokazywał −0,3 °C (kolektor 17,5 → 17,2),
                // choć woda w zbiorniku zjechała z 37,5 do 21,0 °C, czyli −16,5 °C.
                if (firstT.isNaN()) firstT = s.tBojler
                lastT = s.tBojler""",
"""                // [0.31.37] tPanel to WODA w zbiorniku słonecznym na dachu, nie płytka
                // kolektora. Potwierdza to ARCHIWUM_SOLARNE_STAN.md §2–§3: czujnik
                // `czujnik_temp_Panel_Plus` (BUS_3, kanał TELEMETRY_CH_PANEL) leży
                // w wodzie, z boku, w połowie wysokości zbiornika 100 l.
                // `tBojler` (BUS_2) to BOJLER PIECA w kotłowni — inna instalacja.
                // 0.31.36 przez pomyłkę liczyła z bojlera pieca i pokazywała −17,5 °C
                // (woda w kotłowni), gdy zbiornik słoneczny zmienił się o −0,2 °C.
                if (firstT.isNaN()) firstT = s.tPanel
                lastT = s.tPanel""")

# ── 2. komentarz recomputeDraws
rep(
"""     *  Poprzednia reguła (`rateBojler < -0.30`) łapała każde zejście o 0,3 °C między dwiema
     *  próbkami minutowymi. Czujnik ma rozdzielczość 0,1 °C, a zbiornik stygnie przez pętlę
     *  termosyfonu — więc takie zejście zdarza się co kilkanaście minut i wychodziło
     *  „Pobrania ciepłej wody: 24 razy · −7,2 °C” (2026-10-28), choć nikt wody nie brał.
     *  Dodatkowo 31,5 − 31,8 w typie Double to −0,3000000000000007, czyli minimalnie
     *  więcej niż próg 0,30 — porównanie łapało granicę przez przypadek.
     *
     *  Teraz pobranie musi być SPADKIEM PODTRZYMANYM:
     *   1. woda schodzi o co najmniej 0,5 °C od maksimum z ostatnich 5 minut,
     *   2. i zostaje niżej przez następne 5 minut.
     *  Drgnięcie czujnika wraca w ciągu minuty, więc punkt 2 je odrzuca.
     *  Epizody bliższe niż 20 minut to jedno pobranie (kąpiel trwa), a spadek większy
     *  niż 15 °C to odczepiona sonda — obcinamy, żeby nie psuł sumy.
     *
     *  Sprawdzone na eksporcie z chmury (28 dni, 39 602 próbki): 33 pobrania w 16 dniach,
     *  2026-10-28 — zero (zbiornik tylko stygł).""",
"""     *  Liczone z `tPanel` — czyli z WODY w zbiorniku słonecznym na dachu
     *  (ARCHIWUM_SOLARNE_STAN.md §2–§3), a nie z bojlera pieca w kotłowni.
     *
     *  Stara reguła (`rateBojler < -0.30`) łapała każde zejście o 0,3 °C między dwiema
     *  próbkami minutowymi. Czujnik ma rozdzielczość 0,1 °C, a zbiornik stygnie przez pętlę
     *  termosyfonu — więc takie zejście zdarza się co kilkanaście minut i wychodziło
     *  „Pobrania ciepłej wody: 24 razy · −7,2 °C” (2026-10-28), choć nikt wody nie brał.
     *  Dodatkowo 31,5 − 31,8 w typie Double to −0,3000000000000007, czyli minimalnie
     *  więcej niż próg 0,30 — porównanie łapało granicę przez przypadek.
     *
     *  Teraz pobranie musi być SPADKIEM PODTRZYMANYM:
     *   1. woda schodzi o co najmniej 0,5 °C od maksimum z ostatnich 5 minut,
     *   2. i zostaje niżej przez następne 5 minut.
     *  Drgnięcie czujnika wraca w ciągu minuty, więc punkt 2 je odrzuca.
     *  Epizody bliższe niż 20 minut to jedno pobranie (kąpiel trwa). Spadek większy
     *  niż 12 °C jest ODRZUCANY jako usterka sondy — 100 l nie traci 25 °C od kranu.
     *
     *  Sprawdzone na eksporcie z chmury (28 dni): 26 pobrań w 17 dniach, po 1–2 na dobę,
     *  każde 0,5–5 °C, głównie po południu. 2026-10-28 — zero (nikt nie brał wody).""")

# ── 3. cialo recomputeDraws: tBojler -> tPanel
rep(
"""            var base = archive[i].tBojler
            var j = i
            while (j > 0 && ts - archive[j - 1].ts <= DRAW_WINDOW_MS) {
                j--
                if (archive[j].tBojler > base) base = archive[j].tBojler
            }
            if (base - archive[i].tBojler >= DRAW_STEP_C) {
                var low = archive[i].tBojler
                var k = i + 1
                while (k < n && archive[k].ts - ts <= DRAW_HOLD_MS) {
                    if (archive[k].tBojler < low) low = archive[k].tBojler
                    k++
                }""",
"""            var base = archive[i].tPanel
            var j = i
            while (j > 0 && ts - archive[j - 1].ts <= DRAW_WINDOW_MS) {
                j--
                if (archive[j].tPanel > base) base = archive[j].tPanel
            }
            if (base - archive[i].tPanel >= DRAW_STEP_C) {
                var low = archive[i].tPanel
                var k = i + 1
                while (k < n && archive[k].ts - ts <= DRAW_HOLD_MS) {
                    if (archive[k].tPanel < low) low = archive[k].tPanel
                    k++
                }""")

# ── 4. odrzucanie skokow zamiast obcinania
rep(
"""            flag[startIdx] = 1
            drop[startIdx] = round1(minOf(maxOf(0.0, base - low), DRAW_MAX_C))
            c = c2""",
"""            val span = maxOf(0.0, base - low)
            if (span <= DRAW_FAULT_C) {
                flag[startIdx] = 1
                drop[startIdx] = round1(span)
            }
            // span > DRAW_FAULT_C: sonda się odczepiła albo wróciła po restarcie —
            // nie udajemy, że ktoś wylał 20 °C wody.
            c = c2""")

rep("        private const val DRAW_MAX_C = 15.0", "        private const val DRAW_FAULT_C = 12.0")
rep("        /** [0.31.36] Progi wykrywania pobrania ciepłej wody — patrz `recomputeDraws()`.",
    "        /** [0.31.37] Progi wykrywania pobrania ciepłej wody — patrz `recomputeDraws()`.")

rep(
"""   [0.31.36] Pobranie wody = spadek podtrzymany ≥ 0,5 °C (recomputeDraws),
   a „zysk dziś” liczony ze zbiornika (tBojler), nie z kolektora.""",
"""   [0.31.37] Wszystko liczone z `tPanel` = woda w zbiorniku słonecznym na dachu
   (BUS_3), nie z bojlera pieca (tBojler/BUS_2). Pobranie wody = spadek
   podtrzymany ≥ 0,5 °C, skok > 12 °C odrzucany jako usterka sondy.""")

io.open(p, "w", encoding="utf-8").write(s)
print("SolarAnalytics.kt: podmienione OK")
