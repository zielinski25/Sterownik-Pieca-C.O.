Sterownik CO — aplikacja Android 0.31.79 (pełny projekt)
========================================================

Baza: 0.31.75 (ta wersja jest na telefonie) + poprawki 0.31.77, 0.31.78, 0.31.79.

SPROSTOWANIE MOICH BŁĘDÓW Z 0.31.78
===================================
1) Nagłówek „PROGNOZA POTENCJAŁU SOLARNEGO ..." NIE BYŁ UCIĘTY.
   W 0.31.78 podzieliłem go na trzy linie i napisałem w README, że był
   ucinany. To nieprawda: na zrzucie z 23:57 widać go w całości
   („PROGNOZA POTENCJAŁU SOLARNEGO NA 7 DNI", karta ~308 dp, tekst ~266 dp).
   Była to zmiana WYGLĄDU, nie naprawa błędu. W 0.31.79 nagłówek wraca
   dokładnie do postaci z 0.31.75/0.31.77, jedno-linijkowy.

2) Zdanie w README 0.31.78 „wcześniej było samo »szacunek«, bez »nie pomiar«"
   było FAŁSZYWE. Stary podpis brzmiał:
       potencjał całej doby, nie od teraz · minus = woda się wychładza · szacunek, nie pomiar
   czyli „nie pomiar" było tam od początku. Poprawione tu i w komentarzach
   w kodzie. Za wprowadzenie w błąd przepraszam.

3) Bramka sprawdzała mój własny nowy napis („szacunek z modelu cieplnego,
   nie pomiar"), co dowodziło tylko tego, że tekst jest nowy. Zamienione na
   sprawdzenia FAKTU: nagłówek musi zawierać liczbę dni, a podpis musi
   zawierać słowa „nie pomiar" w dowolnym sformułowaniu (regex).

PRAWDZIWY BŁĄD NAPRAWIONY W 0.31.79 — „DO KOŃCA DNIA"
=====================================================
Objaw (zrzuty 23:57): kafelek pokazywał „+0,1 °C" z podpisem
„bez słońca · woda stygnie do północy", a arkusz Czujniki w tym samym
wierszu nie pokazywał ŻADNEJ liczby — tylko „bez słońca do końca dnia".
Karta 7-dniowa „DZIŚ" liczyła −18,0 + 0,1 = −17,9.

Dwie przyczyny, obie w kodzie:
  a) wzór na „resztę doby" był przepisany DWA razy: w
     SolarAnalytics.dailyForecast7 i w Weather.endOfDayGain.
     Nic nie gwarantowało, że dadzą tę samą liczbę;
  b) podpis karty po zachodzie był zaszyty na sztywno: „woda stygnie
     do północy" — bez patrzenia na znak. Tego dnia woda miała 0,5 °C
     przy powietrzu 6,5 °C, więc model liczył +0,1 °C (ocieplanie do
     temperatury powietrza). Wartość była dobra, KŁAMAŁ PODPIS.

