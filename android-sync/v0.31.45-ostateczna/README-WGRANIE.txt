Sterownik CO — wersja 0.31.45 (Android)
======================================
TA WERSJA MA WSZYSTKO: Twoje ustawienia z 0.31.12–0.31.32 ORAZ naszą pracę
0.31.33–0.31.44. Poprzednia paczka miała w środku nieaktualną instrukcję
(opis z 0.31.35) — to jest poprawione.

CO SIĘ STAŁO — W SKRÓCIE
------------------------
W repo na GitHubie drzewo Androida było cofnięte do 0.31.11. Wszystko, co
zrobiłeś między 0.31.12 a 0.31.32, istniało tylko w paczkach ZIP na Twoim
komputerze — nigdy nie trafiło do repo. Paczki 0.31.33–0.31.43 budowałem
z repo, więc stały na starym fundamencie. Stąd brak „Bufora offline"
i inny opis kafelka SERWO.

Komputer, na którym kompilujesz, nie ma z tym nic wspólnego.

POTWIERDZENIE Z TWOJEGO DYSKU GOOGLE
------------------------------------
Na Dysku leży plik STATUS-AKTUALNE.txt z 18.10.2026 (folder „Aktualne").
Jest w nim zapisane, że telefon chodził na 0.31.19, a GitHub — sprawdzony
wtedy na main i dwóch gałęziach — miał 0.31.11. To niezależny dowód,
spisany na długo przed tą rozmową.

ZNALEZIONE NA DYSKU (do wglądu, nie trzeba nic robić)
-----------------------------------------------------
Pełne projekty Androida: 0.31.12, 0.31.13, 0.31.14, 0.31.16, 0.31.17,
0.31.18. Wszystkie starsze od tego, z czego zbudowałem 0.31.45.
Snapshoty całości: SterownikCO-AI-STEROWANIE-2026-10-28.zip,
SterownikCO-AI-ANALIZA-2026-10-26.zip, Android-baza-przed-aktualizacja.
Szczegółowy spis z numerami ID: narzedzia/wykresy225/USTALENIA-DYSK-GOOGLE-2026-10-29.md

CO JEST W ŚRODKU (96 plików .kt)
--------------------------------
USTAWIENIA → „Bufor offline"
  przełącznik zapisu na flash centrali (spoolrt_wl / spoolrt_wyl) + liczniki
  od startu centrali. Stan czytany wyłącznie ze statusu centrali, więc
  przełącznik nie kłamie. Wymaga firmware v3.54.0+; na starszym jest
  nieaktywny z wyjaśnieniem.

KAFELEK „SERWO" W PROCENTACH
  „klapa 42%", w podpisie „syberek 17%", pigułka AUTO / RĘCZNY,
  po komendzie „CEL 65% · czekam" aż centrala potwierdzi nową pozycję,
  tryb BEZPIECZNY ma własny opis.

RESZTA PRZYWRÓCONEGO
  - dźwięki alarmów wybierane z aplikacji + synchronizacja z centralą,
  - terminal DLOG sam się zatrzymuje po 5 min bez dotykania ekranu,
  - dzienny cache historii i komunikaty o brakujących dniach,
  - naprawa czasu próbek po restarcie centrali,
  - 8 plików źródłowych, bez których tamte funkcje nie działały.

CZEGO CELOWO NIE MA
  - żadnych napisów, plakietek ani liczników nad wykresem lub w jego polu
    (etykiety luk, „luk: N", pasek statusu pobierania — usunięte na Twoje
    polecenie),
  - kafelków W CHMURZE / KOLEJKA / WYSYŁKA.

JAK ZBUDOWAĆ (PowerShell)
-------------------------
  cd "$env:USERPROFILE\Downloads\SterownikCO-ANDROID\SterownikCO-Android-v0.31.45"
  powershell -ExecutionPolicy Bypass -File .\zbuduj.ps1

zbuduj.ps1 sam znajduje Javę 17–22 i Android SDK, zapisuje local.properties,
buduje APK, kopiuje do Pobranych jako SterownikCO-v0.31.45-debug.apk
i wypisuje gotową komendę instalacji.

Instalacja kablem:
  & "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r "$env:USERPROFILE\Downloads\SterownikCO-v0.31.45-debug.apk"

Bez kabla: przenieś APK na telefon (Dysk / Telegram) i otwórz go.

versionCode = 31045 (wyższy od 31044), więc aktualizacja wejdzie bez
odinstalowywania i bez utraty ustawień.

PO CZYM POZNASZ, ŻE TO WŁAŚCIWA WERSJA
--------------------------------------
Ustawienia → między „Czujnik dymu" a „Data i czas" jest „Bufor offline".
Pulpit → kafelek SERWO pokazuje procenty, nie sam tryb.

CO SPRAWDZIŁEM PRZED WYDANIEM TEJ PACZKI
----------------------------------------
Bez kompilatora (w tym środowisku nie ma JDK ani Android SDK), ale:
  - 96 plików .kt, 287 symboli najwyższego poziomu — bez duplikatów,
  - wszystkie importy com.sterownikco.pro.* wskazują na istniejące symbole,
  - wszystkie odwołania R.string / R.drawable / R.raw mają zasób,
  - 19 plików XML parsuje się bez błędu, kodowanie UTF-8 czyste,
  - każde wywołanie Rtdb.* (26 funkcji) i telCache.* ma poprawną liczbę
    argumentów,
  - manifest: 11 uprawnień, wszystkie używane przez kod,
  - brak markerów konfliktu scalania.

ZNALEZIONY MARTWY KOD (nie błąd, nic nie psuje)
-----------------------------------------------
AppModel.kt:74 ma pole tcache (TelemetryDayCache), którego nikt nie używa —
zastąpił je nowszy cache z 0.31.43 (telCache). Zostawione świadomie:
usunięcie wymaga zmiany zachowania, a tego nie mogę tu przetestować.

UWAGA O BŁĘDZIE KOMPILACJI
--------------------------
Ta wersja składa dwa strumienie pracy, których nigdy wcześniej nie było
razem. Jeżeli Gradle zgłosi „Unresolved reference", prześlij mi dokładnie
tę linijkę z komunikatu (plik + numer wiersza) — poprawka jest wtedy
kwestią minut.
