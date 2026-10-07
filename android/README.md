# Sterownik Pieca C.O. — port natywny (Android / Kotlin + Compose)

1:1 przeniesienie panelu HTML `../Piec.html` (v3.33.x, „Sterownik Pieca C.O.”)
na aplikację Android: ta sama nawigacja, te same polskie opisy, te same kolory,
ten sam model danych (Firebase Realtime Database + lokalne API ESP32), te same
przyciski, arkusze i diagramy.

## Build

```bash
cd android
./gradlew :app:assembleDebug        # Android Studio: Open → folder `android`
```

Wymagania: JDK 17, Android SDK 35 (compile/target), minSdk 26.
`gradle/wrapper/gradle-wrapper.jar` (Gradle 8.7) jest w repozytorium, więc nie
jest potrzebny lokalny `gradle`.

> Uwaga: w środowisku, w którym powstawał ten port, **nie było JDK/Android SDK**
> ani dostępu do dystrybucji Gradle — kod nie był więc nigdy kompilowany.
> Jedyna wykonana weryfikacja to kontrola równowagi nawiasów/nawiasów klamrowych
> i spójności symboli między plikami. Pierwszy `assembleDebug` może wymagać
> drobnych poprawek (importy, nazwy argumentów).

## Ekran ↔ źródło w `Piec.html`

| Element panelu | Linia w `Piec.html` | Plik w porcie |
|---|---|---|
| Ilustracje SVG (`ILU`) i ikony (`data-ico`) | 3524–4025 | `ui/svg/*`, `ui/art/Ilu.kt`, `ui/icons/AppIcons.kt` |
| Motyw (zmienne CSS, `--surface`, `--cyan`…) | 12–2422 | `ui/theme/Color.kt`, `ui/theme/Theme.kt` |
| Stan `S`, `liveTick`, komendy lokalne | 4069–4425 | `core/PiecState.kt` |
| Kafelki `TILES` + `renderDashboard()` | 4040–4425 | `core/TileSpecs.kt`, `ui/components/Hmi.kt` |
| Pasek górny, `sysstrip`, banery, hero, quick, sekcje, trend, arkusz, toast | 2960–3175, 4330–4425 | `ui/components/Chrome.kt` |
| Klasy CSS tile/well/badge/analysis/nav/trend | 1200–2422 | `ui/components/Kit.kt`, `Hmi.kt` |
| Klient Firebase (`/piec/status`, `/piec/cmd` + ACK) | 4425–5145 | `core/RtdbClient.kt`, `core/AppModel.kt` |
| Symulacja / `symuluj …` / override | 5100–5240 | `core/PiecState.kt` (`applyCommand`, `sym`) |
| Arkusze czujników `ALL_SENSORS`, `sensorModalContent`, `symulacjaSekcjaHtml`, `MENUS.dym`, `overheat` | 5104–5345, 5574 | `core/Sensors.kt`, `ui/screens/sheets/Sensors.kt` |
| `MENUS.pompa / serwo / mieszadlo / czas` (diagramy Trociniak/Kopciuch) | 5386–5562 | `ui/screens/sheets/Control.kt` |
| Moduł Wi‑Fi (`/api/wifi/list`, `scan/start`, `scan/result`, `add`, `delete`) | 5591–5855 | `ui/screens/sheets/System.kt` (`WifiSheet`) |
| Moduł Telegram (`/api/telegram/save|send|status`, raport, katalog zdarzeń) | 5855–6045 | `System.kt` (`TelegramSheet`) |
| Terminal 21 DLOG (kategorie, poziomy, konsola, `execTerminalCmd`) | 6058–6470 | `System.kt` (`TerminalSheet`) + `AppModel` (`terminal*`) |
| Logi + filtracja + eksport | 6472–6518 | `System.kt` (`LogsSheet`) |
| OTA / GitHub Releases (`fwLabel`, `fwMeta`, `tagNorm`, `runOtaProcedure`) | 6520–6750 | `System.kt` (`OtaSheet`) |
| Sesja operatora | 6752–6790 | `System.kt` (`SessionSheet`) |
| Nawigacja `PAGES` + skutki uboczne wejścia na zakładkę | 6792–6800 | `AppModel.navigate` |
| Telemetria: dekodowanie rekordu, boundy, filtr anomalii | 6800–7250 | `core/Telemetry.kt` |
| Ładowanie zakresów (szardy dzienne, paginacja `ts`) | 7195–7240 | `Rtdb.fetchTelemetry` |
| `updateChartsNowStat`, `renderSeriesChips`, plakietka `pill-real` | 7242–7281 | `AppModel.chart*`, `ui/screens/Charts.kt` |
| `updateChartCanvas` (skala, podwójna oś Y, area band, alarmy, SMOOTH/STEPPED, kursor) | 7289–7740 | `ui/chart/ChartEngine.kt` |
| Arkusze: serie / oś / narzędzia / analiza | 7846–8060 | `Charts.kt` (`ChartSeriesSheet`, `ChartAxisSheet`, `ChartToolsSheet`, `ChartAnalysisSheet`) |
| `SolarAnalytics` (bilans, detekcja poboru CWU, CSV) | 8045–8228 | `core/SolarAnalytics.kt` |
| Zakładka Pogoda (meteo, godziny, dni, łuk słoneczny) | 3232–3390, 8230–8430 | `ui/screens/Weather.kt`, `core/Weather.kt` |
| Strona Ustawienia / Więcej (karty `data-menu`) | 3393–3425 | `ui/screens/Lists.kt` |
| Szuflada DEMO (`buildDemo`, presety, suwaki) | 8861–9014 | `ui/screens/demo/DemoDrawer.kt` |
| Modal logowania Firebase (`#authModal`) | 3458–3500 | `ui/screens/Overlays.kt` (`AuthModal`) |
| Toast poleceń (`#toast`) | 1830–1890, 4980–5040 | `Chrome.kt` (`Toast`) |

