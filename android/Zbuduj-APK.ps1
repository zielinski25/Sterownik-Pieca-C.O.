<#
.SYNOPSIS
    Buduje aplikacje Android "Sterownik Pieca C.O." (APK debug) na Windows
    i kopiuje na pulpit SterownikCO-debug.apk oraz Piec.html.

.DESCRIPTION
    Projekt uzywa Gradle 8.9 (wrapper w repozytorium), ktory uruchamia sie
    wylacznie na Javie 17-22. Aktualne Android Studio (2026.x) ma wbudowana
    Jave 25 (JBR 25.0.x) - Gradle 8.9 konczy sie na niej komunikatem
    "What went wrong: 25.0.3".

    Skrypt:
      1. szuka zainstalowanego JDK 17-22 (najpierw 17, potem 21): JAVA_HOME,
         PATH, JDK pobrane wczesniej przez ten skrypt, %USERPROFILE%\.jdks
         (JDK pobierane przez Android Studio), starsze Android Studio,
         Program Files, rejestr Windows;
      2. jesli go nie ma - pobiera przenosne Eclipse Temurin JDK 17 (ZIP
         z api.adoptium.net, ok. 190 MB, jednorazowo) do
         %LOCALAPPDATA%\SterownikCO\jdk i sprawdza sume SHA-256.
         Bez instalatora i bez uprawnien administratora;
      3. znajduje Android SDK (local.properties, ANDROID_HOME,
         ANDROID_SDK_ROOT, %LOCALAPPDATA%\Android\Sdk);
      4. uruchamia gradlew.bat :app:assembleDebug z JAVA_HOME ustawionym
         tylko dla tego procesu - systemowa Java, zmienne srodowiskowe
         i Android Studio zostaja bez zmian;
      5. kopiuje APK i Piec.html na pulpit.

.PARAMETER JavaHome
    Katalog JDK 17-22 wskazany recznie (pomija wyszukiwanie).

.PARAMETER PobierzJdk
    Pomija wyszukiwanie i uzywa Temurin JDK 17 z %LOCALAPPDATA%\SterownikCO\jdk
    (pobiera go, jesli jeszcze go tam nie ma).

.PARAMETER Testy
    Po skopiowaniu APK uruchamia tez testy jednostkowe (:app:testDebugUnitTest).

.PARAMETER BezPulpitu
    Nie kopiuje plikow na pulpit.

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File "$env:USERPROFILE\SterownikCO\android\Zbuduj-APK.ps1"
#>
[CmdletBinding()]
param(
    [string]$JavaHome,
    [switch]$PobierzJdk,
    [switch]$Testy,
    [switch]$BezPulpitu
)

# Komunikaty Gradle/curl na stderr nie moga przerywac skryptu (Windows PowerShell 5.1
# zamienia je na bledy, gdy wyjscie jest przekierowane). Kody wyjscia sa sprawdzane jawnie.
$ErrorActionPreference = 'Continue'
$ProgressPreference = 'SilentlyContinue'    # pasek postepu Invoke-WebRequest w PS 5.1 bardzo spowalnia pobieranie

$AndroidDir    = $PSScriptRoot
$RepoDir       = $null
if ($AndroidDir) { $RepoDir = Split-Path -Parent $AndroidDir }
$JdkCacheDir   = Join-Path $env:LOCALAPPDATA 'SterownikCO\jdk'
$GradleMinJava = 17
$GradleMaxJava = 22                         # Gradle 8.9 dziala na Javie 17-22; Java 25 wymaga Gradle 9.1+
$PreferredJava = @(17, 21, 22, 20, 19, 18)  # 17 = poziom bajtkodu projektu (jvmTarget)
$DownloadJava  = 17

function Write-Step([string]$Text) {
    Write-Host ''
    Write-Host "==> $Text" -ForegroundColor Cyan
}

function Write-Info([string]$Text) {
    Write-Host "    $Text"
}

function Exit-WithError([string]$Text) {
    Write-Host ''
    Write-Host "BLAD: $Text" -ForegroundColor Red
    exit 1
}

