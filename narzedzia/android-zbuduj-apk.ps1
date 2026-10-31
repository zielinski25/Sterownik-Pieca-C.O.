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
if (-not $Zip) {
  $paczka = Get-ChildItem -LiteralPath "$env:USERPROFILE\Downloads", ([Environment]::GetFolderPath('Desktop')) `
    -Filter 'SterownikCO-Android-v*.zip' -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime | Select-Object -Last 1
  if (-not $paczka) {
    throw "Nie znalazlem zadnej paczki SterownikCO-Android-v*.zip w Pobranych ani na Pulpicie. Podaj sciezke: -Zip ""C:\...\plik.zip"""
  }
  $Zip = $paczka.FullName
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
if (-not (Test-Path -LiteralPath $apk)) { throw "Gradle zameldowal sukces, ale nie ma pliku: $apk" }
$cel = Join-Path ([Environment]::GetFolderPath('Desktop')) "SterownikCO-$wersja-debug.apk"
Copy-Item -LiteralPath $apk -Destination $cel -Force

$plik = Get-Item -LiteralPath $cel
Write-Host ""
Write-Host "GOTOWE" -ForegroundColor Green
Write-Host "APK:   $($plik.FullName)"
Write-Host "Rozmiar: $([math]::Round($plik.Length / 1MB, 1)) MB"
Write-Host "Wersja w Ustawieniach aplikacji: v$wersja-debug (pakiet com.sterownikco.pro.debug)"
