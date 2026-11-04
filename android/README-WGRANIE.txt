Sterownik CO — aplikacja Android 0.31.77 (pełny projekt)
========================================================

Baza: 0.31.75 — dokładnie ta wersja, którą masz na telefonie.
Zmiana jest JEDNA: usunięte wykrywanie pobrań wody.

Co zniknęło z ekranu
--------------------
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
Jeśli chcesz obie zmiany razem, mówisz słowo i robię 0.31.78 =
0.31.76 (wąska szyna) + usunięte pobrania.

Wersja
------
versionCode 31077 / versionName "0.31.77" (app/build.gradle.kts)
Numer podbity, bo paczka różni się od 0.31.75 — dwie różne paczki
z tym samym numerem to dokładnie pomyłka z 0.31.53, której nie chcemy
powtarzać.

Budowanie
---------
APK buduje workflow .github/workflows/apk-android.yml na runnerze
GitHuba (Java 17 + SDK z obrazu), z katalogu android/ w repozytorium.
Gotowy plik: android-sync/apk/SterownikCO-0.31.77-debug.apk.
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
5. Ustawienia → numer wersji: 0.31.77.

Weryfikacja bez telefonu
------------------------
Brama statyczna (narzedzia/sprawdz_uklad.py): 58 OK / 0 BŁĄD.
Kontrola negatywna na 0.31.75: 8 BŁĄD (3 × numer wersji + 5 × pobrania).

Pliki zmienione (5)
-------------------
app/build.gradle.kts
app/src/main/java/com/sterownikco/pro/core/SolarAnalytics.kt
app/src/main/java/com/sterownikco/pro/ui/screens/Weather.kt
app/src/main/java/com/sterownikco/pro/ui/screens/sheets/Sensors.kt
README-WGRANIE.txt (ten plik)