function Get-JdkMajor([string]$Dir) {
    # Glowna wersja Javy z pliku "release" JDK (JAVA_VERSION="17.0.12" -> 17, "1.8.0_421" -> 8).
    # 0 = katalog nie jest pelnym JDK (brak bin\javac.exe albo pliku release).
    if ([string]::IsNullOrWhiteSpace($Dir)) { return 0 }
    try {
        $javac = [IO.Path]::Combine($Dir, 'bin', 'javac.exe')
        $releaseFile = [IO.Path]::Combine($Dir, 'release')
        if (-not (Test-Path -LiteralPath $javac -PathType Leaf)) { return 0 }
        if (-not (Test-Path -LiteralPath $releaseFile -PathType Leaf)) { return 0 }
    } catch {
        return 0
    }
    $major = 0
    $osArch = ''
    foreach ($line in @(Get-Content -LiteralPath $releaseFile -ErrorAction SilentlyContinue)) {
        if ($line -match '^\s*JAVA_VERSION\s*=\s*"?(\d+)(?:\.(\d+))?') {
            $major = [int]$Matches[1]
            if ($major -eq 1 -and $Matches[2]) { $major = [int]$Matches[2] }
        } elseif ($line -match '^\s*OS_ARCH\s*=\s*"?([A-Za-z0-9_]+)') {
            $osArch = $Matches[1]
        }
    }
    # 32-bitowe JDK na 64-bitowym Windows nie udzwignie -Xmx2048m z gradle.properties
    if ($osArch -match '^(x86|i[3-6]86)$' -and [Environment]::Is64BitOperatingSystem) { return 0 }
    return $major
}

function Get-JdkCandidateDirs {
    $dirs = New-Object System.Collections.Generic.List[string]

    if ($env:JAVA_HOME) { $dirs.Add($env:JAVA_HOME) }

    foreach ($cmd in @(Get-Command -Name 'java.exe' -CommandType Application -All -ErrorAction SilentlyContinue)) {
        try { $dirs.Add([IO.Path]::GetDirectoryName([IO.Path]::GetDirectoryName($cmd.Path))) } catch { }
    }

    $patterns = New-Object System.Collections.Generic.List[string]
    $patterns.Add("$JdkCacheDir\*")                                   # pobrane wczesniej przez ten skrypt
    if ($env:USERPROFILE) {
        $patterns.Add("$env:USERPROFILE\.jdks\*")                      # Android Studio / IntelliJ: Download JDK
        $patterns.Add("$env:USERPROFILE\.gradle\jdks\*")
        $patterns.Add("$env:USERPROFILE\.gradle\jdks\*\*")
    }
    if ($env:GRADLE_USER_HOME) {
        $patterns.Add("$env:GRADLE_USER_HOME\jdks\*")
        $patterns.Add("$env:GRADLE_USER_HOME\jdks\*\*")
    }
    if ($env:LOCALAPPDATA) {
        $patterns.Add("$env:LOCALAPPDATA\Programs\*\jbr")              # Android Studio z JetBrains Toolbox
        $patterns.Add("$env:LOCALAPPDATA\JetBrains\Toolbox\apps\*\*\*\jbr")
        $patterns.Add("$env:LOCALAPPDATA\Programs\Eclipse Adoptium\*")
    }
    $roots = @()
    foreach ($root in @($env:ProgramW6432, $env:ProgramFiles, ${env:ProgramFiles(x86)})) {
        if ($root -and ($roots -notcontains $root)) { $roots += $root }
    }
    $subdirs = @('Android\*\jbr', 'Android\*\jre', 'Eclipse Adoptium\*', 'Eclipse Foundation\*', 'Java\*',
                 'Microsoft\jdk-*', 'Zulu\*', 'Amazon Corretto\*', 'BellSoft\*', 'OpenJDK\*', 'RedHat\*', 'Semeru\*')
    foreach ($root in $roots) {
        foreach ($sub in $subdirs) { $patterns.Add("$root\$sub") }
    }
    foreach ($pattern in $patterns) {
        foreach ($item in @(Get-Item -Path $pattern -Force -ErrorAction SilentlyContinue)) {
            if ($item.PSIsContainer) { $dirs.Add($item.FullName) }
        }
    }

    $regRoots = @('HKLM:\SOFTWARE\JavaSoft\JDK', 'HKLM:\SOFTWARE\Eclipse Adoptium\JDK', 'HKLM:\SOFTWARE\Eclipse Foundation\JDK',
                  'HKLM:\SOFTWARE\Microsoft\JDK', 'HKLM:\SOFTWARE\Azul Systems\Zulu', 'HKCU:\SOFTWARE\JavaSoft\JDK')
    foreach ($regRoot in $regRoots) {
        foreach ($key in @(Get-ChildItem -LiteralPath $regRoot -Recurse -ErrorAction SilentlyContinue)) {
            foreach ($valueName in @('JavaHome', 'Path', 'InstallationPath')) {
                try {
                    $value = $key.GetValue($valueName)
                    if ($value) { $dirs.Add([string]$value) }
                } catch { }
            }
        }
    }
    return $dirs
}

