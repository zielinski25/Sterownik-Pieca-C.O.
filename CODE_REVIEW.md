# STEROWNIK CO — Code Review & Bug Report

**Data:** 2026-10-03  
**Wersja:** v0.30.5 / v1.0 Premium Redesign  
**Status:** 🔴 Krytyczne bugi do naprawy

---

## 🐛 ZNALEZIONE BUGI

### 🔴 CRITICAL — Battery Drain (postInvalidateDelayed)

**Problem:** Animacje działają nawet gdy aplikacja jest w tle!

**Lokalizacja:**
- `BoilerHeroView.kt` linia ~100: `postInvalidateDelayed(50)`
- `DashboardTileView.kt` linia ~120: `postInvalidateDelayed(50)`
- `DashboardTileView.kt` linia ~125: `postInvalidateDelayed(32)`

**Skutek:**
- 20-30 FPS animacji w tle
- Battery drain ~5-10% na godzinę w tle
- CPU usage ~10-15% stale

**Fix:**
```kotlin
// Dodaj do każdego View:
override fun onDetachedFromWindow() {
    super.onDetachedFromWindow()
    // Zatrzymaj animacje
}

override fun onAttachedToWindow() {
    super.onAttachedToWindow()
    // Wznów animacje
}

// W MainActivity:
override fun onStop() {
    super.onStop()
    // Zatrzymaj wszystkie animacje
    heroBoiler.stopAnimations()
    tileViews.values.forEach { it.stopAnimations() }
}
```

---

### 🔴 CRITICAL — Memory Leak (Thread Pools)

**Problem:** ExecutorServices nie są shutdown w niektórych obiektach.

**Lokalizacja:**
- `WeatherService.kt`: `private val executor = Executors.newSingleThreadExecutor()` — NIGDY nie shutdown
- `TelemetryCache.kt`: `private val executor = Executors.newSingleThreadExecutor()` — NIGDY nie shutdown

**Skutek:**
- Thread pools żyją do końca procesu
- Memory leak przy rotacji ekranu
- Blokada GC

**Fix:**
```kotlin
// WeatherService.kt
fun shutdown() { executor.shutdownNow() }

// W MainActivity.onDestroy():
WeatherService.shutdown()
TelemetryCache.shutdown()
```

---

### 🟡 HIGH — Brak RecyclerView (Performance)

**Problem:** Dashboard używa LinearLayout z 13+ kafelkami.

**Skutek:**
- Wszystkie kafelki renderowane naraz
- Brak recyklingu widoków
- Memory usage ~2-3x wyższy

**Fix:** Użyj RecyclerView z GridLayoutManager + ViewHolder pattern

---

### 🟡 HIGH — Brak Offline Support

**Problem:** Aplikacja nie działa bez internetu.

**Fix:**
1. Local cache ostatniego statusu (SharedPreferences)
2. Offline command queue (Room DB)
3. Network observer (ConnectivityManager)

---

### 🟡 HIGH — Security: Plaintext Credentials

**Problem:** `FirebaseDevConfig.kt` zawiera hardcoded credentials.

**Fix:** Przenieś do `local.properties` + `buildConfigField`

---

### 🟠 MEDIUM — Brak Error Handling (Weather)

**Problem:** WeatherService nie ma retry ani cache.

**Fix:** Exponential backoff retry + cache ostatniej pogody

---

### 🟠 MEDIUM — Main Thread I/O (LogStorage)

**Problem:** `LogStorage.loadAll()` może block main thread.

**Fix:** Przenieś do Room DB lub async executor

---

### 🟠 MEDIUM — Brak Rate Limiting (Commands)

**Problem:** Użytkownik może spamować komendami.

**Fix:** Cooldown 2s między komendami

---

### 🟢 LOW — Brak Unit Tests

**Fix:** Dodaj JUnit + Mockito testy dla TelemetryAnalytics

---

### 🟢 LOW — Brak Analytics

**Fix:** Firebase Analytics + Crashlytics

---

## 💡 SUGESTIE ULEPSZE (z community IoT/smart home)

### 1. Widget na ekran główny 🔥
- Widget 2x2 z główną temperaturą
- Widget 4x1 z szybkimi akcjami
- Update co 5 minut

### 2. Dark/Light Mode Toggle 🌗
- Auto / Dark / Light
- `res/values-night/colors.xml`

### 3. Powiadomienia Push 🔔
- Alarm dymu → natychmiastowe push
- Przegrzanie → push
- Utrata połączenia → push po 5 min
- Firebase Cloud Messaging

### 4. Export/Import danych 📊
- Export historii do CSV
- Share wykres jako obraz
- FileProvider + Intent.ACTION_SEND

### 5. Voice Commands 🎤
- Google Assistant App Actions
- "Hey Google, włącz pompę"

### 6. Biometric Auth 🔐
- Face ID / Fingerprint unlock
- `androidx.biometric:biometric:1.1.0`

### 7. Energy Saving Mode 🔋
- Wyłącz animacje gdy bateria < 20%
- PowerManager.isPowerSaveMode()

### 8. Multi-language 🌍
- res/values-pl/strings.xml
- res/values-en/strings.xml

---

**Priorytet napraw:**
1. 🔴 Battery Drain (natychmiast!)
2.  Memory Leak (natychmiast!)
3. 🟡 Offline Support (wysoki)
4. 🟡 Security (wysoki)
5. 🟠 Weather retry (średni)
