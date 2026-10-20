$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$patchPath = Join-Path $PSScriptRoot 'Android-LAN-Local-Mode.diff'
$coreDir = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\core'
$rtdbPath = Join-Path $coreDir 'RtdbClient.kt'
$appModelPath = Join-Path $coreDir 'AppModel.kt'
$lanLinkPath = Join-Path $coreDir 'LanLink.kt'
$espHistoryPath = Join-Path $coreDir 'EspLocalHistory.kt'
$servicePath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\service\AlarmMonitorService.kt'
$systemPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\ui\screens\sheets\System.kt'
$cacheStorePath = Join-Path $coreDir 'TelemetryDayStore.kt'

if (-not (Test-Path $patchPath)) {
    throw 'Patch file not found. Run this script from the repository checkout.'
}
foreach ($path in @($rtdbPath, $appModelPath, $servicePath, $systemPath)) {
    if (-not (Test-Path $path)) {
        throw "Required Android source file is missing: $path"
    }
}
if (-not (Test-Path $cacheStorePath)) {
    throw 'The chart day-cache change is missing. Run Apply-Telemetry-Day-Cache.ps1 first.'
}

function Test-NewState {
    if (-not (Test-Path $lanLinkPath) -or -not (Test-Path $espHistoryPath)) { return $false }
    $lanLink = Get-Content -Raw -LiteralPath $lanLinkPath
    $rtdb = Get-Content -Raw -LiteralPath $rtdbPath
    $appModel = Get-Content -Raw -LiteralPath $appModelPath
    $service = Get-Content -Raw -LiteralPath $servicePath
    $system = Get-Content -Raw -LiteralPath $systemPath
    return $lanLink.Contains('internal object LanLink') -and
        $rtdb.Contains('suspend fun sendCommandAuto(') -and
        $rtdb.Contains('suspend fun lanStatus(') -and
        $appModel.Contains('private suspend fun pollLan(') -and
        $service.Contains('pollStatusForMonitor(') -and
        $system.Contains('private fun LanSection(')
}

$rtdb = Get-Content -Raw -LiteralPath $rtdbPath
$appModel = Get-Content -Raw -LiteralPath $appModelPath
$oldState = -not (Test-Path $lanLinkPath) -and
    -not (Test-Path $espHistoryPath) -and
    $rtdb.Contains('store.load(start, end, onProgress)') -and
    -not $rtdb.Contains('suspend fun sendCommandAuto(') -and
    -not $appModel.Contains('private suspend fun pollLan(')

if (Test-NewState) {
    Write-Host 'Already applied: LAN mode (controller on the local network, works without internet).' -ForegroundColor Green
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
            throw 'Could not apply the LAN-mode patch.'
        }
    }
    finally {
        Pop-Location
    }

    if (-not (Test-NewState)) {
        throw 'Post-apply verification failed. Review RtdbClient.kt, AppModel.kt, AlarmMonitorService.kt and System.kt before building.'
    }
    Write-Host 'Done: the app now talks to the controller directly on the local network (LAN), also without internet.' -ForegroundColor Green
}
else {
    throw 'A partial or unexpected source state was found. No files were changed.'
}
