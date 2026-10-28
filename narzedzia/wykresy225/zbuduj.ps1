# zbuduj.ps1 - build aplikacji Sterownik CO (wersja debug) jedna komenda.
#
# Po co:
#  1. Gradle 8.9 nie dziala na Javie 25, a Android Studio 2026 daje w komplecie
#     wlasnie JBR 25. Objaw: "* What went wrong: 25.0.3" (sama wersja Javy).
#  2. Gradle nie wie, gdzie lezy Android SDK. Objaw:
#     "SDK location not found. Define a valid SDK location with an
#      ANDROID_HOME environment variable or by setting the sdk.dir path ...".
# Ten skrypt zalatwia oba: sam znajduje Jave 17-22 i samo SDK, zapisuje
# local.properties i dopiero potem buduje.
#
# Uzycie (w folderze projektu, tam gdzie lezy settings.gradle.kts):
#     powershell -ExecutionPolicy Bypass -File .\zbuduj.ps1
#
# Z wlasna sciezka:
#     powershell -ExecutionPolicy Bypass -File .\zbuduj.ps1 -Jdk "C:\Program Files\Eclipse Adoptium\jdk-21.0.12.101-hotspot" -Sdk "C:\Users\ty\AppData\Local\Android\Sdk"

param(
    [string]$Jdk = "",
    [string]$Sdk = "",
    [string]$OutApk = ""
)

Set-Location -LiteralPath $PSScriptRoot

$compileSdk = 35   # musi zgadzac sie z app/build.gradle.kts

# ---------------------------------------------------------------- JAVA --------
function Get-JavaVersionText {
    param([string]$JavaExe)
    if (-not (Test-Path -LiteralPath $JavaExe)) { return "" }
    $out = & $JavaExe -version 2>&1
    if ($out -is [array]) { return ($out -join " ") }
    return [string]$out
}

# JDK nadaje sie, gdy glowna wersja to 17-22 (Gradle 8.9 nie obsluguje Javy 25).
function Test-JdkUsable {
    param([string]$Home_)
    if ([string]::IsNullOrWhiteSpace($Home_)) { return $false }
    $txt = Get-JavaVersionText (Join-Path $Home_ "bin\java.exe")
    if ($txt -eq "") { return $false }
    if ($txt -match '"1\.8') { return $false }
    if ($txt -match '"(22|21|20|19|18|17)\.') { return $true }
    return $false
}

$foundJdk = $null
if ($Jdk -ne "") { if (Test-JdkUsable $Jdk) { $foundJdk = $Jdk } }

if ($null -eq $foundJdk) {
    $cands = New-Object System.Collections.ArrayList
    if ($env:JAVA_HOME) { [void]$cands.Add($env:JAVA_HOME) }
    $onPath = Get-Command java -ErrorAction SilentlyContinue
    if ($onPath) { [void]$cands.Add((Split-Path (Split-Path $onPath.Source -Parent) -Parent)) }
    foreach ($root in @("C:\Program Files\Eclipse Adoptium", "C:\Program Files\Java",
                        "C:\Program Files\Microsoft", "C:\Program Files\Amazon Corretto",
                        "C:\Program Files\Zulu", "C:\Program Files\BellSoft")) {
        if (Test-Path -LiteralPath $root) {
            Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue |
                ForEach-Object { [void]$cands.Add($_.FullName) }
        }
    }
    [void]$cands.Add("C:\Program Files\Android\Android Studio\jbr")
    foreach ($c in $cands) { if (Test-JdkUsable $c) { $foundJdk = $c; break } }
}

if ($null -eq $foundJdk) {
    Write-Host ""
    Write-Host "BLAD: nie znalazlem JDK w wersji 17-22."
    Write-Host "Java 25 z Android Studio NIE nadaje sie (Gradle 8.9 jej nie obsluguje)."
    Write-Host "Zainstaluj JDK 21 (Temurin): https://adoptium.net/temurin/releases/?version=21"
    Write-Host 'Albo podaj sciezke: -Jdk "C:\Program Files\Java\jdk-21"'
    exit 1
}

$env:JAVA_HOME = $foundJdk
Write-Host "== JDK: $foundJdk"
& (Join-Path $foundJdk "bin\java.exe") -version

# ----------------------------------------------------------------- SDK --------
# Prawdziwe SDK poznajemy po folderze "platforms" albo "build-tools".
function Test-SdkUsable {
    param([string]$Path_)
    if ([string]::IsNullOrWhiteSpace($Path_)) { return $false }
    if (-not (Test-Path -LiteralPath $Path_)) { return $false }
    return ((Test-Path -LiteralPath (Join-Path $Path_ "platforms")) -or
            (Test-Path -LiteralPath (Join-Path $Path_ "build-tools")))
}

$foundSdk = $null
if ($Sdk -ne "") {
    if (Test-SdkUsable $Sdk) { $foundSdk = $Sdk }
    else { Write-Host "UWAGA: podana sciezka SDK nie wyglada na Android SDK: $Sdk" }
}

