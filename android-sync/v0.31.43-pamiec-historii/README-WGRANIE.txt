Sterownik CO — wersja 0.31.43 (Android)
======================================
APLIKACJA MA TERAZ PAMIĘĆ HISTORII i dociąga z chmury TYLKO to, czego nie ma.

CO BYŁO (do 0.31.42)
--------------------
Telefon nie miał żadnej pamięci historii. Każde wejście w „Wykresy",
każde przełączenie zakresu i każde „Odśwież" ściągało z Firebase całe
wybrane okno od nowa:
    7 DNI  → 9944 próbek
    30 DNI → ~43 000 próbek
Próbki żyły tylko w pamięci aplikacji — po jej zamknięciu przepadały.

CO JEST W 0.31.43
-----------------
1) Historia zapisuje się na dysku telefonu:
       /data/data/com.sterownikco.pro/files/telemetria/historia.tsv.gz
   Format: gzip, jeden wiersz = jedna próbka (13 kanałów + pompa + bitmapa
   symulacji). 30 dni to ~1 MB. Trzymamy 35 dni, starsze kasujemy.

2) Przy każdym wejściu w wykresy aplikacja liczy, CZEGO BRAKUJE w wybranym
   oknie, i pyta Firebase tylko o te przedziały. Zwykle jest to jeden ogon:
   „od ostatniej zapamiętanej próbki do teraz".

3) Luki, o które aplikacja już pytała i chmura ich nie ma (np. 24.10
   10:03–10:14 albo 12:26–13:43), zapisuje jako „potwierdzone puste"
   w pliku luki.tsv. Dzięki temu nie pyta o nie w kółko przy każdym wejściu.

4) Bez zasięgu wykres i tak się rysuje — z pamięci telefonu.

EFEKT W PRAKTYCE
----------------
- Pierwsze wejście po instalacji: jak dawniej, pobiera całe wybrane okno.
- Drugie wejście w 7 DNI (choćby po restarcie telefonu): z pamięci, dochodzi
  tylko ostatni kawałek — kilka próbek zamiast 9944.
- Otwieranie wykresów częściej niż co 6 minut: ZERO zapytań do Firebase.
- Przełączenie 7 DNI → 30 DNI: dociąga tylko brakujące 23 dni, nie 43 000
  próbek od nowa.

USTAWIENIA / OGRANICZENIA
-------------------------
- Pamięć jest per telefon. Po odinstalowaniu aplikacji znika.
- Nie zapisujemy „żywych" odczytów z paska statusu — tylko historię z chmury.
- Zapis jest atomowy (najpierw plik tymczasowy, potem podmiana), więc
  zabicie aplikacji w trakcie zapisu nie psuje pamięci.
- Plik pamięci rośnie do ~1 MB na 35 dni. Można go usunąć w każdej chwili —
  aplikacja po prostu pobierze historię od nowa.

CO ZMIENIONO W KODZIE
---------------------
  NOWY  core/TelemetryCache.kt   — pamięć na dysku + liczenie braków
  core/AppModel.kt               — loadRange: pamięć → brakujące → chmura
  core/RtdbClient.kt             — fetchWindow(start, end): pobiera dokładny
                                   przedział, nie całe okno
  app/build.gradle.kts           — versionCode 31043 / 0.31.43

JAK ZBUDOWAĆ (PowerShell, w folderze projektu)
----------------------------------------------
  cd "$env:USERPROFILE\Downloads\SterownikCO-ANDROID\SterownikCO-Android-v0.31.43"
  Remove-Item Env:\JAVA_HOME -ErrorAction SilentlyContinue
  $env:JAVA_HOME = Split-Path (Split-Path (Get-Command java).Source -Parent) -Parent
  powershell -ExecutionPolicy Bypass -File .\zbuduj.ps1

Instalacja:
  & "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r "app\build\outputs\apk\debug\app-debug.apk"
  (albo plik APK na telefon przez Telegram/Dysk)

versionCode = 31043 (wyższy od 31042).
