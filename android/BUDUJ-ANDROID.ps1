# Budowanie aplikacji SterownikCO z TEGO folderu (bez pobierania czegokolwiek).
# Uzycie:  powershell -NoProfile -ExecutionPolicy Bypass -File .\BUDUJ-ANDROID.ps1
# Wynik:   Downloads\SterownikCO-<wersja>-debug.apk
$ErrorActionPreference = 'Stop'
$d = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not (Test-Path "$d\gradlew.bat")) { throw "Uruchom ten skrypt z folderu projektu (brak gradlew.bat w $d)" }
$ver = (Select-String -Path "$d\app\build.gradle.kts" -Pattern 'versionName\s*=\s*"([^"]+)"').Matches[0].Groups[1].Value
"Projekt: $d   wersja: $ver"

# Android SDK
$sdk = @($env:ANDROID_HOME, $env:ANDROID_SDK_ROOT, "$env:LOCALAPPDATA\Android\Sdk", "C:\Android\Sdk") |
  Where-Object { $_ -and (Test-Path (Join-Path $_ 'platforms')) } | Select-Object -First 1
if (-not $sdk) { throw "Nie znalazlem Android SDK (Android Studio -> SDK Manager -> Android SDK Platform 35)" }
Set-Content "$d\local.properties" ("sdk.dir=" + $sdk.Replace('\','\\').Replace(':','\:')) -Encoding ASCII
"SDK: $sdk"

# JDK 17-22 (Gradle 8.9): JAVA_HOME albo JBR z Android Studio
$jbr = "C:\Program Files\Android\Android Studio\jbr"
if ((-not $env:JAVA_HOME) -and (Test-Path $jbr)) { $env:JAVA_HOME = $jbr }
"JDK: $env:JAVA_HOME"

Set-Location $d
& .\gradlew.bat --no-daemon :app:testDebugUnitTest :app:assembleDebug
if ($LASTEXITCODE -ne 0) { throw "Gradle zakonczyl sie bledem (kod $LASTEXITCODE)" }

$apk = "$env:USERPROFILE\Downloads\SterownikCO-$ver-debug.apk"
Copy-Item "$d\app\build\outputs\apk\debug\app-debug.apk" $apk -Force
""
"=== GOTOWE: $apk ==="
Get-FileHash $apk -Algorithm SHA256 | Format-List Hash
