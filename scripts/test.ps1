$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$compiler = Join-Path $root ".tools\llvm-mingw\bin\clang++.exe"
$outputDirectory = Join-Path $root "build\tests"
New-Item -ItemType Directory -Force $outputDirectory | Out-Null
$output = Join-Path $outputDirectory "AudioRecoveryTests.exe"
& $compiler -std=c++20 -Wall -Wextra -Wpedantic -pthread -DUNICODE -D_UNICODE `
    -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_WIN32_WINNT=0x0A00 `
    ("-I" + (Join-Path $root "src")) `
    (Join-Path $root "tests\AudioRecoveryTests.cpp") `
    (Join-Path $root "src\dsp\SpectrumProcessor.cpp") `
    (Join-Path $root "src\render\LayeredRenderer.cpp") `
    -static -lole32 -luuid -lavrt -lshell32 -luser32 -lgdi32 -ladvapi32 -o $output
if ($LASTEXITCODE -ne 0) { throw "Audio recovery test build failed: $LASTEXITCODE" }
& $output
if ($LASTEXITCODE -ne 0) { throw "Audio recovery tests failed: $LASTEXITCODE" }