if ($null -eq $foundSdk) {
    $sdkCands = New-Object System.Collections.ArrayList
    if ($env:ANDROID_HOME)    { [void]$sdkCands.Add($env:ANDROID_HOME) }
    if ($env:ANDROID_SDK_ROOT){ [void]$sdkCands.Add($env:ANDROID_SDK_ROOT) }
    [void]$sdkCands.Add((Join-Path $env:LOCALAPPDATA "Android\Sdk"))
    [void]$sdkCands.Add("C:\Android\Sdk")
    [void]$sdkCands.Add("C:\Program Files\Android\Sdk")
    [void]$sdkCands.Add("C:\Program Files (x86)\Android\android-sdk")

    foreach ($c in $sdkCands) { if (Test-SdkUsable $c) { $foundSdk = $c; break } }
}

if ($null -eq $foundSdk) {
    # Ostatnia proba: szukamy adb.exe w typowych miejscach (SDK zawsze je ma).
    Write-Host "== Szukam Android SDK po pliku adb.exe (moze potrwac chwile)..."
    $adb = Get-ChildItem -Path @("$env:LOCALAPPDATA", "$env:ProgramFiles", "C:\") `
                         -Filter "adb.exe" -Recurse -ErrorAction SilentlyContinue -Depth 6 |
           Select-Object -First 1
    if ($adb) {
        # ...\Sdk\platform-tools\adb.exe  ->  ...\Sdk
        $cand = Split-Path (Split-Path $adb.FullName -Parent) -Parent
        if (Test-SdkUsable $cand) { $foundSdk = $cand }
    }
}

if ($null -eq $foundSdk) {
    Write-Host ""
    Write-Host "BLAD: nie znalazlem Android SDK."
    Write-Host "Skoro masz Android Studio, SDK zwykle jest tutaj:"
    Write-Host ("  " + (Join-Path $env:LOCALAPPDATA "Android\Sdk"))
    Write-Host "Otworz Android Studio > Tools > SDK Manager i zainstaluj:"
    Write-Host "  - Android SDK Platform 35"
    Write-Host "  - Android SDK Build-Tools"
    Write-Host "Potem uruchom ponownie ten skrypt albo podaj sciezke:"
    Write-Host '  -Sdk "C:\Users\ty\AppData\Local\Android\Sdk"'
    exit 1
}

$env:ANDROID_HOME = $foundSdk
$env:ANDROID_SDK_ROOT = $foundSdk
Write-Host ""
Write-Host "== Android SDK: $foundSdk"

# local.properties: Gradle czyta wlasnie stad. W pliku .properties backslash jest
# znakiem ucieczki, dlatego zapisujemy sciezke z ukosnikami w prawo.
$sdkForProps = $foundSdk -replace '\\', '/'
$lp = "sdk.dir=$sdkForProps"
Set-Content -LiteralPath "local.properties" -Value $lp -Encoding ASCII
Write-Host "== Zapisano local.properties: $lp"

# Kontrola: czy jest platforma wymagana przez projekt (compileSdk).
$platDir = Join-Path $foundSdk "platforms"
$plats = @()
if (Test-Path -LiteralPath $platDir) {
    $plats = Get-ChildItem -LiteralPath $platDir -Directory -ErrorAction SilentlyContinue |
            Select-Object -ExpandProperty Name
}
Write-Host ("== Zainstalowane platformy: " + (($plats -join ", ") -replace '^$','(brak)'))
if ($plats -notcontains "android-$compileSdk") {
    Write-Host ""
    Write-Host "UWAGA: brakuje platformy android-$compileSdk, ktorej wymaga projekt."
    Write-Host "Android Studio > Tools > SDK Manager > zaznacz 'Android $compileSdk' > Apply."
    Write-Host "Bez tego build padnie na brakujacym android.jar."
    Write-Host ""
}

Write-Host "== Zatrzymuje stare demony Gradle..."
& .\gradlew.bat --stop 2>&1 | Out-Null

Write-Host ""
Write-Host "== Buduje APK (pierwszy raz moze potrwac kilka minut)..."
& .\gradlew.bat :app:assembleDebug --console=plain
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "BLAD: build nie wyszedl (kod $LASTEXITCODE)."
    Write-Host "Wklej ostatnie 30 linii z konsoli - poprawimy."
    exit $LASTEXITCODE
}

$apk = "app\build\outputs\apk\debug\app-debug.apk"
if (-not (Test-Path -LiteralPath $apk)) {
    Write-Host "BLAD: build sie udal, ale nie ma pliku $apk"
    exit 2
}

if ($OutApk -eq "") {
    $OutApk = Join-Path $env:USERPROFILE "Downloads\SterownikCO-v0.31.35-debug.apk"
}
Copy-Item -LiteralPath $apk -Destination $OutApk -Force

$info = Get-Item -LiteralPath $OutApk
Write-Host ""
Write-Host "== GOTOWE"
Write-Host ("APK: " + $info.FullName)
Write-Host ("Rozmiar: " + $info.Length + " B")
Write-Host ""
Write-Host "Instalacja kablem (telefon z wlaczonym debugowaniem USB):"
Write-Host ('  & "' + (Join-Path $foundSdk "platform-tools\adb.exe") + '" install -r "' + $info.FullName + '"')
