import io, sys

# ── 1. AppModel: ponowne liczenie prognozy po scaleniu historii z chmury ──────
p = "android/app/src/main/java/com/sterownikco/pro/core/AppModel.kt"
s = io.open(p, encoding="utf-8").read()
old = """    fun refreshSolarFromCloud() = scope.launch {
        if (!connected || Rtdb.refreshToken.isBlank()) return@launch
        val now = System.currentTimeMillis()
        // Jedno pobranie doby na 5 minut — polling statusu leci co 8 s i nie może
        // zamieniać się w 180 zapytań do RTDB na kwadrans.
        if (now - solarHistoryAt < 5 * 60_000L) return@launch
        solarHistoryAt = now
        try {
            val pts = Rtdb.fetchTelemetry(24 * 3600) ?: return@launch
            val added = solar.mergeCloudToday(pts)
            if (added > 0) { S.bump(); refreshSheet() }
        } catch (e: Exception) {
            // Brak chmury nie może psuć karty — zostaje to, co już wiadomo.
        }
    }"""
new = """    fun refreshSolarFromCloud() = scope.launch {
        if (connected && Rtdb.refreshToken.isNotBlank()) {
            val now = System.currentTimeMillis()
            // Jedno pobranie doby na 5 minut — polling statusu leci co 8 s i nie może
            // zamieniać się w 180 zapytań do RTDB na kwadrans.
            if (now - solarHistoryAt >= 5 * 60_000L) {
                solarHistoryAt = now
                try {
                    val pts = Rtdb.fetchTelemetry(24 * 3600)
                    if (pts != null) {
                        val added = solar.mergeCloudToday(pts)
                        if (added > 0) { S.bump(); refreshSheet() }
                    }
                } catch (e: Exception) {
                    // Brak chmury nie może psuć karty — zostaje to, co już wiadomo.
                }
            }
        }
        // [0.31.38] „Do końca dnia · szacunek” potrzebuje DWÓCH rzeczy naraz:
        // próbki wody i godzin pogody. Po starcie pogoda przychodzi pierwsza (sieć),
        // historia z chmury dopiero po niej — model startował bez próbki, zwracał NaN
        // i karta zostawała na „brak prognozy” do najbliższego odświeżenia pogody
        // (15 min), a na ekranie 2026-10-29 00:37 pokazywała to obok poprawnych kart
        // 7-dniowych. Dlatego po scaleniu historii ponawiamy obliczenie.
        if (!solar.forecastGain.isFinite()) {
            solar.estimateForecastGain(weather?.hourly ?: emptyList())
        }
    }"""
assert s.count(old) == 1, s.count(old)
s = s.replace(old, new)
io.open(p, "w", encoding="utf-8").write(s)
print("AppModel.kt OK")

# ── 2. Arkusz: uczciwy komunikat, gdy brak próbki wody ───────────────────────
p2 = "android/app/src/main/java/com/sterownikco/pro/ui/screens/sheets/Sensors.kt"
t = io.open(p2, encoding="utf-8").read()
old2 = """            when {
                !sunLeft -> "bez słońca do końca dnia"
                !solar.forecastGain.isFinite() -> "brak prognozy\""""
new2 = """            when {
                !sunLeft -> "bez słońca do końca dnia"
                // [0.31.38] „brak prognozy” myliło dwie przyczyny: brak pogody
                // i brak próbki wody do wystartowania modelu.
                !solar.hasSamples -> "brak próbki wody z czujnika"
                !solar.forecastGain.isFinite() -> "brak prognozy\""""
assert t.count(old2) == 1, t.count(old2)
t = t.replace(old2, new2)
io.open(p2, "w", encoding="utf-8").write(t)
print("Sensors.kt OK")

# ── 3. Pulpit: ten sam komunikat ────────────────────────────────────────────
p3 = "android/app/src/main/java/com/sterownikco/pro/ui/screens/Weather.kt"
w = io.open(p3, encoding="utf-8").read()
old3 = """private fun endOfDaySub(sol: SolarAnalytics, sunLeft: Boolean): String = when {
    !sunLeft -> "bez słońca do końca dnia"
    !sol.forecastGain.isFinite() -> "brak prognozy\""""
new3 = """private fun endOfDaySub(sol: SolarAnalytics, sunLeft: Boolean): String = when {
    !sunLeft -> "bez słońca do końca dnia"
    // [0.31.38] Rozdzielone: brak próbki wody vs brak pogody.
    !sol.hasSamples -> "brak próbki wody z czujnika"
    !sol.forecastGain.isFinite() -> "brak prognozy\""""
assert w.count(old3) == 1, w.count(old3)
w = w.replace(old3, new3)
io.open(p3, "w", encoding="utf-8").write(w)
print("Weather.kt OK")

# ── 4. Wersja 0.31.38 ───────────────────────────────────────────────────────
p4 = "android/app/build.gradle.kts"
g = io.open(p4, encoding="utf-8").read()
assert g.count("versionCode = 31037") == 1 and g.count('versionName = "0.31.37"') == 1
g = g.replace("versionCode = 31037", "versionCode = 31038").replace('versionName = "0.31.37"', 'versionName = "0.31.38"')
io.open(p4, "w", encoding="utf-8").write(g)
print("wersja 0.31.38")
