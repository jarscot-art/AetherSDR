param(
    [ValidateSet('feature-original','feature-prepared','baseline-prepared')]
    [string]$Phase,
    [string]$Source,
    [string]$Reports,
    [string]$Collector
)
$ErrorActionPreference = 'Stop'
$build = Join-Path $Source 'build'
$Reports = [IO.Path]::GetFullPath($Reports)
$directory = Join-Path $Reports $Phase
New-Item -ItemType Directory -Force $directory | Out-Null
$five = @('icom_civ_test','icom_control_profile_test','icom_backend_seam_test','transmit_model_test','backend_family_switch_test')
$four = @($five | Where-Object { $_ -ne 'icom_civ_test' })
$sourceCommit = (git -C $Source rev-parse HEAD).Trim()
git -C $Source diff --exit-code -- src tests CMakeLists.txt .github/workflows
if ($LASTEXITCODE -ne 0) { throw 'Application/test/build/workflow sources were modified on runner' }

function New-ChildInfo([string]$Executable, [string[]]$Arguments, [string]$Work) {
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $info.WorkingDirectory = $Work
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.Environment.Clear()
    # Dumps may contain stack memory. Do not let test/collector children inherit
    # Actions credentials or service tokens from the runner environment.
    $allow = @('SystemRoot','WINDIR','SystemDrive','ComSpec','TEMP','TMP','USERPROFILE',
        'APPDATA','LOCALAPPDATA','USERNAME','USERDOMAIN','PATHEXT','OS',
        'PROCESSOR_ARCHITECTURE','PROCESSOR_IDENTIFIER','NUMBER_OF_PROCESSORS','PATH',
        'QT_ROOT_DIR','QT_PLUGIN_PATH','QML2_IMPORT_PATH')
    foreach ($name in $allow) {
        $value = [Environment]::GetEnvironmentVariable($name)
        if ($null -ne $value) { $info.Environment[$name] = $value }
    }
    $info.Environment['QT_QPA_PLATFORM'] = 'offscreen'
    $info.Environment['AETHER_WDSP_FFTW_TIMELIMIT'] = '0.001'
    $info.Environment['AETHER_WDSP_WISDOM_DIR'] = Join-Path $build 'test-fftw-wisdom'
    $info.Environment['AETHER_SETTINGS_DIR'] = Join-Path $directory 'isolated-settings'
    return $info
}

function Save-Capture([int]$TargetPid, [string]$Destination) {
    New-Item -ItemType Directory -Force $Destination | Out-Null
    $info = New-ChildInfo $Collector @("$TargetPid", $Destination, "$build;$(Split-Path $Collector)") $build
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    $null = $process.Start()
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $completed = $process.WaitForExit(90000)
    if (-not $completed) { $process.Kill($true); $process.WaitForExit() }
    $stdout.GetAwaiter().GetResult() | Set-Content "$Destination/collector.stdout.log"
    $stderr.GetAwaiter().GetResult() | Set-Content "$Destination/collector.stderr.log"
    @{ completed=$completed; exitCode=$process.ExitCode; targetPid=$TargetPid;
       finishedUtc=[DateTime]::UtcNow.ToString('o') } |
        ConvertTo-Json | Set-Content "$Destination/capture.json"
    $process.Dispose()
}

