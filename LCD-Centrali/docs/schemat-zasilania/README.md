# Schemat zasilania panelu LCD z ogniwa Li-ion (TP4056 → MT3608 → ESP32)

Cztery strony HTML, bez zależności – otwórz w przeglądarce (dwuklik) albo
przez podgląd online niżej.

| Plik | Co zawiera |
|---|---|
| `index.html` | **Widok montażowy** (strona startowa): układ pionowy, rysunki modułów TP4056 (03962A), MT3608, DevKit V1 30-pin, 18650, płytka uniwersalna (R1/R2 100k, R3 10k, R4 1k, C1 470 nF, przycisk, Q1 C945, moduł brzęczyka 3-pin), 21 numerowanych przewodów z podświetlaniem; lista zakupów i uwagi o typie modułu brzęczyka. |
| `symulacja.html` | Schemat ideowy + **symulacja**: USB/ogniwo/napięcie/MT3608/czas bez dotyku → stany GPIO34/35/14/13, linia logu `tag=BATERIA`, decyzja o uśpieniu, bilans prądów i czas pracy. Obliczenia przepisane z `src/main.cpp` (`zasilanieUSB()`, `bateriaOdczytajNapiecie()`, tabela `TABELA_BAT`). |
| `brzeczyk.html` | Osobny, duży rysunek podłączenia modułu brzęczyka BEEP V2.0 przez tranzystor **Q1 = C945** (nóżki E‑C‑B, baza z prawej; 5 przewodów + procedura testu). |
| `dzielnik.html` | Osobny rysunek dzielnika baterii 100k/100k + C1 470 nF → GPIO34 (D34), z **testem bez ogniwa** (R1 na 3V3 → 1,65 V na D34) i tabelą „co jest nie tak, gdy…”. |

Podgląd online (bez pobierania):
- https://htmlpreview.github.io/?https://raw.githubusercontent.com/zielinski25/Sterownik-Pieca-C.O./main/LCD-Centrali/docs/schemat-zasilania/index.html
- https://htmlpreview.github.io/?https://raw.githubusercontent.com/zielinski25/Sterownik-Pieca-C.O./main/LCD-Centrali/docs/schemat-zasilania/symulacja.html
- https://htmlpreview.github.io/?https://raw.githubusercontent.com/zielinski25/Sterownik-Pieca-C.O./main/LCD-Centrali/docs/schemat-zasilania/brzeczyk.html
- https://htmlpreview.github.io/?https://raw.githubusercontent.com/zielinski25/Sterownik-Pieca-C.O./main/LCD-Centrali/docs/schemat-zasilania/dzielnik.html

## Co jest zlutowane (stan 20.11.2026)

Ogniwo → TP4056 z DW01A → MT3608 nastawiony na 5,0–5,1 V → `VIN` DevKita;
dzielnik 100k/100k + **C1 470 nF** z `OUT+` na GPIO34; CHRG+STDBY (pady po
wylutowanych LED) + 10k do 3V3 na GPIO35; przycisk GPIO14→1 kΩ→GND (1 kΩ z
powodu impulsu wyjściowego GPIO14 przy starcie); brzęczyk BEEP V2.0 (IO
zmostkowane z VCC, zasilany z VIN 5 V), masę modułu przełącza **Q1 C945**
sterowany z GPIO13 przez 1 kΩ. Wspólna masa z `OUT−`, nigdy z `B−`.

Sprawdzone miernikiem i logiem:

