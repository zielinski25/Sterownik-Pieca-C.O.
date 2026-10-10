# Patch bezpieczeństwa, baterii i debugowej ikony

Pakiet zawiera:

- `Android-DirectBoot-78299c9-Audited.diff` — pełny patch do czystej bazy Android `78299c9`;
- `Android-DirectBoot-Security-Battery-Icon.diff` — nakładkę dla kopii, w której poprzedni Direct Boot/debug-branding jest już zastosowany;
- `Remove-Obsolete-Android-Default-FB-Password.ps1` — usuwa nieużywaną deklarację po nazwie, bez powielania jej wartości w patchu;
- `Android-DirectBoot-Audit-Followups.diff` — follow-up redakcji `tg_config` i anulowania żądań HTTP;
- `Android-DirectBoot-Debug-Icon-Final.png` — samodzielny PNG końcowej ikony debug, do skopiowania po zastosowaniu patchy.

Pełny patch i nakładki zawierają binarny diff PNG. Jeśli Git for Windows zgłosi błąd parsowania przy `GIT binary patch`, **nie wymuszaj patcha**. Zdefiniuj w PowerShell z katalogu głównego repozytorium:

```powershell
$debugIcon = 'android/app/src/debug/res/drawable-nodpi/ic_launcher_debug_art.png'
```

Przy kontrolach i zastosowaniu patchy Direct Boot/ikony użyj `--exclude="$debugIcon"`. To pomija wyłącznie binarny obraz, a pozostawia zmiany tekstowe zasobów debug. Końcowy PNG skopiuj poleceniami z sekcji „Końcowa grafika debug”. Nie wykluczaj całego `android/app/src/debug/res` — katalog zawiera też branding debug.

## Czysta kopia od bazy 78299c9

Z katalogu głównego klona pobierz aktualizację. Jeśli `git pull --ff-only` zgłosi konflikt z Twoimi lokalnymi zmianami, zatrzymaj się i nie resetuj ich.

```powershell
git pull --ff-only
git status --porcelain

$debugIcon = 'android/app/src/debug/res/drawable-nodpi/ic_launcher_debug_art.png'
git apply --check --unidiff-zero --exclude="$debugIcon" .\android-sync\Android-DirectBoot-78299c9-Audited.diff
```

Jeśli kontrola przejdzie:

```powershell
git apply --unidiff-zero --exclude="$debugIcon" .\android-sync\Android-DirectBoot-78299c9-Audited.diff
powershell -NoProfile -ExecutionPolicy Bypass -File .\android-sync\Remove-Obsolete-Android-Default-FB-Password.ps1
```

## Kopia testowa z wcześniejszym Direct Boot i brandingiem

Jeśli w kopii masz już poprzedni patch Direct Boot, poprawkę pętli alarmu, debug branding oraz poprawkę zasobu Androida, zastosuj **tylko nakładkę**:

```powershell
$debugIcon = 'android/app/src/debug/res/drawable-nodpi/ic_launcher_debug_art.png'
git apply --check --unidiff-zero --exclude="$debugIcon" .\android-sync\Android-DirectBoot-Security-Battery-Icon.diff
```

Jeśli kontrola przejdzie:

```powershell
git apply --unidiff-zero --exclude="$debugIcon" .\android-sync\Android-DirectBoot-Security-Battery-Icon.diff
powershell -NoProfile -ExecutionPolicy Bypass -File .\android-sync\Remove-Obsolete-Android-Default-FB-Password.ps1
```

Jeżeli `--check` nie przechodzi, zatrzymaj się: nie używaj `--reject` i nie wymuszaj patcha. Nie nakładaj patcha pełnego na już zmodyfikowaną kopię.

## Follow-up audytu (logi poufnych komend i anulowanie HTTP)

Po zastosowaniu właściwego pełnego patcha/nakładki i uruchomieniu skryptu usuwającego nieużywaną deklarację zastosuj dodatkową poprawkę:

```powershell
git apply --check --unidiff-zero .\android-sync\Android-DirectBoot-Audit-Followups.diff
if ($LASTEXITCODE -ne 0) { throw "Kontrola nie przeszła — zatrzymaj się." }
git apply --unidiff-zero .\android-sync\Android-DirectBoot-Audit-Followups.diff
```

Follow-up maskuje payload `tg_config` w toastach/logu aplikacji, kieruje lokalne żądania Telegrama przez klienta bez proxy/przekierowań i łączy anulowanie coroutine z `Call.cancel()` dla zwykłych żądań oraz pogody. Dodaje też test jednostkowy redaktora. Nie usuwa tokenu z samego polecenia wysyłanego do RTDB i nie dodaje autoryzacji/TLS do firmware. Jeśli `--check` nie przechodzi, nie używaj `--reject`.

## Końcowa grafika debug

Po zastosowaniu pełnego patcha albo nakładki Direct Boot zastosuj tekstowo nakładkę ikony, pomijając binarny PNG:

```powershell
$debugIcon = 'android/app/src/debug/res/drawable-nodpi/ic_launcher_debug_art.png'
git apply --check --exclude="$debugIcon" .\android-sync\Android-DirectBoot-Debug-Icon-Overlay.diff
if ($LASTEXITCODE -ne 0) { throw "Kontrola nie przeszła — zatrzymaj się." }
git apply --exclude="$debugIcon" .\android-sync\Android-DirectBoot-Debug-Icon-Overlay.diff
```

Następnie skopiuj finalny plik PNG do zasobów wariantu debug:

```powershell
$debugIconDir = '.\android\app\src\debug\res\drawable-nodpi'
New-Item -ItemType Directory -Path $debugIconDir -Force | Out-Null
Copy-Item -LiteralPath .\android-sync\Android-DirectBoot-Debug-Icon-Final.png -Destination (Join-Path $debugIconDir 'ic_launcher_debug_art.png') -Force
```

Nie zmieniaj produkcyjnej ikony ani nie nakładaj samodzielnego `Android-DirectBoot-Debug-Icon.diff` — pełny patch/overlay zawiera konfigurację `ic_launcher.xml`.

## Build lokalny

```powershell
cd .\android
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug
```

To są zadania kompilacji/testów jednostkowych — nie flashują ESP32 i nie wysyłają poleceń do kotła. Nie instaluj APK ani nie wykonuj testów wyjść/alarmów na działającym piecu. Test IP jest jednostkowy i nie otwiera połączenia sieciowego.

## Ważne

- `TEST` rozróżnia wariant aplikacji, **nie** backend. Debug nadal wskazuje tę samą RTDB; traktuj go jako zdolny do wysłania poleceń, dopóki nie powstanie osobny backend albo jawny read-only guard.
- `allowBackup=false` i reguły backupu wykluczają również transfer urządzenie–urządzenie; ustawienia mogą nie przenieść się na nowy telefon.
- Nowe zmiany nie były kompilowane ani uruchamiane w tym sandboxie (brak JDK). Build na komputerze użytkownika jest wymagany przed instalacją.
