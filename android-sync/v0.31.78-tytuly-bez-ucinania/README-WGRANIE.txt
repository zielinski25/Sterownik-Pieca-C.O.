Sterownik CO — aplikacja Android 0.31.78 (pełny projekt)
========================================================

Baza: 0.31.75 — dokładnie ta wersja, którą masz na telefonie.
Dwie zmiany: (1) usunięte wykrywanie pobrań wody [0.31.77],
(2) tytuły kart przestały być ucinane w połowie słowa [0.31.78].

Nowość 0.31.78 — ucięty tytuł „BILANS & ZYSK KOLEKTORA SŁON"
------------------------------------------------------------
Zgłoszenie ze zrzutu: tytuł karty kończył się na „...SŁON", tuż przy
przycisku „Eksportuj CSV". To nie było ucięcie ekranu — tak działał kod:
tytuł miał `maxLines = 1`, `softWrap = false` i `overflow = Clip`,
czyli obcinał tekst w połowie słowa BEZ wielokropka. Wyglądało to jak
awaria rysowania, a był to skrót bez ostrzeżenia.

Teraz (Weather.kt, WCard):
  • tytuł zawija się do 2 linii (`softWrap = true`, `maxLines = 2`),
  • gdy nadal się nie mieści — kończy się wielokropkiem „…", który widać,
  • podtytuł też używa wielokropka zamiast obcięcia (spójnie).

Nagłówek prognozy 7-dniowej też był ucięty
-------------------------------------------
Było: „📅 PROGNOZA POTENCJAŁU SOLARNEGO · 7 DNI" w jednej linii —
na karcie kończyło się na „SOLARNEGO", a dalsza część („· 7 DNI") nie
była widoczna wcale. Teraz:
    📅 PROGNOZA POTENCJAŁU SOLARNEGO
    7 DNI · potencjał całej doby, nie od teraz · minus = woda się wychładza
    szacunek z modelu cieplnego, nie pomiar
Czyli liczba dni jest widoczna, a podpis mówi wprost, że to NIE pomiar
(wcześniej było samo „szacunek").

Poprawiona nota w Czujnikach
----------------------------
Było: „Zysk = o ile °C ogrzała się woda w zbiorniku…" (mieszało różnicę
z wartością temperatury) i zawiłe „z chmury, co minutę, także gdy
aplikacja była zamknięta". Teraz: „Zysk = różnica temperatur wody […]:
ostatni odczyt minus pierwszy z dzisiaj", potem osobno, co znaczy
„Zmierzone", a co „Szacunek", i dlaczego nie ma liczby pobrań.

Co zniknęło z ekranu (0.31.77)
------------------------------
1. Kafelek „🚰 POBRANIA DZIŚ" w karcie BILANS & ZYSK KOLEKTORA SŁONECZNEGO
   (zakładka POGODA). W tym miejscu są teraz 3 karty zamiast 4:
     • SPADEK/ZYSK DZIŚ (zmierzony),
     • PANEL − POWIETRZE,
     • DO KOŃCA DNIA (prognoza).
2. Wiersz „Pobrania ciepłej wody" w arkuszu Czujniki.
3. Zdanie w nocie wyjaśniającej, że „pobranie liczy się jako spadek
   o co najmniej 0,5 °C utrzymany ponad 5 minut".

Dlaczego — z liczbami
---------------------
Nie ma sygnału zaworu w telemetrii, więc aplikacja ZGADYWAŁA pobranie
ze spadku temperatury wody. 04.11.2026 o 22:46 pokazała:
    5 pobrań · −23,3 °C
a cała woda w zbiorniku straciła tego dnia tylko −15,8 °C.
Woda traci ciepło wyłącznie przez pobrania i wychładzanie, więc suma
ochłodzeń od pobrań NIE MOŻE przekroczyć spadku doby. 23,3 > 15,8,
czyli co najmniej 7,5 °C było policzone podwójnie — zwykłe nocne
wychładzanie liczone jako pobrania.

Kontrola energetyczna: 100 l × 23,3 °C = 2,7 kWh, a szczyt słońca tego
dnia to 273 W/m² (kolektor dał najwyżej ~1 kWh). Nie da się odebrać
2,7 kWh ze zbiornika, do którego wpłynęło 1 kWh.

Analiza i mapa kodu: narzedzia/uklad-2026-11-01/POBRANIA-usuniecie-analiza.md

