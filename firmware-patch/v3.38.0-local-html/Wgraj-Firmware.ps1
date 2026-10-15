<#
.SYNOPSIS
    Kompiluje firmware centrali (main_centrala.cpp z tego katalogu) w Twoim projekcie
    PlatformIO i - tylko na wyrazne zyczenie - wgrywa je na centrale (USB albo OTA).

.DESCRIPTION
    Skrypt:
      1. znajduje projekt PlatformIO centrali (platformio.ini z [env:esp32-s3-n16r8])
         w typowych miejscach: Dokumenty\PlatformIO\Projects, Dokumenty, Pulpit,
         Pobrane, OneDrive, folder uzytkownika - albo bierze katalog z -Projekt;
      2. sprawdza lokalna biblioteke DHT22 lib\dhtESP32-rmt (gdy jej brak, pobiera
         wersje 2.0.2 z github.com/junkfix/dhtESP32-rmt);
      3. kopiuje obecny src\main_centrala.cpp do folderu kopie-firmware w projekcie
         i wstawia plik z tego katalogu;
      4. kompiluje (pio run). Gdy platforma pioarduino nie moze podmienic swojego rdzenia
         PlatformIO w dzialajacym penv (Windows), doinstalowuje go i probuje drugi raz.
         Gdy kompilacja sie nie uda, przywraca poprzedni plik;
      5. kopiuje firmware.bin na pulpit jako firmware_<wersja>.bin;
      6. tylko z -Wgraj USB albo -Wgraj OTA wgrywa firmware na centrale.

    Kompilacja odbywa sie w Twoim projekcie, wiec uzywa tych samych wersji bibliotek
    (.pio\libdeps), z ktorymi dzialal dotychczasowy firmware.

.PARAMETER Projekt
    Katalog projektu PlatformIO (ten z platformio.ini). Bez tego - wyszukiwanie.

.PARAMETER Wgraj
    Brak (domyslnie) - tylko kompilacja i firmware.bin na pulpicie, bez wgrywania.
    USB - wgranie kablem:            pio run -e esp32-s3-n16r8 -t upload
    OTA - wgranie przez siec lokalna: pio run -e esp32-s3-ota -t upload

.PARAMETER Port
    Port COM dla -Wgraj USB (np. COM5). Bez tego PlatformIO wybiera port sam.

.PARAMETER AdresOta
    Adres IP centrali dla -Wgraj OTA. Bez tego - upload_port z platformio.ini.

.PARAMETER BezPulpitu
    Nie kopiuje firmware.bin na pulpit.

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File "$env:USERPROFILE\SterownikCO\firmware-patch\v3.38.0-local-html\Wgraj-Firmware.ps1"

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File "$env:USERPROFILE\SterownikCO\firmware-patch\v3.38.0-local-html\Wgraj-Firmware.ps1" -Wgraj OTA
#>
[CmdletBinding()]
param(
    [string]$Projekt,
    [ValidateSet('Brak', 'USB', 'OTA')]
    [string]$Wgraj = 'Brak',
    [string]$Port,
    [string]$AdresOta,
    [switch]$BezPulpitu
)

# Komunikaty PlatformIO na stderr nie moga przerywac skryptu (Windows PowerShell 5.1
# zamienia je na bledy, gdy wyjscie jest przekierowane). Kody wyjscia sa sprawdzane jawnie.
$ErrorActionPreference = 'Continue'
$ProgressPreference = 'SilentlyContinue'

$FwDir     = $PSScriptRoot
$EnvUsb    = 'esp32-s3-n16r8'
$EnvOta    = 'esp32-s3-ota'
$DhtTag    = '2.0.2'
$DhtZipUrl = 'https://codeload.github.com/junkfix/dhtESP32-rmt/zip/refs/tags/' + $DhtTag

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

function Get-FirmwareVersion([string]$File) {
    # FIRMWARE_VERSION z pliku zrodlowego, np. "v3.47.1"; '' gdy pliku lub stalej brak.
    if ([string]::IsNullOrWhiteSpace($File)) { return '' }
    if (-not (Test-Path -LiteralPath $File -PathType Leaf)) { return '' }
    $hit = Select-String -LiteralPath $File -Pattern 'static const char\* FIRMWARE_VERSION = "([^"]+)"' -List
    if ($hit) { return $hit.Matches[0].Groups[1].Value }
    return ''
}

