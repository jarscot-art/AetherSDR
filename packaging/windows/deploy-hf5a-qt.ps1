# windeployqt classifies qt6keychain.dll as a Qt module and resolves it from
# qtpaths QT_INSTALL_BINS, not PATH or --dir (the deployment destination).
# See qtbase/src/tools/windeployqt/main.cpp: isQtModule,
# findDependentQtLibraries and deploy. Keep the installed Qt kit read-only.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$QtRoot,
    [Parameter(Mandatory)][string]$KeychainDll,
    [Parameter(Mandatory)][string]$DeployDir,
    [Parameter(Mandatory)][string]$EvidenceDir
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Require-File([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf) -or
        (Get-Item -LiteralPath $Path).Length -eq 0) {
        throw "Missing or empty deployment input: $Path"
    }
}

$QtRoot = (Resolve-Path -LiteralPath $QtRoot).Path
$DeployDir = (Resolve-Path -LiteralPath $DeployDir).Path
$EvidenceDir = (Resolve-Path -LiteralPath $EvidenceDir).Path
$KeychainDll = (Resolve-Path -LiteralPath $KeychainDll).Path
$exe = Join-Path $DeployDir 'AetherSDR.exe'
foreach ($file in @($exe, $KeychainDll, "$QtRoot/bin/windeployqt.exe", "$QtRoot/bin/qtpaths.exe")) {
    Require-File $file
}
$keychainHash = (Get-FileHash -LiteralPath $KeychainDll -Algorithm SHA256).Hash

# Capture the actual imports before deployment, including QtKeychain's own DLLs.
foreach ($file in @($exe, $KeychainDll)) {
    $imports = & dumpbin /nologo /DEPENDENTS $file
    if ($LASTEXITCODE -ne 0) { throw "Cannot inspect imports: $file" }
    "IMAGE: $file" | Add-Content "$EvidenceDir/qt-predeploy-imports.log"
    $imports | Add-Content "$EvidenceDir/qt-predeploy-imports.log"
    $required = if ($file -eq $exe) { 'qt6keychain.dll' } else { 'Qt6Core.dll' }
    if (-not ($imports | Select-String -SimpleMatch -Quiet -Pattern $required)) {
        throw "Expected import $required missing from $file"
    }
}

# A real copy (no hard links/junctions) with its own qt.conf. Copy the whole kit
# so module metadata, plugins and QML use exactly the installed Qt version.
$privateQt = Join-Path ([IO.Path]::GetTempPath()) ('hf5a-deploy-qt-' + [guid]::NewGuid().ToString('N'))
if ($privateQt.StartsWith($QtRoot.TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar,
                         [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Private deployment kit must be outside the installed Qt directory'
}
New-Item -ItemType Directory $privateQt | Out-Null
& robocopy $QtRoot $privateQt /E /COPY:DAT /DCOPY:DAT /XJ /R:0 /W:0 /NFL /NDL |
    Set-Content "$EvidenceDir/qt-private-copy.log"
if ($LASTEXITCODE -lt 0 -or $LASTEXITCODE -ge 8) { throw "Private Qt copy failed: $LASTEXITCODE" }
$privateBin = Join-Path $privateQt 'bin'
@('[Paths]', 'Prefix=..') | Set-Content "$privateBin/qt.conf" -Encoding ascii
# A version-specific qt6.conf takes precedence if the installed kit has one.
if (Test-Path "$privateBin/qt6.conf") {
    @('[Paths]', 'Prefix=..') | Set-Content "$privateBin/qt6.conf" -Encoding ascii
}
Copy-Item -LiteralPath $KeychainDll -Destination "$privateBin/qt6keychain.dll" -Force
Copy-Item -LiteralPath $KeychainDll -Destination "$DeployDir/qt6keychain.dll" -Force
$qtpaths = Join-Path $privateBin 'qtpaths.exe'
$deployer = Join-Path $privateBin 'windeployqt.exe'
Require-File $qtpaths
Require-File $deployer

$version = & $qtpaths --qt-version
if ($LASTEXITCODE -ne 0 -or "$version".Trim() -ne '6.12.0') { throw 'Unexpected deployment Qt version' }
$query = & $qtpaths --query
if ($LASTEXITCODE -ne 0) { throw 'Private qtpaths query failed' }
$query | Set-Content "$EvidenceDir/qt-private-query.log"
$bins = & $qtpaths --query QT_INSTALL_BINS
if ($LASTEXITCODE -ne 0 -or @($bins).Count -ne 1) { throw 'Cannot determine Qt DLL search directory' }
if ([IO.Path]::GetFullPath("$bins".Trim()) -ne [IO.Path]::GetFullPath($privateBin)) {
    throw "windeployqt would search outside the private bin: $bins"
}
Require-File (Join-Path ("$bins".Trim()) 'qt6keychain.dll')
$help = & $deployer --help
if ($LASTEXITCODE -ne 0 -or -not ($help | Select-String -SimpleMatch -Quiet -Pattern '--qtpaths')) {
    throw 'This windeployqt does not support an explicit qtpaths executable'
}
$help | Set-Content "$EvidenceDir/qt-deployer-help.log"
@{qtVersion="$version".Trim(); originalQt=$QtRoot; privateQt=$privateQt;
  qtInstallBins="$bins".Trim(); keychainSource=$KeychainDll; keychainSHA256=$keychainHash} |
    ConvertTo-Json | Set-Content "$EvidenceDir/qt-deployment-inputs.json"

& $deployer --qtpaths $qtpaths --verbose 2 --release --no-translations --no-compiler-runtime --dir $DeployDir $exe |
    Tee-Object "$EvidenceDir/portable-deploy.log"
if ($LASTEXITCODE -ne 0) { throw "Qt portable deployment failed: $LASTEXITCODE" }
foreach ($file in @("$privateBin/qt6keychain.dll", "$DeployDir/qt6keychain.dll")) {
    Require-File $file
    if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $keychainHash) {
        throw "QtKeychain deployment changed the staged DLL: $file"
    }
}
# Import closure and the PATH-isolated EXE check remain mandatory in the caller.
exit 0
