════════════════════════════════════════════════════════════════════════════
 SterownikCO Android  v0.31.32  →  v0.31.33
 Oś czasu na wykresach, lokalne archiwum centrali, licznik "w chmurze"
════════════════════════════════════════════════════════════════════════════

Nakładka na drzewo v0.31.32. Podmienia 6 plików, dokłada 2 testy.
ZERO ręcznych zmian w kodzie.

Ta wersja nie zmienia modelu fizycznego ani wyglądu kafelków. Naprawia
TRZY rzeczy, które sprawdziliśmy na Twoich danych z 24.10:

  A. wykresy kłamały na osi X (dziury w danych były niewidoczne),
  B. telefon nie widział 10 próbek, które centrala ma, a chmura nie,
  C. nie było sposobu sprawdzić, czy próbki naprawdę doszły do chmury.


| | |
|---|---|
| ZIP          | SterownikCO-Android-v0.31.33-OS-CZASU.zip |
| wersja       | versionCode 3039 / "v0.31.33" |
| pliki .kt    | 6 podmienianych + 2 nowe testy |


────────────────────────────────────────────────────────────────────────────
 0. HASHE
────────────────────────────────────────────────────────────────────────────

  AppModel.kt            2196 linii  101 994 B   (core\)
    375064D482F89300676156109D467CB833F7CD10B85493DC8F9CD6AC63B3DC35

  PiecState.kt            418 linii   19 976 B   (core\)
    B7705D1A124246ADFEF07650C77811EC28EE755EBBC8A627EF18F11CB0BBB848

  RtdbClient.kt           509 linii   25 704 B   (core\)
    4FA20D09083E8EEB504E9A22251D824E08CC3F7E38D572EB22FEB3F90DA71A3D

  Telemetry.kt            317 linii   15 731 B   (core\)
    65425A54F1E5B031EEB5440AE634A3252169EA7C81D2E2137234BD8C06462C17

  ChartEngine.kt          654 linie   32 675 B   (ui\chart\)
    572655B39EEE158462C018857D01F432DC57EF278AC576074BED4188DAB079F8

  Charts.kt               660 linii   33 631 B   (ui\screens\)
    382AA7125715BAB3CEA3082DEFCA0C9D383103FFE941089B5DD39529DB518122

  --- nowe testy (nie trzeba ich kopiować, żeby aplikacja działała) ---

  EspHistoryCsvTest.kt     98 linii    4 370 B   (src\test\...\core\)
    AD506C6C91565EC7C49A0CBF5AF8E2873BE7D5BB9384B3CEF39569391B98478C

  ChartTimeAxisTest.kt     86 linii    3 888 B   (src\test\...\ui\chart\)
    5564BE712917F5F191378AFABB102E0B610059F3BA4FFFF6B55209DFA61F7F9D


────────────────────────────────────────────────────────────────────────────
 1. PUNKT A — OŚ X LICZONA Z CZASU, NIE Z NUMERU PRÓBKI

Było: x = numer_próbki / (liczba_próbek - 1).
Skutek: każda dziura w transmisji ZNIKAŁA. 10 minut przerwy zajmowało
na wykresie tyle samo miejsca, co jedna minuta.

  Twoje dane z 24.10: przerwa 10:03:05 → 10:14:05 (10,0 min).
  Na wykresie: nie do odróżnienia od zwykłej minuty.

Do tego etykiety kłamały. Zrzut z 11:27 pokazywał okno 50 minut z prawą
krawędzią 10:34 — choć dane w Firebase sięgają 11:26. Oś liczyła WIERSZE,
a wiersze w tym oknie kończyły się na 10:34.

Jest: x = (czas_próbki − czas_pierwszej) / (czas_ostatniej − czas_pierwszej).
Jedna funkcja (ChartTimeAxis) rysuje linię, stawia kursor i liczy etykiety —
więc wszystko zgadza się co do piksela.

  - podziałki osi X: równe odstępy W CZASIE (co 1/4 okna),
  - kursor pod palcem: najbliższa próbka po czasie (bisekcja),
  - etykiety skrajne nie ucinają się już przy krawędzi,
  - dymek pokazuje "⏸ przerwa w danych: N min", gdy przed próbką była dziura.


