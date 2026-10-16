#requires -Version 5.1
$ErrorActionPreference = 'Stop'

# Run from the build worktree's android-sync directory. The original ZIP lives
# in the sibling checkout; it is only read and is never modified.
$BuildRoot = Split-Path -Parent $PSScriptRoot
$OriginalRepo = Split-Path -Parent $BuildRoot
$FirmwareZip = Join-Path $OriginalRepo 'Cenatrala Pieca.zip'
$FirmwarePatch = Join-Path $PSScriptRoot 'Android-Shared-Alarm-Mute-Firmware.patch'
$Desktop = [Environment]::GetFolderPath('Desktop')

if (-not (Test-Path -LiteralPath $FirmwareZip)) {
    throw "Nie znaleziono lokalnego archiwum firmware: $FirmwareZip"
}
if (-not (Test-Path -LiteralPath $FirmwarePatch)) {
    throw "Nie znaleziono patcha firmware: $FirmwarePatch"
}

$TempDir = Join-Path $env:TEMP ("Sterownik-Pieca-fw-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $TempDir | Out-Null
try {
    Expand-Archive -LiteralPath $FirmwareZip -DestinationPath $TempDir
    $CppPath = Join-Path $TempDir 'src\main_centrala.cpp'
    if (-not (Test-Path -LiteralPath $CppPath)) {
        throw "W ZIP-ie nie znaleziono src\main_centrala.cpp"
    }

    # Remember the source encoding marker and line endings before git apply.
    $OriginalBytes = [System.IO.File]::ReadAllBytes($CppPath)
    $HasUtf8Bom = $OriginalBytes.Length -ge 3 -and
        $OriginalBytes[0] -eq 0xEF -and $OriginalBytes[1] -eq 0xBB -and $OriginalBytes[2] -eq 0xBF
    $TextOffset = if ($HasUtf8Bom) { 3 } else { 0 }
    $OriginalText = [System.Text.Encoding]::UTF8.GetString(
        $OriginalBytes, $TextOffset, $OriginalBytes.Length - $TextOffset
    )
    $OutputNewline = if ($OriginalText.Contains("`r`n")) { "`r`n" } else { "`n" }

    # The archive's Windows CRLF line endings differ from the LF patch. Ignore
    # whitespace-only differences while checking and applying the source patch.
    Push-Location $TempDir
    try {
        & git apply --check --ignore-space-change $FirmwarePatch
        if ($LASTEXITCODE -ne 0) {
            throw "Patch nie pasuje do lokalnego src\main_centrala.cpp; ZIP pozostaje bez zmian."
        }
        & git apply --ignore-space-change $FirmwarePatch
        if ($LASTEXITCODE -ne 0) {
            throw "Patch application failed; the ZIP remains unchanged."
        }
    }
    finally {
        Pop-Location
    }

    $PatchedBytes = [System.IO.File]::ReadAllBytes($CppPath)
    $Text = [System.Text.Encoding]::UTF8.GetString(
        $PatchedBytes, $TextOffset, $PatchedBytes.Length - $TextOffset
    )
    # Normalize before editing, then restore the source file's original newline style.
    $Text = $Text.Replace("`r`n", "`n").Replace("`r", "`n")

    $VersionPattern = '(?m)^[ \t]*static[ \t]+const[ \t]+char[ \t]*\*[ \t]*FIRMWARE_VERSION[ \t]*=[ \t]*"v3\.36\.2";[ \t]*$'
    $VersionMatches = [regex]::Matches($Text, $VersionPattern)
    if ($VersionMatches.Count -ne 1) {
        throw "Expected one FIRMWARE_VERSION v3.36.2 constant; the source version differs. The ZIP remains unchanged."
    }
    $VersionMatch = $VersionMatches[0]
    $VersionLine = [regex]::Replace($VersionMatch.Value, 'v3\.36\.2', 'v3.37.0', 1)
    $Text = $Text.Remove($VersionMatch.Index, $VersionMatch.Length).Insert($VersionMatch.Index, $VersionLine)

    $Changelog = @'
// [v3.37.0 ALARM-SHARED-MUTE] - 2026-10-12
// Co: Wspolne, zdalne wyciszanie alarmow dymu i ogrzewania z timerem.
// Problem: Wyciszenie nie bylo wspoldzielone przez telefony i brzeczyk centrali;
//     brakowalo tez automatycznego wznowienia dzwieku po zadanym czasie.
// Przyczyna: Firmware nie przyjmowal wspolnej komendy wyciszenia ani nie
//     raportowal stanu i pozostalego czasu wyciszenia obu alarmow.
// Naprawa: Dodano alarm_mute dym|ogrz|oba <sekundy|off>, raportowanie stanu
//     wyciszenia w /piec/status i timer 1..3600 s; starsze komendy pozostaja.
// FIRMWARE_VERSION v3.36.2 -> v3.37.0 (MINOR: nowa funkcjonalnosc).
//
'@
    $Changelog = $Changelog.Replace("`r`n", "`n").Replace("`r", "`n")
    $Changelog = $Changelog.TrimEnd([char[]]"`n") + "`n`n"
    $Text = $Changelog + $Text

    if ($OutputNewline -eq "`r`n") {
        $Text = $Text.Replace("`n", "`r`n")
    }
    $Utf8 = [System.Text.UTF8Encoding]::new([bool]$HasUtf8Bom)
    [System.IO.File]::WriteAllText($CppPath, $Text, $Utf8)

    $FinalVersionPattern = '(?m)^[ \t]*static[ \t]+const[ \t]+char[ \t]*\*[ \t]*FIRMWARE_VERSION[ \t]*=[ \t]*"v3\.37\.0";[ \t]*$'
    if ([regex]::Matches($Text, $FinalVersionPattern).Count -ne 1 -or
        $Text -notmatch 'alarm_mute <dym\|ogrz\|oba>') {
        throw "Final C++ verification failed; the file will not be copied."
    }

    $OutputPath = Join-Path $Desktop 'main_centrala-v3.37.0-shared-alarm-mute.cpp'
    if (Test-Path -LiteralPath $OutputPath) {
        $OutputPath = Join-Path $Desktop ("main_centrala-v3.37.0-shared-alarm-mute-{0}.cpp" -f (Get-Date -Format 'yyyyMMdd-HHmmss'))
    }
    Copy-Item -LiteralPath $CppPath -Destination $OutputPath
    Get-Item -LiteralPath $OutputPath | Select-Object FullName, Length
}
finally {
    Remove-Item -LiteralPath $TempDir -Recurse -Force
}