Naprawa:
  • SolarAnalytics.restOfDayDeltaC() — jedno źródło prawdy. Używają go:
      - kafelek „DO KOŃCA DNIA" (wartość),
      - podpis karty (opis słowny),
      - pierwszy dzień prognozy 7-dniowej („DZIŚ" = zmierzone + ta liczba),
      - wiersz „Do końca dnia · szacunek" w arkuszu Czujniki.
  • Arkusz pokazuje teraz TĘ SAMĄ liczbę co kafelek, np.
      „+0,1°C · bez słońca · woda się jeszcze ociepla do północy".
  • Opis zależy od znaku, nie od godziny:
      d < 0   → „woda stygnie do północy"
      d > 0   → „woda się jeszcze ociepla do północy"
      d ≈ 0   → „bez zmian do północy"
  • Karta „DZIŚ" zostaje −17,9 °C (= −18,0 + 0,1) — to jest poprawne,
    bo obie liczby pochodzą teraz z tej samej funkcji. Nic nie zmieniam
    w danych, tylko usuwam możliwość rozjechania się i kłamliwy podpis.

Czego w tym wydaniu NIE ma
--------------------------
WĄSKIEJ SZYNY 56 dp z 0.31.76 (same ikony w poziomie, dymki po
przytrzymaniu). Nie ma jej, bo nie potwierdziłeś, że ma wejść razem
z tą poprawką. W poziomie działa szyna 68 dp z podpisami — tak jak na
Twoim telefonie. Jeśli chcesz: mówisz słowo i robię kolejne wydanie
= 0.31.79 + wąska szyna.

Co jest dalej w środku (z wcześniejszych wydań)
-----------------------------------------------
  • 0.31.77 — wykrywanie pobrań wody USUNIĘTE (kafelek, wiersz w Czujnikach,
    heurystyka w kodzie). Powód: brak sygnału zaworu, a 04.11 licznik
    pokazał −23,3 °C przy całkowitym spadku doby −15,8 °C. Kolumny
    `pobor_wody`/`spadek_poboru_C` w CSV zostają (zapisują 0).
  • 0.31.78 — tytuł karty („BILANS & ZYSK KOLEKTORA SŁONECZNEGO") nie jest
    już obcinany w połowie słowa: `maxLines=1 + softWrap=false + Clip`
    zamienione na zawijanie do 2 linii z wielokropkiem. To była prawdziwa
    usterka (widać ją na zrzucie: „...KOLEKTORA SŁON").
  • Nota w Czujnikach: „Zysk = różnica temperatur wody […]: ostatni odczyt
    minus pierwszy z dzisiaj" (wcześniej „o ile °C ogrzała się woda").

Bez zmian
---------
Wykresy, SERWO, historia, kolory, kolejność pozycji, „LIVE", liczniki,
kafelek pogody, pobieranie z Open-Meteo (co 15 min), prognoza solarna,
łuk słoneczny. FIRMWARE BEZ ZMIAN: piec zostaje na 3.57.9.

Wersja
------
versionCode 31079 / versionName "0.31.79" (app/build.gradle.kts)

Budowanie
---------
APK buduje workflow .github/workflows/apk-android.yml na runnerze GitHuba
(Java 17 + SDK z obrazu) z katalogu android/ w repozytorium.
Gotowy plik: android-sync/apk/SterownikCO-0.31.79-debug.apk
UWAGA: podpisany kluczem debug runnera — przed instalacją odinstaluj
starą wersję (dane w Firebase zostają, ustawienia lokalne znikną).

Co sprawdzić na telefonie
-------------------------
1. POGODA → karta BILANS: 3 kafelki, brak „POBRANIA DZIŚ".
2. Tytuł karty mieści się w całości albo kończy „…" — nigdy w połowie słowa.
3. Nagłówek prognozy: „PROGNOZA POTENCJAŁU SOLARNEGO NA 7 DNI" w jednej linii.
4. Kafelek „DO KOŃCA DNIA" i arkusz Czujniki → wiersz „Do końca dnia ·
   szacunek" pokazują TĘ SAMĄ liczbę.
5. Podpis kafelka zgadza się ze znakiem: przy dodatniej wartości nie pisze
   „stygnie".
6. Karta „DZIŚ" = „SPADEK/ZYSK DZIŚ" + wartość z punktu 4.
7. Ustawienia → numer wersji: 0.31.79.

Weryfikacja bez telefonu
------------------------
Brama statyczna (narzedzia/sprawdz_uklad.py): 70 OK / 0 BŁĄD.
Kontrola negatywna: 0.31.78 → 9 BŁĄD, 0.31.75 → 17 BŁĄD.

Pliki zmienione względem 0.31.78 (4)
------------------------------------
app/build.gradle.kts
app/src/main/java/com/sterownikco/pro/core/SolarAnalytics.kt
app/src/main/java/com/sterownikco/pro/ui/screens/Weather.kt
app/src/main/java/com/sterownikco/pro/ui/screens/sheets/Sensors.kt
