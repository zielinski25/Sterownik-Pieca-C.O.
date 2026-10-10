$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$patchPath = Join-Path $PSScriptRoot 'Android-Terminal-Controls-Fullscreen.diff'
$systemPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\ui\screens\sheets\System.kt'
$chromePath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\ui\components\Chrome.kt'
$overlaysPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\ui\screens\Overlays.kt'
$appModelPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\core\AppModel.kt'

foreach ($path in @($patchPath, $systemPath, $chromePath, $overlaysPath, $appModelPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required file is missing: $path"
    }
}

function Read-SourceState {
    return @{
        System = Get-Content -Raw -Encoding UTF8 -LiteralPath $systemPath
        Chrome = Get-Content -Raw -Encoding UTF8 -LiteralPath $chromePath
        Overlays = Get-Content -Raw -Encoding UTF8 -LiteralPath $overlaysPath
        AppModel = Get-Content -Raw -Encoding UTF8 -LiteralPath $appModelPath
    }
}

function Test-ControlsApplied($state) {
    return (
        $state.System.Contains('var servoCommand by remember') -and
        $state.System.Contains('openServoPositionDialog("klapa")') -and
        $state.System.Contains('openServoPositionDialog("syberek")') -and
        $state.System.Contains('percent !in 0..100') -and
        $state.Chrome.Contains('fullScreen: Boolean = false') -and
        $state.Overlays.Contains('fullScreen = id == "terminal"')
    )
}

function Test-ExpectedBase($state) {
    return (
        $state.AppModel.Contains('fun terminalExec(cmd: String)') -and
        $state.AppModel.Contains('fun exportTerminalLogs(uri: android.net.Uri)') -and
        $state.System.Contains('saveLogsLauncher.launch(m.terminalLogFileName())') -and
        $state.System.Contains('listOf("diag remote on", "diag remote off", "pompa_wl", "pompa_wyl", "pompa_auto", "klapa 50", "syberek 50", "status", "diag status", "update_panel")') -and
        $state.Chrome.Contains('fun Sheet(title: String, icon: String, onDismiss: () -> Unit, content: @Composable ColumnScope.() -> Unit)') -and
        $state.Overlays.Contains('onDismiss = { m.openSheet(null) }') -and
        -not $state.System.Contains('var servoCommand by remember') -and
        -not $state.Chrome.Contains('fullScreen: Boolean = false') -and
        -not $state.Overlays.Contains('fullScreen = id == "terminal"')
    )
}

$state = Read-SourceState
if (Test-ControlsApplied $state) {
    Write-Host 'Terminal controls and full-screen window are already applied.' -ForegroundColor DarkGreen
    exit 0
}
if (-not (Test-ExpectedBase $state)) {
    throw 'The DLOG/export patch is missing or the terminal source is in an unexpected/partial state. No files were changed.'
}

Push-Location $repoRoot
try {
    & git apply --check $patchPath
    if ($LASTEXITCODE -ne 0) {
        throw 'Patch check failed; no source files were changed.'
    }
    & git apply $patchPath
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not apply the terminal controls patch.'
    }
}
finally {
    Pop-Location
}

$state = Read-SourceState
if (-not (Test-ControlsApplied $state)) {
    throw 'Post-apply verification failed. Review System.kt, Chrome.kt, and Overlays.kt before building.'
}
Write-Host 'Applied: full-screen terminal, grouped controls, and editable actuator positions from 0 to 100 percent.' -ForegroundColor Green
