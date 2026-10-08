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

Wymagania: **JDK 17–21** (Studio: `jbr-21`), Android SDK 35 (compile/target),
minSdk 26, Gradle 8.9 (wrapper w repo — `gradle/wrapper/gradle-wrapper.jar`
jest commitowany, więc lokalny `gradle` nie jest potrzebny). Zestawienie
narzędzi: AGP 8.7.3 · Kotlin 2.0.21 · Compose BOM 2024.12.01.

### „Incompatible Gradle JVM version" przy syncu

Gradle 8.9 nie uruchamia się na JDK 25 (i nowszych) — komunikat
`The project's Gradle version 8.9 is incompatible with the Gradle JVM version 25.x`.
Rozwiązanie (jedno kliknięcie): w panelu błędu wybrać
**„Apply compatible Gradle JDK configuration and sync"**. Ręcznie:

```
Settings → Build, Execution, Deployment → Build Tools → Gradle
  → Gradle JDK: jbr-21 (JetBrains Runtime bundled z Studio)
  → Apply → Sync Project with Gradle Files
```

Zostawienie `sourceCompatibility/targetCompatibility = 17` i `jvmTarget = "17"`
jest celowe — kod ma działać na urządzeniach z bootimage JDK 17, a Gradle/Kotlin
uruchamiają się na JDK 21. Jeżeli koniecznie chcesz budować na JDK 25, trzeba
podbic Gradle do 9.x (i AGP do gałęzi dopasowanej do 9.x) — wtedy ten plik
`gradle-wrapper.properties` wymaga zmiany `distributionUrl`.

> Bieżący Linux sandbox nie ma JDK ani Android SDK, więc nie można tu powtórzyć
> `./gradlew :app:assembleDebug`. Poprzedni build Windows został zgłoszony jako
> udany na JDK 21.0.12.1; instalacja APK na urządzeniu nie została potwierdzona.

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
| Symulacja czujników na sterowniku / `symuluj …` / `symuluj_stop` + flagi `symulacja_*` z `/piec/status` | 5100–5240 | `core/AppModel.kt` (wysyłka/ACK), `core/PiecState.kt` (stan potwierdzony), `ui/screens/sheets/Sensors.kt` |
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
| Modal logowania Firebase (`#authModal`) | 3458–3500 | `ui/screens/Overlays.kt` (`AuthModal`) |
| Toast poleceń (`#toast`) | 1830–1890, 4980–5040 | `Chrome.kt` (`Toast`) |

## Jak to działa

* `MainActivity` → jedna aktywność, `setContent { SterownikApp(model) }`.
* `AppModel` (tworzony w `onCreate`) trzyma `PiecState`, timer (`4 s` poll,
  `15 min` pogoda, `1 s` odświeżanie arkusza), logi,
  toasty, telemetrię i preferencje (`SharedPreferences`, klucze jak `safeStorage`
  w panelu: `piec_fb_token`, `piec_tg_token`, `piec_chart_prefs_v2`…).
* Polecenia urządzeń są wysyłane do `/piec/cmd` i stan jest uznawany za
  potwierdzony dopiero po ACK. Bez sesji Firebase polecenie nie jest wysyłane
  ani stosowane optymistycznie lokalnie.
* Symulacja czujników (`symuluj …` / `symuluj_stop`) jest wysyłana do
  `/piec/cmd` i uznawana za potwierdzoną dopiero po ACK sterownika. Aktywny stan
  pochodzi z flag `symulacja_*` w `/piec/status`; wartości są oznaczane `~`, a w
  wykresach kreskowany jest tylko symulowany odcinek — próbki rzeczywiste
  pozostają ciągłe. Test dymu może uruchomić prawdziwy alarm, buzzer oraz ruch
  aktuatorów. Przycisk „Kafelki aplikacji” steruje tylko kafelkami wewnątrz
  aplikacji; widget ekranu głównego jest osobnym elementem
  Androida.
* Natywne widżety Androida korzystają z jednej migawki statusu sterownika:
  prawdziwe odczyty oraz aktywne symulacje sterownika oznaczone `~`.
  Dostępne układy: kompaktowy (do 2 czujników), szeroki pasek (do 4) i pełny panel 2×2 (do 4).
  Podczas dodawania lub ponownej konfiguracji można wybrać piec C.O., bojler
  C.W.U., panel słoneczny i/lub temperaturę zewnętrzną; wybór jest zapisany
  osobno dla każdego widżetu. Temperatury są zaokrąglane do pełnych stopni.
  `updatePeriodMillis=0`: system/launcher nie wykonuje cyklicznego odpytywania.
  Widget odświeża się po nadejściu danych z pollingu aplikacji (co 4 s na
  pierwszym planie) lub usługi SSE; zmieniona sygnatura odświeża go od razu,
  a identyczna jest renderowana najwyżej raz na 60 s, jeśli kolejne push'e
  nadal napływają. Bez danych z usługi nie ma gwarantowanego odświeżenia.
  Wszystkie warianty pokazują status/nieaktualne dane uczciwie i otwierają
  aplikację po dotknięciu.

## Świadome uproszczenia (żeby nic nie „udawało”)

1. **Brak fałszywych danych.** Skaner Wi‑Fi i raporty Telegram odpytują
   wyłącznie centralę (`/api/wifi/*`, `/api/telegram/*`). Gdy centrala jest
   nieosiągalna, widać uczciwy komunikat (tak jak w oryginale po
   `FIX-WIFI-REAL` / `FIX-TELEGRAM-REAL`), żadnych wymyślonych SSID.
2. **Terminal 21 DLOG**: brak logów startowych i próbek demo. Widok pozostaje
   pusty, dopóki aplikacja nie otrzyma rzeczywistych wpisów; bieżący port
   obsługuje potwierdzane komendy Remote, ale nie subskrybuje jeszcze strumienia
   `/piec/telemetry/diagnostics`.
3. **Ręczne OTA `.bin`**: w panelu był `<input type=file>`; w aplikacji
   podajesz ścieżkę pliku (brak natywnego pickera w tym porcie). Komenda
   `update` / `update_panel` pokazuje wyłącznie ACK przyjęcia; panel nie
   przedstawia postępu ani wyniku flashowania jako potwierdzonego.
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
   ├─ AndroidManifest.xml          # aktywności, konfigurator + odbiorniki widgetów
   ├─ res/                          # ikona, theme, strings, layout i metadane widgetu
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
      ├─ core/                      # PiecState, RtdbClient, Telemetry, Weather,
      │                              # SolarAnalytics, AppModel, TileSpecs, Sensors, Prefs
      └─ widget/                    # natywne widgety i ich ekran konfiguracji
```
