$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$patchPath = Join-Path $PSScriptRoot 'Android-Terminal-DLOG-Category-Batch.diff'
$upgradePatchPath = Join-Path $PSScriptRoot 'Android-Terminal-DLOG-Category-Batch-Mutex-Upgrade.diff'
$appModelPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\core\AppModel.kt'
$systemPath = Join-Path $repoRoot 'android\app\src\main\java\com\sterownikco\pro\ui\screens\sheets\System.kt'

foreach ($path in @($patchPath, $upgradePatchPath, $appModelPath, $systemPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required file is missing: $path"
    }
}

function Read-SourceState {
    return @{
        AppModel = Get-Content -Raw -Encoding UTF8 -LiteralPath $appModelPath
        System = Get-Content -Raw -Encoding UTF8 -LiteralPath $systemPath
    }
}

function Test-BatchFixApplied($state) {
    return (
        $state.AppModel.Contains('var termCategoryBatchBusy by mutableStateOf(false)') -and
        $state.AppModel.Contains('private val terminalCommandMutex = Mutex()') -and
        $state.AppModel.Contains('private suspend fun <T> withTerminalCommandLock(action: suspend () -> T)') -and
        $state.AppModel.Contains('diag remote cat $cat $state') -and
        -not ($state.AppModel.Contains('diag remote cat ALL $state')) -and
        $state.System.Contains('enabled = !m.termCategoryBatchBusy')
    )
}

function Test-InitialBatchFix($state) {
    return (
        $state.AppModel.Contains('var termCategoryBatchBusy by mutableStateOf(false)') -and
        $state.AppModel.Contains('diag remote cat $cat $state') -and
        -not ($state.AppModel.Contains('diag remote cat ALL $state')) -and
        $state.System.Contains('enabled = !m.termCategoryBatchBusy') -and
        -not ($state.AppModel.Contains('private val terminalCommandMutex = Mutex()'))
    )
}

function Test-ExpectedBase($state) {
    return (
        $state.AppModel.Contains('diag remote cat ALL $state') -and
        $state.System.Contains('openServoPositionDialog("klapa")') -and
        $state.System.Contains('openServoPositionDialog("syberek")') -and
        -not ($state.AppModel.Contains('var termCategoryBatchBusy by mutableStateOf(false)')) -and
        -not ($state.System.Contains('m.termCategoryBatchBusy'))
    )
}

$state = Read-SourceState
if (Test-BatchFixApplied $state) {
    Write-Host 'DLOG category batch fix is already applied.' -ForegroundColor DarkGreen
    exit 0
}

if (Test-InitialBatchFix $state) {
    $activePatch = $upgradePatchPath
    Write-Host 'Updating the earlier DLOG batch fix to serialize terminal commands.' -ForegroundColor DarkYellow
}
elseif (Test-ExpectedBase $state) {
    $activePatch = $patchPath
}
else {
    throw 'Terminal controls or the DLOG category source are in an unexpected/partial state. No files were changed.'
}

Push-Location $repoRoot
try {
    & git apply --check $activePatch
    if ($LASTEXITCODE -ne 0) {
        throw 'Category-batch patch check failed; no source files were changed.'
    }
    & git apply $activePatch
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not apply the DLOG category batch patch.'
    }
}
finally {
    Pop-Location
}

$state = Read-SourceState
if (-not (Test-BatchFixApplied $state)) {
    throw 'Post-apply verification failed. Review AppModel.kt and System.kt before building.'
}
Write-Host 'Applied: all DLOG categories use individual ACK commands and a serialized terminal queue.' -ForegroundColor Green
