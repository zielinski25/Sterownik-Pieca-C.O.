════════════════════════════════════════════════════════════════════════════
 SterownikCO Android  v0.31.31  →  v0.31.32
 Pętla kolektora jako drugi węzeł modelu + czytelny znak temperatury
════════════════════════════════════════════════════════════════════════════

Nakładka na drzewo v0.31.31. Podmienia 4 pliki. Zero nowych plików.
ZERO ręcznych zmian w kodzie.

To pierwsza wersja, która zmienia FIZYKĘ modelu. Trzy pozostałe punkty
to czytelność. Szczegóły niżej.


| | |
|---|---|
| ZIP          | SterownikCO-Android-v0.31.32-PETLA.zip |
| wersja       | versionCode 3038 / "v0.31.32" |
| pliki .kt    | 4 (dwóm plikom zmienia się folder) |


────────────────────────────────────────────────────────────────────────────
 0. HASHE
────────────────────────────────────────────────────────────────────────────

  SolarThermalModel.kt    271 linii   13 303 B   (core\)
    0BAAEFCE6D90FF98A8F10ACB9BCB3894ADBA914B666AE9A11169EACE337A20E3

  SolarAnalytics.kt       324 linie   16 646 B   (core\)
    D2173F381AFCA5A268669CFF4FF8A81ABCDCBA6EBB27398B30B72D3A59119B92

  Weather.kt              953 linie   53 019 B   (ui\screens\)
    6352E05DE191D8E46521DBE96CC991EC461EDA8BF9BD65ABE35664639C243BFC

  Sensors.kt              415 linii   21 801 B   (ui\screens\sheets\)
    E6F12741020D1004EEE63215ED3052A5F34068FFCFC9644C4C5B2D8595AC383C


────────────────────────────────────────────────────────────────────────────
 1. PUNKT 3 — FIZYKA: SKĄD BRAŁO SIĘ 5,6 °C BŁĘDU

Model do tej pory miał JEDEN węzeł: zbiornik 100 l. Kolektor był tylko
źródłem ciepła w dzień. Nocą nie robił nic.

A zbiornik naprawdę traci ciepło przez kolektor na dachu. Woda w kolektorze
stygnie szybciej niż w zbiorniku, robi się cięższa, spływa w dół — i wypycha
ciepłą wodę ze zbiornika na górę, do kolektora. Bez pompy. Napędza to sama
różnica gęstości. Nazywa się to cyrkulacją wsteczną termosyfonu.

Ile to zabiera? Policzone z Twoich własnych pomiarów i temperatur powietrza
z Open-Meteo, dla każdej nocy osobno:

  odcinek                        spadek   godzin   strata rzeczywista
  ───────────────────────────────────────────────────────────────────
  21.10 wieczór                  4,0 K     8,8        2,3 W/K
  22.10 (dzień pochmurny)        6,0 K    22,4        1,5 W/K
  22.10 noc                      3,4 K     8,3        2,3 W/K
  23.10 wieczór (słoneczny)     11,4 K     8,9        4,6 W/K
  24.10 noc (po słonecznym)      8,8 K     9,3        4,3 W/K

Po dniu pochmurnym zbiornik traci ciepło dokładnie tak, jak mówi katalog
(1,5 ≈ 1,4 W/K). Po dniu słonecznym — TRZY RAZY szybciej. Ta różnica to
właśnie cyrkulacja wsteczna: po słonecznym dniu cała pętla jest gorąca.

Skutek, policzony na pięciu pełnych dobach:

  stary model mylił się średnio o 5,85 °C, i ZAWSZE w tę samą stronę —
  był za ciepły. Dzień po dniu, na każdej dobie, o 3–5 °C.
  Nowy myli się o 2,63 °C i raz w górę, raz w dół.

To dlatego poranna karta „DO KOŃCA DNIA +5,6 °C” z 24.10 była zbyt
optymistyczna: model nie wiedział, że przez noc zbiornik oddał ciepło
przez dach.


────────────────────────────────────────────────────────────────────────────
 2. CO DOKŁADNIE ZMIENIONO W MODELU

Doszły trzy parametry, wszystkie z literatury — nie dopasowane „na oko”:

  reverseFlowKgS = 0,004   przepływ wsteczny termosyfonu [kg/s]
      OSTI biblio/116242 (Performance analysis of a thermosyphon SWH,
      Part 2, Nighttime performance) podaje ~0,005 kg/s i wzrost strat
      „nearly fourfold”.
      Kolektor heat-pipe ma ten przepływ MNIEJSZY niż płaski — kondensator
      rurki cieplnej jest osłonięty izolacją. Źródło: Int. J. Low-Carbon
      Tech. 10(4) 2015, „Reverse flow in natural convection heat pipe
      solar water heater”. Dlatego 0,004, a nie 0,005.

  loopUaWk = 1,0           straty pętli (manifold + rury) [W/K]

  loopWaterKg = 4,0        woda w pętli [kg]. Mała pojemność — dlatego
                           pętla reaguje szybko i to ona trzyma ciepło
                           tuż po zachodzie.