function Get-Sha256Hex([string]$File) {
    # Czyste .NET zamiast Get-FileHash (Windows PowerShell 5.1 uruchomiony z PowerShell 7
    # dziedziczy PSModulePath i nie znajduje funkcji z modulu Microsoft.PowerShell.Utility).
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

function Test-IniHasEnv([string]$Dir, [string]$EnvName) {
    # $true, gdy platformio.ini w katalogu ma sekcje [env:<EnvName>].
    if ([string]::IsNullOrWhiteSpace($Dir)) { return $false }
    $ini = Join-Path $Dir 'platformio.ini'
    if (-not (Test-Path -LiteralPath $ini -PathType Leaf)) { return $false }
    $hit = Select-String -LiteralPath $ini -Pattern ('^\s*\[env:' + [regex]::Escape($EnvName) + '\]') -List
    return [bool]$hit
}

function Find-CentralaProjects {
    # Katalogi z platformio.ini zawierajacym [env:esp32-s3-n16r8] w typowych miejscach.
    $docs = [Environment]::GetFolderPath('MyDocuments')
    $desk = [Environment]::GetFolderPath('Desktop')
    $roots = @(
        @((Join-Path $docs 'PlatformIO\Projects'), 3),
        @($docs, 4),
        @($desk, 4),
        @((Join-Path $env:USERPROFILE 'Downloads'), 3),
        @($env:OneDrive, 5),
        @($env:USERPROFILE, 3)
    )
    $found = @{}
    foreach ($r in $roots) {
        $root = [string]$r[0]
        $depth = [int]$r[1]
        if ([string]::IsNullOrWhiteSpace($root)) { continue }
        if (-not (Test-Path -LiteralPath $root -PathType Container)) { continue }
        $inis = @(Get-ChildItem -LiteralPath $root -Filter 'platformio.ini' -File -Recurse -Depth $depth -ErrorAction SilentlyContinue)
        foreach ($ini in $inis) {
            $dir = $ini.DirectoryName
            $key = $dir.ToLowerInvariant()
            if ($found.ContainsKey($key)) { continue }
            if (Test-IniHasEnv $dir $EnvUsb) { $found[$key] = $dir }
        }
    }
    return @($found.Values)
}

function Find-Pio {
    # pio.exe z PATH albo ze srodowiska PlatformIO IDE (VS Code): %USERPROFILE%\.platformio\penv.
    $candidates = @()
    foreach ($name in @('pio.exe', 'platformio.exe')) {
        $cmd = Get-Command -Name $name -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($cmd) { $candidates += $cmd.Path }
    }
    $coreDirs = @()
    if ($env:PLATFORMIO_CORE_DIR) { $coreDirs += $env:PLATFORMIO_CORE_DIR }
    $coreDirs += (Join-Path $env:USERPROFILE '.platformio')
    foreach ($core in $coreDirs) {
        foreach ($name in @('pio.exe', 'platformio.exe')) {
            $candidates += [IO.Path]::Combine($core, 'penv', 'Scripts', $name)
        }
    }
    foreach ($c in $candidates) {
        if ($c -and (Test-Path -LiteralPath $c -PathType Leaf)) { return $c }
    }
    return $null
}

function Get-PioarduinoCorePin {
    # Rdzen platformio przypiety przez zainstalowana platforme pioarduino (python_deps
    # w builder\penv_setup.py), np. ...pioarduino/platformio-core/archive/refs/tags/v6.1.19.zip
    $coreDir = $env:PLATFORMIO_CORE_DIR
    if (-not $coreDir) { $coreDir = Join-Path $env:USERPROFILE '.platformio' }
    $platforms = Join-Path $coreDir 'platforms'
    if (-not (Test-Path -LiteralPath $platforms -PathType Container)) { return $null }
    $files = @(Get-ChildItem -LiteralPath $platforms -Filter 'penv_setup.py' -File -Recurse -Depth 3 -ErrorAction SilentlyContinue)
    foreach ($f in $files) {
        $hit = Select-String -LiteralPath $f.FullName -Pattern '"platformio"\s*:\s*"(https://github\.com/pioarduino/platformio-core/archive/refs/tags/v([0-9][0-9.]*)\.zip)"' -List
        if ($hit) {
            return [pscustomobject]@{ Url = $hit.Matches[0].Groups[1].Value; Version = $hit.Matches[0].Groups[2].Value }
        }
    }
    return $null
}

function Get-PenvPackageVersion([string]$Scripts, [string]$Name) {
    # Wersja dystrybucji w penv (uv pip list / pip list w formacie JSON); '' gdy brak.
    $py = Join-Path $Scripts 'python.exe'
    $uv = Join-Path $Scripts 'uv.exe'
    if (Test-Path -LiteralPath $uv -PathType Leaf) {
        $json = (& $uv 'pip' 'list' ('--python=' + $py) '--format=json' 2>$null) -join ''
    } else {
        $json = (& $py '-m' 'pip' 'list' '--format=json' '--disable-pip-version-check' 2>$null) -join ''
    }
    if (-not $json) { return '' }
    try { $list = $json | ConvertFrom-Json } catch { return '' }
    foreach ($pkg in @($list)) {
        if ([string]$pkg.name -ieq $Name) { return [string]$pkg.version }
    }
    return ''
}

function Repair-PioarduinoCore([string]$PioExe) {
    # Platforma pioarduino porownuje wersje pakietu "platformio" w penv z przypieta (np. 6.1.19).
    # Gdy sie rozni (np. PlatformIO IDE zainstalowalo 6.2.0), przy kazdym 'pio run' probuje
    # przeinstalowac rdzen. Na Windows to sie nie udaje, bo uruchomiony pio.exe blokuje wlasne
    # pliki ("Failed to install Python dependencies into penv"). Tu wymagana wersja jest
    # instalowana, gdy pio nie dziala. $true = cos zainstalowano (warto sprobowac ponownie).
    $scripts = Split-Path -Parent $PioExe
    $py = Join-Path $scripts 'python.exe'
    if (-not (Test-Path -LiteralPath $py -PathType Leaf)) {
        Write-Info "PlatformIO spoza penv ($py nie istnieje) - bez naprawy rdzenia."
        return $false
    }
    $pin = Get-PioarduinoCorePin
    if (-not $pin) {
        Write-Info 'Nie znalazlem przypietej wersji rdzenia pioarduino (penv_setup.py) - bez naprawy.'
        return $false
    }
    $have = Get-PenvPackageVersion $scripts 'platformio'
    if ($have -eq $pin.Version) {
        Write-Info ('Pakiet platformio {0} zgodny z platforma - przyczyna bledu jest inna.' -f $have)
        return $false
    }
    $haveLabel = $have
    if (-not $haveLabel) { $haveLabel = 'brak' }
    Write-Info ('Pakiet platformio w penv: {0}, platforma pioarduino wymaga {1} - instaluje platformio=={1}...' -f $haveLabel, $pin.Version)
    $spec = 'platformio==' + $pin.Version
    $uv = Join-Path $scripts 'uv.exe'
    if (Test-Path -LiteralPath $uv -PathType Leaf) {
        & $uv 'pip' 'install' '--quiet' ('--python=' + $py) $spec | Out-Host
    } else {
        & $py '-m' 'pip' 'install' '--quiet' '--disable-pip-version-check' $spec | Out-Host
    }
    return ($LASTEXITCODE -eq 0)
}

function Install-DhtLibrary([string]$LibDir) {
    # Pobiera dhtESP32-rmt (ZIP z codeload.github.com) do lib\dhtESP32-rmt\ (.h + .cpp + LICENSE).
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    $tmpZip = Join-Path $env:TEMP ('dhtESP32-rmt-' + $DhtTag + '.zip')
    $tmpDir = Join-Path $env:TEMP ('dhtESP32-rmt-' + $DhtTag)
    Remove-Item -LiteralPath $tmpZip -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $tmpDir -Recurse -Force -ErrorAction SilentlyContinue

    $ok = $false
    $curl = Get-Command -Name 'curl.exe' -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($curl) {
        & $curl.Path -fsSL --retry 3 -o $tmpZip $DhtZipUrl
        if ($LASTEXITCODE -eq 0 -and (Test-Path -LiteralPath $tmpZip -PathType Leaf)) { $ok = $true }
    }
    if (-not $ok) {
        try {
            Invoke-WebRequest -Uri $DhtZipUrl -OutFile $tmpZip -UseBasicParsing -ErrorAction Stop
            $ok = $true
        } catch {
            Exit-WithError ("Nie udalo sie pobrac biblioteki dhtESP32-rmt: {0}" -f $_.Exception.Message)
        }
    }
    try {
        Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction Stop
        [System.IO.Compression.ZipFile]::ExtractToDirectory($tmpZip, $tmpDir)
    } catch {
        Exit-WithError ("Nie udalo sie rozpakowac biblioteki dhtESP32-rmt: {0}" -f $_.Exception.Message)
    }
    $unpacked = Get-ChildItem -LiteralPath $tmpDir -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $unpacked) { Exit-WithError 'Pobrane archiwum dhtESP32-rmt jest puste.' }

    $dest = Join-Path $LibDir 'dhtESP32-rmt'
    try {
        New-Item -ItemType Directory -Force -Path $dest -ErrorAction Stop | Out-Null
        foreach ($name in @('dhtESP32-rmt.h', 'dhtESP32-rmt.cpp', 'LICENSE')) {
            Copy-Item -LiteralPath (Join-Path $unpacked.FullName $name) -Destination $dest -Force -ErrorAction Stop
        }
    } catch {
        Exit-WithError ("Nie udalo sie zapisac biblioteki w {0}: {1}" -f $dest, $_.Exception.Message)
    }
    Remove-Item -LiteralPath $tmpZip -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $tmpDir -Recurse -Force -ErrorAction SilentlyContinue
    Write-Info ("Zapisano dhtESP32-rmt {0}: {1}" -f $DhtTag, $dest)
}

