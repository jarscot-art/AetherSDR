# Windows PowerShell 5.1; packaging wrapper, no additional application flags.
param([switch]$OfflineCheck)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
try {
    $exe=Join-Path $PSScriptRoot 'AetherSDR.exe'
    if (-not (Test-Path $exe -PathType Leaf)) { throw 'AetherSDR.exe missing; extract the complete ZIP first.' }
    if (-not $OfflineCheck) {
        foreach ($root in @([Environment]::GetFolderPath('LocalApplicationData'),[Environment]::GetFolderPath('ApplicationData'))) {
            if (Test-Path (Join-Path $root 'AetherSDR')) {
                throw 'Existing AetherSDR user data detected. Use a separate Windows test account; do not delete or move production data.'
            }
        }
        Write-Host 'Full desktop, NOT RX-only. Native credentials are isolated only by a separate Windows account.'
        if ((Read-Host 'Confirm this is a separate test account, not the production account: type TEST') -cne 'TEST') {
            throw 'Test-account confirmation required; GUI not started.'
        }
    }
    $profile=Join-Path $PSScriptRoot ('test-profiles\'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $profile -ErrorAction Stop | Out-Null
    $profile=(Resolve-Path $profile).Path
    $env:AETHER_SETTINGS_DIR=$profile
    Remove-Item Env:AETHER_AUTOMATION,Env:AETHER_ICOM_PASSWORD,Env:QT_PLUGIN_PATH,Env:QT_QPA_PLATFORM_PLUGIN_PATH -ErrorAction SilentlyContinue
    if ($OfflineCheck) { $env:QT_QPA_PLATFORM='offscreen' }
    else { Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue }
    function Config([string[]]$Arguments) {
        # Quote the Windows command line explicitly; PowerShell 5.1's native
        # invocation otherwise strips JSON quotes from --config set values.
        $quoted=@(foreach ($arg in $Arguments) {
            $q=[regex]::Replace($arg, '(\\*)"', '$1$1\"')
            $q=[regex]::Replace($q, '(\\+)$', '$1$1')
            '"'+$q+'"'
        })
        $info=New-Object System.Diagnostics.ProcessStartInfo
        $info.FileName=$exe
        $info.Arguments='--config '+($quoted -join ' ')
        $info.UseShellExecute=$false
        $info.CreateNoWindow=$true
        $info.RedirectStandardOutput=$true
        $info.RedirectStandardError=$true
        $info.StandardOutputEncoding=New-Object System.Text.UTF8Encoding($false)
        $info.StandardErrorEncoding=New-Object System.Text.UTF8Encoding($false)
        $process=[Diagnostics.Process]::Start($info)
        try {
            $stdout=$process.StandardOutput.ReadToEndAsync()
            $stderr=$process.StandardError.ReadToEndAsync()
            if (-not $process.WaitForExit(45000)) { $process.Kill(); throw 'Settings CLI timed out.' }
            if ($process.ExitCode -ne 0) { throw "Settings CLI failed: $($Arguments[0]): $($stderr.Result)" }
            return $stdout.Result.Trim()
        } finally { $process.Dispose() }
    }
    # Qt returns mixed Windows/forward separators; compare canonical absolute paths.
    $actualDatabase=[IO.Path]::GetFullPath((Config @('path')))
    $expectedDatabase=[IO.Path]::GetFullPath((Join-Path $profile 'AetherSDR.db'))
    if ($actualDatabase -ine $expectedDatabase) { throw 'Settings escaped the new profile.' }
    $settings=@{
        AutoConnectToLastRadio='False'
        AutomationBridge='{"enabled":false,"txAllowed":false,"txAck":false,"readOnly":true}'
        Updates='{"checkOnStartup":false}'
        QsoRecordingDir=(Join-Path $profile 'Recordings')
    }
    foreach ($key in $settings.Keys) {
        $null=Config @('set',$key,$settings[$key])
        if ((Config @('get',$key)) -cne $settings[$key]) { throw "Setting did not persist: $key" }
    }
    foreach ($key in @('LastConnectedRadioSerial','LastRoutedRadioIp')) {
        if (Config @('list',$key)) { throw "Unexpected saved connection: $key" }
    }
    Write-Host "Isolated profile: $profile"
    if ($OfflineCheck) {
        $marker=Join-Path $profile 'qt-startup.txt'
        $p=Start-Process -FilePath $exe -ArgumentList @('--update-selftest',('"'+$marker+'"')) -PassThru
        if (-not $p.WaitForExit(45000)) { $p.Kill(); throw 'Offline Qt startup timed out.' }
        if ($p.ExitCode -ne 0 -or (Get-Content -Raw $marker).Trim() -ne '26.11.1') { throw 'Offline Qt startup failed.' }
        @{status='PASS'; profile=$profile; database=(Join-Path $profile 'AetherSDR.db');
          autoConnect=$false; mainWindowStarted=$false; nativeCredentialIsolation='requires separate Windows account'} |
          ConvertTo-Json | Set-Content (Join-Path $profile 'launcher-check.json') -Encoding UTF8
    } else {
        $p=Start-Process -FilePath $exe -WorkingDirectory $PSScriptRoot -PassThru -Wait
        if ($p.ExitCode -ne 0) { throw "AetherSDR exited with $($p.ExitCode)" }
    }
    exit 0
} catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
}
