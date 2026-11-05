$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false # expected child failures are assertions
$root = Join-Path $env:RUNNER_TEMP ([guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $root | Out-Null
try {
    $success = Join-Path $root 'cmdlets-only.ps1'
    $failure = Join-Path $root 'throw.ps1'
    $nonzero = Join-Path $root 'exit.ps1'
    "Write-Output 'cmdlets completed'" | Set-Content $success
    "throw 'expected fixture failure'" | Set-Content $failure
    'exit 7' | Set-Content $nonzero

    # A cmdlet-only script cannot supply a native-process exit code in-process.
    $global:LASTEXITCODE = $null
    & $success
    if ($null -ne $LASTEXITCODE -or -not ($LASTEXITCODE -ne 0)) {
        throw 'Did not reproduce the unset LASTEXITCODE false-failure condition'
    }
    foreach ($previous in @($null, 47)) {
        $global:LASTEXITCODE = $previous
        & pwsh -NoLogo -NoProfile -NonInteractive -File $success
        if ($LASTEXITCODE -ne 0) { throw 'Child success inherited an unrelated exit code' }
    }
    & pwsh -NoLogo -NoProfile -NonInteractive -File $failure
    if ($LASTEXITCODE -eq 0) { throw 'Child exception was hidden' }
    & pwsh -NoLogo -NoProfile -NonInteractive -File $nonzero
    if ($LASTEXITCODE -ne 7) { throw 'Explicit child failure was not preserved' }
    Write-Host 'PASS: unset/stale parent status cannot falsify child success; exception and explicit failure remain failures'
} finally {
    Remove-Item $root -Recurse -Force
}
exit 0
