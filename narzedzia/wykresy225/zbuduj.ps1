# zbuduj.ps1 - build aplikacji Sterownik CO (wersja debug) jedna komenda.
#
# Po co: Gradle 8.9 nie dziala na Javie 25, a Android Studio 2026 daje w komplecie
# wlasnie JBR 25. Skutek to mylacy blad "* What went wrong: 25.0.3" (sama wersja
# Javy, bez zadnego opisu). Ten skrypt sam znajduje JDK 17 albo 21 i nim buduje.
#
# Uzycie (w folderze projektu, tam gdzie lezy settings.gradle.kts):
#     powershell -ExecutionPolicy Bypass -File .\zbuduj.ps1
#
# Opcjonalnie wlasna sciezka do JDK:
#     powershell -ExecutionPolicy Bypass -File .\zbuduj.ps1 -Jdk "C:\Program Files\Eclipse Adoptium\jdk-21.0.12.1-hotspot"

param(
    [string]$Jdk = "",
    [string]$OutApk = ""
)

Set-Location -LiteralPath $PSScriptRoot

function Get-JavaVersionText {
    param([string]$JavaExe)
    if (-not (Test-Path -LiteralPath $JavaExe)) { return "" }
    $out = & $JavaExe -version 2>&1
    if ($out -is [array]) { return ($out -join " ") }
    return [string]$out
}

# JDK nadaje sie, gdy glowna wersja to 17, 18, 19, 20, 21 albo 22
# (Gradle 8.9 obsluguje do 22; Java 25 NIE dziala).
function Test-JdkUsable {
    param([string]$Home_)
    if ([string]::IsNullOrWhiteSpace($Home_)) { return $false }
    $txt = Get-JavaVersionText (Join-Path $Home_ "bin\java.exe")
    if ($txt -eq "") { return $false }
    if ($txt -match '"1\.8') { return $false }
    if ($txt -match '"(2[2]|2[1]|20|19|18|17)\.') { return $true }
    return $false
}

$found = $null

if ($Jdk -ne "") {
    if (Test-JdkUsable $Jdk) { $found = $Jdk }
    else {
        Write-Host "UWAGA: podany JDK nie jest w wersji 17-22:" $Jdk
        Write-Host (Get-JavaVersionText (Join-Path $Jdk "bin\java.exe"))
    }
}

if ($null -eq $found) {
    $candidates = New-Object System.Collections.ArrayList
    if ($env:JAVA_HOME) { [void]$candidates.Add($env:JAVA_HOME) }

    # java z PATH
    $onPath = Get-Command java -ErrorAction SilentlyContinue
    if ($onPath) {
        $p = Split-Path (Split-Path $onPath.Source -Parent) -Parent
        [void]$candidates.Add($p)
    }

    # typowe miejsca instalacji JDK
    foreach ($root in @("C:\Program Files\Eclipse Adoptium", "C:\Program Files\Java",
                        "C:\Program Files\Microsoft", "C:\Program Files\Amazon Corretto",
                        "C:\Program Files\Zulu", "C:\Program Files\BellSoft")) {
        if (Test-Path -LiteralPath $root) {
            Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue |
                ForEach-Object { [void]$candidates.Add($_.FullName) }
        }
    }

    # JBR z Android Studio - tylko gdy akurat jest w wersji 17-22
    [void]$candidates.Add("C:\Program Files\Android\Android Studio\jbr")

    foreach ($c in $candidates) {
        if (Test-JdkUsable $c) { $found = $c; break }
    }
}

if ($null -eq $found) {
    Write-Host ""
    Write-Host "BLAD: nie znalazlem JDK w wersji 17-22."
    Write-Host "Java 25 z Android Studio NIE nadaje sie (Gradle 8.9 jej nie obsluguje)."
    Write-Host ""
    Write-Host "Co zrobic: zainstaluj JDK 21 (Temurin) i uruchom ponownie:"
    Write-Host "  https://adoptium.net/temurin/releases/?version=21"
    Write-Host ""
    Write-Host "Albo podaj sciezke do istniejacego JDK:"
    Write-Host '  powershell -ExecutionPolicy Bypass -File .\zbuduj.ps1 -Jdk "C:\Program Files\Java\jdk-21"'
    exit 1
}

$env:JAVA_HOME = $found
Write-Host "== JDK uzyty do budowania:"
& (Join-Path $found "bin\java.exe") -version

Write-Host ""
Write-Host "== Zatrzymuje stare demony Gradle (te spod Javy 25)..."
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
Write-Host ('  & "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r "' + $info.FullName + '"')
