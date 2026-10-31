# 237 — APK prosto na pulpit (wersja 0.31.55)

Data: 2026-10-30. Dotyczy: aplikacja Android **0.31.55** (`versionCode 31055`).

## Najpierw uczciwie: dlaczego nie dam gotowego pliku .apk

APK to nie jest coś, co się „przepisuje" — trzeba go **skompilować**, a do tego
potrzebny jest Android SDK (kilkaset MB) i Gradle. W moim środowisku SDK **nie ma**
i nie mogę go pobrać (serwery Google i Gradle są dla mnie zablokowane). Dlatego
zamiast pliku dostajesz **polecenie, które robi to samo u Ciebie** — i na końcu
samo wrzuca gotowy plik na pulpit.

Sprawdzone przed napisaniem tego: `ANDROID_HOME` pusty, brak `sdkmanager`,
brak `gradle`, brak katalogu `platform-tools`. Budowanie tutaj jest niemożliwe.

## Co robi polecenie (jedna linia)

1. pobiera paczkę 0.31.55 z Dysku do `%TEMP%\v0.31.55.zip` (jeśli już jest — nie pobiera drugi raz),
2. sprawdza SHA-256 pobranego ZIP-a: ma być `762FFAF1EB6D213190B0ABD4E2CF3512FC8024B04392C53EEE94BBEF65F523BA`,
3. rozpakowuje do `%USERPROFILE%\Documents\SterownikCO-0.31.55` (jeśli jeszcze nie rozpakowane),
4. znajduje Android SDK — najpierw w starym projekcie `SterownikCO-0.31.49` (z `local.properties`), potem w `%LOCALAPPDATA%\Android\Sdk`, potem w `ANDROID_HOME` / `ANDROID_SDK_ROOT`, `C:\Android\Sdk`, `D:\Android\Sdk`,
5. ustawia `ANDROID_HOME` + `ANDROID_SDK_ROOT` (dlatego **nie** trzeba tworzyć `local.properties` i nie ma błędu `SDK location not found`),
6. ustawia JDK z Android Studio (`jbr`), jeśli `JAVA_HOME` nie jest ustawiony,
7. buduje `assembleDebug`,
8. **kopiuje gotowy APK na pulpit jako `SterownikCO-0.31.55.apk`** (pulpit brany przez `[Environment]::GetFolderPath('Desktop')`, więc zadziała też, gdy pulpit jest przeniesiony do OneDrive),
9. wypisuje pełną ścieżkę, rozmiar i datę pliku.

## Polecenie

```
$u="https://drive.google.com/uc?export=download&id=15mzOoGYD29Amsiorq8KdU8KpNL-F3DfP"; $z="$env:TEMP\v0.31.55.zip"; $d="$env:USERPROFILE\Documents\SterownikCO-0.31.55"; $desk=[Environment]::GetFolderPath('Desktop'); if((-not (Test-Path -LiteralPath $z)) -or ((Get-Item -LiteralPath $z).Length -lt 1000000)){ Invoke-WebRequest -Uri $u -OutFile $z -UseBasicParsing }; $h=(Get-FileHash -LiteralPath $z -Algorithm SHA256).Hash; if($h -ne "762FFAF1EB6D213190B0ABD4E2CF3512FC8024B04392C53EEE94BBEF65F523BA"){ Write-Host "UWAGA: suma ZIP inna niz oczekiwana: $h" } else { Write-Host "SUMA ZIP OK" }; if(-not (Test-Path -LiteralPath "$d\gradlew.bat")){ Expand-Archive -LiteralPath $z -DestinationPath $d -Force }; $lp="$env:USERPROFILE\Documents\SterownikCO-0.31.49\android\local.properties"; $sdk=$null; if(Test-Path -LiteralPath $lp){ $m=Select-String -LiteralPath $lp -Pattern '^sdk\.dir=(.+)$' | Select-Object -First 1; if($m){ $sdk=$m.Matches[0].Groups[1].Value.Replace('\:',':').Replace('\\','\') } }; if(-not $sdk){ $sdk=@("$env:LOCALAPPDATA\Android\Sdk","$env:ANDROID_HOME","$env:ANDROID_SDK_ROOT","C:\Android\Sdk","D:\Android\Sdk") | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -First 1 }; if($sdk){ $env:ANDROID_HOME=$sdk; $env:ANDROID_SDK_ROOT=$sdk; Write-Host "SDK: $sdk" } else { Write-Host "BRAK SDK — ustaw ANDROID_HOME recznie" }; $jbr="C:\Program Files\Android\Android Studio\jbr"; if((-not $env:JAVA_HOME) -and (Test-Path -LiteralPath $jbr)){ $env:JAVA_HOME=$jbr; Write-Host "JDK: $jbr" }; cd $d; .\gradlew.bat assembleDebug; $apk="$d\app\build\outputs\apk\debug\app-debug.apk"; if(Test-Path -LiteralPath $apk){ Copy-Item -LiteralPath $apk -Destination "$desk\SterownikCO-0.31.55.apk" -Force; Write-Host ""; Write-Host "=== GOTOWE — APK NA PULPICIE ==="; Get-Item -LiteralPath "$desk\SterownikCO-0.31.55.apk" | Format-List FullName,Length,LastWriteTime } else { Write-Host ""; Write-Host "=== NIE ZBUDOWALO SIE — patrz komunikat wyzej ===" }
```

