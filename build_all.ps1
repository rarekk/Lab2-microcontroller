# Rebuild every Lab 2 module with ESP-IDF v6.1.
# Run with:
#   powershell -NoProfile -ExecutionPolicy Bypass -File C:\microcontrollerPJ\Lab2\build_all.ps1

$ErrorActionPreference = "Stop"
$env:PYTHONUTF8 = "1"

$idfProfile = "C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1"
if (-not $env:IDF_PATH) {
    if (-not (Test-Path $idfProfile)) {
        throw "ESP-IDF profile not found: $idfProfile"
    }
    . $idfProfile | Out-Null
}

$modules = @(
    "module1_gpio_switch",
    "module2_brightness_control",
    "module3_seven_segment"
)

foreach ($module in $modules) {
    $moduleDir = Join-Path $PSScriptRoot $module
    Write-Host ""
    Write-Host "=== Building $module ===" -ForegroundColor Cyan

    Push-Location $moduleDir
    try {
        if (-not (Test-Path "build\flasher_args.json")) {
            idf.py set-target esp32
            if ($LASTEXITCODE -ne 0) {
                throw "set-target failed for $module"
            }
        }

        idf.py build
        if ($LASTEXITCODE -ne 0) {
            throw "build failed for $module"
        }

        if (-not (Test-Path "build\flasher_args.json")) {
            throw "build/flasher_args.json was not generated for $module"
        }
    }
    finally {
        Pop-Location
    }
}

Write-Host ""
Write-Host "All modules built. Open one module folder as the Wokwi workspace." -ForegroundColor Green
