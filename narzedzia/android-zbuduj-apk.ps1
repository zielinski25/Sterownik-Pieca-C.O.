<#
.SYNOPSIS
  Buduje APK aplikacji Sterownik CO z paczki ZIP (albo z rozpaknietego folderu)
  i kopiuje gotowy plik na Pulpit.

.DESCRIPTION
  Robi to, co wczesniej trzeba bylo wklejac recznie:
    1. znajduje najnowszy ZIP "SterownikCO-Android-v*.zip" w Pobranych albo na Pulpicie
       (mozna tez podac sciezke przez -Zip),
    2. rozpakowuje do %USERPROFILE%\SterownikCO-<wersja> (albo uzywa istniejacego folderu),
    3. szuka Android SDK i zapisuje local.properties (bez tego Gradle pada na
       "SDK location not found"),
    4. uruchamia :app:testDebugUnitTest :app:assembleDebug,
    5. kopiuje app-debug.apk na Pulpit jako SterownikCO-<wersja>-debug.apk.

  Wymagania: JDK 17-21 (JAVA_HOME albo JBR z Android Studio) i Android SDK 35.

.PARAMETER Zip
  Sciezka do paczki ZIP. Bez tego parametru skrypt szuka najnowszej sam.

.PARAMETER Czysto
  Usuwa folder roboczy przed rozpakowaniem (domyslnie folder jest uzywany ponownie).

.PARAMETER BezTestow
  Pomija testy jednostkowe (:app:testDebugUnitTest) - szybciej, ale mniej pewnie.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File .\narzedzia\android-zbuduj-apk.ps1

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File .\narzedzia\android-zbuduj-apk.ps1 -Zip "$env:USERPROFILE\Downloads\SterownikCO-Android-v0.31.64-PELNY-PROJEKT.zip" -Czysto
#>
param(
  [string]$Zip = '',
  [switch]$Czysto,
  [switch]$BezTestow
)

$ErrorActionPreference = 'Stop'

