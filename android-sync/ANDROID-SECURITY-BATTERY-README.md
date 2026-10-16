# Patch bezpieczeństwa, baterii i debugowej ikony

Pakiet zawiera:

- `Android-DirectBoot-78299c9-Audited.diff` — pełny patch do czystej bazy Android `78299c9` (wariant z binarnym diffem PNG);
- `Android-DirectBoot-78299c9-Audited-TextOnly.diff` — ten sam patch tekstowy, bez binarnego bloku PNG; zalecany na Windows;
- `Android-DirectBoot-Security-Battery-Icon.diff` — nakładkę dla kopii z wcześniejszym Direct Boot/debug-brandingiem (wariant z binarnym diffem PNG);
- `Android-DirectBoot-Security-Battery-Icon-TextOnly.diff` — tekstowa wersja tej nakładki, bez binarnego bloku PNG; zalecana na Windows;
- `Remove-Obsolete-Android-Default-FB-Password.ps1` — usuwa nieużywaną deklarację po nazwie, bez powielania jej wartości w patchu;
- `Android-DirectBoot-Audit-Followups.diff` — follow-up redakcji `tg_config` i anulowania żądań HTTP;
- `Android-DirectBoot-Debug-Icon-Final.png` — samodzielny PNG końcowej ikony debug, do skopiowania po zastosowaniu patchy.

## Windows / Git for Windows

W pełnych plikach `.diff` znajduje się binarny blok PNG. Na Git for Windows może on powodować błąd `git diff header lacks filename information` przy linii `GIT binary patch`, także z `--exclude`. **Nie stosuj wtedy plików z binarnym blokiem ani nie wymuszaj patcha.** Użyj wariantów `-TextOnly.diff` i skopiuj końcowy PNG poleceniami z sekcji „Końcowa grafika debug”. Warianty tekstowe pomijają tylko plik PNG; zachowują pozostałe hunki debug-brandingu.

Wszystkie komendy wykonuj z katalogu głównego repozytorium. Jeśli dowolne `git apply --check` zakończy się błędem, zatrzymaj się i nie uruchamiaj odpowiadającego mu `git apply`.

## Czysta kopia od bazy 78299c9

```powershell
git pull --ff-only
git status --porcelain

git apply --check --unidiff-zero .\android-sync\Android-DirectBoot-78299c9-Audited-TextOnly.diff
```

Jeśli kontrola przejdzie:

```powershell
git apply --unidiff-zero .\android-sync\Android-DirectBoot-78299c9-Audited-TextOnly.diff
powershell -NoProfile -ExecutionPolicy Bypass -File .\android-sync\Remove-Obsolete-Android-Default-FB-Password.ps1
```

## Kopia testowa z wcześniejszym Direct Boot i brandingiem

Jeśli w kopii masz już poprzedni patch Direct Boot, poprawkę pętli alarmu, debug branding oraz poprawkę zasobu Androida, zastosuj **tylko tekstową nakładkę**:

```powershell
git apply --check --unidiff-zero .\android-sync\Android-DirectBoot-Security-Battery-Icon-TextOnly.diff
```

Jeśli kontrola przejdzie:

```powershell
git apply --unidiff-zero .\android-sync\Android-DirectBoot-Security-Battery-Icon-TextOnly.diff
powershell -NoProfile -ExecutionPolicy Bypass -File .\android-sync\Remove-Obsolete-Android-Default-FB-Password.ps1
```

Jeżeli kontrola nie przechodzi, zatrzymaj się: nie używaj `--reject` i nie wymuszaj patcha. Nie nakładaj pełnego patcha na już zmodyfikowaną kopię.

## Follow-up audytu (logi poufnych komend i anulowanie HTTP)

Po zastosowaniu właściwego pełnego patcha/nakładki i uruchomieniu skryptu usuwającego nieużywaną deklarację zastosuj dodatkową poprawkę:

```powershell
git apply --check --unidiff-zero .\android-sync\Android-DirectBoot-Audit-Followups.diff
if ($LASTEXITCODE -ne 0) { throw "Kontrola nie przeszła — zatrzymaj się." }
git apply --unidiff-zero .\android-sync\Android-DirectBoot-Audit-Followups.diff
```

Follow-up maskuje payload `tg_config` w toastach/logu aplikacji, kieruje lokalne żądania Telegrama przez klienta bez proxy/przekierowań i łączy anulowanie coroutine z `Call.cancel()` dla zwykłych żądań oraz pogody. Dodaje też test jednostkowy redaktora. Nie usuwa tokenu z samego polecenia wysyłanego do RTDB i nie dodaje autoryzacji/TLS do firmware.

## Końcowa grafika debug

Po zastosowaniu pełnego patcha albo nakładki skopiuj samodzielny plik PNG do zasobów wariantu debug:

```powershell
$debugIconDir = '.\android\app\src\debug\res\drawable-nodpi'
New-Item -ItemType Directory -Path $debugIconDir -Force | Out-Null
Copy-Item -LiteralPath .\android-sync\Android-DirectBoot-Debug-Icon-Final.png -Destination (Join-Path $debugIconDir 'ic_launcher_debug_art.png') -Force
```

Nie zmienia to produkcyjnej ikony. Nie nakładaj na Windows `Android-DirectBoot-Debug-Icon-Overlay.diff` — jego jedyną zmianą jest binarny obraz zastąpiony tutaj przez kopiowanie PNG.

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
