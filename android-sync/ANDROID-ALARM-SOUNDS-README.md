# Cztery dodatkowe dźwięki alarmowe Androida

Dodaje do istniejącego selektora cztery syntetyczne, oryginalne warianty ostrzeżeń:

- **Syrena falująca** — płynny, powtarzany wzrost i spadek tonu.
- **Syrena ratunkowa** — naprzemienny, dwutonowy sygnał.
- **Klakson pulsacyjny** — sześć krótkich, niskich impulsów.
- **Gong ostrzegawczy** — trzy opadające, łagodnie wygasające tony.

Pliki WAV są krótkie (około 2,3–3,2 s), mono, PCM 16-bit / 22,05 kHz. Są generowane syntetycznie; nie zawierają nagrań ani dźwięków zewnętrznych. Dotychczasowe wybory i ich domyślne ustawienia pozostają bez zmian. Każdy typ alarmu nadal pamięta własny wybór, a istniejący przycisk podglądu odtwarza wybraną próbkę.

## Zastosowanie w kopii Windows

Po pobraniu zmian do checkoutu projektu uruchom z katalogu głównego repozytorium:

```powershell
.\android-sync\Apply-Alarm-Sound-Expansion.ps1
```

Jeśli polityka PowerShell blokuje skrypt, użyj:

```powershell
powershell -ExecutionPolicy Bypass -File .\android-sync\Apply-Alarm-Sound-Expansion.ps1
```

Skrypt sprawdza i stosuje tekstowy patch do `AlarmNotify.kt`, `AlarmCenter.kt` oraz `AlarmCenterTest.kt`, a następnie kopiuje cztery WAV-y do `android\app\src\main\res\raw`. Jest idempotentny: rozpoznaje już zastosowany patch i identyczne pliki; przy konflikcie zatrzymuje się zamiast nadpisywać różniący się plik. Wymaga wcześniejszego zastosowania istniejącego patcha Androida z selektorem alarmów (w tym `AlarmCenterTest.kt`).

Po zastosowaniu zbuduj aplikację i uruchom testy jednostkowe:

```powershell
cd .\android
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug
```

Powstaje pakiet Debug w `android\app\build\outputs\apk\debug\app-debug.apk` (`com.sterownikco.pro.debug`). Ten zestaw nie zmienia firmware, komunikacji z RTDB ani wyjść kotła. Nie wykonano testu alarmu na urządzeniu ani testu runtime; przed użyciem można odsłuchać próbkę z istniejącego podglądu w aplikacji, bez wywoływania alarmu kotła.

## Zakres plików

Commit dystrybucyjny zawiera wyłącznie patch tekstowy, skrypt i zasoby w `android-sync/`. Nie obejmuje ani nie nadpisuje pozostałych lokalnych, niezacommitowanych zmian Androida.
