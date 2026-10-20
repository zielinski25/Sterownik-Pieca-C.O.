<#
.SYNOPSIS
    Porzadkuje pulpit z plikow Sterownika Pieca C.O. Nic nie usuwa - tylko przenosi.

.DESCRIPTION
    Bez parametrow skrypt tylko POKAZUJE plan. Przenosi dopiero z -Wykonaj.

    Najnowszy plik kazdego rodzaju trafia do folderu Pulpit\SterownikCO:
      Piec*.html          - panel WWW,
      main_centrala*.cpp  - zrodlo firmware centrali,
      firmware*.bin       - firmware do wgrania,
      SterownikCO*.apk    - aplikacja Android.
    Do Pulpit\SterownikCO\archiwum trafiaja:
      - starsze pliki tych rodzajow (takze te, ktore juz lezaly w SterownikCO),
      - archiwa ZIP sterownika: Cenatrala*.zip, SterownikCO*.zip, main_centrala*.zip,
      - stare kopie projektu PlatformIO centrali: foldery, ktore same zawieraja
        platformio.ini z sekcja [env:esp32-s3-n16r8].
    Wszystko inne zostaje na pulpicie - skrypt tylko to wypisuje.
    Gdy w miejscu docelowym jest juz element o tej nazwie, przenoszony dostaje dopisek
    " (2)", " (3)"... - nic nie jest nadpisywane.

.PARAMETER Wykonaj
    Naprawde przenosi (bez tego tylko podglad).

.PARAMETER DodatkoweKopie
    Archiwizuje tez dodatkowe APK, paczki chart-fix, kopie panelu .bak oraz
    nazwane kopie projektow sterownika. Nie wybiera z nich aktualnej wersji.

.PARAMETER ResztaPulpitu
    Grupuje pozostale rozpoznane elementy w Projekty, Druk3D, Dokumenty
    i Do-przejrzenia. Nie przenosi skrotow ani nieznanych folderow.

.PARAMETER Pulpit
    Inny folder do uporzadkowania (domyslnie pulpit biezacego uzytkownika).

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File "$env:USERPROFILE\SterownikCO\narzedzia\Porzadki-Pulpitu.ps1"

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File "$env:USERPROFILE\SterownikCO\narzedzia\Porzadki-Pulpitu.ps1" -Wykonaj
#>
[CmdletBinding()]
param(
    [switch]$Wykonaj,
    [switch]$DodatkoweKopie,
    [switch]$ResztaPulpitu,
    [string]$Pulpit
)

$ErrorActionPreference = 'Continue'

# Kazdy nieprzewidziany blad przerywa skrypt (kod 1) zamiast przenosic dalej z niepelnym planem.
trap {
    Write-Host ''
    Write-Host ('BLAD: {0}' -f $_.Exception.Message) -ForegroundColor Red
    if ($_.InvocationInfo) { Write-Host ('  linia {0}: {1}' -f $_.InvocationInfo.ScriptLineNumber, ([string]$_.InvocationInfo.Line).Trim()) }
    exit 1
}

$FolderName  = 'SterownikCO'
$ArchiveName = 'archiwum'
$EnvMarker   = 'esp32-s3-n16r8'

# Rodzaje plikow: najnowszy zostaje w SterownikCO, starsze ida do archiwum.
$Kinds = @(
    @{ Label = 'panel HTML';      Pattern = 'Piec*.html' },
    @{ Label = 'zrodlo firmware'; Pattern = 'main_centrala*.cpp' },
    @{ Label = 'firmware .bin';   Pattern = 'firmware*.bin' },
    @{ Label = 'aplikacja APK';   Pattern = 'SterownikCO*.apk' }
)
$ZipPatterns = @('Cenatrala*.zip', 'SterownikCO*.zip', 'main_centrala*.zip')

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

function Get-Files([string]$Dir, [string]$Pattern) {
    # Pliki bezposrednio w katalogu pasujace do wzorca (dodatkowe -like omija dopasowania
    # Windows po krotkich nazwach 8.3).
    if (-not (Test-Path -LiteralPath $Dir -PathType Container)) { return @() }
    return @(Get-ChildItem -LiteralPath $Dir -Filter $Pattern -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like $Pattern })
}