function Find-CompatibleJdk {
    $seen = @{}
    $found = @()
    foreach ($candidate in @(Get-JdkCandidateDirs)) {
        if ([string]::IsNullOrWhiteSpace($candidate)) { continue }
        $dir = $candidate.Trim().Trim([char[]]'"')
        if ($dir.Length -gt 3) { $dir = $dir.TrimEnd([char[]]'\/') }
        $key = $dir.ToLowerInvariant()
        if ($seen.ContainsKey($key)) { continue }
        $seen[$key] = $true
        $major = Get-JdkMajor $dir
        if ($major -le 0) { continue }
        if ($major -ge $GradleMinJava -and $major -le $GradleMaxJava) { $note = 'pasuje' } else { $note = 'nie pasuje do Gradle 8.9' }
        Write-Info ('Java {0,-3} {1}   [{2}]' -f $major, $dir, $note)
        $found += [pscustomobject]@{ Dir = $dir; Major = $major }
    }
    foreach ($want in $PreferredJava) {
        foreach ($jdk in $found) {
            if ($jdk.Major -eq $want) { return $jdk }
        }
    }
    return $null
}

function Get-Sha256Hex([string]$File) {
    # Czyste .NET zamiast Get-FileHash: Windows PowerShell 5.1 uruchomiony z PowerShell 7
    # dziedziczy PSModulePath i nie znajduje funkcji z modulu Microsoft.PowerShell.Utility.
    $sha = [System.Security.Cryptography.SHA256]::Create()
    $stream = $null
    try {
        $stream = [System.IO.File]::OpenRead($File)
        $bytes = $sha.ComputeHash($stream)
        return ([BitConverter]::ToString($bytes).Replace('-', '').ToLowerInvariant())
    } catch {
        return ''
    } finally {
        if ($stream) { $stream.Dispose() }
        $sha.Dispose()
    }
}

