$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$patchPath = Join-Path $PSScriptRoot 'Android-Terminal-DLOG-Receive.diff'
$rtdbPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\core\RtdbClient.kt'
$appModelPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\core\AppModel.kt'
$overlaysPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\ui\screens\Overlays.kt'
$readmePath = Join-Path $repoRoot 'android\README.md'

if (-not (Test-Path -LiteralPath $patchPath)) {
    throw 'Patch file not found. Run this script from the repository checkout.'
}
foreach ($path in @($rtdbPath, $appModelPath, $overlaysPath, $readmePath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required Android source file is missing: $path"
    }
}

$rtdb = Get-Content -Raw -LiteralPath $rtdbPath
$appModel = Get-Content -Raw -LiteralPath $appModelPath
$overlays = Get-Content -Raw -LiteralPath $overlaysPath
$readme = Get-Content -Raw -LiteralPath $readmePath

$alreadyApplied =
    $rtdb.Contains('fun currentFirebaseUid(): String?') -and
    $rtdb.Contains('queryParameters: Map<String, String>') -and
    $appModel.Contains('private fun startTerminalDlogPolling()') -and
    $appModel.Contains('/piec/devices/$uid/telemetry/diagnostics') -and
    $appModel.Contains('terminalAppendDlogChunk') -and
    $appModel.Contains('Remote potwierdzony; oczekiwanie na pierwszy chunk DLOG.') -and
    $overlays.Contains('onDismiss = { m.openSheet(null) }') -and
    $readme.Contains('maksymalnie 50 ostatnich chunków bieżącej sesji')

$expectedOld =
    -not $rtdb.Contains('fun currentFirebaseUid(): String?') -and
    $appModel.Contains('Wersja Android nie odbiera strumienia DLOG.') -and
    -not $appModel.Contains('private fun startTerminalDlogPolling()') -and
    $overlays.Contains('onDismiss = { m.sheet = null }') -and
    $readme.Contains('nie subskrybuje jeszcze strumienia')

if ($alreadyApplied) {
    Write-Host 'Already applied: the Android terminal polls real DLOG chunks.' -ForegroundColor Green
}
elseif ($expectedOld) {
    Push-Location $repoRoot
    try {
        & git apply --check $patchPath
        if ($LASTEXITCODE -ne 0) {
            throw 'git apply check failed; no source files were changed.'
        }
        & git apply $patchPath
        if ($LASTEXITCODE -ne 0) {
            throw 'Could not apply the DLOG receive patch.'
        }
    }
    finally {
        Pop-Location
    }

    $rtdb = Get-Content -Raw -LiteralPath $rtdbPath
    $appModel = Get-Content -Raw -LiteralPath $appModelPath
    $overlays = Get-Content -Raw -LiteralPath $overlaysPath
    $readme = Get-Content -Raw -LiteralPath $readmePath
    if (-not $rtdb.Contains('fun currentFirebaseUid(): String?') -or
        -not $appModel.Contains('private fun startTerminalDlogPolling()') -or
        -not $appModel.Contains('/piec/devices/$uid/telemetry/diagnostics') -or
        $appModel.Contains('Wersja Android nie odbiera strumienia DLOG.') -or
        -not $overlays.Contains('onDismiss = { m.openSheet(null) }') -or
        -not $readme.Contains('maksymalnie 50 ostatnich chunków bieżącej sesji')) {
        throw 'Post-apply verification failed. Review the four files before building.'
    }
    Write-Host 'Done: the terminal now reads real DLOG chunks while its sheet is open.' -ForegroundColor Green
}
else {
    throw 'A partial or unexpected source state was found. No files were changed.'
}
