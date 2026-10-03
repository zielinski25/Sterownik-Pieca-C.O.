# GŁĘBOKA ANALIZA ROZBIEŻNOŚCI: C++ vs APK

## 🔴 KRYTYCZNE ROZBIEŻNOŚCI (BŁĘDY)

### 1. Nazwa pola mieszadła - NIEZGODNOŚĆ
**C++ wysyła:** `"mieszadlo": true/false` (linia 14025)
**APK czyta:** `d.optBoolean("mieszadloWlaczony", false)` (linia 976)

**Efekt:** APK NIGDY nie widzi stanu mieszadła - zawsze pokazuje "WYŁĄCZONE"

**Fix needed:** Zmienić w APK na `optBoolean("mieszadlo", false)`

---

### 2. Brakujące pola w APK
C++ wysyła te pola, których APK nie czyta:
- `t_ogrz_sr` - temperatura ogrzewania średnia
- `t_trociny` - temperatura trocin
- `t_powrot` - temperatura powrotu
- `dym_swiezy` - czy odczyt dymu jest świeży
- `pompa_ms_od_zmiany` - ms od ostatniej zmiany pompy
- `pompa_override_min` - min do końca override pompy
- `rozpalanie` - czy aktywne rozpalanie
- `miesiac`, `godzina`, `minuta`, `dzien`, `rok` - czas RTC
- `wifi_mode`, `rssi` - diagnostyka WiFi

**Efekt:** Utrata informacji, ale nie krytyczne

---

## 🟢 POPRAWNE KONTRAKTY

### Komendy Firebase
| Komenda | C++ obsługuje | APK wysyła | Status |
|---------|---------------|------------|--------|
| `pompa_wl` | ✅ | ✅ | OK |
| `pompa_wyl` | ✅ | ✅ | OK |
| `pompa_auto` | ✅ | ✅ | OK |
| `mieszadlo_wl` | ✅ | ✅ | OK |
| `mieszadlo_wyl` | ✅ | ✅ | OK |
| `mieszadlo_auto` | ✅ | ✅ | OK |
| `tryb_auto` | ✅ | ✅ | OK |
| `tryb_reczny` | ✅ | ✅ | OK |
| `tryb_bezpieczny` | ✅ | ✅ | OK |
| `klapa <0-100>` | ✅ | ✅ | OK (po fix skalowania) |
| `syberek <0-100>` | ✅ | ✅ | OK (po fix skalowania) |
| `wycisz` | ✅ | ✅ | OK |
| `reset_alarmow` | ✅ | ✅ | OK |
| `ustaw progAlarmTemp 100` | ✅ | ✅ | OK |
| `symuluj <field> <value>` | ✅ | ✅ | OK |
| `symuluj_stop <field>` | ✅ | ✅ | OK |

### Pola statusu
| Pole | C++ wysyła | APK czyta | Jednostki | Status |
|------|-----------|-----------|-----------|--------|
| `t_zewn` | ✅ | ✅ | °C | OK |
| `t_ogrz` | ✅ | ✅ | °C | OK |
| `t_bojler` | ✅ | ✅ | °C | OK |
| `t_panel` | ✅ | ✅ | °C | OK |
| `t_pokoj` | ✅ | ✅ | °C | OK |
| `cisnienie` | ✅ | ✅ | hPa | OK |
| `wilgotnosc` | ✅ | ✅ | % | OK |
| `pompa` | ✅ | ✅ | bool | OK |
| `dym_alarm` | ✅ | ✅ | bool | OK |
| `dym_wlaczony` | ✅ | ✅ | bool | OK |
| `alarm_ogrzewanie` | ✅ | ✅ | bool | OK |
| `alarm_panel` | ✅ | ✅ | bool | OK |
| `tryb_serwa` | ✅ | ✅ | 1/2/3 | OK |
| `klapa` | ✅ | ✅ | 0-180° → % | OK (po fix) |
| `syberka` | ✅ | ✅ | 0-90° → % | OK (po fix) |
| `wybor` | ✅ | ✅ | 1/2/3 | OK |
| `mieszadlo` | ✅ | ❌ `mieszadloWlaczony` | bool | **BŁĄD** |
| `ip` | ✅ | ✅ | string | OK |

---

## 🔍 ANALIZA SKALOWANIA SERW

### C++ (main_centrala.cpp)
```cpp
// Linia 3: pozycjaKlapa i pozycjaSyberka sa fizycznymi katami w stopniach
// Linia 13978: j += F("\"klapa\":"); j += String(pozycjaKlapa);  // 0-180
// Linia 13979: j += F("\"syberka\":"); j += String(pozycjaSyberek);  // 0-90
// Linia 27323: int procent = constrain(cmd.substring(6).toInt(), 0, 100);
// Linia 16486: int docelowyKat = procentNaKat(docelowaKlapaProcent, 0, 180);
```

### APK (MainActivity.kt) - PO FIX
```kotlin
// Linia 970: val flapDeg = d.optInt("klapa",0)
// Linia 971: val flap = (flapDeg * 100 / 180).coerceIn(0,100)  // ° → %
// Linia 1480: val klapaDeg = d?.optInt("klapa",0)?:0
// Linia 1481: val klapaPct = (klapaDeg * 100 / 180).coerceIn(0,100)  // ° → %
```

**Status:** ✅ POPRAWNE po fix

---

## 📊 PODSUMOWANIE

| Kategoria | Status | Uwagi |
|-----------|--------|-------|
| Komendy Firebase | ✅ OK | Wszystkie komendy działają |
| ACK mechanism | ✅ OK | Po fix ścieżki /piec/ack.json |
| Skalowanie serw | ✅ OK | Po konwersji ° → % |
| Auto-send UX | ✅ OK | Jak Home Assistant/Tado |
| Auto-close dialog | ✅ OK | Po 1.1s z animacją |
| **Pole mieszadła** | ❌ **BŁĄD** | `mieszadlo` vs `mieszadloWlaczony` |
| Dodatkowe pola | ⚠️ INFO | C++ wysyła więcej niż APK czyta |

---

## 🛠️ FIXY ZASTOSOWANE

### ✅ Naprawione w tym buildzie:

1. **Pole mieszadła** (KRITYCZNE)
   - Zmieniono: `optBoolean("mieszadloWlaczony", false)` → `optBoolean("mieszadlo", false)`
   - Teraz APK poprawnie widzi stan mieszadła z C++

2. **Skalowanie serw** (wcześniej)
   - Konwersja ° → % dla klapa (0-180°) i syberka (0-90°)

3. **ACK path** (wcześniej)
   - Zmieniono ścieżkę z `/piec/ack/{cmdId}` → `/piec/ack.json`

4. **Auto-send + auto-close** (wcześniej)
   - UX jak w Home Assistant/Tado/Netatmo

---

## 📊 PODSUMOWANIE

| Kategoria | Status | Uwagi |
|-----------|--------|-------|
| Komendy Firebase | ✅ OK | Wszystkie komendy działają |
| ACK mechanism | ✅ OK | Po fix ścieżki /piec/ack.json |
| Skalowanie serw | ✅ OK | Po konwersji ° → % |
| Auto-send UX | ✅ OK | Jak Home Assistant/Tado |
| Auto-close dialog | ✅ OK | Po 1.1s z animacją |
| **Pole mieszadła** | ✅ **NAPRAWIONE** | `mieszadlo` matchuje C++ |
| Dodatkowe pola | ⚠️ INFO | C++ wysyła więcej niż APK czyta |
