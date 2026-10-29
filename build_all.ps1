# Rebuild all five Lab 2 seven-segment modules with ESP-IDF v6.1.
#
# Usage:
#   powershell -NoProfile -ExecutionPolicy Bypass -File .\build_all.ps1
# or double-click / run:
#   build_all.cmd
#
# The script sets the ESP-IDF environment itself, so it does not depend on a
# PowerShell profile and does not need an already-activated IDF shell.

$ErrorActionPreference = "Stop"
$env:PYTHONUTF8 = "1"

$idfPath  = if ($env:IDF_PATH)  { $env:IDF_PATH }  else { "C:\esp\v6.1\esp-idf" }
$toolsPath = if ($env:IDF_TOOLS_PATH) { $env:IDF_TOOLS_PATH } else { "C:\Espressif\tools" }
$venvPath = if ($env:IDF_PYTHON_ENV_PATH) { $env:IDF_PYTHON_ENV_PATH } else { Join-Path $toolsPath "python\v6.1\venv" }
$venvPython = Join-Path $venvPath "Scripts\python.exe"
$idfPy = Join-Path $idfPath "tools\idf.py"

if (-not (Test-Path $venvPython)) {
    throw "ESP-IDF Python venv not found: $venvPython"
}
if (-not (Test-Path $idfPy)) {
    throw "idf.py not found: $idfPy"
}

# Same environment the official Microsoft.v6.1.PowerShell_profile.ps1 exports.
$env:IDF_PATH = $idfPath
$env:IDF_TOOLS_PATH = $toolsPath
$env:IDF_PYTHON_ENV_PATH = $venvPath
$env:ESP_IDF_VERSION = "6.1"
$env:IDF_VERSION = "6.1.0"
$env:ESP_ROM_ELF_DIR = Join-Path $toolsPath "esp-rom-elfs\20241011\"
$env:ESP_CLANG_LIBS_PATH = Join-Path $toolsPath "esp-clang-libs\esp-21.1.3_20260408\esp-clang\lib"
$env:IDF_COMPONENT_LOCAL_STORAGE_URL = "file://" + $toolsPath.Replace("\", "/")
$env:IDF_CCACHE_ENABLE = "1"

$toolPaths = @(
    (Join-Path $toolsPath "ccache\4.12.1\ccache-4.12.1-windows-x86_64"),
    (Join-Path $toolsPath "cmake\4.0.3\bin"),
    (Join-Path $toolsPath "dfu-util\0.11\dfu-util-0.11-win64"),
    (Join-Path $toolsPath "esp32ulp-elf\2.38_20240113\esp32ulp-elf\bin"),
    (Join-Path $toolsPath "idf-exe\1.0.3"),
    (Join-Path $toolsPath "ninja\1.12.1"),
    (Join-Path $toolsPath "riscv32-esp-elf\esp-15.2.0_20251204\riscv32-esp-elf\bin"),
    (Join-Path $toolsPath "xtensa-esp-elf\esp-15.2.0_20251204\xtensa-esp-elf\bin"),
    (Join-Path $venvPath "Scripts")
) -join ";"
$env:PATH = "$toolPaths;$env:PATH"

$modules = @(
    "task_1_to_4_auto_count_0_9",
    "task_5_to_8_switch_count_0_9",
    "task_9_to_12_press_counter_max_9",
    "task_13_dual_segment_88",
    "task_14_to_17_counter_00_99"
)

$summary = @()

foreach ($module in $modules) {
    $moduleDir = Join-Path $PSScriptRoot $module
    if (-not (Test-Path $moduleDir)) {
        throw "Module folder missing: $moduleDir"
    }

    Write-Host ""
    Write-Host "=== Building $module ===" -ForegroundColor Cyan
    Push-Location $moduleDir
    try {
        # set-target is needed on a fresh checkout where sdkconfig/CMakeCache
        # do not exist yet. Pre-built Wokwi binaries alone are not enough for
        # a rebuild.
        if (-not (Test-Path "sdkconfig") -or -not (Test-Path "build\CMakeCache.txt")) {
            Write-Host "--- idf.py set-target esp32" -ForegroundColor DarkGray
            & $venvPython $idfPy set-target esp32
            if ($LASTEXITCODE -ne 0) {
                throw "set-target failed for $module"
            }
        }

        Write-Host "--- idf.py build" -ForegroundColor DarkGray
        & $venvPython $idfPy build
        if ($LASTEXITCODE -ne 0) {
            throw "build failed for $module"
        }

        $required = @(
            "build\flasher_args.json",
            "build\bootloader\bootloader.bin",
            "build\partition_table\partition-table.bin",
            "build\$module.bin",
            "build\$module.elf"
        )
        foreach ($file in $required) {
            if (-not (Test-Path $file)) {
                throw "Missing Wokwi artifact: $module\$file"
            }
        }

        $appBin = Get-Item "build\$module.bin"
        $summary += [pscustomobject]@{
            Module = $module
            AppBin = $appBin.Name
            Bytes  = $appBin.Length
            Status = "OK"
        }
    }
    finally {
        Pop-Location
    }
}

Write-Host ""
Write-Host "Build summary" -ForegroundColor Green
$summary | Format-Table -AutoSize

Write-Host "Open ONE module folder as the Wokwi workspace and run the simulator." -ForegroundColor Green