────────────────────────────────────────────────────────────────────────────
 2. PUNKT B — TELEFON CZYTA LOKALNE ARCHIWUM CENTRALI

Do tej pory aplikacja NIGDY nie wołała /api/history. Wykresy szły wyłącznie
z Firebase. Sprawdziliśmy 1:1 na 537 wspólnych próbkach — zgadza się co do
0,1 °C — ale centrala ma 10 próbek (10:04:05–10:13:05), których w chmurze
NIE MA. Telefon ich nie widział.

Jest: przy każdym pobraniu historii (6h / 24h) aplikacja równolegle prosi
centralę o /api/history i skleja oba źródła po znaczniku czasu:

  - ta sama próbka w obu źródłach → jedna, z chmury (nie dubluje się),
  - próbka tylko w centrali → wchodzi na wykres, z ujemnym numerem,
  - pasek statusu mówi wprost: "z tego N tylko z archiwum centrali",
  - w dymku przy takiej próbce: "z archiwum centrali (brak w chmurze)".

Zakresy 7 dni i 30 dni zostają wyłącznie z chmury — archiwum centrali ma
~24–38 h, więc dla dłuższych zakresów nic nie wnosi.

Limit czasu: 6 s. Poza domową siecią centrala nie odpowiada i wykresy z chmury
pokazują się normalnie, tylko bez doklejki z centrali.

Nowe: parseEspHistoryCsv() w Telemetry.kt — czyta 14 kolumn lokalnego CSV.
Odporność: ostatnia kolumna bywa ",0" (stare pliki) → wtedy czas z etykiety
"RRRR-MM-DD GG:MM". Zła linia jest pomijana, nie wywala całego wykresu.


────────────────────────────────────────────────────────────────────────────
 3. PUNKT C — "OSTATNIA PRÓBKA W CHMURZE" NA EKRANIE WYKRESÓW

Pod paskiem statusu doszły trzy kafelki (pokazują się tylko z firmware
v3.55.0 lub nowszym, bo starsze nie przysyła tych pól):

  W CHMURZE   →  "3 min temu"        (zielone < 3 min, pomarańczowe < 10 min)
  KOLEJKA     →  "3 szt."           (ile próbek czeka w sterowniku;
                                     do 10 to stan normalny — tyle idzie
                                     w jednej paczce, zielone)
  WYSYŁKA     →  "BEZ ZALEGŁOŚCI"   albo "W TOKU"

To jedyna liczba, która mówi, czy dane NAPRAWDĘ doszły do chmury —
a nie czy centrala próbowała je wysłać.

UWAGA: "W CHMURZE: 1 min temu" to stan NORMALNY. Telemetria idzie co 60 s,
więc ta liczba oscyluje między 0 a ~70 s. Rośnie do kilkuset sekund tylko
wtedy, gdy internet siadł albo wysyłka się zablokowała.


────────────────────────────────────────────────────────────────────────────
 4. CO ZROBIĆ — 4 KROKI
────────────────────────────────────────────────────────────────────────────

KROK 1. Podmień sześć plików

  AppModel.kt     →  app\src\main\java\com\sterownikco\pro\core\
  PiecState.kt    →  app\src\main\java\com\sterownikco\pro\core\
  RtdbClient.kt   →  app\src\main\java\com\sterownikco\pro\core\
  Telemetry.kt    →  app\src\main\java\com\sterownikco\pro\core\
  ChartEngine.kt  →  app\src\main\java\com\sterownikco\pro\ui\chart\
  Charts.kt       →  app\src\main\java\com\sterownikco\pro\ui\screens\

  TRZY RÓŻNE FOLDERY. ChartEngine.kt jest w `ui\chart`, nie w `ui\screens`.

  Pytanie "plik istnieje, zamienić?" → TAK.

