Sterownik CO — aplikacja Android v0.31.35 „DZIURY, POMPA I SOLAR Z CHMURY"
======================================================================

CO TA WERSJA ZMIENIA — dwa tematy naraz
---------------------------------------
A. WYKRESY (to samo co w 0.31.34)
 1. Pompa CO nie rysuje już pionowych zielonych „ścian" na całą wysokość.
    W trybie mieszanym zajmuje niski pasek przy dolnej krawędzi wykresu.
    W trybie „tylko pompa" — bez zmian.
 2. Przerwa w danych jest widoczna i PODPISANA: dwie przerywane pionowe
    kreski + napis „luka 1 h 12 min" nad przerwą.
 3. Podpis pod wykresem mówi, ile jest przerw i jak długa jest najdłuższa.
 4. Pobieranie historii odporniejsze na błędy sieci:
    - nieudana strona nie kasuje już całego dnia,
    - po pobraniu aplikacja dopytuje każdą lukę dłuższą niż 3 minuty.

B. PANEL SŁONECZNY (nowość w 0.31.35)
 5. „Zysk dziś · zmierzone" liczy się teraz z HISTORII CHMURY (co minutę),
    a nie tylko z próbek zebranych przy otwartej aplikacji. Wcześniej karta
    pokazywała „brak próbek z dziś" wieczorem, mimo że chmura ma całą dobę.
 6. To samo dotyczy „Pobrań ciepłej wody" — też liczone z historii.
 7. Metoda liczenia BEZ ZMIAN: zysk = ostatnia minus pierwsza próbka doby,
    pobranie wody = spadek szybszy niż 0,3 °C/min.

CZEGO TA WERSJA NIE ROBI
------------------------
- Nie zmienia progu 6 minut ani samego rysowania poza opisanym wyżej.
- Nie zmienia modelu prognozy (karty DZIŚ / JUTRO / 7 dni) — ten sam wzór.
- Nie rusza archiwum centrali (/api/history), plików hist_short/hist_long,
  firmware'u ani ustawień.

JAK WGRAC (4 kroki)
-------------------
1. Rozpakuj ZIP. Jest w nim 10 plików .kt. Zmienione w tej wersji:
   AppModel.kt, ChartEngine.kt, RtdbClient.kt, SolarAnalytics.kt, Sensors.kt.
   Pozostałe 5 są identyczne z v0.31.33 (dołączone, żeby nic nie trzeba było
   szukać).
2. Podmień w projekcie pliki o tych samych nazwach. UWAGA: Sensors.kt leży
   w katalogu ui/screens/sheets/, SolarAnalytics.kt w core/, reszta w core/
   i ui/chart/ — użyj nazw, Android Studio pokaże ścieżkę.
3. Zwiększ versionName na "0.31.35" i versionCode o 1.
4. Build > Generate Signed Bundle / APK > APK, podpisz tym samym kluczem,
   zainstaluj na telefonie (nadinstalowanie, dane zostają).

JAK SPRAWDZIĆ, ŻE DZIAŁA
------------------------
Wykresy:
- Wykres „TEMPERATURY + POMPA C.O.", 7 DNI, okolice 24.10 godz. 12–14 —
  przerwa z napisem „luka … min" (ta przerwa jest prawdziwa, w chmurze
  nie ma tych minut).
- Noc 24/25.10 — wielogodzinna dziura powinna zniknąć.
- Pompa: przy dolnej krawędzi cienki pasek zamiast zielonych słupów.

Panel słoneczny:
- Otwórz kafelek „Panel słoneczny (solary)".
- „Zysk dziś · zmierzone" powinno pokazać LICZBĘ, nie „brak próbek z dziś".
  Wzorzec z chmury na 28.10 (do 21:07): zbiornik 36,8 → 21,0 °C,
  czyli „Spadek dziś · zmierzone −15,8 °C" (woda stygła, piec nie grzał).
  Dla porównania 27.10 dałoby +6,0 °C.
- „Pobrania ciepłej wody" — jeśli w dniu były spadki szybsze niż 0,3 °C/min,
  pojawi się liczba zamiast „brak".

JEŚLI COŚ NIE ZADZIAŁA
----------------------
Pliki z katalogu v0.31.33-os-czasu-i-archiwum-esp są nietknięte — można wrócić.
Plik ZMIANY-0.31.34-do-0.31.35.diff pokazuje linia po linii, co doszło
w tej wersji (względem 0.31.34).
SHA256SUMS.txt — sumy kontrolne plików .kt z tego ZIP-a.