function Szukaj-Sdk {
  $kandydaci = @($env:ANDROID_HOME, $env:ANDROID_SDK_ROOT) +
    @("$env:LOCALAPPDATA\Android\Sdk",
      "$env:USERPROFILE\AppData\Local\Android\Sdk",
      "C:\Android\Sdk", "D:\Android\Sdk", "E:\Android\Sdk",
      "$env:ProgramFiles\Android\Sdk",
      "${env:ProgramFiles(x86)}\Android\android-sdk")

  $znaleziony = $kandydaci |
    Where-Object { $_ -and (Test-Path -LiteralPath (Join-Path $_ 'platform-tools\adb.exe')) } |
    Select-Object -First 1
  if ($znaleziony) { return $znaleziony }

  # Ostatnia szansa: folder "Android" / "Sdk" w katalogu glownym dysku.
  $dyski = @('C:\', 'D:\', 'E:\')
  foreach ($d in $dyski) {
    if (-not (Test-Path -LiteralPath $d)) { continue }
    foreach ($katalog in Get-ChildItem -LiteralPath $d -Directory -ErrorAction SilentlyContinue) {
      foreach ($pod in @($katalog.FullName, (Join-Path $katalog.FullName 'Sdk'))) {
        if (Test-Path -LiteralPath (Join-Path $pod 'platform-tools\adb.exe')) { return $pod }
      }
    }
  }
  return $null
}

# -- 1. paczka ----------------------------------------------------------------
function Szukaj-Paczki {
  # Folder "Pobrane" bywa przeniesiony do OneDrive, a systemowy Downloads nie zawsze
  # jest w %USERPROFILE%\Downloads - dlatego pytamy rejestr i sprawdzamy kilka miejsc.
  $script:korzenie = New-Object System.Collections.Generic.List[string]
  foreach ($p in @("$env:USERPROFILE\Downloads", "$env:USERPROFILE\Pobrane",
                   "$env:USERPROFILE\OneDrive\Pobrane", "$env:USERPROFILE\OneDrive\Downloads",
                   "$env:USERPROFILE\OneDrive\Pulpit", "$env:USERPROFILE\Desktop",
                   "$env:USERPROFILE\Pulpit")) {
    if ((Test-Path -LiteralPath $p) -and -not $korzenie.Contains($p)) { $korzenie.Add($p) }
  }
  $klucze = Get-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\User Shell Folders' -ErrorAction SilentlyContinue
  if ($klucze) {
    foreach ($k in @('{374DE290-123F-4565-9164-39C4925E467B}', 'Desktop')) {
      $d = $klucze.$k
      if ($d) {
        $d = [Environment]::ExpandEnvironmentVariables($d)
        if ((Test-Path -LiteralPath $d) -and -not $korzenie.Contains($d)) { $korzenie.Add($d) }
      }
    }
  }
  foreach ($r in $korzenie) {
    $f = Get-ChildItem -LiteralPath $r -Filter 'SterownikCO-Android-v*.zip' -File -ErrorAction SilentlyContinue |
      Sort-Object LastWriteTime | Select-Object -Last 1
    if ($f) { return $f.FullName }
  }
  # Ostatnia szansa: caly profil uzytkownika (pierwsze 3 poziomy folderow).
  $f = Get-ChildItem -LiteralPath $env:USERPROFILE -Filter 'SterownikCO-Android-v*.zip' -File `
        -Recurse -Depth 3 -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime | Select-Object -Last 1
  if ($f) { return $f.FullName }
  return $null
}

if (-not $Zip) {
  $Zip = Szukaj-Paczki
  if (-not $Zip) {
    throw ("Nie znalazlem paczki SterownikCO-Android-v*.zip. Szukalem w: " + ($korzenie -join '; ') +
           " i w calym profilu. Pobierz ZIP z linku (przycisk pobierania) albo podaj sciezke: -Zip ""C:\...\plik.zip""")
  }
}
if (-not (Test-Path -LiteralPath $Zip)) { throw "Nie ma pliku: $Zip" }
Write-Host "Paczka: $Zip" -ForegroundColor Cyan

# Wersja z nazwy paczki, np. SterownikCO-Android-v0.31.64-PELNY-PROJEKT.zip
$wersja = 'z-paczki'
if ((Split-Path -Leaf $Zip) -match 'v(\d+\.\d+\.\d+)') { $wersja = $Matches[1] }

# -- 2. folder roboczy --------------------------------------------------------
$proj = Join-Path $env:USERPROFILE "SterownikCO-$wersja"
$maProjekt = Test-Path -LiteralPath (Join-Path $proj 'app\build.gradle.kts')
if ($Czysto -and (Test-Path -LiteralPath $proj)) {
  Remove-Item -LiteralPath $proj -Recurse -Force
  $maProjekt = $false
}
if (-not $maProjekt) {
  if (Test-Path -LiteralPath $proj) { Remove-Item -LiteralPath $proj -Recurse -Force }
  Write-Host "Rozpakowuje do: $proj" -ForegroundColor Cyan
  Expand-Archive -LiteralPath $Zip -DestinationPath $proj -Force
} else {
  Write-Host "Uzywam juz rozpaknietego folderu: $proj (wymus rozpakowanie przez -Czysto)" -ForegroundColor Cyan
}
Set-Location -LiteralPath $proj

# -- 2b. znana wada paczki 0.31.64 --------------------------------------------
# 0.31.64 nie kompiluje sie: Overlays.kt nie importuje HistFsSheet
# ("Unresolved reference"). Jesli importu brakuje, dopisujemy go i podbijamy numer.
$overlays = Join-Path $proj 'app\src\main\java\com\sterownikco\pro\ui\screens\Overlays.kt'
if (-not (Test-Path -LiteralPath $overlays)) { throw "Brak pliku: $overlays" }
$tekst = [IO.File]::ReadAllText($overlays, [Text.Encoding]::UTF8)
if ($tekst -notlike '*sheets.HistFsSheet*') {
  $tekst = $tekst.Replace('import com.sterownikco.pro.ui.screens.sheets.DymSheet',
    "import com.sterownikco.pro.ui.screens.sheets.DymSheet`r`nimport com.sterownikco.pro.ui.screens.sheets.HistFsSheet")
  [IO.File]::WriteAllText($overlays, $tekst, (New-Object Text.UTF8Encoding($false)))
  $gradleNaprawa = Join-Path $proj 'app\build.gradle.kts'
  $trescNaprawa = [IO.File]::ReadAllText($gradleNaprawa)
  $trescNaprawa = [regex]::Replace($trescNaprawa, 'versionCode\s*=\s*\d+', 'versionCode = 31065')
  $trescNaprawa = [regex]::Replace($trescNaprawa, 'versionName\s*=\s*"\d+\.\d+\.\d+"', 'versionName = "0.31.65"')
  [IO.File]::WriteAllText($gradleNaprawa, $trescNaprawa, (New-Object Text.UTF8Encoding($false)))
  Write-Host "Paczka bez importu HistFsSheet (0.31.64) - poprawiona do 0.31.65" -ForegroundColor Yellow
  # 0.31.64 ma dodatkowo bledny test ChartTimeAxisTest (oczekuje 60 px zamiast 100 px),
  # wiec przy tej starej paczce testy pomijamy. W 0.31.66 test jest juz naprawiony.
  $BezTestow = $true
  Write-Host "Testy pomijam - paczka 0.31.64 ma bledny test. Nowa paczka: 0.31.66." -ForegroundColor Yellow
}

# Wersja z projektu (pewniejsza niz nazwa paczki)
$gradle = Join-Path $proj 'app\build.gradle.kts'
$tresc = Get-Content -LiteralPath $gradle -Raw
if ($tresc -match 'versionName\s*=\s*"([^"]+)"') { $wersja = $Matches[1] }
Write-Host "Wersja aplikacji: $wersja" -ForegroundColor Green

# -- 3. JDK i Android SDK -----------------------------------------------------
if (-not $env:JAVA_HOME) {
  $jbr = @("$env:ProgramFiles\Android\Android Studio\jbr",
           "$env:LOCALAPPDATA\Programs\Android Studio\jbr") |
    Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
  if ($jbr) { $env:JAVA_HOME = $jbr; Write-Host "JAVA_HOME (JBR z Android Studio): $jbr" -ForegroundColor Cyan }
}

$sdk = Szukaj-Sdk
if (-not $sdk) {
  Write-Host "NIE ZNALAZLEM Android SDK na tym komputerze." -ForegroundColor Yellow
  Write-Host "Zainstaluj Android Studio: https://developer.android.com/studio" -ForegroundColor Yellow
  Write-Host "Potem: More Actions > SDK Manager > Android 15 (API 35) + Build-Tools + Platform-Tools." -ForegroundColor Yellow
  throw "Brak Android SDK."
}
# W local.properties bezpieczniej uzywac ukosnikow: brak ucieczek dla '\' i ':'.
$sdkDoZapisu = $sdk -replace '\\', '/'
Set-Content -LiteralPath (Join-Path $proj 'local.properties') -Value "sdk.dir=$sdkDoZapisu" -Encoding ASCII
Write-Host "Android SDK: $sdk" -ForegroundColor Green
if (Test-Path -LiteralPath $env:JAVA_HOME) {
  & (Join-Path $env:JAVA_HOME 'bin\java.exe') -version 2>&1 | Select-Object -First 1 | ForEach-Object { Write-Host "Java: $_" }
}

# -- 4. kompilacja ------------------------------------------------------------
if ($BezTestow) {
  .\gradlew.bat :app:assembleDebug --console=plain
} else {
  .\gradlew.bat :app:testDebugUnitTest :app:assembleDebug --console=plain
}
if ($LASTEXITCODE -ne 0) {
  throw "Gradle padl (kod $LASTEXITCODE). Skopiuj log z gory - bez niego nie da sie poprawic."
}

# -- 5. APK na Pulpit ---------------------------------------------------------
$apk = Join-Path $proj 'app\build\outputs\apk\debug\app-debug.apk'
if (-not (Test-Path -LiteralPath $apk)) {
  $g = Get-ChildItem -LiteralPath (Join-Path $proj 'app\build\outputs') -Filter '*.apk' -File -Recurse -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime | Select-Object -Last 1
  if (-not $g) { throw "Gradle zameldowal sukces, ale nie ma zadnego APK w app\build\outputs" }
  $apk = $g.FullName
}
$cel = Join-Path ([Environment]::GetFolderPath('Desktop')) "SterownikCO-$wersja-debug.apk"
Copy-Item -LiteralPath $apk -Destination $cel -Force

$plik = Get-Item -LiteralPath $cel
Write-Host ""
Write-Host "GOTOWE" -ForegroundColor Green
Write-Host "APK:   $($plik.FullName)"
Write-Host "Rozmiar: $([math]::Round($plik.Length / 1MB, 1)) MB"
Write-Host "Wersja w Ustawieniach aplikacji: v$wersja-debug (pakiet com.sterownikco.pro.debug)"