function Get-ProjectVersion([string]$Dir) {
    # $null = folder nie jest projektem centrali; inaczej wersja z src\main_centrala.cpp albo ''.
    $ini = Join-Path $Dir 'platformio.ini'
    if (-not (Test-Path -LiteralPath $ini -PathType Leaf)) { return $null }
    $isCentrala = Select-String -LiteralPath $ini -Pattern ('^\s*\[env:' + [regex]::Escape($EnvMarker) + '\]') -List -Quiet
    if (-not $isCentrala) { return $null }
    $src = Join-Path $Dir 'src\main_centrala.cpp'
    if (-not (Test-Path -LiteralPath $src -PathType Leaf)) { return '' }
    $hit = Select-String -LiteralPath $src -Pattern 'static const char\* FIRMWARE_VERSION = "([^"]+)"' -List
    if ($hit) { return $hit.Matches[0].Groups[1].Value }
    return ''
}

function Get-FreePath([string]$Dir, [string]$Name, [bool]$IsDir) {
    # Sciezka w $Dir, ktora jeszcze nie istnieje: Nazwa, potem "Nazwa (2)", "Nazwa (3)"...
    $dest = Join-Path $Dir $Name
    if (-not (Test-Path -LiteralPath $dest)) { return $dest }
    $base = $Name
    $ext = ''
    if (-not $IsDir) {
        $base = [IO.Path]::GetFileNameWithoutExtension($Name)
        $ext = [IO.Path]::GetExtension($Name)
    }
    for ($i = 2; $i -lt 1000; $i++) {
        $candidate = Join-Path $Dir ('{0} ({1}){2}' -f $base, $i, $ext)
        if (-not (Test-Path -LiteralPath $candidate)) { return $candidate }
    }
    return $null
}

function New-PlanItem([string]$Source, [string]$DestDir, [string]$Why, [bool]$IsDir) {
    return [pscustomobject]@{ Source = $Source; DestDir = $DestDir; Why = $Why; IsDir = $IsDir }
}

# ---------------------------------------------------------------------------

Write-Host 'Sterownik Pieca C.O. - porzadki na pulpicie'

