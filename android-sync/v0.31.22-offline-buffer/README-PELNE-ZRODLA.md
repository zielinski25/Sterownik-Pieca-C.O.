# v0.31.22 — pełne źródła (gdy nakładka 13 plików nie wystarcza)

## Objaw

`gradlew :app:assembleDebug` wywala się na `compileDebugKotlin` z błędami typu:

```
Unresolved reference 'ServoPositionPreview'
Unresolved reference 'ServoPositionAxis'
Unresolved reference 'AlarmSoundSync'
Unresolved reference 'parseServoPositionCommand'
Unresolved reference 'commandChangesDashboardState'
Unresolved reference 'acceptedAtElapsedMs'
```

## Przyczyna

Mała paczka (`SterownikCO-Android-v0.31.22-BUFOR-OFFLINE.zip`, 13 plików)
zakłada drzewo źródłowe **v0.31.18/v0.31.19**. Jeśli projekt na dysku jest
starszy, brakuje w nim trzech plików, których nowy `AppModel.kt` i
`Dashboard.kt` wymagają:

| brakujący plik | co zawiera |
|---|---|
| `core/ServoPositionPreview.kt` | `ServoPositionAxis`, `ServoPositionPreview`, `parseServoPositionCommand`, `SERVO_POSITION_PREVIEW_TIMEOUT_MS`, `shouldKeepServoPositionPreview`, `acceptedAtElapsedMs`, `targetDegrees` |
| `core/AlarmSoundSync.kt` | `AlarmSoundSync` |
| `core/DashboardCommandRefresh.kt` | `commandChangesDashboardState` |

Nadpisanie 13 plików nie dokłada brakujących — kompilator widzi wywołania,
których nie ma gdzie rozwiązać.

## Rozwiązanie

`SterownikCO-Android-v0.31.22-PELNE-ZRODLA.zip` — cały projekt:
140 wpisów, 92 pliki `.kt`, `res/`, `gradlew.bat` + `gradle-wrapper.jar`,
`settings.gradle.kts`, `gradle.properties`. **1 119 837 B**,
sha256 `28749f6b07a06baceac9b106585c686bdf2c5fcaef284bddab3c6b41a28c1748`.

Wersja w środku: `versionCode = 3028`, `versionName = "v0.31.22"`
(+ `-debug` przy buildzie debug).

### Jak użyć

```powershell
$zip = "$env:USERPROFILE\Downloads\SterownikCO-Android-v0.31.22-PELNE-ZRODLA.zip"
$dst = "$env:USERPROFILE\Desktop\SterownikCO-0.31.22"

Expand-Archive -LiteralPath $zip -DestinationPath $dst -Force
Set-Location $dst
.\gradlew.bat :app:assembleDebug --console=plain
```

APK: `app\build\outputs\apk\debug\app-debug.apk`

Rozpakowanie do **nowego** folderu jest celowe: stary projekt zostaje
nietknięty, więc jest do czego wrócić. `local.properties` (ścieżka do
Android SDK) Android Studio dopisze sam przy pierwszym otwarciu; jeśli
budujesz z terminala bez Studio, ustaw `ANDROID_HOME` albo dopisz
`local.properties` z `sdk.dir=...`.

## Czego nie sprawdzono

W środowisku, w którym powstała paczka, **nie ma JDK, Kotlinc ani Gradle** —
nie ma dowodu „kompiluje się”. Sprawdzone: kompletność drzewa (wszystkie
pliki, do których odwołuje się kod), obecność wrappera, zgodność numeru
wersji, brak katalogów `build/` i `.gradle/` w archiwum.
