# SterownikCO Android 0.31.107 — brak odczytu czujnika → automatycznie wartość z internetu (🌐)

Baza: 0.31.102. `versionCode = 31107`, `versionName = "0.31.107"`. Zmiana tylko w aplikacji.

0.31.103 nie kompilowała się (kolizja sygnatur JVM `setNetFallback` = setter właściwości `netFallback`);
w 0.31.104 metoda nazywa się `chooseNetFallback()` (jak `chooseOutdoorTileSource()`). Funkcjonalnie bez zmian.

## Co robi
Gdy wybrany czujnik **nie daje odczytu** (BRAK DANYCH) albo **dane centrali są nieaktualne**
(centrala offline / brak świeżego statusu), aplikacja sama podstawia wartość z Open-Meteo:

| kafelek | czujnik | zastępczo z internetu | oznaczenie |
|---|---|---|---|
| Zewnętrzna (Zew 1 · DS18B20 lub Zew 2 · DHT22) | `t_zewn` / `t_zewn_2` | temperatura bieżąca (2 m) | mały **globus** obok wartości, plakietka **PROGNOZA**, opis `Zew 2 brak danych · internet z 12:45` |
| Wilgotność (DHT22) | `wilgotnosc` | wilgotność względna | mały **globus**, plakietka **PROGNOZA**, opis `DHT22 brak danych · internet z 12:45` |
| widżet — pole ZEWN. | jw. | temperatura bieżąca | etykieta `ZEW 2 🌐` |

Zasady (identyczne dla kafelka i widżetu, pokryte testami):
- zastępowanie tylko gdy prognoza jest świeża (pobranie ≤ 2 h) i ma liczbę — inaczej dalej BRAK DANYCH / NIEAKTUALNE;
- **symulacja na centrali ma pierwszeństwo** — symulowanego czujnika nie zastępujemy;
- prawidłowy odczyt (także `0.0`) nigdy nie jest zastępowany;
- źródło „Pogoda · Open-Meteo" (3) zawsze ma globus — globus = „to jest z internetu, nie pomiar";
- nic z tego nie wpływa na sterowanie piecem ani na dane zapisywane do wykresów.

## 0.31.105 — wilgotność „81 % LIVE" mimo martwego DHT22
Centrala po padzie DHT22 **trzyma ostatnią wilgotność** (`poprzedniaWilgotnosc`) i wysyła ją dalej jako zwykłą
liczbę, więc 0.31.104 pokazywała `81 % LIVE` bez globusa. Temperatura z tej samej ramki (`t_zewn_2`) jest
przez centralę wygaszana po 135 s → `null`. Aplikacja używa tego jako sygnału: **Zew 2 bez odczytu = DHT22
nie odpowiada = wilgotność z centrali martwa** → kafelek Wilgotność bierze wartość z internetu (globus,
PROGNOZA, opis `DHT22 nie odpowiada · internet z HH:mm`). Przy wyłączonym Auto: ostatnia wartość, ale
plakietka NIEAKTUALNE i opis `DHT22 nie odpowiada · ostatnia wartość`. Symulacja wilgotności nie zależy od DHT22.
Test: `HumidityTileTest.heldHumidityAfterDhtDeathUsesInternet`.

## 0.31.106 — ilustracja kafelka Zewnętrzna
Duża ilustracja kafelka przy źródle „Pogoda · Open-Meteo" (i przy auto-zastąpieniu) to **ikona pogody
jak w zakładce Pogoda** (słońce / księżyc / chmury / deszcz / śnieg / burza / mgła wg kodu WMO i pory dnia),
a nie globus. Globus zostaje wyłącznie jako mały znacznik obok wartości (= „z internetu"). Usunięto
ilustrację-globus `Ilu.pogoda()`; `artKey` kafelka zawsze `zewn`.

## 0.31.107 — słońce w kafelku i temperatura odczuwalna
- **Animacja słońca w kafelku Zewnętrzna** była odwrotnie niż w zakładce Pogoda: tarcza i promienie obracały
  się wokół osi kolektora słonecznego (19.5, 4.5 — wspólne klasy `sun-spin`/`sun-pulse`), więc słońce
  „tańczyło" wokół promieni. Teraz własne klasy `zw-sun`/`zw-rays` z osią w środku (12, 12): tarcza pulsuje
  w miejscu, promienie krążą wokół niej — identycznie jak hero w zakładce Pogoda.
- **Odczuwalna** w kafelku: gdy wartość jest z internetu, opis (mała czcionka) pokazuje temperaturę
  odczuwalną z Open-Meteo: `odczuwalna -8.2 °C · z 11:49` (źródło Pogoda) lub `Zew 2 brak danych · odcz. -8.2 °C`
  (auto-zastąpienie). Przy działającym czujniku odczuwalnej nie ma (to nie jest pomiar).
  Test: `OutdoorTileSourceTest.internetValueShowsFeelsLikeInDescription`.

## Przełącznik
Arkusz **Temperatura zewnętrzna** oraz **Wilgotność** → sekcja **GDY CZUJNIK NIE DAJE ODCZYTU**:
`🌐 Auto · z internetu` (domyślnie) / `Pokaż brak danych`. Jedno ustawienie dla obu kafelków i widżetu
(`piec_net_fallback_v1`). W arkuszu Wilgotność widać aktualną wilgotność z internetu i godzinę pobrania.

## Pliki
- `core/TileSpecs.kt` — `TileSpec.net`, `OutdoorSource.shouldFallback()`, `outdoorTemperatureTile(... netFallback)`, nowe `humidityTile(...)`.
- `core/AppModel.kt` — `netFallback` + `chooseNetFallback()`, zapis `K_POGODA_RH`, odświeżanie widżetu po każdym pobraniu prognozy.
- `core/Prefs.kt` — `K_NET_FALLBACK`, `K_POGODA_RH`.
- `ui/components/Hmi.kt` — globus 11 dp przy wartości gdy `spec.net`.
- `ui/screens/sheets/Sensors.kt` — `NetFallbackPicker`, notatka w arkuszu Wilgotność.
- `ui/screens/Dashboard.kt` — klucz `m.netFallback` w `remember`.
- `widget/PiecWidget.kt` — `WidgetNetFallback.pick()`, etykieta z 🌐, `stale` liczone przed renderem komórek.
- Testy: `OutdoorTileSourceTest` (+4), nowy `HumidityTileTest` (3), `WidgetOutdoorSourceTest` (+1).

## Budowanie
W folderze projektu: `powershell -NoProfile -ExecutionPolicy Bypass -File .\BUDUJ-ANDROID.ps1`
