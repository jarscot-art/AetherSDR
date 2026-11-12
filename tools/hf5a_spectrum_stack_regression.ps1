# Rebuild only the two test entry TUs from the failing commit, then restore exact
# bytes and rebuild. Never patch production sources or change the stack reserve.
$ErrorActionPreference = 'Stop'
$base = '8aedeaabeadbeae8141adbe51c769def21890dee'
$files = @('tests/spectrum_confirmed_geometry_test.cpp','tests/tx_marker_preview_test.cpp')
$saved = @{}
foreach ($file in $files) { $saved[$file] = [IO.File]::ReadAllBytes((Resolve-Path $file)) }
$results = (Resolve-Path 'hf5a-stage6-2b-results').Path
$env:QT_QPA_PLATFORM = 'offscreen'
$env:AETHER_AUTOMATION_NO_TX = '1'
$env:AETHER_WDSP_FFTW_TIMELIMIT = '0.001'
$env:AETHER_WDSP_WISDOM_DIR = Join-Path (Resolve-Path build) 'test-fftw-wisdom'
$env:AETHER_TEST_CRASH_DIR = Join-Path $results 'crashes'
New-Item -ItemType Directory -Force $env:AETHER_TEST_CRASH_DIR | Out-Null
$rows = @()
try {
    git fetch --no-tags --depth=1 origin $base
    if ($LASTEXITCODE -ne 0) { throw 'Cannot fetch exact failing source commit' }
    foreach ($file in $files) {
        $source = git show "${base}:$file"
        if ($LASTEXITCODE -ne 0) { throw "Cannot read baseline $file" }
        [IO.File]::WriteAllText((Resolve-Path $file), ($source -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
    }
    cmake --build build --parallel 2 --target spectrum_confirmed_geometry_test tx_marker_preview_test |
        Tee-Object (Join-Path $results 'stack-baseline-build.log')
    if ($LASTEXITCODE -ne 0) { throw 'Cannot build baseline test entry points' }
    foreach ($name in @('spectrum_confirmed_geometry_test','tx_marker_preview_test','spectrum_pan_pixels_retina_test')) {
        $exe = if ($name -eq 'spectrum_pan_pixels_retina_test') { 'spectrum_confirmed_geometry_test' } else { $name }
        $env:QT_SCALE_FACTOR = if ($name -eq 'spectrum_pan_pixels_retina_test') { '2' } else { '1' }
        $args = @{FilePath=(Join-Path (Resolve-Path build) "$exe.exe"); WorkingDirectory=(Resolve-Path build).Path;
            NoNewWindow=$true; PassThru=$true;
            RedirectStandardOutput=(Join-Path $results "$name.baseline.stdout.log");
            RedirectStandardError=(Join-Path $results "$name.baseline.stderr.log")}
        if ($name -eq 'spectrum_pan_pixels_retina_test') { $args.ArgumentList = '--pan-pixels-retina' }
        $process = Start-Process @args
        if (-not $process.WaitForExit(60000)) { $process.Kill(); throw "Baseline timed out: $name" }
        $process.WaitForExit()
        $code = [BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$process.ExitCode), 0)
        $hex = '0x{0:X8}' -f $code
        $stderr = Get-Content $args.RedirectStandardError -Raw
        # Captured on Windows: geometry/Retina overflow the stack; the preview
        # fixture dereferences its missing IcomSession before creating the widget.
        $expected = if ($name -eq 'tx_marker_preview_test') { '0xC0000005' } else { '0xC00000FD' }
        $nullSessionWrite = $name -ne 'tx_marker_preview_test' -or $stderr -match 'ACCESS operation=1 address=0xDA'
        $rows += [pscustomobject]@{test=$name; exitCode=$hex; expectedCode=$expected;
            reproduced=($hex -eq $expected -and $nullSessionWrite);
            exceptionRecorded=($stderr -match "NATIVE_EXCEPTION code=$expected")}
        Write-Host "BASELINE $name exit=$hex"
        Write-Host $stderr
    }
} finally {
    foreach ($file in $files) { [IO.File]::WriteAllBytes((Resolve-Path $file), $saved[$file]) }
    $env:QT_SCALE_FACTOR = '1'
    $rows | ConvertTo-Json | Set-Content (Join-Path $results 'stack-baseline.json')
    cmake --build build --parallel 2 --target spectrum_confirmed_geometry_test tx_marker_preview_test |
        Tee-Object (Join-Path $results 'stack-restored-build.log')
    if ($LASTEXITCODE -ne 0) { throw 'Restored test build failed' }
}
if ($rows.Count -ne 3 -or @($rows | Where-Object { -not $_.reproduced -or -not $_.exceptionRecorded }).Count) {
    throw 'Original native failures not reproduced exactly; inspect exception logs/dumps'
}
