param([Parameter(Mandatory = $true)][string]$ReportPath)
$ErrorActionPreference = 'Stop'

# Check the actual staging layout consumed by CMake, including cache-hit paths.
$required = @(
    'third_party/onnxruntime/include/onnxruntime_c_api.h',
    'third_party/onnxruntime/include/onnxruntime_cxx_api.h',
    'third_party/onnxruntime/lib/onnxruntime.lib',
    'third_party/onnxruntime/bin/onnxruntime.dll',
    'third_party/sherpa-onnx/include/sherpa-onnx/c-api/c-api.h',
    'third_party/sherpa-onnx/lib/sherpa-onnx-c-api.lib',
    'third_party/sherpa-onnx/lib/sherpa-onnx-c-api.dll',
    'third_party/sherpa-onnx/lib/onnxruntime.lib',
    'third_party/sherpa-onnx/lib/onnxruntime.dll',
    'third_party/qtkeychain/include/qt6keychain/keychain.h',
    'third_party/qtkeychain/lib/qt6keychain.lib',
    'third_party/qtkeychain/bin/qt6keychain.dll',
    'third_party/qtkeychain/lib/cmake/Qt6Keychain/Qt6KeychainConfig.cmake',
    'third_party/rtl-deps/include/rtl-sdr.h',
    'third_party/rtl-deps/lib/rtlsdr.lib',
    'third_party/rtl-deps/bin/rtlsdr.dll',
    'third_party/rtl-deps/bin/libusb-1.0.dll'
)
if (-not $env:VULKAN_SDK) { throw 'VULKAN_SDK was not exported to the next workflow step' }
foreach ($relative in @('Bin/glslc.exe', 'Lib/vulkan-1.lib', 'Include/vulkan/vulkan.h',
                        'share/cmake/SPIRV-Headers/SPIRV-HeadersConfig.cmake')) {
    $required += Join-Path $env:VULKAN_SDK $relative
}
$rows = @(foreach ($path in $required) {
    if (-not (Test-Path $path -PathType Leaf) -or (Get-Item $path).Length -le 0) {
        throw "Missing or empty required dependency: $path"
    }
    [pscustomobject]@{path=$path; bytes=(Get-Item $path).Length; sha256=(Get-FileHash $path -Algorithm SHA256).Hash}
})
$rows | ConvertTo-Json | Set-Content $ReportPath
Write-Host "Verified $($rows.Count) nonempty dependency files; manifest: $ReportPath"
