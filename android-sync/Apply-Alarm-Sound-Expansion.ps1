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
    throw 'Patch or Android source folder not found. Run this script from the repository checkout.'
}
foreach ($path in @($notifyPath, $centerPath, $testPath)) {
    if (-not (Test-Path $path)) {
        throw "Required source file is missing: $path. Apply the base Android alarm-picker patch first."
    }
}

# Validate inputs and conflicts before changing source; never overwrite a different WAV file.
foreach ($file in $files) {
    $source = Join-Path $assetsPath $file
    $destination = Join-Path $rawPath $file
    if (-not (Test-Path $source)) {
        throw "Source asset is missing: $source"
    }
    if (Test-Path $destination) {
        $sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash
        $targetHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $destination).Hash
        if ($sourceHash -ne $targetHash) {
            throw "A different file already exists; leaving it untouched: $destination"
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
        throw 'A partial patch was detected. Stopping without changing source files.'
    }

    Push-Location $repoRoot
    try {
        & git apply --check $patchPath
        if ($LASTEXITCODE -ne 0) {
            throw 'git apply check failed; no source files were changed.'
        }
        & git apply $patchPath
        if ($LASTEXITCODE -ne 0) {
            throw 'Could not apply the text patch.'
        }
    }
    finally {
        Pop-Location
    }
}

# Confirm the picker IDs, raw-resource mappings, and persistence allow-list match.
$notify = Get-Content -Raw -LiteralPath $notifyPath
$center = Get-Content -Raw -LiteralPath $centerPath
$tests = Get-Content -Raw -LiteralPath $testPath
foreach ($id in $ids) {
    $quotedId = '"' + $id + '"'
    if (-not $notify.Contains($quotedId) -or
        -not $center.Contains($quotedId) -or
        -not $notify.Contains(('R.raw.alarm_' + $id))) {
        throw "The sound patch is incomplete: $id"
    }
}
if (-not $tests.Contains('newAlarmSoundChoicesCanBeSavedForEachAlarmType')) {
    throw 'The new sound persistence test was not found.'
}

foreach ($file in $files) {
    $source = Join-Path $assetsPath $file
    $destination = Join-Path $rawPath $file
    if (-not (Test-Path $destination)) {
        Copy-Item -LiteralPath $source -Destination $destination
    }
}

Write-Host 'Done: four alarm sounds are ready. Existing different files were not overwritten.' -ForegroundColor Green
