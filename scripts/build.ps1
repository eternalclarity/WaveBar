param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$toolchain = Join-Path $root ".tools\llvm-mingw"
$compiler = Join-Path $toolchain "bin\clang++.exe"
$resourceCompiler = Join-Path $toolchain "bin\x86_64-w64-mingw32-windres.exe"
$outputDirectory = Join-Path $root ("build\" + $Configuration.ToLowerInvariant())
$output = Join-Path $outputDirectory "WaveBar.exe"

if (-not (Test-Path $compiler)) {
    throw "Portable LLVM-MinGW toolchain not found at $compiler"
}
if (-not (Test-Path $resourceCompiler)) {
    throw "Resource compiler not found at $resourceCompiler"
}

New-Item -ItemType Directory -Force $outputDirectory | Out-Null

$resourceObject = Join-Path $outputDirectory "WaveBar.res.o"
$resourceArguments = @(
    (Join-Path $root "src\WaveBar.rc"),
    "-I",
    (Join-Path $root "src"),
    "-O",
    "coff",
    "-o",
    $resourceObject
)

Push-Location $root
try {
    & $resourceCompiler @resourceArguments
}
finally {
    Pop-Location
}

if ($LASTEXITCODE -ne 0) {
    throw "WaveBar resource compilation failed with exit code $LASTEXITCODE"
}

$sources = @(
    (Join-Path $root "src\main.cpp"),
    (Join-Path $root "src\app\Application.cpp"),
    (Join-Path $root "src\audio\WasapiLoopback.cpp"),
    (Join-Path $root "src\dsp\SpectrumProcessor.cpp"),
    (Join-Path $root "src\render\LayeredRenderer.cpp")
)

$arguments = @(
    "-std=c++20",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-pthread",
    "-DUNICODE",
    "-D_UNICODE",
    "-DWIN32_LEAN_AND_MEAN",
    "-DNOMINMAX",
    "-D_WIN32_WINNT=0x0A00",
    "-municode",
    "-mwindows",
    "-I" + (Join-Path $root "src")
)

if ($Configuration -eq "Release") {
    $arguments += @("-O2", "-DNDEBUG", "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections", "-s")
} else {
    $arguments += @("-O0", "-g")
}

$arguments += $sources
$arguments += $resourceObject
$arguments += @(
    "-static",
    "-lole32",
    "-luuid",
    "-lavrt",
    "-lshell32",
    "-luser32",
    "-lgdi32",
    "-o",
    $output
)

Write-Host "Building WaveBar ($Configuration)..."
& $compiler @arguments

if ($LASTEXITCODE -ne 0) {
    throw "WaveBar build failed with exit code $LASTEXITCODE"
}

Write-Host "Built: $output"