KROK 2. (opcjonalnie) dorzuć dwa testy

  EspHistoryCsvTest.kt   →  app\src\test\java\com\sterownikco\pro\core\
  ChartTimeAxisTest.kt   →  app\src\test\java\com\sterownikco\pro\ui\chart\

  Bez nich aplikacja działa tak samo. Z nimi `gradlew test` sprawdzi parser
  archiwum i geometrię osi czasu bez telefonu.

KROK 3. Podbij wersję

  Plik: app\build.gradle.kts

  Znajdź:
      versionCode = 3038
      versionName = "v0.31.32"

  Zamień na:
      versionCode = 3039
      versionName = "v0.31.33"

  Jeśli masz inny numer niż 3038 — wpisz swój numer + 1.
  Ważne tylko, żeby był większy od poprzedniego.

KROK 4. Zbuduj i wgraj

  cd C:\Users\Rafcio\Desktop\SterownikCO-0.31.23
  .\gradlew assembleDebug

  Oczekiwany koniec: BUILD SUCCESSFUL
  Plik: app\build\outputs\apk\debug\app-debug.apk


────────────────────────────────────────────────────────────────────────────
 5. JAK SPRAWDZIĆ, ŻE DZIAŁA
────────────────────────────────────────────────────────────────────────────

NAJPROSTSZY — ekran WYKRESY, pod paskiem statusu:

  Nowe trzy kafelki: W CHMURZE / KOLEJKA / WYSYŁKA.
  Jeśli ich nie ma → pliki nie weszły ALBO firmware centrali jest starszy
  niż v3.55.0 (starsze nie przysyła pól tel_*).

DRUGI — oś X:

  Wybierz zakres 24h, dotknij punktu na wykresie.
  Godzina w dymku musi być tą samą godziną, którą pokazuje oś pod palcem.
  Wcześniej po przerwie w danych kursor potrafił pokazać wartość z innej
  godziny niż pozycja palca.

TRZECI — archiwum centrali:

  Pasek statusu ma napisać "Załadowano N próbek · z tego K tylko
  z archiwum centrali", gdy w chmurze brakuje próbek.
  Przy próbce z centrali dymek dopisze "z archiwum centrali (brak w chmurze)".


────────────────────────────────────────────────────────────────────────────
 6. JEŚLI SIĘ NIE KOMPILUJE
────────────────────────────────────────────────────────────────────────────

  Unresolved reference: ChartTimeAxis / chartSampleIndexAtTimeX
      → ChartEngine.kt nie został podmieniony (folder ui\chart).

  Unresolved reference: parseEspHistoryCsv
      → Telemetry.kt nie został podmieniony (folder core).

  Unresolved reference: fetchEspHistory
      → RtdbClient.kt nie został podmieniony (folder core).

  Unresolved reference: tel_cloud_ago / tel_ring / tel_pending
      → PiecState.kt nie został podmieniony (folder core).

  Unresolved reference: cloudAgeLabel / Kpi
      → Charts.kt nie został podmieniony (folder ui\screens).

  Unresolved reference: combineCloudAndEsp / withTimeoutOrNull
      → AppModel.kt nie został podmieniony (folder core).

  Cokolwiek innego — wklej komunikat, sprawdzę.


────────────────────────────────────────────────────────────────────────────
 7. CZEGO TO NIE ROBI
────────────────────────────────────────────────────────────────────────────

  - Nie kompilowałem tego w swoim środowisku (brak JDK/Gradle/SDK).
    Kod przeszedł przegląd i testy logiki, ale BUILD robisz u siebie.
  - Archiwum centrali sięga ~24–38 h; dla 7/30 dni wykresy dalej są
    wyłącznie z chmury.
  - Próbki z archiwum centrali nie mają znacznika symulacji (lokalny CSV
    go nie zawiera) — dlatego są opisane jako "z archiwum centrali",
    a nie kreskowane.
  - Nie zmienia modelu solarnego ani kafelków. To osobna paczka.
════════════════════════════════════════════════════════════════════════════
