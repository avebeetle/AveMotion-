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

$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Push-Location $repo
try {
    Write-Host "[AveMotion Part 20.4] Configure: $Preset"
    cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

    Write-Host "[AveMotion Part 20.4] Build: $Preset"
    cmake --build --preset $Preset --parallel
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }

    Write-Host "[AveMotion Part 20.4] Win32 preview lifecycle validation"
    ctest --preset $Preset `
        -R "avemotion\.(cmake\.win32_manifest_wiring|win32\.preview\.selftest|direct2d\.(header|contract|smoke))" `
        -V --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Win32 preview lifecycle validation failed." }

    if (-not $SkipCapture) {
        Write-Host "[AveMotion Part 20.4] Native Direct2D capture corpus"
        ctest --preset $Preset `
            -R "avemotion\.(capture\.metrics|direct2d\.(capture_preflight|capture))" `
            -V --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw "Direct2D capture corpus failed." }
    }

    if (-not $SkipFullSuite) {
        Write-Host "[AveMotion Part 20.4] Remaining Telegram + Direct2D suite"
        $excludedTests = @(
            '^avemotion\.win32\.preview\.selftest$',
            '^avemotion\.direct2d\.(header|contract|smoke)$'
        )
        if ($SkipCapture) {
            # Keep the portable metrics/preflight in the remaining suite, but
            # do not run the expensive native 75-case capture.
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
    $status = Select-String -LiteralPath $previewReport -Pattern '^status=pass$'
    if (-not $status) {
        throw "Win32 preview self-test report does not contain status=pass."
    }
    $dpiGate = Select-String -LiteralPath $previewReport -Pattern '^finalDpi=144(?:\.0+)?$'
    if (-not $dpiGate) {
        throw "Win32 preview self-test did not finish on the expected 144-DPI target."
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

    Write-Host "[AveMotion Part 20.4] PASS"
    Write-Host "Preview report: $previewReport"
    Write-Host "Preview image:  $previewImage"

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
        Write-Host "[AveMotion Part 20.4] Launching interactive preview."
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
