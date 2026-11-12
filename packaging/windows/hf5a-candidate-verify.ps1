# CI only. Verify the packaged launcher under Windows PowerShell 5.1.
param([Parameter(Mandatory=$true)][string]$Bundle,[Parameter(Mandatory=$true)][string]$Report)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$bundlePath=(Resolve-Path $Bundle).Path
$native=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'AetherSDR'
$sentinel=Join-Path $native 'hf5a-production-sentinel.txt'
if (Test-Path $sentinel) { throw 'Do not overwrite an existing sentinel.' }
New-Item -ItemType Directory -Force $native | Out-Null
[guid]::NewGuid().ToString() | Set-Content $sentinel
function Snapshot {
    return @((Get-ChildItem $native -Recurse -File | Sort-Object FullName | ForEach-Object {
        $_.FullName+' '+(Get-FileHash $_.FullName -Algorithm SHA256).Hash
    })) -join "`n"
}
$before=Snapshot
$savedPath=$env:PATH
$env:AETHER_SETTINGS_DIR=$native # Hostile inherited override must be replaced.
$env:AETHER_AUTOMATION='1'
$env:AETHER_ICOM_PASSWORD='disposable-test-value'
try {
    $env:PATH="$env:SystemRoot\System32;$env:SystemRoot"
    $launcher=Join-Path $bundlePath 'hf5a-candidate-launch.ps1'
    $ps=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    for ($i=0;$i -lt 2;$i++) {
        & $ps -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $launcher -OfflineCheck
        if ($LASTEXITCODE -ne 0) { throw 'Packaged launcher offline check failed.' }
    }
    $proof=@(Get-ChildItem (Join-Path $bundlePath 'test-profiles') -Filter launcher-check.json -Recurse -File |
        ForEach-Object { Get-Content -Raw $_.FullName | ConvertFrom-Json })
    if ($proof.Count -ne 2 -or $proof[0].profile -eq $proof[1].profile -or @($proof | Where-Object { $_.status -ne 'PASS' }).Count) {
        throw 'Each launch must create and validate a distinct profile.'
    }
    $ErrorActionPreference='Continue' # Expected refusal writes to stderr.
    & $ps -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $launcher 2>&1 | Out-Host
    $refusalCode=$LASTEXITCODE
    $ErrorActionPreference='Stop'
    if ($refusalCode -ne 1) { throw 'Launcher did not refuse the existing production-account directory.' }
    if ((Snapshot) -cne $before) { throw 'Production-profile sentinel tree changed.' }
    @{status='PASS'; twoDistinctProfiles=$true; inheritedProfileOverridden=$true;
      productionTreeUnchanged=$true; productionAccountLaunchRefused=$true;
      offlineScope='settings CLI + Qt startup; no MainWindow or radio session'; launches=$proof} |
      ConvertTo-Json -Depth 5 | Set-Content $Report -Encoding UTF8
} finally {
    $env:PATH=$savedPath
}
# Only disposable profiles created by the preceding checks; never ship CI settings.
Remove-Item (Join-Path $bundlePath 'test-profiles') -Recurse -Force