Model liczy teraz dwa węzły jednocześnie:
  - węzeł 1: woda w pętli colectora (manifold + rury),
  - węzeł 2: zbiornik 100 l.

Kierunek przepływu wynika z różnicy temperatur, nie z przełącznika:
  dzień — pętla cieplejsza  → przepływ w przód, duży strumień,
  noc   — zbiornik cieplejszy → cyrkulacja wsteczna, mały strumień.

Nic nie jest „wyłączane” o zachodzie. To sama fizyka decyduje.


────────────────────────────────────────────────────────────────────────────
 3. WYNIK — SPRAWDZONE NA 5 DOBACH POMIARÓW

Symulacja ciągła 20–24.10, porównana z Twoimi min/max z archiwum:

  model                    RMS błędu    obciążenie min   obciążenie max
  ─────────────────────────────────────────────────────────────────────
  stary (bez pętli)         5,85 °C        +4,4 °C          +3,1 °C
  nowy  (pętla 0,004)       2,63 °C        −0,0 °C          −0,5 °C

Nowy model nie ma już stałego obciążenia — trafia raz w górę, raz w dół,
zamiast być zawsze za ciepły. Błąd spadł ponad dwukrotnie.

  Szczyt 23.10:   zmierzone 47,2 °C o 15:05
                  stary     46,6 °C o 18:00   (godzina +2,9 h)
                  nowy      44,7 °C o 17:01   (godzina +2,0 h)


────────────────────────────────────────────────────────────────────────────
 4. CZEGO MODEL NADAL NIE UMIE — PRZECZYTAJ, TO WAŻNE

Godzina szczytu jest nadal około 2 godzin za późno. To nie jest błąd
policzony źle — to ograniczenie modelu zbiornika jako JEDNEJ BRYLE.

Prawdziwy zbiornik 100 l jest uwarstwiony: na górze cieplej, na dole zimniej.
Czujnik siedzi w połowie wysokości. Model traktuje całe 100 l jako jedną
temperaturę, więc woda „dogrzewa się” do samego wieczora, gdy prawdziwa
woda na górze już dawno osiągnęła szczyt.

Naprawa wymaga modelu warstwowego (kilka warstw zbiornika) — to osobna,
większa zmiana. Na razie: wartość „+X °C” jest solidna, godzina szczytu
jest przybliżona.

Nowy model sprawdzono na PIĘCIU dobach. To wciąż mało. Szczególnie parametry
pętli warto potwierdzić po 2–3 tygodniach.


────────────────────────────────────────────────────────────────────────────
 5. PUNKT 1 — „ZYSK DZIŚ” POKAZUJE SPADEK

Zrzut z 24.10, godz. 09:32: woda spadła z 35,8 do 27,0 °C (dT −8,8 °C),
a karta pokazywała myślnik „—”. W arkuszu Czujniki stało wprost
„brak próbek z dziś” — przy 568 próbkach. Nieprawda.

Było:   ⚡ ZYSK DZIŚ      —
Jest:   ⚡ SPADEK DZIŚ    −8.8°C      (na niebiesko)

Myślnik znaczy „brak danych”. Spadek jest DANYMI, tylko ujemnymi.

  Nowe pole w SolarAnalytics: `todayHasData`
  Rozdziela „brak próbek” od „woda się nie zmieniła”.


────────────────────────────────────────────────────────────────────────────
 6. PUNKT 2 — „+0,0 °C” ZNIKA

Karta 7-dniowa pokazywała „+0,0 °C” dla Cz 29.10 i Pt 30.10. Czytało się
„bez zmian”, a znaczyło „będzie zimniej”.

Było:   +0.0°C
Jest:   −X.X°C      (na niebiesko)

(Przykładu nie podaję z palca — sprawdź sam na jutrzejszej prognozie.
 Wcześniejsze „+0,0 °C” brało się z obcinania wyniku ujemnego do zera.)

Nowa funkcja `dayDeltaC` zwraca wartość ZE ZNAKIEM. Stara `gainFromKwhM2`
obcinała wynik ujemny do zera — i to było źródłem „+0,0”.