## Co ma wyjść

```
SUMA ZIP OK
SDK: C:\Users\Rafcio\AppData\Local\Android\Sdk
JDK: C:\Program Files\Android\Android Studio\jbr
BUILD SUCCESSFUL in ...

=== GOTOWE — APK NA PULPICIE ===

FullName      : C:\Users\Rafcio\Desktop\SterownikCO-0.31.55.apk
Length        : 12xxxxx
LastWriteTime : 30.10.2026 ...
```

Na pulpicie ma być **jeden** plik: `SterownikCO-0.31.55.apk`. Rozmiar zwykle
10–15 MB (zależy od tego, ile zasobów wchodzi do debug-builda).

## Potem: instalacja na telefonie

1. Podłącz telefon kablem (tryb „transfer plików") albo wyślij APK na telefon.
2. Skopiuj `SterownikCO-0.31.55.apk` do pamięci telefonu.
3. Otwórz plik na telefonie → „Zainstaluj" (Android zapyta o zgodę na instalację
   z nieznanych źródeł, jeśli to pierwszy raz).
4. W aplikacji: Ustawienia → na dole ma być **`v0.31.55`**.

## Gdy coś nie wyjdzie

| komunikat | co zrobić |
|---|---|
| `BRAK SDK — ustaw ANDROID_HOME recznie` | w Android Studio: Settings → Languages & Frameworks → Android SDK, skopiuj „Android SDK Location" i wpisz `$env:ANDROID_HOME="ta sciezka"` w tym samym oknie PowerShella, potem puść polecenie jeszcze raz |
| `UWAGA: suma ZIP inna niz oczekiwana` | pobrał się stary plik z pamięci: skasuj `%TEMP%\v0.31.55.zip` i puść polecenie ponownie |
| `NIE ZBUDOWALO SIE` | przewiń wyżej — Gradle wypisuje prawdziwy powód (najczęściej brak akceptacji licencji SDK albo inna wersja JDK; aplikacja wymaga JDK 17–21) |
| plik na pulpicie ma 0–1 KB | to nie APK, tylko błąd zapisu — usuń i powtórz kopiowanie |

## Powiązane

- Paczka (ZIP, 1 165 938 B): Dysk `15mzOoGYD29Amsiorq8KdU8KpNL-F3DfP`
- Wcześniejszy dokument: `narzedzia/jak-zbudowac-apk-2026-10-30/236-JEDNO-POLECENIE-APK.md`
- Wszystkie linki: `narzedzia/paczki-udostepnione/LINKI-2026-10-29.md`