| Co | Wynik |
|---|---|
| dzielnik, 3,32 V na OUT+ (bez ogniwa, z zasilacza) | log `tag=BATERIA napiecie=3.32V procent=1` |
| dzielnik, ogniwo Cubot Note S 3,97 V | `procent=76` |
| GPIO35 z ładowarką / bez | 0,317 V / 3,3 V |
| brzęczyk przez C945 | ekran alarmu → gra; WYCISZ → cisza; +7 mA przy ciągłym dźwięku |
| brzęczyk – rodzaj (wniosek 27.11) | test „R5 do 3V3 → ciągły pisk” = moduł **aktywny** (własny generator). Do panelu v1.5.18 firmware podawał PWM 50 % (3000 / 1200–1600 Hz) jak dla pasywnego → cicho; od **v1.5.19** stałe napięcie w fazie tonu (`BRZECZYK_AKTYWNY`), rytm jak dotąd |
| pobór z USB 5 V (bez ogniwa, WiFi) | ekran 100 %: 254/243/234 mA przy 240/160/80 MHz; ekran 5 %: 157 mA; szczyt startu < 260 mA |
| **pobór z ogniwa** (24.11, multimetr szeregowo OUT+ TP4056 → VIN+ MT3608, WiFi, pulpit) | ekran 100 %: **110 mA** @ 80 MHz (skoki do 150), 117–126 mA @ 160 MHz (do 140), 130 mA @ 240 MHz (do 160); ekran 5 %: **45–55 mA**; **głęboki sen: 2 mA** (cel < 10 mA – spełniony). Skoki = nadajnik WiFi. Dalsze pomiary przy 80 MHz |
| ogniwo docelowe (24.11) | jedno **5000–5500 mAh** Li-ion/LiPo 3,7 V (ogniwa z Cubota wymontowane); R_PROG 1,2 kΩ = 1 A = 0,2 C, ładowanie ok. 6 h; progi w programie bez zmian (tabela % jest napięciowa) |
| BC547 wylutowany ze złomu | nie przełączał (E/C odwrotnie, być może uszkodzony) → wymieniony na C945 |

## Sprawdzenie v1.5.0 / v1.5.1 (sen na baterii) po wgraniu

Firmware panelu od v1.5.0 używa tego sprzętu: na baterii po minucie bez
dotyku przygasza ekran do 5 %, po pięciu usypia, budzi go przycisk GPIO14
albo włożenie ładowarki (GPIO35). Krok po kroku, log z Seriala obok:

1. **Ładowarka w TP4056, ogniwo podpięte.** Odczekaj 6 min bez dotykania.
   Ekran ma być jasny cały czas, w logu żadnej linii `tag=SEN`, `tag=BATERIA
   … ladowanie=1`.
2. **Wyjmij ładowarkę** (ogniwo > 3,4 V). Po ~60 s ekran przygasa
   (`tag=SEN zdarzenie=przyciemnienie powod=bez_dotyku`). Dotknij szyby –
   jasność wraca (`zdarzenie=jasnosc powod=dotyk`). Naciśnij przycisk GPIO14
   – to samo.
3. **Nie dotykaj 5 min.** Ekran gaśnie, brzęczyk milczy, w logu ostatnia
   linia `tag=SEN zdarzenie=usypianie powod=bez_dotyku napiecie=… bez_dotyku_s=300`.
   Multimetr (200 mA) szeregowo w OUT+ TP4056 → VIN+ MT3608: **zmierzone
   24.11: 2 mA** (cel < 10 mA – spełniony).
4. **Przycisk GPIO14** → panel startuje od nowa, w logu
   `tag=ZASILANIE zdarzenie=wybudzenie zrodlo=przycisk`. Od v1.5.1 bez
   ekranu startowego: pulpit z „--” po ~0,3 s (`tag=SEN
   zdarzenie=pulpit_po_wybudzeniu`), dane po 1–2 s (v1.5.0 pokazywało tu
   „WYSWIETLACZ CO” i „Laczenie z WiFi”).
5. Znów uśpij (albo poczekaj 5 min) i **włóż ładowarkę** → start,
   `zrodlo=usb`.
6. **Alarm** (np. ekran alarmu z centrali) na baterii: ekran nie przygasa i
   panel nie śpi, dopóki alarm trwa (`sen_wstrzymany powod=alarm` raz w logu).
7. Ogniwo rozładowane do < 3,30 V bez ładowarki: napis „BATERIA ROZLADOWANA /
   Podlacz ladowarke. Panel usypia.” i sen po 2,5 s; ładowarka budzi.
8. (v1.5.1) **Dotknij przyciemnionego ekranu w kafelek** – ma się tylko
   rozjaśnić, nic nie otwierać; drugie dotknięcie działa normalnie.
9. (v1.5.1) **Ekran WYŚWIETLACZ, wiersz „Na baterii”**: ustaw „Przyciemnij
   po: 30 s” – po pół minuty bez dotyku 5 %; „Uśpij po: nigdy” – panel nie
   zasypia; po restarcie wybór zostaje.

Jeśli po wybudzeniu przyciskiem panel od razu zasypia z powrotem albo
budzi się sam: sprawdź, czy GPIO14 bez wciśniętego przycisku ma 3,3 V (pull‑up
wewnętrzny) i czy GPIO35 bez ładowarki ma 3,3 V (10k do 3V3) – ext1 budzi na
stan niski GPIO35.