function Save-Dependencies([string]$Destination) {
    New-Item -ItemType Directory -Force $Destination | Out-Null
    $env:PATH | Set-Content "$Destination/PATH.txt"
    $search = @($build, "$env:SystemRoot\System32", $env:SystemRoot, $build) + @($env:PATH -split ';')
    $search | Set-Content "$Destination/search-paths.txt"
    $queue = [Collections.Generic.Queue[string]]::new()
    foreach ($name in $five) { $queue.Enqueue((Join-Path $build "$name.exe")) }
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $imports = [Collections.Generic.List[object]]::new()
    while ($queue.Count) {
        $binary = $queue.Dequeue()
        if (-not $seen.Add($binary)) { continue }
        if (-not (Test-Path $binary)) { throw "Missing executable/library: $binary" }
        $slug = [IO.Path]::GetFileName($binary)
        $lines = @(& dumpbin.exe /DEPENDENTS $binary)
        $exit = $LASTEXITCODE
        $lines | Set-Content "$Destination/$slug.dependents.txt"
        if ($exit -ne 0) { throw "dumpbin failed for $binary ($exit)" }
        & dumpbin.exe /HEADERS $binary | Set-Content "$Destination/$slug.headers.txt"
        foreach ($line in $lines) {
            if ($line -notmatch '^\s+([\w.+-]+\.dll)\s*$') { continue }
            $dll = $Matches[1]
            $resolved = $null
            $kind = 'MISSING'
            if ($dll -match '^(api|ext)-ms-') {
                $kind = 'WINDOWS_API_SET'
            } else {
                foreach ($entry in $search) {
                    if (-not $entry) { continue }
                    $candidate = Join-Path $entry $dll
                    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                        $resolved = [IO.Path]::GetFullPath($candidate)
                        $kind = 'FOUND'
                        break
                    }
                }
            }
            $imports.Add([pscustomobject]@{binary=$binary; dll=$dll; status=$kind; resolved=$resolved})
            if ($resolved -and -not $resolved.StartsWith($env:SystemRoot,[StringComparison]::OrdinalIgnoreCase)) {
                $queue.Enqueue($resolved)
            }
        }
    }
    $imports | Export-Csv "$Destination/import-resolution.csv" -NoTypeInformation
    $imports | ConvertTo-Json -Depth 4 | Set-Content "$Destination/import-resolution.json"
    $imports | Where-Object status -eq 'MISSING' | Format-Table -AutoSize | Out-String |
        Set-Content "$Destination/missing-imports.txt"
}

if ($Phase -eq 'feature-original') {
    # Smoke-test the collector before relying on it for any hung Qt process.
    $probe = [Diagnostics.Process]::new()
    $probe.StartInfo = New-ChildInfo $Collector @('--probe') $build
    $null = $probe.Start()
    Start-Sleep -Milliseconds 200
    try { Save-Capture $probe.Id (Join-Path $directory 'collector-smoke-test') }
    finally { if (-not $probe.HasExited) { $probe.Kill($true); $probe.WaitForExit() }; $probe.Dispose() }
    $probeResult = Get-Content -Raw "$directory/collector-smoke-test/capture.json" | ConvertFrom-Json
    if (-not $probeResult.completed -or $probeResult.exitCode -ne 0 -or
        -not (Test-Path "$directory/collector-smoke-test/process.dmp")) {
        throw 'Collector smoke test failed; cannot guarantee evidence before killing test processes'
    }
    Get-ChildItem "$Source/third_party" -Filter '*.dll' -Recurse -File |
        ForEach-Object { [pscustomobject]@{ path=$_.FullName; bytes=$_.Length;
            sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash } } |
        Export-Csv "$Reports/vendor-dll-inventory.csv" -NoTypeInformation
    $line = 0
    Get-Content "$Source/CMakeLists.txt" | ForEach-Object {
        $line++; if ($_ -match 'POST_BUILD|\.dll|FFTW3_DLL') { "${line}: $_" }
    } | Set-Content "$Reports/standard-app-dll-staging.txt"
}

if ($Phase -ne 'feature-original') {
    # Reproduce only runtime copying normally performed by AetherSDR POST_BUILD.
    # This directory affects child PATH, never tracked sources or global PATH.
    $runtime = Join-Path $build 'hf5a-runtime'
    New-Item -ItemType Directory -Force $runtime | Out-Null
    $folders = @('third_party/fftw3/bin','third_party/deepfilter/lib/windows-x86_64',
        'third_party/hidapi/bin','third_party/onnxruntime/bin','third_party/sherpa-onnx/bin')
    $copies = @(foreach ($folder in $folders) {
        $path = Join-Path $Source $folder
        if (-not (Test-Path $path)) { continue }
        foreach ($dll in Get-ChildItem $path -Filter '*.dll' -File) {
            $destination = Join-Path $runtime $dll.Name
            Copy-Item $dll.FullName $destination -Force
            [pscustomobject]@{source=$dll.FullName; destination=$destination;
                sha256=(Get-FileHash $destination -Algorithm SHA256).Hash}
        }
    })
    $copies | Export-Csv "$directory/staged-dlls.csv" -NoTypeInformation
    $env:PATH = "$runtime;$env:PATH"
}
Save-Dependencies (Join-Path $directory 'dll')

