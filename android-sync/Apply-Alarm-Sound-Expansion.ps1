$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$patchPath = Join-Path $PSScriptRoot 'Android-Alarm-Sound-Expansion.diff'
$assetsPath = Join-Path $PSScriptRoot 'Alarm-Sound-Assets'
$rawPath = Join-Path $repoRoot 'android\app\src\main\res\raw'
$notifyPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\core\AlarmNotify.kt'
$centerPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\core\AlarmCenter.kt'
$testPath = Join-Path $repoRoot 'android\app\src\test\java\com\sterownikco\pro\core\AlarmCenterTest.kt'
$ids = @('syrena_falujaca', 'syrena_ratunkowa', 'klakson_pulsacyjny', 'gong_ostrzegawczy')
$files = @(
    'alarm_syrena_falujaca.wav',
    'alarm_syrena_ratunkowa.wav',
    'alarm_klakson_pulsacyjny.wav',
    'alarm_gong_ostrzegawczy.wav'
)

if (-not (Test-Path $patchPath) -or -not (Test-Path $rawPath)) {
    throw 'Nie znaleziono patcha albo katalogu projektu Android. Uruchom skrypt z checkoutu repozytorium.'
}
foreach ($path in @($notifyPath, $centerPath, $testPath)) {
    if (-not (Test-Path $path)) {
        throw "Brak oczekiwanego pliku źródłowego: $path. Najpierw zastosuj bazowy patch Androida z selektorem alarmów."
    }
}

# Sprawdź wszystkie wejścia i kolizje przed zmianą źródeł; nigdy nie nadpisuj innego WAV-a.
foreach ($file in $files) {
    $source = Join-Path $assetsPath $file
    $destination = Join-Path $rawPath $file
    if (-not (Test-Path $source)) { throw "Brak zasobu źródłowego: $source" }
    if (Test-Path $destination) {
        $sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash
        $targetHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $destination).Hash
        if ($sourceHash -ne $targetHash) {
            throw "Plik już istnieje i ma inną zawartość; nie nadpisuję: $destination"
        }
    }
}

$notify = Get-Content -Raw -LiteralPath $notifyPath
$center = Get-Content -Raw -LiteralPath $centerPath
$tests = Get-Content -Raw -LiteralPath $testPath
$alreadyApplied = $true
foreach ($id in $ids) {
    $quotedId = '"' + $id + '"'
    if (-not $notify.Contains($quotedId) -or
        -not $center.Contains($quotedId) -or
        -not $notify.Contains(('R.raw.alarm_' + $id))) {
        $alreadyApplied = $false
    }
}
if (-not $tests.Contains('newAlarmSoundChoicesCanBeSavedForEachAlarmType')) {
    $alreadyApplied = $false
}

if (-not $alreadyApplied) {
    $partial = $false
    foreach ($id in $ids) {
        $quotedId = '"' + $id + '"'
        if ($notify.Contains($quotedId) -or $center.Contains($quotedId)) {
            $partial = $true
        }
    }
    if ($partial) {
        throw 'Wykryto częściowo zastosowaną poprawkę. Zatrzymuję się bez nadpisywania źródeł.'
    }

    Push-Location $repoRoot
    try {
        & git apply --check -- $patchPath
        if ($LASTEXITCODE -ne 0) { throw 'Kontrola git apply --check nie powiodła się; żaden plik nie został zmieniony.' }
        & git apply -- $patchPath
        if ($LASTEXITCODE -ne 0) { throw 'Nie udało się zastosować tekstowego patcha.' }
    }
    finally {
        Pop-Location
    }
}

# Potwierdź, że lista, zasoby R.raw i allow-list zapisu są zsynchronizowane.
$notify = Get-Content -Raw -LiteralPath $notifyPath
$center = Get-Content -Raw -LiteralPath $centerPath
$tests = Get-Content -Raw -LiteralPath $testPath
foreach ($id in $ids) {
    $quotedId = '"' + $id + '"'
    if (-not $notify.Contains($quotedId) -or
        -not $center.Contains($quotedId) -or
        -not $notify.Contains(('R.raw.alarm_' + $id))) {
        throw "Niepełne zastosowanie wyboru dźwięku: $id"
    }
}
if (-not $tests.Contains('newAlarmSoundChoicesCanBeSavedForEachAlarmType')) {
    throw 'Nie znaleziono testu zapisu nowych wyborów alarmu.'
}

foreach ($file in $files) {
    $source = Join-Path $assetsPath $file
    $destination = Join-Path $rawPath $file
    if (-not (Test-Path $destination)) {
        Copy-Item -LiteralPath $source -Destination $destination
    }
}

Write-Host 'Gotowe: dodano cztery dźwięki alarmowe. Istniejące, różniące się pliki nie były nadpisywane.' -ForegroundColor Green
