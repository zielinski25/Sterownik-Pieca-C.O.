# ============================================================================
#  Sterownik CO - Android 0.31.70 : od ZIP-a do APK na pulpicie
#  Ten sam przepis co przy 0.31.69, zmienione tylko: numer wersji, suma MD5
#  i katalog roboczy.
#
#  URUCHOMIENIE (dwie drogi):
#    A) wklej całą jedną linię z czatu do okna PowerShell
#    B) albo w tym katalogu:   powershell -ExecutionPolicy Bypass -File buduj-0.31.70.ps1
#
#  WYMAGA: ZIP w Pobranych albo na Pulpicie, Android SDK, JDK z Android Studio.
# ============================================================================

$ErrorActionPreference = 'Stop'

# 1) znajdz ZIP (najnowszy z Pobranych/Pulpitu)
$z = (Get-ChildItem -LiteralPath "$env:USERPROFILE\Downloads","$env:USERPROFILE\Desktop" `
        -Filter 'SterownikCO-Android-v0.31.70*.zip' -ErrorAction SilentlyContinue |
      Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
if (-not $z) { throw 'Nie znalazlem ZIP-a 0.31.70 w Pobranych ani na Pulpicie - pobierz go z linku na Dysku' }

# 2) sprawdz, czy to na pewno ta paczka (MD5 = 063D01BA...807A, 1 196 952 B)
$md5 = (Get-FileHash -LiteralPath $z -Algorithm MD5).Hash.ToLower()
Write-Host ("ZIP: {0}  {1} B  MD5 {2}" -f $z, (Get-Item -LiteralPath $z).Length, $md5) -ForegroundColor Cyan
if ($md5 -ne '063d01bab8cae9ba69fd7005fccb807a') { throw "Zly ZIP (MD5 $md5) - pobierz jeszcze raz z linku" }

# 3) rozpakuj do czystego katalogu
$work = Join-Path $env:TEMP 'app03170'
if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force }
Expand-Archive -LiteralPath $z -DestinationPath $work -Force
$root = (Get-ChildItem -LiteralPath $work -Recurse -Depth 3 -Filter gradlew.bat | Select-Object -First 1).Directory.FullName
if (-not $root) { throw 'W paczce nie ma gradlew.bat' }
Write-Host "Projekt: $root" -ForegroundColor Cyan

# 4) sprawdz numer wersji W SRODKU paczki
$gb = (@('app\build.gradle.kts','app\build.gradle') | Where-Object { Test-Path -LiteralPath (Join-Path $root $_) } | Select-Object -First 1)
$bb = Get-Content -LiteralPath (Join-Path $root $gb) -Raw
$vn = [regex]::Match($bb,'versionName\s*=?\s*"([^"]+)"').Groups[1].Value
$vc = [regex]::Match($bb,'versionCode\s*=?\s*(\d+)').Groups[1].Value
Write-Host "Wersja w paczce: $vn (versionCode $vc)" -ForegroundColor Yellow
if ($vn -ne '0.31.70') { throw "To nie 0.31.70, tylko '$vn' - zla paczka" }

# 4b) paczka 0.31.70 musi miec plik motywu dla wyciecia kamery
$cut = Join-Path $root 'app\src\main\res\values-v27\themes.xml'
if (-not (Test-Path -LiteralPath $cut)) { throw 'Brak res\values-v27\themes.xml - to nie jest 0.31.70' }
Write-Host 'Motyw wyciecia kamery: jest' -ForegroundColor Yellow

# 5) wskaż SDK i JDK
$sdk = $env:ANDROID_HOME
if (-not $sdk) { $sdk = Join-Path $env:LOCALAPPDATA 'Android\Sdk' }
if (-not (Test-Path -LiteralPath $sdk)) { throw "Brak Android SDK: $sdk" }
Set-Content -LiteralPath (Join-Path $root 'local.properties') -Value ('sdk.dir=' + ($sdk -replace '\\','\\')) -Encoding ASCII
if (-not $env:JAVA_HOME) {
  foreach ($j in @('C:\Program Files\Android\Android Studio\jbr','C:\Program Files\Android\Android Studio\jre')) {
    if (Test-Path -LiteralPath $j) { $env:JAVA_HOME = $j; break }
  }
}

# 6) zbuduj (testy + APK)
cd $root
& (Join-Path $root 'gradlew.bat') --no-daemon testDebugUnitTest assembleDebug
if ($LASTEXITCODE -ne 0) { throw "Gradle padl (kod $LASTEXITCODE)" }

# 7) skopiuj APK na pulpit
$apk = Join-Path $root 'app\build\outputs\apk\debug\app-debug.apk'
if (-not (Test-Path -LiteralPath $apk)) { throw 'Brak app-debug.apk' }
$dst = Join-Path ([Environment]::GetFolderPath('Desktop')) 'SterownikCO-Android-v0.31.70.apk'
Copy-Item -LiteralPath $apk -Destination $dst -Force
$f = Get-Item -LiteralPath $dst
Write-Host ("APK: {0}" -f $dst) -ForegroundColor Green
Write-Host ("{0} B  SHA256 {1}" -f $f.Length, (Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash) -ForegroundColor Green
