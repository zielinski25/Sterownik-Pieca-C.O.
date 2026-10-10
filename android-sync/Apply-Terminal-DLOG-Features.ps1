$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$receivePatch = Join-Path $PSScriptRoot 'Android-Terminal-DLOG-Receive.diff'
$exportPatch = Join-Path $PSScriptRoot 'Android-Terminal-DLOG-Export.diff'
$rtdbPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\core\RtdbClient.kt'
$appModelPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\core\AppModel.kt'
$systemPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\ui\screens\sheets\System.kt'
$overlaysPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\ui\screens\Overlays.kt'
$androidReadmePath = Join-Path $repoRoot 'android\README.md'

foreach ($path in @($receivePatch, $exportPatch, $rtdbPath, $appModelPath, $systemPath, $overlaysPath, $androidReadmePath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required file is missing: $path"
    }
}

function Read-SourceState {
    return @{
        Rtdb = Get-Content -Raw -Encoding UTF8 -LiteralPath $rtdbPath
        AppModel = Get-Content -Raw -Encoding UTF8 -LiteralPath $appModelPath
        System = Get-Content -Raw -Encoding UTF8 -LiteralPath $systemPath
        Overlays = Get-Content -Raw -Encoding UTF8 -LiteralPath $overlaysPath
        Readme = Get-Content -Raw -Encoding UTF8 -LiteralPath $androidReadmePath
    }
}

function Test-ReceiveApplied($state) {
    return (
        $state.Rtdb.Contains('fun currentFirebaseUid(): String?') -and
        $state.Rtdb.Contains('queryParameters: Map<String, String>') -and
        $state.AppModel.Contains('private fun startTerminalDlogPolling()') -and
        $state.AppModel.Contains('/piec/devices/$uid/telemetry/diagnostics') -and
        $state.AppModel.Contains('terminalAppendDlogChunk') -and
        $state.AppModel.Contains('Remote potwierdzony; oczekiwanie na pierwszy chunk DLOG.') -and
        $state.Overlays.Contains('onDismiss = { m.openSheet(null) }') -and
        $state.Readme.Contains('maksymalnie 50 ostatnich chunków bieżącej sesji')
    )
}

function Test-OldReceiveState($state) {
    return (
        -not $state.Rtdb.Contains('fun currentFirebaseUid(): String?') -and
        $state.AppModel.Contains('Wersja Android nie odbiera strumienia DLOG.') -and
        -not $state.AppModel.Contains('private fun startTerminalDlogPolling()') -and
        $state.Overlays.Contains('onDismiss = { m.sheet = null }') -and
        $state.Readme.Contains('nie subskrybuje jeszcze strumienia')
    )
}

function Test-ExportApplied($state) {
    return (
        $state.AppModel.Contains('fun terminalLogFileName(): String') -and
        $state.AppModel.Contains('fun exportTerminalLogs(uri: android.net.Uri)') -and
        $state.System.Contains('ActivityResultContracts.CreateDocument("text/plain")') -and
        $state.System.Contains('saveLogsLauncher.launch(m.terminalLogFileName())') -and
        $state.Readme.Contains('Eksport bufora terminala DLOG')
    )
}

function Test-OldExportState($state) {
    return (
        $state.AppModel.Contains('fun terminalTogglePause()') -and
        -not $state.AppModel.Contains('fun terminalLogFileName(): String') -and
        -not $state.AppModel.Contains('fun exportTerminalLogs(uri: android.net.Uri)') -and
        $state.System.Contains('fun TerminalSheet(m: AppModel)') -and
        -not $state.System.Contains('rememberLauncherForActivityResult') -and
        -not $state.Readme.Contains('Eksport bufora terminala DLOG')
    )
}

$state = Read-SourceState
$receiveApplied = Test-ReceiveApplied $state
$exportApplied = Test-ExportApplied $state

if (-not $receiveApplied -and -not (Test-OldReceiveState $state)) {
    throw 'A partial or unexpected DLOG receiver state was found. No source files were changed.'
}
if (-not $exportApplied -and -not (Test-OldExportState $state)) {
    throw 'A partial or unexpected log-export state was found. No source files were changed.'
}

Push-Location $repoRoot
try {
    if (-not $receiveApplied) {
        & git apply --check $receivePatch
        if ($LASTEXITCODE -ne 0) {
            throw 'DLOG receive patch check failed; no source files were changed.'
        }
        & git apply $receivePatch
        if ($LASTEXITCODE -ne 0) {
            throw 'Could not apply the DLOG receive patch.'
        }
        Write-Host 'Applied the real-DLOG receiver.' -ForegroundColor Green
    }
    else {
        Write-Host 'Real-DLOG receiver is already applied.' -ForegroundColor DarkGreen
    }

    $state = Read-SourceState
    if (-not (Test-ReceiveApplied $state)) {
        throw 'DLOG receiver verification failed. Review the Android source files before building.'
    }

    if (-not $exportApplied) {
        & git apply --check $exportPatch
        if ($LASTEXITCODE -ne 0) {
            throw 'Log-export patch check failed; the receiver may already be applied, but export files were not changed.'
        }
        & git apply $exportPatch
        if ($LASTEXITCODE -ne 0) {
            throw 'Could not apply the log-export patch.'
        }
        Write-Host 'Applied the save-logs button and text-file export.' -ForegroundColor Green
    }
    else {
        Write-Host 'Log export is already applied.' -ForegroundColor DarkGreen
    }
}
finally {
    Pop-Location
}

$state = Read-SourceState
if (-not (Test-ReceiveApplied $state) -or -not (Test-ExportApplied $state)) {
    throw 'Final verification failed. Review the Android source files before building.'
}
Write-Host 'Done: the terminal receives real DLOG entries and can save its retained buffer as UTF-8 text.' -ForegroundColor Green
