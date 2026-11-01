Sterownik CO — wersja 0.31.42 (Android)
======================================
Zdjęte z nad wykresu wszystko, co użytkownik podkreślił na zrzucie 02:54.

CO ZDJAŁEM
----------
1) Pasek statusu nad wykresem — cały:
       „Załadowano 9944 próbek z RTDB (tylko chmura — archiwum centrali
        jest w domu) · odcinki symulowane są kreskowane”
   razem z niebieską plakietką „FIREBASE RTDB” po prawej.

2) Trzy kafelki pod nim:
       W CHMURZE · KOLEJKA · WYSYŁKA

Po pobraniu historii nad wykresem nie ma teraz ŻADNEGO tekstu.
Z góry na dół: tytuł „WYKRESY TELEMETRII”, karta DANE / ZAKRES,
potem od razu karta wykresu.

CO ZOSTAŁO
----------
Jedna cienka szara linijka, która pokazuje się WYŁĄCZNIE w czasie pobierania:
       „Pobieranie historii (7d) z bazy Firebase…”
i znika sama, gdy dane są gotowe (także wtedy, gdy sieć zawiedzie).
Bez niej nie byłoby widać, czy przycisk „Odśwież” w prawym górnym rogu
cokolwiek robi — 7 dni to kilka sekund ciszy.
Jeśli i tej linijki nie chcesz — powiedz, zdejmę ją jednym ruchem.

JAK PROGRAM POBRERA DANE (stan faktyczny w kodzie)
--------------------------------------------------
Nie pobiera „całej bazy” — pobiera DOKŁADNIE wybrane okno:
  6 H / 24 H / 7 DNI / 30 DNI  → tyle dni, ile widać na wykresie.
Dzieli to na doby i pobiera kawałkami po 300 próbek (stronicowanie po `ts`),
a po zebraniu całości sprawdza, czy nie została dziura większa niż 3 minuty,
i dopyta tylko ten kawałek.

ALE: nie ma pamięci podręcznej w telefonie. Każde wejście w wykresy,
każde przełączenie zakresu (np. 7 DNI → 30 DNI) i każde „Odśwież”
pobiera to okno od nowa. 9944 próbki na zrzucie to 7 dni × 1440 minut.
Próbki trzymane są tylko w pamięci aplikacji, po zamknięciu znikają.

Do 6 H i 24 H aplikacja pyta dodatkowo archiwum centrali w domu
(GET /api/history) i dokleja próbki, których nie ma w chmurze — to działa
tylko w domowej sieci. Z zewnątrz (np. przez LTE) ten krok po 6 sekundach
się poddaje i wykres pokazuje samą chmurę.

JAK ZBUDOWAĆ (PowerShell, w folderze projektu)
----------------------------------------------
  cd "$env:USERPROFILE\Downloads\SterownikCO-ANDROID\SterownikCO-Android-v0.31.42"
  Remove-Item Env:\JAVA_HOME -ErrorAction SilentlyContinue
  $env:JAVA_HOME = Split-Path (Split-Path (Get-Command java).Source -Parent) -Parent
  powershell -ExecutionPolicy Bypass -File .\zbuduj.ps1

Instalacja:
  & "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r "app\build\outputs\apk\debug\app-debug.apk"
  (albo plik APK na telefon przez Telegram/Dysk)

versionCode = 31042 (wyższy od 31041).