Co usunięto z kodu (wariant A — „usunąć całkiem")
-------------------------------------------------
  core/SolarAnalytics.kt:
    - recomputeDraws() — heurystyka szukająca spadków (0,5 °C / 5 min,
      scalanie epizodów do 20 min, odrzucanie skoków > 12 °C),
    - rebuildDrawEvents(), waterDrawEvents, DrawEvent,
    - stałe DRAW_STEP_C, DRAW_WINDOW_MS, DRAW_HOLD_MS, DRAW_MERGE_MS,
      DRAW_FAULT_C,
    - pola todayDrawCount, todayDrawDrop, drawCount, accumulatedDrawDrop,
    - liczenie pobrań w recomputeToday() i w updateUi().
  ui/screens/Weather.kt — kafelek + komentarz z uzasadnieniem.
  ui/screens/sheets/Sensors.kt — wiersz + poprawiona nota.
Razem ~70 linii mniej, żadnej nowej funkcji.

Czego to NIE zmienia
--------------------
  • „SPADEK/ZYSK DZIŚ" i karta DZIŚ w prognozie 7-dniowej — liczone
    z różnicy temperatur (todayGain), nie z pobrań. Zostają bez zmian.
  • Prognoza solarna, „DO KOŃCA DNIA", łuk słoneczny, 7 dni,
  • wykresy, SERWO, historia, kolory, kolejność pozycji, „LIVE", liczniki,
  • kafelki pogody i pobieranie z Open-Meteo (co 15 min),
  • firmware — BEZ ZMIAN, piec zostaje na 3.57.9.
  • Eksport CSV: kolumny `pobor_wody` i `spadek_poboru_C` ZOSTAJĄ
    (zapisują 0), żeby stare pliki i czytniki się nie rozjechały.

Czego w tym wydaniu NIE ma (ważne)
----------------------------------
To jest 0.31.75 + usunięte pobrania. NIE ma w nim zmiany szyny
nawigacji z 0.31.76 (56 dp, same ikony w poziomie, dymki po
przytrzymaniu). W poziomie działa więc szyna 68 dp z podpisami —
dokładnie jak na Twoim telefonie teraz.
Jeśli chcesz obie zmiany razem, mówisz słowo i robię kolejne wydanie =
0.31.76 (wąska szyna 56 dp) + bez pobrań + te tytuły.

Wersja
------
versionCode 31078 / versionName "0.31.78" (app/build.gradle.kts)
Numer podbity, bo paczka różni się od 0.31.75 — dwie różne paczki
z tym samym numerem to dokładnie pomyłka z 0.31.53, której nie chcemy
powtarzać. 0.31.77 różni się od 0.31.78 tylko tytułami i notą.

Budowanie
---------
APK buduje workflow .github/workflows/apk-android.yml na runnerze
GitHuba (Java 17 + SDK z obrazu), z katalogu android/ w repozytorium.
Gotowy plik: android-sync/apk/SterownikCO-0.31.78-debug.apk.
Lokalnie (gdy brak sdk.dir): najpierw utwórz local.properties,
    "sdk.dir=C:/Users/<Ty>/AppData/Local/Android/Sdk" | Set-Content local.properties
dopiero potem:  .\gradlew.bat assembleRelease
UWAGA: APK z runnera jest podpisany kluczem debug — przed instalacją
odinstaluj starą wersję (dane w Firebase zostają).

Co sprawdzić na telefonie
-------------------------
1. POGODA → karta BILANS: nie ma kafelka POBRANIA DZIŚ, są 3 karty.
2. Czujniki → nie ma wiersza „Pobrania ciepłej wody".
3. „SPADEK/ZYSK DZIŚ" pokazuje tę samą liczbę co wcześniej (to pomiar).
4. Prognoza 7 dni, wykresy, SERWO — bez zmian.
5. Tytuł karty BILANS mieści się w całości (albo kończy „…", nigdy w połowie słowa).
6. Nagłówek „PROGNOZA POTENCJAŁU SOLARNEGO" + osobna linia „7 DNI · …".
7. Ustawienia → numer wersji: 0.31.78.

Weryfikacja bez telefonu
------------------------
Brama statyczna (narzedzia/sprawdz_uklad.py): 63 OK / 0 BŁĄD.
Kontrola negatywna na 0.31.77: 8 BŁĄD (3 × numer wersji + 5 × ucinanie
tytułów / nota). Na 0.31.75: 13 BŁĄD.

Pliki zmienione (6)
-------------------
app/build.gradle.kts
app/src/main/java/com/sterownikco/pro/core/SolarAnalytics.kt
app/src/main/java/com/sterownikco/pro/ui/screens/Weather.kt
app/src/main/java/com/sterownikco/pro/ui/screens/sheets/Sensors.kt
README-WGRANIE.txt (ten plik)