$desk = $Pulpit
if (-not $desk) { $desk = [Environment]::GetFolderPath('Desktop') }
if (-not $desk -or -not (Test-Path -LiteralPath $desk -PathType Container)) {
    Exit-WithError "Nie znalazlem folderu pulpitu ($desk)."
}
$desk = (Resolve-Path -LiteralPath $desk).ProviderPath.TrimEnd('\')
$target = Join-Path $desk $FolderName
$archive = Join-Path $target $ArchiveName
Write-Info "Pulpit: $desk"

# --- plan ------------------------------------------------------------------
$toArchive = New-Object System.Collections.Generic.List[object]
$toTarget = New-Object System.Collections.Generic.List[object]

foreach ($kind in $Kinds) {
    $files = @()
    # @() - funkcja bez wynikow zwraca $null, a += dopisaloby go jako element
    $files += @(Get-Files $desk $kind.Pattern)
    $files += @(Get-Files $target $kind.Pattern)
    if ($files.Count -eq 0) { continue }
    $sorted = @($files | Sort-Object -Property LastWriteTime -Descending)
    $newest = $sorted[0].FullName
    foreach ($f in $sorted) {
        $inTarget = ($f.DirectoryName.TrimEnd('\') -ieq $target)
        if ($f.FullName -eq $newest) {
            if (-not $inTarget) { $toTarget.Add((New-PlanItem $f.FullName $target ('najnowszy: ' + $kind.Label) $false)) }
        } else {
            $toArchive.Add((New-PlanItem $f.FullName $archive ('starszy: ' + $kind.Label) $false))
        }
    }
}

foreach ($pattern in $ZipPatterns) {
    foreach ($f in @(Get-Files $desk $pattern)) {
        $toArchive.Add((New-PlanItem $f.FullName $archive 'archiwum ZIP sterownika' $false))
    }
}

foreach ($d in @(Get-ChildItem -LiteralPath $desk -Directory -ErrorAction SilentlyContinue)) {
    if ($d.Name -ieq $FolderName) { continue }
    $ver = Get-ProjectVersion $d.FullName
    if ($null -eq $ver) { continue }
    $why = 'stara kopia projektu PlatformIO'
    if ($ver) { $why = $why + ', ' + $ver }
    $toArchive.Add((New-PlanItem $d.FullName $archive $why $true))
}

# Dodatkowy etap jest opt-in: nazwy z kolejnych paczek nie dowodza, ktora wersja
# jest aktualna. Wszystkie ida do archiwum, bez zmiany APK juz w SterownikCO.
if ($DodatkoweKopie) {
    $alreadyPlanned = @{}
    foreach ($group in @($toArchive, $toTarget)) {
        foreach ($p in $group) { $alreadyPlanned[$p.Source.ToLowerInvariant()] = $true }
    }
    foreach ($item in @(Get-ChildItem -LiteralPath $desk -ErrorAction Stop)) {
        if ($alreadyPlanned.ContainsKey($item.FullName.ToLowerInvariant())) { continue }
        # Nie archiwizuj junctionow/symlinkow do projektow w innych miejscach.
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
        $why = ''
        if ($item.PSIsContainer) {
            if ($item.Name -match '^(piec-android-chartfix-.+|piec-android-test|piec-co-final|piec-fw-v[0-9].*|sterownik-co-android_v[0-9].*|Sterownik-Pieca-C\.O\.-fresh-.+)$') {
                $why = 'dodatkowa kopia projektu (sprawdz przed przeniesieniem)'
            }
        } else {
            if ($item.Name -like 'Sterownik-*.apk' -or $item.Name -like 'SterownikPieca*.apk') {
                $why = 'dodatkowy APK (wersja niezweryfikowana)'
            } elseif ($item.Name -like 'Android-Chart-Fix-*.zip') {
                $why = 'paczka poprawek Android'
            } elseif ($item.Name -like 'Piec.html.*.bak') {
                $why = 'kopia panelu HTML'
            }
        }
        if ($why) { $toArchive.Add((New-PlanItem $item.FullName $archive $why ([bool]$item.PSIsContainer))) }
    }
}

# Ostatni etap: tylko nazwy i typy potwierdzone przez uzytkownika.
# Nie nazywamy projektow starymi ani nie archiwizujemy ich: moga byc uzywane.
if ($ResztaPulpitu) {
    $alreadyPlanned = @{}
    foreach ($group in @($toArchive, $toTarget)) {
        foreach ($p in $group) { $alreadyPlanned[$p.Source.ToLowerInvariant()] = $true }
    }
    $folderGroups = @{
        'app' = 'Projekty'; 'piec' = 'Projekty'
        'files' = 'Druk3D'; 'Marlin_Ender3V2S1_drukarka3D' = 'Druk3D'
        'OFERTA' = 'Dokumenty'; 'Nowy folder' = 'Do-przejrzenia'
    }
    $documentNames = @('TR.docx', 'Produkty promowane.txt', 'Nowy Dokument tekstowy.txt')
    foreach ($item in @(Get-ChildItem -LiteralPath $desk -ErrorAction Stop)) {
        if ($alreadyPlanned.ContainsKey($item.FullName.ToLowerInvariant())) { continue }
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
        $category = ''
        if ($item.PSIsContainer) {
            if ($folderGroups.ContainsKey($item.Name)) { $category = $folderGroups[$item.Name] }
        } elseif ($item.Name -like 'obj_*.stl') {
            $category = 'Druk3D'
        } elseif ($documentNames -contains $item.Name) {
            $category = 'Dokumenty'
        }
        if ($category) {
            $destDir = Join-Path $desk $category
            $toTarget.Add((New-PlanItem $item.FullName $destDir ('grupowanie: ' + $category) ([bool]$item.PSIsContainer)))
        }
    }
}

# --- podglad ---------------------------------------------------------------
# Listy przechodzone petla w petli: w Windows PowerShell 5.1 "@(lista) + @(lista)"
# dla List[object] konczy sie bledem "Argument types do not match".
$planned = @{}
foreach ($group in @($toArchive, $toTarget)) {
    foreach ($p in $group) { $planned[$p.Source.ToLowerInvariant()] = $true }
}

if ($Wykonaj) {
    Write-Step 'Przenoszenie'
} else {
    Write-Step 'Plan (podglad - nic jeszcze nie przeniesiono)'
}
if (($toArchive.Count + $toTarget.Count) -eq 0) {
    Write-Info 'Nic do przeniesienia - pliki sterownika sa juz uporzadkowane.'
} else {
    foreach ($group in @($toTarget, $toArchive)) {
        foreach ($p in $group) {
            $leaf = Split-Path -Leaf $p.Source
            if ($p.IsDir) { $leaf = $leaf + '\' }
            $where = $p.DestDir.Substring($desk.Length + 1) + '\'
            $from = ''
            if ((Split-Path -Parent $p.Source).TrimEnd('\') -ieq $target) { $from = $FolderName + '\' }
            Write-Info ('{0,-22} <- {1}{2}  ({3})' -f $where, $from, $leaf, $p.Why)
        }
    }
}

# --- wykonanie --------------------------------------------------------------
$moved = 0
$failed = 0
if ($Wykonaj -and ($toArchive.Count + $toTarget.Count) -gt 0) {
    try {
        foreach ($group in @($toArchive, $toTarget)) {
            foreach ($p in $group) {
                New-Item -ItemType Directory -Force -Path $p.DestDir -ErrorAction Stop | Out-Null
            }
        }
    } catch {
        Exit-WithError ("Nie udalo sie utworzyc folderu docelowego (pulpit {0}): {1}" -f $desk, $_.Exception.Message)
    }
    # Najpierw archiwum - zwalnia nazwy w SterownikCO dla najnowszych plikow.
    foreach ($group in @($toArchive, $toTarget)) {
        foreach ($p in $group) {
            $leaf = Split-Path -Leaf $p.Source
            $dest = Get-FreePath $p.DestDir $leaf $p.IsDir
            if (-not $dest) {
                $failed++
                Write-Info "NIE PRZENIESIONO $leaf - brak wolnej nazwy w $($p.DestDir)"
                continue
            }
            try {
                if ($p.IsDir) {
                    [IO.Directory]::Move($p.Source, $dest)
                } else {
                    [IO.File]::Move($p.Source, $dest)
                }
                $moved++
                if ((Split-Path -Leaf $dest) -ne $leaf) { Write-Info ("{0} -> {1} (nazwa zajeta)" -f $leaf, (Split-Path -Leaf $dest)) }
            } catch {
                $failed++
                Write-Info ("NIE PRZENIESIONO {0}: {1}" -f $leaf, $_.Exception.Message)
            }
        }
    }
    Write-Info ("Przeniesiono: {0}, nieudane: {1}." -f $moved, $failed)
}

# --- co zostaje -------------------------------------------------------------
$rest = @(Get-ChildItem -LiteralPath $desk -ErrorAction SilentlyContinue | Where-Object {
    if ($_.Name -ieq $FolderName) { return $false }
    # w podgladzie elementy z planu jeszcze leza na pulpicie - nie licz ich jako "zostaje"
    if (-not $Wykonaj -and $planned.ContainsKey($_.FullName.ToLowerInvariant())) { return $false }
    return $true
})
Write-Step ('Zostaje na pulpicie (poza planem; nierozpoznane lub inne pliki): {0}' -f $rest.Count)
$shown = 0
foreach ($r in ($rest | Sort-Object -Property @{ Expression = { -not $_.PSIsContainer } }, 'Name')) {
    if ($shown -ge 80) {
        Write-Info ('... i {0} wiecej' -f ($rest.Count - $shown))
        break
    }
    $label = $r.Name
    if ($r.PSIsContainer) { $label = $label + '\' }
    Write-Info $label
    $shown++
}

Write-Host ''
if (-not $Wykonaj) {
    if (($toArchive.Count + $toTarget.Count) -gt 0) {
        Write-Host 'To byl podglad. Aby przeniesc, uruchom skrypt ponownie z parametrem -Wykonaj' -ForegroundColor Yellow
    }
    exit 0
}
if ($failed -gt 0) {
    Write-Host ("Gotowe z bledami: {0} element(y) nie przeniesione (np. otwarte w innym programie)." -f $failed) -ForegroundColor Yellow
    exit 1
}
if ($ResztaPulpitu) {
    Write-Host 'Gotowe. Przeniesione elementy sa w folderach wskazanych w planie.' -ForegroundColor Green
} else {
    Write-Host ('Gotowe. Pliki sterownika sa w {0}' -f $target) -ForegroundColor Green
}
