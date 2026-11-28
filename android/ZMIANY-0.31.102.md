# SterownikCO Android 0.31.102 — „Pogoda · Open-Meteo" jako trzecie źródło kafelka Zewnętrzna

Baza: 0.31.101 (pełne źródła odtworzone — patrz `REKONSTRUKCJA-0.31.101.md`).
`versionCode = 31102`, `versionName = "0.31.102"`. **Zmiana tylko w aplikacji** —
centrala, panel LCD i strona WWW bez zmian (nic nie trzeba wgrywać do pieca).

## Co nowego

Arkusz **Temperatura zewnętrzna → ŹRÓDŁO KAFELKA ZEWNĘTRZNA** ma trzeci przycisk
**🌐 Pogoda · Open-Meteo** (ikona globusa). Wybór — jak dotąd — dotyczy kafelka na
pulpicie i widżetu telefonu, jest zapamiętany w telefonie (`zewnZrodlo` centrali
to osobne ustawienie strony WWW).

| źródło | wartość | plakietka | ilustracja |
|---|---|---|---|
| Zew 1 · DS18B20 | `t_zewn` z centrali | LIVE / NIEAKTUALNE / SYMULACJA | pogoda wg kodu |
| Zew 2 · DHT22 | `t_zewn_2` z centrali | LIVE / NIEAKTUALNE / BRAK DANYCH | pogoda wg kodu |
| **Pogoda · Open-Meteo** | temperatura bieżąca (2 m) z prognozy, którą aplikacja i tak pobiera dla zakładki Pogoda | **PROGNOZA** (zielona), **PROGNOZA NIEAKTUALNA** gdy pobranie starsze niż 2 h, BRAK DANYCH gdy nic nie pobrano | globus |

- Opis kafelka: `Pogoda · Open-Meteo · z 12:45` (godzina ostatniego pobrania).
- Prognoza nie zależy od łączności z centralą: offline pieca nie zmienia plakietki
  PROGNOZA; symulacja „zewn" na centrali nie dotyczy prognozy (Zew 1 bez zmian).
- W arkuszu pod kartami Zew 1 / Zew 2 jest karta **Pogoda · Open-Meteo** z
  temperaturą, odczuwalną, godziną pobrania i przyciskiem **Odśwież prognozę**.
- Przełączenie na Pogodę od razu uruchamia odświeżenie prognozy (jeśli minęło
  >30 min od poprzedniego).

### Widżet
Przy źródle 3 pole ZEWN. pokazuje etykietę **POGODA** i temperaturę z ostatniego
udanego pobrania Open-Meteo (zapis w preferencjach: `piec_pogoda_temp_v1`,
`piec_pogoda_ts_v1`). Widżet używa jej tylko, gdy ma ≤ 2 h; starsza → `—`.
Format i kolory jak dla temperatury zewnętrznej (`-2,5°`). Status widżetu
(LIVE / NIEAKTUALNE) nadal opisuje łączność z centralą.

## Pliki
- `core/TileSpecs.kt` — `object OutdoorSource` (1/2/3, świeżość 2 h, `clock()`),
  `outdoorTemperatureTile(... forecast, forecastAtMs, nowMs)`.
- `core/AppModel.kt` — `outdoorTileSource` 1|2|3, `weatherFetchedAtWall`,
  `rememberForecastTemp()` wywoływane po każdym udanym pobraniu i przy odtworzeniu cache.
- `core/Prefs.kt` — `K_POGODA_TEMP`, `K_POGODA_TS`.
- `ui/screens/sheets/Sensors.kt` — trzeci przycisk (ikona `globe`), `ForecastSourceCard`.
- `ui/components/Kit.kt` — `Seg(... icons = mapOf(...))` (ikona przed etykietą).
- `ui/icons/AppIcons.kt` — ikona `globe`; `ui/art/Ilu.kt` — ilustracja `pogoda`.
- `widget/PiecWidget.kt` — `WidgetOutdoorSource.key(... 3) = "pogoda_temp"`, `WidgetForecast.read()`.
- `widget/WidgetTemperatureFormat.kt` — `pogoda_temp` z jednym miejscem po przecinku.
- Testy: `OutdoorTileSourceTest` (+4), `WidgetOutdoorSourceTest` (+2).

## Budowanie (Windows, jak dotąd)
```powershell
$d = "$env:USERPROFILE\Documents\SterownikCO-0.31.102"
Expand-Archive "$env:USERPROFILE\Downloads\SterownikCO-Android-0.31.102-PELNY-PROJEKT.zip" $d -Force
cd $d
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug
Copy-Item app\build\outputs\apk\debug\app-debug.apk "$env:USERPROFILE\Downloads\SterownikCO-0.31.102-debug.apk" -Force
```
Źródła nie były kompilowane w środowisku, w którym powstały (brak JDK/SDK) —
Gradle na PC jest pierwszym kompilatorem; w razie błędu wklej komunikat.