# ---------------------------------------------------------------------------

Write-Host 'Sterownik Pieca C.O. - firmware centrali (PlatformIO)'

if (-not $FwDir) {
    Exit-WithError 'Uruchom skrypt jako plik: powershell -NoProfile -ExecutionPolicy Bypass -File "...\Wgraj-Firmware.ps1"'
}
$NewCpp = Join-Path $FwDir 'main_centrala.cpp'
if (-not (Test-Path -LiteralPath $NewCpp -PathType Leaf)) {
    Exit-WithError "Brak pliku $NewCpp. Pobierz najpierw aktualna wersje repozytorium."
}
$newVer = Get-FirmwareVersion $NewCpp
if (-not $newVer) { Exit-WithError "Nie znalazlem FIRMWARE_VERSION w $NewCpp." }
Write-Info "Nowy firmware: $newVer ($NewCpp)"

# --- 1. projekt ------------------------------------------------------------
Write-Step 'Projekt PlatformIO centrali'
$projDir = $null
if ($Projekt) {
    $projDir = $Projekt.Trim().Trim('"').TrimEnd('\', '/')
    if (-not (Test-IniHasEnv $projDir $EnvUsb)) {
        Exit-WithError "W katalogu $projDir nie ma platformio.ini z sekcja [env:$EnvUsb]."
    }
    $projDir = (Resolve-Path -LiteralPath $projDir).ProviderPath
} else {
    Write-Info 'Szukam platformio.ini z [env:esp32-s3-n16r8]...'
    $projects = @(Find-CentralaProjects)
    if ($projects.Count -eq 0) {
        Exit-WithError ('Nie znalazlem projektu PlatformIO centrali. Podaj jego katalog, np.: ' +
            '-Projekt "C:\Users\Ty\Documents\PlatformIO\Projects\Centrala"')
    }
    $scored = @(foreach ($p in $projects) {
        $time = (Get-Item -LiteralPath (Join-Path $p 'platformio.ini')).LastWriteTime
        $src = Join-Path $p 'src\main_centrala.cpp'
        if (Test-Path -LiteralPath $src -PathType Leaf) {
            $srcTime = (Get-Item -LiteralPath $src).LastWriteTime
            if ($srcTime -gt $time) { $time = $srcTime }
        }
        [pscustomobject]@{ Dir = $p; Time = $time; Version = (Get-FirmwareVersion $src) }
    })
    $scored = @($scored | Sort-Object -Property Time -Descending)
    if ($scored.Count -gt 1) {
        Write-Info 'Znalezione projekty:'
        foreach ($s in $scored) {
            $v = $s.Version
            if (-not $v) { $v = 'brak main_centrala.cpp' }
            Write-Info ('  {0}  ({1}, zmieniany {2:yyyy-MM-dd HH:mm})' -f $s.Dir, $v, $s.Time)
        }
        Write-Info 'Wybieram ostatnio zmieniany. Inny wskazesz parametrem -Projekt "katalog".'
    }
    $projDir = $scored[0].Dir
}
$targetCpp = Join-Path $projDir 'src\main_centrala.cpp'
$oldVer = Get-FirmwareVersion $targetCpp
$oldLabel = $oldVer
if (-not $oldLabel) { $oldLabel = 'brak' }
Write-Info "Uzywam: $projDir"
Write-Info "Obecny firmware w projekcie: $oldLabel"

$envName = $EnvUsb
if ($Wgraj -eq 'OTA') {
    $envName = $EnvOta
    if (-not (Test-IniHasEnv $projDir $EnvOta)) {
        Exit-WithError "platformio.ini nie ma sekcji [env:$EnvOta]. Wgraj kablem: -Wgraj USB"
    }
}

# --- 2. PlatformIO ----------------------------------------------------------
Write-Step 'PlatformIO'
$pio = Find-Pio
if (-not $pio) {
    Exit-WithError ('Nie znalazlem PlatformIO (pio.exe). Zainstaluj w VS Code rozszerzenie ' +
        '"PlatformIO IDE" albo dodaj pio do PATH.')
}
Write-Info "Uzywam: $pio"

# --- 3. biblioteka DHT22 ----------------------------------------------------
Write-Step 'Biblioteka DHT22 (lib\dhtESP32-rmt)'
$libDir = Join-Path $projDir 'lib'
$dhtHeader = $null
if (Test-Path -LiteralPath $libDir -PathType Container) {
    $dhtHeader = Get-ChildItem -LiteralPath $libDir -Filter 'dhtESP32-rmt.h' -File -Recurse -Depth 2 -ErrorAction SilentlyContinue | Select-Object -First 1
}
if ($dhtHeader) {
    Write-Info ('Jest: ' + $dhtHeader.FullName)
} else {
    Write-Info "Brak - pobieram dhtESP32-rmt $DhtTag z github.com/junkfix/dhtESP32-rmt..."
    Install-DhtLibrary $libDir
}

# --- 4. podmiana pliku z kopia ----------------------------------------------
Write-Step 'Podmiana src\main_centrala.cpp'
$backup = $null
$srcDir = Join-Path $projDir 'src'
if ((Test-Path -LiteralPath $targetCpp -PathType Leaf) -and ((Get-Sha256Hex $targetCpp) -eq (Get-Sha256Hex $NewCpp))) {
    Write-Info "Projekt ma juz ten plik ($newVer) - bez zmian."
} else {
    try {
        if (Test-Path -LiteralPath $targetCpp -PathType Leaf) {
            $backupDir = Join-Path $projDir 'kopie-firmware'
            New-Item -ItemType Directory -Force -Path $backupDir -ErrorAction Stop | Out-Null
            $backup = Join-Path $backupDir ('main_centrala_{0}_{1}.cpp' -f $oldLabel, (Get-Date -Format 'yyyyMMdd-HHmmss'))
            Copy-Item -LiteralPath $targetCpp -Destination $backup -Force -ErrorAction Stop
            Write-Info "Kopia poprzedniego pliku ($oldLabel): $backup"
        } else {
            New-Item -ItemType Directory -Force -Path $srcDir -ErrorAction Stop | Out-Null
        }
        Copy-Item -LiteralPath $NewCpp -Destination $targetCpp -Force -ErrorAction Stop
    } catch {
        Exit-WithError ("Nie udalo sie podmienic pliku: {0}" -f $_.Exception.Message)
    }
    Write-Info "Wstawiono $newVer."
}

# --- 5. kompilacja ----------------------------------------------------------
Write-Step "Kompilacja: pio run -e $envName (pierwsza moze potrwac kilka minut)"
& $pio run -d $projDir -e $envName
$code = $LASTEXITCODE
if ($code -ne 0 -and (Repair-PioarduinoCore $pio)) {
    Write-Step "Kompilacja - druga proba po doinstalowaniu PlatformIO Core"
    & $pio run -d $projDir -e $envName
    $code = $LASTEXITCODE
}
if ($code -ne 0) {
    if ($backup) {
        Copy-Item -LiteralPath $backup -Destination $targetCpp -Force -ErrorAction SilentlyContinue
        Write-Info "Przywrocono poprzedni src\main_centrala.cpp ($oldLabel)."
    }
    Exit-WithError "Kompilacja nie powiodla sie (kod $code). Skopiuj komunikaty z linii zawierajacych 'error:'."
}

$bin = [IO.Path]::Combine($projDir, '.pio', 'build', $envName, 'firmware.bin')
if (Test-Path -LiteralPath $bin -PathType Leaf) {
    $binSize = (Get-Item -LiteralPath $bin).Length
    Write-Info ('firmware.bin: {0} ({1:N0} B)' -f $bin, $binSize)
    if (-not $BezPulpitu) {
        $desktop = [Environment]::GetFolderPath('Desktop')
        if ($desktop -and (Test-Path -LiteralPath $desktop -PathType Container)) {
            $destBin = Join-Path $desktop ('firmware_{0}.bin' -f $newVer)
            Copy-Item -LiteralPath $bin -Destination $destBin -Force -ErrorAction SilentlyContinue
            if (Test-Path -LiteralPath $destBin -PathType Leaf) { Write-Info "Na pulpicie: $destBin" }
        }
    }
} else {
    Write-Info "Nie znalazlem $bin (inny build_dir w platformio.ini?)."
}

# --- 6. wgranie (tylko na wyrazne zyczenie) ---------------------------------
$releaseTag = $newVer.TrimStart('v', 'V')
if ($Wgraj -eq 'Brak') {
    Write-Host ''
    Write-Host 'Gotowe - skompilowano, niczego nie wgrywano.' -ForegroundColor Green
    Write-Info 'Wgranie na centrale: uruchom ten skrypt ponownie z parametrem'
    Write-Info '  -Wgraj OTA  (przez siec lokalna, komputer w tej samej sieci co centrala)'
    Write-Info '  -Wgraj USB  (kablem)'
    Write-Info ("Albo: GitHub Release z tagiem {0} i plikiem firmware.bin, potem /update w Telegramie." -f $releaseTag)
    exit 0
}

$uploadArgs = @('run', '-d', $projDir, '-e', $envName, '-t', 'upload')
if ($Wgraj -eq 'USB' -and $Port) { $uploadArgs += @('--upload-port', $Port) }
if ($Wgraj -eq 'OTA' -and $AdresOta) { $uploadArgs += @('--upload-port', $AdresOta) }
Write-Step ("Wgrywanie {0} na centrale ({1})" -f $newVer, $Wgraj)
& $pio @uploadArgs
$code = $LASTEXITCODE
if ($code -ne 0) {
    if ($Wgraj -eq 'OTA') {
        # -f wiaze mocniej niz +, dlatego sklejanie jest w nawiasie
        Exit-WithError (("Wgranie OTA nie powiodlo sie (kod {0}). Sprawdz: komputer w tej samej sieci co " +
            "centrala, adres centrali (-AdresOta 192.168.x.x), zapora Windows dla Pythona. " +
            "Mozesz tez wgrac kablem: -Wgraj USB") -f $code)
    }
    Exit-WithError (("Wgranie USB nie powiodlo sie (kod {0}). Sprawdz kabel (z transmisja danych) i port " +
        "(-Port COM5). Gdy nie pomaga: przytrzymaj BOOT na plytce podczas laczenia.") -f $code)
}

Write-Host ''
Write-Host ("Wgrano {0}." -f $newVer) -ForegroundColor Green
Write-Info 'Centrala uruchomi sie ponownie (ok. 10-30 s). Potem sprawdz w aplikacji albo w panelu'
Write-Info ("wersje firmware ({0}) i znacznik LAN, gdy telefon jest w sieci centrali." -f $newVer)
if ($backup) { Write-Info "Powrot do poprzedniej wersji: skopiuj $backup jako src\main_centrala.cpp i wgraj ponownie." }
