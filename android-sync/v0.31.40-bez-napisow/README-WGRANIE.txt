Sterownik CO — wersja 0.31.40 (Android)
======================================
Sprzątanie wykresu: zdjęte napisy i tło luk, które dodałem w 0.31.39.

CO BYŁO ŹLE (zrzuty 2026-10-29, 02:26)
--------------------------------------
Przy zakresie 24 H z oknem czterech dni na wykresie stało SZEŚĆ napisów
„luka ... min” i nachodziły na siebie:

    luka 1 h 1 min        ← na siebie
    luka 11 min
    luka 8 h 8 min        ← na siebie
    luka 1 h 17 min

Zasłaniały krzywe i nie wnosiły nic nowego, bo pasek statusu nad wykresem
podaje to samo, tylko dokładniej:

    „luk: 6 (najdłuższa 1 h 17 min)”

CO JEST W 0.31.40
-----------------
- Zdjete napisy „luka ... min” z pola wykresu.
- Zdjęte delikatne tło luki (dodane w 0.31.39) — było zbędne.
- Zostają DWIE pionowe przerywane kreski na granicach przerwy. Tyle wystarczy,
  żeby zobaczyć, gdzie danych brakuje, i nie zasłaniają niczego.
- Usunięty nieużywany import `androidx.compose.ui.geometry.Size`.

Informacja o lukach nie znika z aplikacji — jest w pasku statusu:
liczba luk i najdłuższa z nich, w minutach lub godzinach.

JAK ZBUDOWAĆ (PowerShell, w folderze projektu)
----------------------------------------------
  cd "$env:USERPROFILE\Downloads\SterownikCO-ANDROID\SterownikCO-Android-v0.31.40"
  Remove-Item Env:\JAVA_HOME -ErrorAction SilentlyContinue
  $env:JAVA_HOME = Split-Path (Split-Path (Get-Command java).Source -Parent) -Parent
  powershell -ExecutionPolicy Bypass -File .\zbuduj.ps1

Instalacja:
  & "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r "app\build\outputs\apk\debug\app-debug.apk"
  (albo plik APK na telefon przez Telegram/Dysk)

versionCode = 31040 (wyższy od 31039).