function Install-TemurinJdk([int]$Feature) {
    # Przenosne Eclipse Temurin JDK (ZIP) do $JdkCacheDir; zwraca katalog JDK.
    $procArch = $env:PROCESSOR_ARCHITEW6432
    if (-not $procArch) { $procArch = $env:PROCESSOR_ARCHITECTURE }
    $arch = 'x64'
    if ($procArch -eq 'ARM64') { $arch = 'aarch64' }

    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    } catch { }

    $api = "https://api.adoptium.net/v3/assets/latest/$Feature/hotspot?architecture=$arch&image_type=jdk&os=windows&vendor=eclipse"
    Write-Info "Sprawdzam najnowsze Eclipse Temurin JDK $Feature ($arch) w api.adoptium.net..."
    $asset = $null
    try {
        $assets = Invoke-RestMethod -Uri $api -UseBasicParsing -TimeoutSec 60 -ErrorAction Stop
        $asset = @($assets)[0]
    } catch {
        Exit-WithError ("Brak polaczenia z api.adoptium.net ({0}).`n       Zainstaluj JDK 17 recznie, np.: winget install EclipseAdoptium.Temurin.17.JDK`n       i uruchom skrypt ponownie (albo podaj sciezke: -JavaHome 'C:\sciezka\do\jdk-17')." -f $_.Exception.Message)
    }
    if (-not $asset -or -not $asset.binary -or -not $asset.binary.package -or -not $asset.binary.package.link) {
        Exit-WithError "api.adoptium.net nie zwrocilo paczki JDK $Feature dla Windows ($arch)."
    }
    $pkg = $asset.binary.package
    $url = [string]$pkg.link
    $zipName = [string]$pkg.name
    $releaseName = [string]$asset.release_name
    if (-not $releaseName) { $releaseName = [IO.Path]::GetFileNameWithoutExtension($zipName) }
    $target = Join-Path $JdkCacheDir $releaseName

    if ((Get-JdkMajor $target) -eq $Feature) {
        Write-Info "Juz pobrane wczesniej: $target"
        return $target
    }

    New-Item -ItemType Directory -Force -Path $JdkCacheDir -ErrorAction Stop | Out-Null
    $zip = Join-Path $JdkCacheDir $zipName
    $part = $zip + '.part'
    Remove-Item -LiteralPath $part -Force -ErrorAction SilentlyContinue
    $sizeMb = [math]::Round(([double]$pkg.size) / 1048576)
    Write-Info "Pobieranie $zipName (ok. $sizeMb MB, jednorazowo)..."

    $downloaded = $false
    $curl = Get-Command -Name 'curl.exe' -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($curl) {
        $curlExe = $curl.Path
        & $curlExe -fL --retry 3 --retry-delay 3 --connect-timeout 30 -o $part $url | Out-Host
        $downloaded = ($LASTEXITCODE -eq 0) -and (Test-Path -LiteralPath $part -PathType Leaf)
    }
    if (-not $downloaded) {
        try {
            Invoke-WebRequest -Uri $url -OutFile $part -UseBasicParsing -ErrorAction Stop
        } catch {
            Remove-Item -LiteralPath $part -Force -ErrorAction SilentlyContinue
            Exit-WithError ("Pobieranie JDK nie powiodlo sie: {0}" -f $_.Exception.Message)
        }
    }

    $expected = ([string]$pkg.checksum).Trim().ToLowerInvariant()
    if ($expected) {
        $actual = Get-Sha256Hex $part
        if ($actual -ne $expected) {
            Remove-Item -LiteralPath $part -Force -ErrorAction SilentlyContinue
            Exit-WithError "Suma SHA-256 pobranego pliku sie nie zgadza (oczekiwana $expected, jest '$actual'). Uruchom skrypt ponownie."
        }
        Write-Info 'Suma SHA-256 zgodna.'
    }
    Move-Item -LiteralPath $part -Destination $zip -Force -ErrorAction Stop

    $unpackDir = Join-Path $JdkCacheDir ('temurin-' + [guid]::NewGuid().ToString('N').Substring(0, 8) + '.tmp')
    New-Item -ItemType Directory -Force -Path $unpackDir -ErrorAction Stop | Out-Null
    Write-Info 'Rozpakowywanie...'
    $unpacked = $false
    $tarExe = [IO.Path]::Combine($env:SystemRoot, 'System32', 'tar.exe')
    if (Test-Path -LiteralPath $tarExe -PathType Leaf) {
        & $tarExe -xf $zip -C $unpackDir | Out-Host
        $unpacked = ($LASTEXITCODE -eq 0)
    }
    if (-not $unpacked) {
        # zapasowo .NET (Expand-Archive pochodzi z modulu, ktory potrafi sie nie zaladowac)
        Remove-Item -LiteralPath $unpackDir -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force -Path $unpackDir -ErrorAction Stop | Out-Null
        try {
            Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction Stop
            [System.IO.Compression.ZipFile]::ExtractToDirectory($zip, $unpackDir)
        } catch {
            Exit-WithError ("Nie udalo sie rozpakowac {0}: {1}" -f $zip, $_.Exception.Message)
        }
    }

    $innerDir = $null
    if ((Get-JdkMajor $unpackDir) -gt 0) { $innerDir = $unpackDir }
    foreach ($d in @(Get-ChildItem -LiteralPath $unpackDir -Directory -ErrorAction SilentlyContinue)) {
        if (-not $innerDir -and (Get-JdkMajor $d.FullName) -gt 0) { $innerDir = $d.FullName }
    }
    if (-not $innerDir) { Exit-WithError "W archiwum $zipName nie ma katalogu JDK." }

    if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Recurse -Force -ErrorAction SilentlyContinue }
    $finalDir = $innerDir
    for ($i = 0; $i -lt 10; $i++) {
        try {
            Move-Item -LiteralPath $innerDir -Destination $target -ErrorAction Stop
            $finalDir = $target
            break
        } catch {
            Start-Sleep -Seconds 2    # np. antywirus jeszcze skanuje rozpakowane pliki
        }
    }
    if ($finalDir -eq $target) {
        Remove-Item -LiteralPath $unpackDir -Recurse -Force -ErrorAction SilentlyContinue
    } else {
        Write-Info "Uwaga: nie udalo sie przeniesc JDK do $target (blokuje go np. antywirus) - uzywam go z $finalDir"
    }
    Remove-Item -LiteralPath $zip -Force -ErrorAction SilentlyContinue
    Write-Info "JDK gotowe: $finalDir"
    return $finalDir
}

