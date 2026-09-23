[CmdletBinding()]
param(
    [string]$Preset = "windows-msvc-win32-preview",
    [string]$Asset = "",
    [switch]$SkipCapture,
    [switch]$SkipFullSuite,
    [switch]$NoLaunch
)

$ErrorActionPreference = "Stop"
[Console]::InputEncoding = [System.Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$env:PYTHONUTF8 = "1"
$env:PYTHONIOENCODING = "utf-8"

function Read-ReportInteger {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Name
    )
    $match = Select-String -LiteralPath $Path -Pattern ("^" + [Regex]::Escape($Name) + "=(\d+)$")
    if (-not $match) {
        throw "Win32 preview self-test report does not contain $Name=<integer>."
    }
    return [UInt64]::Parse($match.Matches[0].Groups[1].Value)
}

$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Push-Location $repo
try {
    Write-Host "[AveMotion Part 21] Configure: $Preset"
    cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

    Write-Host "[AveMotion Part 21] Build: $Preset"
    cmake --build --preset $Preset --parallel
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }

    Write-Host "[AveMotion Part 21] Central player + Win32 lifecycle validation"
    ctest --preset $Preset `
        -R "avemotion\.(cmake\.(win32_manifest_wiring|player_wiring)|player\.scheduler|win32\.preview\.selftest|direct2d\.(header|contract|smoke))" `
        -V --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Central player / Win32 lifecycle validation failed." }

    if (-not $SkipCapture) {
        Write-Host "[AveMotion Part 21] Native Direct2D capture corpus"
        ctest --preset $Preset `
            -R "avemotion\.(capture\.metrics|direct2d\.(capture_preflight|capture))" `
            -V --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw "Direct2D capture corpus failed." }
    }

    if (-not $SkipFullSuite) {
        Write-Host "[AveMotion Part 21] Remaining Telegram + Direct2D suite"
        $excludedTests = @(
            '^avemotion\.cmake\.(win32_manifest_wiring|player_wiring)$',
            '^avemotion\.player\.scheduler$',
            '^avemotion\.win32\.preview\.selftest$',
            '^avemotion\.direct2d\.(header|contract|smoke)$'
        )
        if ($SkipCapture) {
            # Keep portable capture metrics/preflight in the remaining suite,
            # but skip the expensive native 75-case WARP capture.
            $excludedTests += '^avemotion\.direct2d\.capture$'
        }
        else {
            # The focused capture stage above already ran all three tests.
            $excludedTests += '^avemotion\.capture\.metrics$'
            $excludedTests += '^avemotion\.direct2d\.(capture_preflight|capture)$'
        }
        $excludeRegex = '(' + ($excludedTests -join '|') + ')'
        & ctest --preset $Preset --output-on-failure -E $excludeRegex
        if ($LASTEXITCODE -ne 0) { throw "Remaining CTest suite failed." }
    }

    $buildRoot = Join-Path $repo "out/build/$Preset"
    $previewArtifactRoot = Join-Path $buildRoot "win32-preview-artifacts"
    $previewReport = Join-Path $previewArtifactRoot "avemotion-win32-preview-selftest.txt"
    $previewImage = Join-Path $previewArtifactRoot "avemotion-win32-preview-selftest.ppm"
    if (-not (Test-Path -LiteralPath $previewReport)) {
        throw "Expected Win32 preview self-test report was not produced: $previewReport"
    }
    if (-not (Test-Path -LiteralPath $previewImage)) {
        throw "Expected Win32 preview self-test image was not produced: $previewImage"
    }
    if (-not (Select-String -LiteralPath $previewReport -Pattern '^status=pass$')) {
        throw "Win32 preview self-test report does not contain status=pass."
    }
    if (-not (Select-String -LiteralPath $previewReport -Pattern '^finalDpi=144(?:\.0+)?$')) {
        throw "Win32 preview self-test did not finish on the expected 144-DPI target."
    }

    $playerTicks = Read-ReportInteger -Path $previewReport -Name "playerTicks"
    $playerFrames = Read-ReportInteger -Path $previewReport -Name "playerFramesReturned"
    $playerWakeups = Read-ReportInteger -Path $previewReport -Name "playerWakeupsScheduled"
    if ($playerTicks -lt 150) {
        throw "Central player self-test executed only $playerTicks ticks; expected at least 150."
    }
    if ($playerFrames -lt 150) {
        throw "Central player returned only $playerFrames frames; expected at least 150."
    }
    if ($playerWakeups -eq 0) {
        throw "Central player did not publish any host wakeup deadlines."
    }

    if (-not $SkipCapture) {
        $captureRoot = Join-Path $buildRoot "direct2d-capture-artifacts"
        $captureManifest = Join-Path $captureRoot "capture_manifest.tsv"
        if (-not (Test-Path -LiteralPath $captureManifest)) {
            throw "Expected Part 19 capture manifest was not produced: $captureManifest"
        }
        $manifestRows = (Get-Content -LiteralPath $captureManifest -Encoding UTF8).Count - 1
        if ($manifestRows -ne 75) {
            throw "Expected 75 capture rows, found $manifestRows."
        }
    }

    Write-Host "[AveMotion Part 21] PASS"
    Write-Host "Preview report: $previewReport"
    Write-Host "Preview image:  $previewImage"
    Write-Host "Player ticks/frames/wakeups: $playerTicks / $playerFrames / $playerWakeups"

    if (-not $NoLaunch) {
        $executable = Join-Path $buildRoot "avemotion_win32_preview.exe"
        if (-not (Test-Path -LiteralPath $executable)) {
            throw "Preview executable was not produced: $executable"
        }
        $arguments = @()
        if ($Asset) {
            $arguments += "--asset"
            $arguments += (Resolve-Path -LiteralPath $Asset).Path
        }
        Write-Host "[AveMotion Part 21] Launching interactive preview through the centralized Player."
        Write-Host "Controls: Space pause, R reverse, L loop, arrows seek, +/- speed, F5 recreate, Esc close."
        & $executable @arguments
        if ($LASTEXITCODE -ne 0) {
            throw "Interactive Win32 preview exited with code $LASTEXITCODE."
        }
    }
}
finally {
    Pop-Location
}
