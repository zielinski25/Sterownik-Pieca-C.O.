# 236 — jedno polecenie: od zera do APK (0.31.54)

Data 2026-10-30. Powód: `SDK location not found` po rozpakowaniu paczki w nowym
folderze. Paczka jest dobra — brakuje tylko `local.properties`, który jest
w `.gitignore` i celowo nie jedzie w ZIP-ie (zależy od komputera, nie od kodu).

## Dlaczego nie ma go w paczce

```
.gitignore
  local.properties      <- dlatego go nie ma
```

Android Studio tworzy ten plik sam przy otwieraniu projektu, a Gradle bez niego
nie wie, gdzie leży SDK. Twój stary folder `SterownikCO-0.31.49` go miał —
nowy `0.31.54` nie ma, bo to świeże rozpakowanie.

## Rozwiązanie: jedno polecenie

Robi wszystko po kolei:

1. jeśli folderu nie ma — pobiera ZIP z Dysku i rozpakowuje
2. szuka SDK: najpierw w **starym projekcie** (`0.31.49\android\local.properties`),
   potem w typowych miejscach (`%LOCALAPPDATA%\Android\Sdk` itd.)
3. ustawia `ANDROID_HOME` (nie trzeba nic ręcznie wpisywać w pliku)
4. jeśli brakuje JDK — bierze to z Android Studio (`jbr`, wersja 21)
5. buduje APK
6. wypisuje ścieżkę, rozmiar i datę gotowego pliku

```powershell
$z="$env:USERPROFILE\Desktop\v0.31.54.zip"; $d="$env:USERPROFILE\Documents\SterownikCO-0.31.54"; if(-not (Test-Path -LiteralPath "$d\gradlew.bat")){ if(-not (Test-Path -LiteralPath $z)){ Invoke-WebRequest "https://drive.google.com/uc?export=download&id=13TmkcbYuo_oVflQJNblJ_Ovt20PvS7SJ" -OutFile $z }; Expand-Archive -LiteralPath $z -DestinationPath $d -Force }; $lp="$env:USERPROFILE\Documents\SterownikCO-0.31.49\android\local.properties"; $sdk=$null; if(Test-Path -LiteralPath $lp){ $m=Select-String -LiteralPath $lp -Pattern '^sdk\.dir=(.+)$' | Select-Object -First 1; if($m){ $sdk=$m.Matches[0].Groups[1].Value.Replace('\:',':').Replace('\\','\') } }; if(-not $sdk){ $sdk=@("$env:LOCALAPPDATA\Android\Sdk","$env:ANDROID_HOME","$env:ANDROID_SDK_ROOT","C:\Android\Sdk","D:\Android\Sdk") | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -First 1 }; if($sdk){ $env:ANDROID_HOME=$sdk; $env:ANDROID_SDK_ROOT=$sdk; Write-Host "SDK: $sdk" } else { Write-Host "BRAK SDK" }; $jbr="C:\Program Files\Android\Android Studio\jbr"; if((-not $env:JAVA_HOME) -and (Test-Path -LiteralPath $jbr)){ $env:JAVA_HOME=$jbr; Write-Host "JDK: $jbr" }; cd $d; .\gradlew.bat assembleDebug; $apk="$d\app\build\outputs\apk\debug\app-debug.apk"; if(Test-Path -LiteralPath $apk){ Write-Host ""; Write-Host "=== GOTOWE ==="; Get-Item -LiteralPath $apk | Format-List FullName,Length,LastWriteTime } else { Write-Host ""; Write-Host "=== NIE ZBUDOWALO SIE ===" }
```

## Co ma wypisać

```
SDK: C:\Users\Rafcio\AppData\Local\Android\Sdk
JDK: C:\Program Files\Android\Android Studio\jbr
[... Gradle ...]
BUILD SUCCESSFUL in 1m 20s

=== GOTOWE ===

FullName      : C:\Users\Rafcio\Documents\SterownikCO-0.31.54\app\build\outputs\apk\debug\app-debug.apk
Length        : 12345678
LastWriteTime : 30.10.2026 11:45:12
```

## Gdy wypisze `BRAK SDK`

To znaczy, że na komputerze nie ma zainstalowanego SDK Androida — i wtedy żadne
polecenie tego nie obejdzie. Trzeba:

1. zainstalować **Android Studio** (przy instalacji zaznaczyć „Android SDK")
2. otworzyć w nim folder `C:\Users\Rafcio\Documents\SterownikCO-0.31.54`
3. powtórzyć polecenie — teraz znajdzie SDK

## Gdy build padnie na czymś innym

| błąd | co zrobić |
|---|---|
| `Failed to find target with hash string 'android-35'` | Android Studio → SDK Manager → zaznacz **Android 15 (API 35)** |
| `Unsupported class file major version` | złe JDK; otwórz projekt w Android Studio i buduj stamtąd |
| `Could not resolve…` | brak internetu — pierwszy build ciągnie biblioteki z Maven |

W każdym z tych przypadków: **wklej mi ostatnie 30 linii**, nie cały log.

## Dlaczego nie wpisuję SDK do `local.properties` na sztywno

Ścieżka `sdk.dir` wymaga podwójnych backslashi (`C\:\\Users\\…`). Łatwo się
na tym potknąć, a zły zapis daje mylący komunikat. Zmienna `ANDROID_HOME`
działa tak samo, a nie wymaga żadnego escapowania — Gradle czyta ją, gdy
`local.properties` nie istnieje.