function Test-AndroidSdk([string]$Dir) {
    if ([string]::IsNullOrWhiteSpace($Dir)) { return $false }
    try {
        if (-not (Test-Path -LiteralPath $Dir -PathType Container)) { return $false }
        foreach ($sub in @('platforms', 'build-tools', 'platform-tools', 'licenses', 'cmdline-tools')) {
            if (Test-Path -LiteralPath ([IO.Path]::Combine($Dir, $sub)) -PathType Container) { return $true }
        }
    } catch { }
    return $false
}

function Read-LocalPropertiesSdk([string]$File) {
    if (-not (Test-Path -LiteralPath $File -PathType Leaf)) { return $null }
    foreach ($line in @(Get-Content -LiteralPath $File -ErrorAction SilentlyContinue)) {
        if ($line -match '^\s*sdk\.dir\s*[=:]\s*(.*?)\s*$') {
            return (($Matches[1] -replace '\\:', ':') -replace '\\\\', '\')
        }
    }
    return $null
}

# --------------------------------------------------------------------------------------

Write-Host 'Sterownik Pieca C.O. - budowanie aplikacji Android (APK debug)' -ForegroundColor Green

if (-not $AndroidDir -or -not (Test-Path -LiteralPath (Join-Path $AndroidDir 'gradlew.bat') -PathType Leaf)) {
    Exit-WithError 'Nie znaleziono gradlew.bat obok skryptu. Uruchom plik android\Zbuduj-APK.ps1 z pobranego repozytorium (powershell -File ...).'
}
$gradlew = Join-Path $AndroidDir 'gradlew.bat'

# 1. Java -----------------------------------------------------------------------------
Write-Step "Java dla Gradle 8.9 (potrzebna wersja $GradleMinJava-$GradleMaxJava)"
foreach ($stale in @(Get-ChildItem -LiteralPath $JdkCacheDir -Directory -Filter '*.tmp' -ErrorAction SilentlyContinue)) {
    Remove-Item -LiteralPath $stale.FullName -Recurse -Force -ErrorAction SilentlyContinue   # po przerwanym rozpakowywaniu
}

$jdkDir = $null
if ($JavaHome) {
    $m = Get-JdkMajor $JavaHome
    if ($m -lt $GradleMinJava -or $m -gt $GradleMaxJava) {
        Exit-WithError "Katalog -JavaHome '$JavaHome' nie zawiera JDK $GradleMinJava-$GradleMaxJava (wykryta wersja: $m)."
    }
    $jdkDir = $JavaHome
} elseif ($PobierzJdk) {
    $jdkDir = @(Install-TemurinJdk $DownloadJava)[-1]
} else {
    Write-Info 'Znalezione JDK:'
    $hit = Find-CompatibleJdk
    if ($hit) {
        $jdkDir = $hit.Dir
    } else {
        Write-Info "Brak JDK $GradleMinJava-$GradleMaxJava na tym komputerze - pobieram przenosne Eclipse Temurin JDK $DownloadJava."
        $jdkDir = @(Install-TemurinJdk $DownloadJava)[-1]
    }
}
if ($jdkDir) {
    $jdkDir = ([string]$jdkDir).Trim().Trim([char[]]'"')
    if ($jdkDir.Length -gt 3) { $jdkDir = $jdkDir.TrimEnd([char[]]'\/') }
}
$jdkMajor = Get-JdkMajor $jdkDir
if ($jdkMajor -lt $GradleMinJava -or $jdkMajor -gt $GradleMaxJava) {
    Exit-WithError "Nie udalo sie przygotowac JDK $GradleMinJava-$GradleMaxJava ($jdkDir)."
}
Write-Info "Uzywam Javy ${jdkMajor} z katalogu $jdkDir"

# org.gradle.java.home w globalnym gradle.properties wygrywa z JAVA_HOME - wtedy nadpisujemy je z linii polecen
$gradleArgs = @()
$gradleUserHome = $env:GRADLE_USER_HOME
if (-not $gradleUserHome -and $env:USERPROFILE) { $gradleUserHome = Join-Path $env:USERPROFILE '.gradle' }
foreach ($propsFile in @((Join-Path $AndroidDir 'gradle.properties'), $(if ($gradleUserHome) { Join-Path $gradleUserHome 'gradle.properties' }))) {
    if (-not $propsFile -or -not (Test-Path -LiteralPath $propsFile -PathType Leaf)) { continue }
    foreach ($line in @(Get-Content -LiteralPath $propsFile -ErrorAction SilentlyContinue)) {
        if ($line -match '^\s*org\.gradle\.java\.home\s*[=:]\s*(.*?)\s*$') {
            Write-Info "Uwaga: $propsFile ustawia org.gradle.java.home=$($Matches[1]) - na czas tej kompilacji nadpisuje to wybranym JDK."
            $gradleArgs = @("-Dorg.gradle.java.home=$jdkDir")
        }
    }
}

# 2. Android SDK ----------------------------------------------------------------------
Write-Step 'Android SDK'
$localProps = Join-Path $AndroidDir 'local.properties'
$lpSdk = Read-LocalPropertiesSdk $localProps
if ($lpSdk -and -not (Test-AndroidSdk $lpSdk)) {
    Move-Item -LiteralPath $localProps -Destination ($localProps + '.bak') -Force -ErrorAction SilentlyContinue
    Write-Info "local.properties wskazywal nieistniejacy SDK ($lpSdk) - odlozono go jako local.properties.bak."
    $lpSdk = $null
}
$sdkDir = $null
foreach ($s in @($lpSdk, $env:ANDROID_HOME, $env:ANDROID_SDK_ROOT, (Join-Path $env:LOCALAPPDATA 'Android\Sdk'), 'C:\Android\Sdk', 'C:\Android\android-sdk')) {
    if (Test-AndroidSdk $s) { $sdkDir = $s; break }
}
if (-not $sdkDir) {
    Exit-WithError "Nie znaleziono Android SDK. Uruchom raz Android Studio (pobierze SDK do $env:LOCALAPPDATA\Android\Sdk) albo ustaw zmienna ANDROID_HOME."
}
$env:ANDROID_HOME = $sdkDir
Remove-Item -Path 'Env:\ANDROID_SDK_ROOT' -ErrorAction SilentlyContinue    # dwie rozne sciezki SDK = blad AGP
Write-Info "Uzywam: $sdkDir"
if (-not (Test-Path -LiteralPath ([IO.Path]::Combine($sdkDir, 'platforms', 'android-35')) -PathType Container)) {
    Write-Info 'Brak platformy android-35 - Gradle sprobuje ja pobrac sam (wymaga zaakceptowanych licencji SDK).'
}

# 3. Kompilacja -----------------------------------------------------------------------
Write-Step 'Kompilacja: gradlew.bat :app:assembleDebug'
$env:JAVA_HOME = $jdkDir                                                   # tylko dla tego procesu
$env:Path = [IO.Path]::Combine($jdkDir, 'bin') + ';' + $env:Path
$gradleExit = 1
Push-Location -LiteralPath $AndroidDir
try {
    & $gradlew @gradleArgs ':app:assembleDebug'
    $gradleExit = $LASTEXITCODE
} finally {
    Pop-Location
}
if ($gradleExit -ne 0) {
    Exit-WithError "Gradle zakonczyl sie kodem $gradleExit. Skopiuj komunikaty powyzej (zwlaszcza sekcje 'What went wrong')."
}

$apk = [IO.Path]::Combine($AndroidDir, 'app', 'build', 'outputs', 'apk', 'debug', 'app-debug.apk')
if (-not (Test-Path -LiteralPath $apk -PathType Leaf)) {
    Exit-WithError "Gradle zakonczyl sie bez bledu, ale nie ma pliku $apk"
}
$apkMb = (Get-Item -LiteralPath $apk).Length / 1048576
$versionName = '?'
foreach ($line in @(Get-Content -LiteralPath ([IO.Path]::Combine($AndroidDir, 'app', 'build.gradle.kts')) -ErrorAction SilentlyContinue)) {
    if ($line -match '^\s*versionName\s*=\s*"([^"]+)"') { $versionName = $Matches[1]; break }
}
Write-Info ('APK: {0} ({1:N1} MB), wersja {2}-debug' -f $apk, $apkMb, $versionName)

# 4. Pulpit ---------------------------------------------------------------------------
if ($BezPulpitu) {
    Write-Info 'Pominieto kopiowanie na pulpit (-BezPulpitu).'
} else {
    Write-Step 'Kopiowanie na pulpit'
    $desktop = [Environment]::GetFolderPath('Desktop')
    if (-not $desktop -or -not (Test-Path -LiteralPath $desktop -PathType Container)) {
        Write-Info 'Nie znaleziono folderu pulpitu - pliki zostaja w repozytorium.'
    } else {
        $copies = @(
            @{ From = $apk; To = (Join-Path $desktop 'SterownikCO-debug.apk') },
            @{ From = (Join-Path $RepoDir 'Piec.html'); To = (Join-Path $desktop 'Piec.html') }
        )
        foreach ($copy in $copies) {
            if (-not (Test-Path -LiteralPath $copy.From -PathType Leaf)) {
                Write-Info "Brak pliku $($copy.From) - pomijam."
                continue
            }
            try {
                Copy-Item -LiteralPath $copy.From -Destination $copy.To -Force -ErrorAction Stop
                Write-Info "Skopiowano: $($copy.To)"
            } catch {
                Exit-WithError ("Nie udalo sie skopiowac {0}: {1}" -f $copy.To, $_.Exception.Message)
            }
        }
    }
}

# 5. Testy (opcjonalnie) --------------------------------------------------------------
if ($Testy) {
    Write-Step 'Testy jednostkowe: gradlew.bat :app:testDebugUnitTest'
    $testExit = 1
    Push-Location -LiteralPath $AndroidDir
    try {
        & $gradlew @gradleArgs ':app:testDebugUnitTest'
        $testExit = $LASTEXITCODE
    } finally {
        Pop-Location
    }
    if ($testExit -ne 0) {
        Exit-WithError "Testy jednostkowe nie przeszly (kod $testExit). APK jest zbudowany. Raport: android\app\build\reports\tests\testDebugUnitTest\index.html"
    }
    Write-Info 'Testy jednostkowe: OK.'
}

Write-Host ''
Write-Host 'Gotowe.' -ForegroundColor Green
if ($jdkDir -like "$JdkCacheDir*") {
    Write-Info 'Android Studio: zeby Studio tez budowalo ten projekt, wybierz w Settings > Build, Execution, Deployment >'
    Write-Info "Build Tools > Gradle > Gradle JDK > Add JDK from disk... katalog: $jdkDir"
}
exit 0
