# Android v0.31.21 — auto-stop terminala DLOG

Data: 2026-10-22
Baza: v0.31.20 (3026)
Wynik: **v0.31.21 (3027)**

## Co to zmienia (dwie rzeczy)

### 1. Auto-stop terminala DLOG

Arkusz „Terminal diagnostyczny" odpytuje Firebase co **5 s**. Gdy użytkownik
zostawi go otwartego i odejdzie, zapytania lecą **bez końca**:

| | przed (0.31.20) | po (0.31.21) |
|---|---|---|
| jedno zapytanie DLOG | co 5 s | co 5 s (bez zmian) |
| brak dotknięcia ekranu | poll trwa w nieskończoność | **stop po 5 min** |
| zapytania w 1. godz. otwartego terminala | 720 | 61 |
| wznowienie | — | dotknięcie / przewinięcie ekranu |
| stan centrali | — | **bez zmian** (nic nie wysyłamy do ESP32) |

Po 5 minutach bezczynności pasek statusu pokazuje:
„Podgląd wstrzymany po 5 min bez dotykania ekranu — dotknij, aby wznowić."

### 2. Kafelek SERWO w procentach

Pulpit pokazywał pod „AUTO (100%)" podpis w stopniach: `klapa 180° · syberek 90°`.
Duża liczba obok **już była w procentach** — poprawiony został tylko ten podpis.

| | przed | po |
|---|---|---|
| klapa w skrajnym otwarciu | `klapa 180°` | `klapa 100%` |
| syberek w skrajnym otwarciu | `syberek 90°` | `syberek 100%` |
| klapa w połowie | `klapa 90°` | `klapa 50%` |
| brak odczytu | `brak prawidłowego odczytu pozycji` | bez zmian |

Przelicznik: `Math.round(klapa * 100 / 180)`, `Math.round(syberka * 100 / 90)`.
Zakres zmiany: **tylko kafelek na Pulpicie**. Arkusz „Serwo" bez zmian
(tam procent jest dużą liczbą, a kąt małą — tak zostaje). Podpis „cel … °"
podczas wysyłania komendy też bez zmian.

## Dlaczego nie ruszamy niczego innego

Uzgodnione z użytkownikiem: **rytm statusu zostaje** — `delay(8000)`
w `AppModel.kt` i `BACKGROUND_STATUS_POLL_MS = 60_000` w
`AlarmMonitorService.kt` bez zmian. Jedyna zmiana w tej wersji to auto-stop
terminala.

Auto-stop **nie wysyła** `terminalRemoteOff()`. Zatrzymujemy wyłącznie własny
odbiór w aplikacji; stan logów na centrali zostaje taki, jak ustawił użytkownik.

## Zmiany w kodzie

| plik | linia | co |
|---|---|---|
| `core/AppModel.kt` | 42–48 | nowa stała `TERMINAL_DLOG_IDLE_STOP_MS = 5L * 60_000L` |
| `core/AppModel.kt` | 1642–1645 | `terminalDlogLastTouch`, `terminalDlogIdleStopped` |
| `core/AppModel.kt` | 1762–1794 | `startTerminalDlogPolling(restart)` — warunek bezczynności przed każdym pollem |
| `core/AppModel.kt` | 1796–1803 | `terminalTouch()` — podtrzymanie i wznowienie |
| `core/AppModel.kt` | 1805–1809 | `stopTerminalDlogPolling()` czyści też flagę bezczynności |
| `ui/screens/sheets/System.kt` | 44–45, 498–512 | `pointerInput` na korzeniu arkusza (pass `Initial`, zdarzenia **nie** są konsumowane, przewijanie konsoli działa jak wcześniej) |
| `ui/screens/Dashboard.kt` | 176–178, 191–194 | podpis kafelka SERWO w procentach (`flapLabel`, `syberkaLabel`) |
| `app/build.gradle.kts` | 15–16 | `versionCode 3027`, `versionName "v0.31.21"` |

### Wyścig, który został zamknięty

`terminalTouch()` woła `startTerminalDlogPolling(restart = true)`. Bez tego
parametru dotknięcie w tej samej klatce, w której pętla się domykała, trafiałoby
na `terminalDlogJob?.isActive == true` i wracało bez startu — terminal zostałby
martwy do zamknięcia arkusza. `restart = true` anuluje dobiegającą korutynę
i startuje nową.

## Czego nie da się sprawdzić w sandboxie

Brak JDK / Android SDK / Gradle → **`assembleDebug` i testy jednostkowe muszą
pójść na laptopie użytkownika**. Zamiast tego wykonano:

- przelicznik procentów sprawdzony na 17 wartościach brzegowych
  (0°→0%, 90°→50%, 180°→100%, 179°→99%, 45° syberka→50%) — OK,

- kontrolę balansu nawiasów i stringów (`kcheck.py`) w 6 plikach — OK,
- port logiki auto-stopu do Pythona, **6 scenariuszy**:
  T1 stop po 305 s / 61 zapytań, T2 dotknięcia co minutę — brak stopu,
  T3 stop 305 s po ostatnim dotknięciu, T4 wznowienie po stopie,
  T5 wyścig (dotknięcie dokładnie w klatce stopu), T6 zamknięcie/otwarcie
  arkusza — **wszystko OK**.

## Pliki

- `SterownikCO-Android-v0.31.21-HISTORIA-CACHE.zip` — paczka (92 568 B,
  sha256 `491f5632…`, 11 wpisów: README + diff + 9 plików źródłowych),
- `README-WGRANIE.txt` — instrukcja dla użytkownika (kopia z ZIP-a),
- `ZMIANY-0.31.19-do-0.31.21.diff` — pełny diff od drzewa, z którego powstał APK 0.31.19,
- `ZMIANY-0.31.20-do-0.31.21.diff` — tylko to, co doszło w tej wersji.
