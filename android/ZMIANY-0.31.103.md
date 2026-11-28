# SterownikCO Android 0.31.103 — brak odczytu czujnika → automatycznie wartość z internetu (🌐)

Baza: 0.31.102. `versionCode = 31103`, `versionName = "0.31.103"`. Zmiana tylko w aplikacji.

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

## Przełącznik
Arkusz **Temperatura zewnętrzna** oraz **Wilgotność** → sekcja **GDY CZUJNIK NIE DAJE ODCZYTU**:
`🌐 Auto · z internetu` (domyślnie) / `Pokaż brak danych`. Jedno ustawienie dla obu kafelków i widżetu
(`piec_net_fallback_v1`). W arkuszu Wilgotność widać aktualną wilgotność z internetu i godzinę pobrania.

## Pliki
- `core/TileSpecs.kt` — `TileSpec.net`, `OutdoorSource.shouldFallback()`, `outdoorTemperatureTile(... netFallback)`, nowe `humidityTile(...)`.
- `core/AppModel.kt` — `netFallback` + `setNetFallback()`, zapis `K_POGODA_RH`, odświeżanie widżetu po każdym pobraniu prognozy.
- `core/Prefs.kt` — `K_NET_FALLBACK`, `K_POGODA_RH`.
- `ui/components/Hmi.kt` — globus 11 dp przy wartości gdy `spec.net`.
- `ui/screens/sheets/Sensors.kt` — `NetFallbackPicker`, notatka w arkuszu Wilgotność.
- `ui/screens/Dashboard.kt` — klucz `m.netFallback` w `remember`.
- `widget/PiecWidget.kt` — `WidgetNetFallback.pick()`, etykieta z 🌐, `stale` liczone przed renderem komórek.
- Testy: `OutdoorTileSourceTest` (+4), nowy `HumidityTileTest` (3), `WidgetOutdoorSourceTest` (+1).

## Budowanie
W folderze projektu: `powershell -NoProfile -ExecutionPolicy Bypass -File .\BUDUJ-ANDROID.ps1`