$selection = & ctest.exe --test-dir $build --show-only=json-v1 --no-tests=error -R '^(icom_civ_test|icom_control_profile_test|icom_backend_seam_test|transmit_model_test|backend_family_switch_test)$'
if ($LASTEXITCODE -ne 0) { throw 'CTest inventory failed' }
$selection -join "`n" | Set-Content "$directory/selection.json"
$selected = @(($selection -join "`n" | ConvertFrom-Json).tests | ForEach-Object name)
if ($selected.Count -ne 5 -or @(Compare-Object ($five | Sort-Object) ($selected | Sort-Object)).Count) {
    throw "Wrong test registration: $selected"
}

$rows = [Collections.Generic.List[object]]::new()
foreach ($name in $four) {
    $caseDir = Join-Path $directory $name
    New-Item -ItemType Directory -Force $caseDir | Out-Null
    $arguments = @('--test-dir',$build,'-R',"^$name`$",'--parallel','1','--verbose',
        '--output-on-failure','--no-tests=error','--output-junit',"$caseDir/results.junit.xml",
        '--output-log',"$caseDir/ctest.log")
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = New-ChildInfo (Get-Command ctest.exe).Source $arguments $build
    $start = [DateTime]::UtcNow
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $null = $process.Start()
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $testPid = 0
    $timeout = $false
    $diagnosticError = $null
    try {
        while (-not $process.WaitForExit(200)) {
            if (-not $testPid) {
                $child = Get-CimInstance Win32_Process -Filter "ParentProcessId = $($process.Id)" |
                    Where-Object Name -eq "$name.exe" | Select-Object -First 1
                if ($child) { $testPid = [int]$child.ProcessId }
            }
            if ($watch.Elapsed.TotalSeconds -lt 60) { continue }
            $timeout = $true
            if (-not $testPid) { throw '60 seconds elapsed, but test PID could not be identified' }
            Get-CimInstance Win32_Process -Filter "ProcessId = $testPid" |
                Select-Object ProcessId,ParentProcessId,Name,ExecutablePath,CreationDate |
                ConvertTo-Json | Set-Content "$caseDir/process.json"
            # Only terminate the known test PID after the collector has returned.
            Save-Capture $testPid (Join-Path $caseDir 'capture')
            break
        }
    } catch { $diagnosticError = $_.Exception.Message }
    finally {
        if ($timeout -and $testPid) {
            $target = Get-Process -Id $testPid -ErrorAction SilentlyContinue
            if ($target) { $target.Kill($true); $target.WaitForExit(); $target.Dispose() }
        }
        if (-not $process.WaitForExit(10000)) { $process.Kill($true); $process.WaitForExit() }
    }
    $watch.Stop()
    $stdout.GetAwaiter().GetResult() | Set-Content "$caseDir/stdout.log"
    $stderr.GetAwaiter().GetResult() | Set-Content "$caseDir/stderr.log"
    $status = if ($timeout) { 'TIMEOUT' } elseif ($process.ExitCode -eq 0) { 'PASS' } else { 'FAIL' }
    $row = [pscustomobject]@{ phase=$Phase; sourceCommit=$sourceCommit; test=$name; status=$status;
        ctestExitCode=$process.ExitCode; secondsIncludingCapture=$watch.Elapsed.TotalSeconds;
        testLimitSeconds=60; captureLimitSeconds=90; testPid=$testPid;
        startedUtc=$start.ToString('o'); diagnosticError=$diagnosticError }
    $row | ConvertTo-Json | Set-Content "$caseDir/result.json"
    $rows.Add($row)
    $rows | ConvertTo-Json -Depth 5 | Set-Content "$directory/results.json"
    $rows | Export-Csv "$directory/results.csv" -NoTypeInformation
    Write-Host "$Phase $name $status (CTest $($process.ExitCode))"
    $process.Dispose()
}
$rows | Format-Table test,status,secondsIncludingCapture -AutoSize
if ($Phase -eq 'feature-prepared') {
    $needsBaseline = @($rows | Where-Object status -eq 'TIMEOUT').Count -gt 0
    "needed=$($needsBaseline.ToString().ToLowerInvariant())" >> $env:GITHUB_OUTPUT
}