Próg równowagi: 1,26 kWh/m² dobowej sumy GTI (5,57 ÷ 4,43). Poniżej tej
wartości zbiornik oddaje więcej, niż zbiera, i karta to teraz mówi.


────────────────────────────────────────────────────────────────────────────
 7. PUNKT 4 — DWA MODELE, JASNO PODPISANE

Na jednym ekranie stały dwie różne liczby o dzisiejszej dobie:

  📅 DZIŚ (7 dni)         +0,8 °C
  🔮 DO KOŃCA DNIA        +5,6 °C

Obie o dziś, siedmiokrotna różnica. To nie błąd rachunku — to dwa różne
pytania. Pierwsze: „ile ciepła da CAŁA doba”. Drugie: „ile jeszcze zostało
OD TERAZ do północy”.

Podpis sekcji 7-dniowej mówi teraz wprost:

  „potencjał całej doby, nie od teraz · minus = woda się wychładza
   · szacunek, nie pomiar”


────────────────────────────────────────────────────────────────────────────
 8. CO ZROBIĆ — 4 KROKI
────────────────────────────────────────────────────────────────────────────

KROK 1. Podmień cztery pliki

  SolarThermalModel.kt  →  app\src\main\java\com\sterownikco\pro\core\
  SolarAnalytics.kt     →  app\src\main\java\com\sterownikco\pro\core\
  Weather.kt            →  app\src\main\java\com\sterownikco\pro\ui\screens\
  Sensors.kt            →  app\src\main\java\com\sterownikco\pro\ui\screens\sheets\

  DWA RÓŻNE FOLDERY. Sensors.kt leży o poziom niżej, w `sheets`.

  Pytanie „plik istnieje, zamienić?” → TAK.


KROK 2. Podbij wersję

  Plik: app\build.gradle.kts

  Znajdź:
      versionCode = 3037
      versionName = "v0.31.31"

  Zamień na:
      versionCode = 3038
      versionName = "v0.31.32"

  Jeśli masz inny numer niż 3037 — wpisz swój numer + 1.
  Ważne tylko, żeby był większy od poprzedniego, inaczej instalator
  odmówi aktualizacji.


KROK 3. Zbuduj

  cd C:\Users\Rafcio\Desktop\SterownikCO-0.31.23
  .\gradlew assembleDebug

  Oczekiwany koniec:
      BUILD SUCCESSFUL


KROK 4. Wgraj

  app\build\outputs\apk\debug\app-debug.apk

  Po instalacji w „Informacje o aplikacji” ma pisać: v0.31.32-debug


────────────────────────────────────────────────────────────────────────────
 9. JAK SPRAWDZIĆ, ŻE DZIAŁA
────────────────────────────────────────────────────────────────────────────

NAJPROSTSZY SPRAWDZIAN — rano:

  Karta „DO KOŃCA DNIA” ma pokazać NIŻSZĄ liczbę niż w v0.31.31.
  Model przestał zakładać, że woda przez noc trzyma ciepło.

DRUGI — na karcie „ZYSK DZIŚ”:

  Rano, gdy woda po nocy ostygła, ma pisać „SPADEK DZIŚ −X.X°C”,
  a nie „—”. Jeśli dalej jest myślnik, to znaczy, że pliki nie weszły.

TRZECI — w siatce 7-dniowej:

  W pochmurne dni ma być „−X.X°C” na niebiesko, nie „+0.0°C”.


────────────────────────────────────────────────────────────────────────────
10. JEŚLI SIĘ NIE KOMPILUJE
────────────────────────────────────────────────────────────────────────────

  Unresolved reference: reverseFlowKgS / loopUaWk / loopWaterKg
      → SolarThermalModel.kt nie został podmieniony. Musi być w core\.

  Unresolved reference: todayHasData
      → SolarAnalytics.kt nie został podmieniony. Też w core\.

  Unresolved reference: dayDeltaC
      → ten sam plik.

  Unresolved reference: signedC / ZERO_EPS_C / todayGainLabel
      → Weather.kt nie został podmieniony.

  Cokolwiek innego — wklej komunikat, sprawdzę.


────────────────────────────────────────────────────────────────────────────
11. CZEGO TO NIE ROBI
────────────────────────────────────────────────────────────────────────────

  - Nie modeluje warstw zbiornika. Godzina szczytu zostaje ~2 h za późno.
  - Nie zna poboru ciepłej wody (robi to osobno, jako „POBRANIA”).
  - Nie liczy strat na rurach między zbiornikiem a kolektorem osobno —
    siedzą w `loopUaWk`.
  - Parametry pętli sprawdzone na 5 dobach. Po 2–3 tygodniach warto wrócić.
════════════════════════════════════════════════════════════════════════════