## Jak to działa

* `MainActivity` → jedna aktywność, `setContent { SterownikApp(model) }`.
* `AppModel` (tworzony w `onCreate`) trzyma `PiecState`, timer (`4 s` poll,
  `2 s` tick symulacji, `15 min` pogoda, `1 s` odświeżanie arkusza), logi,
  toasty, telemetrię i preferencje (`SharedPreferences`, klucze jak `safeStorage`
  w panelu: `piec_fb_token`, `piec_tg_token`, `piec_chart_prefs_v2`…).
* Komendy: `sendCommand("ustaw …")` → `/piec/cmd` z ACK (12 s), tak jak w
  oryginale; gdy brak sesji Firebase, stan zmienia się lokalnie i widać
  efekt od razu (jak `S.applyCommand` w `Piec.html`).
* Tryb DEMO (`⚙ DEMO`, prawy dolny róg) działa bez sieci i logowania —
  presety i suwaki są przeniesione 1:1, łącznie z komunikatami toast.

## Świadome uproszczenia (żeby nic nie „udawało”)

1. **Brak fałszywych danych.** Skaner Wi‑Fi i raporty Telegram odpytują
   wyłącznie centralę (`/api/wifi/*`, `/api/telegram/*`). Gdy centrala jest
   nieosiągalna, widać uczciwy komunikat (tak jak w oryginale po
   `FIX-WIFI-REAL` / `FIX-TELEGRAM-REAL`), żadnych wymyślonych SSID.
2. **Terminal 21 DLOG**: logi startowe i próbki `generateSimulatedDLogChunk()`
   są symulowane dokładnie jak w panelu (to samo 11 kategorii próbek);
   strumień z `/piec/telemetry/diagnostics` nie jest nasłuchiwany ciągiem —
   komendy `diag remote on|off|level|cat` są wysyłane do centrali.
3. **Ręczne OTA `.bin`**: w panelu był `<input type=file>`; w aplikacji
   podajesz ścieżkę pliku (brak natywnego pickera w tym porcie), komenda
   `ota_upload` / `update` / `update_panel` jest kolejkowana tak samo jak
   w oryginale (zero udawanego postępu).
4. **Studio pogodowe**: wykres godzinowy jest narysowany w Canvasie
   (linia + wypełnienie) zamiast pełnego silnika `wMainChartCanvas` z
   interaktywnym tooltipem; dobór serii, zakresy i opisy są jak w oryginale.
5. **Łuk słoneczny**: słońce jest pozycjonowane ułamkiem doby, a godzin
   wschodu/zachodu/zenitu panel również nie liczył (pochodzą z ostatniego
   odświeżenia) — wartości tekstowe są pokazane w tym samym formacie.
6. **`.status-pill`** nie ma reguły bazowej w CSS `Piec.html` (jest tylko
   wariant `.sim`); w porcie użyto identycznych barw co `.pill.live/.err`
   plus `--cyan` dla `.sim`.
7. Arkusze: dolny bottom‑sheet (`#sheet`) odpowiada 75 % wysokości ekranu,
   promień 24 dp — dokładnie jak `#sheet { height:75%; border-radius:24px 24px 0 0 }`.

## Pliki

```
android/
├─ app/build.gradle.kts            # Compose BOM 2024.12.01, okhttp 4.12.0, minSdk 26
└─ app/src/main/
   ├─ AndroidManifest.xml          # jedna aktywność, zgoda na cleartext HTTP (LAN)
   ├─ res/                          # ikona aplikacji, theme, strings
   └─ java/com/sterownikco/pro/
      ├─ MainActivity.kt
      ├─ ui/SterownikApp.kt         # korzeń: pasek, strony, nawigacja, nakładki
      ├─ ui/theme/                  # Pal (zmienne CSS), Dimens, Txt, SterownikTheme
      ├─ ui/svg/                    # parser + renderer SVG (dla ILU i ikon)
      ├─ ui/art/Ilu.kt              # ilustracje bohero/kotła/pompy 1:1 z ILU
      ├─ ui/icons/AppIcons.kt       # ikony `data-ico`
      ├─ ui/components/             # Kit, Hmi (Tile), Chrome (TopBar…Toast)
      ├─ ui/chart/ChartEngine.kt    # updateChartCanvas
      ├─ ui/screens/                # Dashboard, Charts, Weather, Lists, Overlays
      ├─ ui/screens/sheets/         # Sensors, Control, System (Wi‑Fi/TG/terminal/logi/OTA/sesja)
      ├─ ui/screens/demo/           # DemoDrawer (buildDemo)
      └─ core/                      # PiecState, RtdbClient, Telemetry, Weather,
                                    # SolarAnalytics, AppModel, TileSpecs, Sensors, Prefs
```
