$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$patchPath = Join-Path $PSScriptRoot 'Android-Telemetry-Day-Cache.diff'
$coreDir = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\core'
$rtdbPath = Join-Path $coreDir 'RtdbClient.kt'
$appModelPath = Join-Path $coreDir 'AppModel.kt'
$storePath = Join-Path $coreDir 'TelemetryDayStore.kt'
$systemPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\ui\screens\sheets\System.kt'
$storeTestPath = Join-Path $repoRoot 'android\app\src\test\java\com\sterownikco\pro\core\TelemetryDayStoreTest.kt'

if (-not (Test-Path $patchPath)) {
    throw 'Patch file not found. Run this script from the repository checkout.'
}
foreach ($path in @($rtdbPath, $appModelPath, $systemPath)) {
    if (-not (Test-Path $path)) {
        throw "Required Android source file is missing: $path"
    }
}

function Test-NewState {
    if (-not (Test-Path $storePath) -or -not (Test-Path $storeTestPath)) { return $false }
    $store = Get-Content -Raw -LiteralPath $storePath
    $rtdb = Get-Content -Raw -LiteralPath $rtdbPath
    $appModel = Get-Content -Raw -LiteralPath $appModelPath
    $system = Get-Content -Raw -LiteralPath $systemPath
    return $store.Contains('class TelemetryDayStore(') -and
        $rtdb.Contains('store.load(start, end, onProgress)') -and
        -not $rtdb.Contains('private suspend fun fetchShardFull(') -and
        $appModel.Contains('fun redownloadChartHistory()') -and
        $appModel.Contains('if (page == 1) loadRange(rangeSec)') -and
        $system.Contains('m.redownloadChartHistory()')
}

$rtdb = Get-Content -Raw -LiteralPath $rtdbPath
$appModel = Get-Content -Raw -LiteralPath $appModelPath
$system = Get-Content -Raw -LiteralPath $systemPath
$oldState = -not (Test-Path $storePath) -and
    -not (Test-Path $storeTestPath) -and
    $rtdb.Contains('suspend fun fetchTelemetry(rangeSec: Int): List<TelemPoint>?') -and
    $rtdb.Contains('private suspend fun fetchShardFull(') -and
    $appModel.Contains('val fb = Rtdb.fetchTelemetry(sec)') -and
    -not $appModel.Contains('fun redownloadChartHistory()') -and
    -not $system.Contains('m.redownloadChartHistory()')

if (Test-NewState) {
    Write-Host 'Already applied: chart history uses the local day cache (Firebase history untouched).' -ForegroundColor Green
}
elseif ($oldState) {
    Push-Location $repoRoot
    try {
        & git apply --check $patchPath
        if ($LASTEXITCODE -ne 0) {
            throw 'git apply check failed; no source files were changed.'
        }
        & git apply $patchPath
        if ($LASTEXITCODE -ne 0) {
            throw 'Could not apply the telemetry day-cache patch.'
        }
    }
    finally {
        Pop-Location
    }

    if (-not (Test-NewState)) {
        throw 'Post-apply verification failed. Review RtdbClient.kt, AppModel.kt, System.kt and TelemetryDayStore.kt before building.'
    }
    Write-Host 'Done: charts now download only new telemetry; complete days are kept on the phone.' -ForegroundColor Green
}
else {
    throw 'A partial or unexpected source state was found. No files were changed.'
}
