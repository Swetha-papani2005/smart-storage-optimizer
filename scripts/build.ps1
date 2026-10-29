# PowerShell C++ Build Script
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Building Smart Storage Optimizer (C++17 Standalone Binary)" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

$engineDir = "$PSScriptRoot\..\cpp-engine"
Set-Location $engineDir

$cmd = "g++ -std=c++17 -O2 -static -static-libgcc -static-libstdc++ -Iinclude src/sha256.cpp src/metadata_collector.cpp src/duplicate_detector.cpp src/waste_analyzer.cpp src/reporter.cpp src/trash_manager.cpp src/main.cpp -o storage_optimizer.exe"

Write-Host "Executing: $cmd" -ForegroundColor Gray
Invoke-Expression $cmd

if ($LASTEXITCODE -eq 0 -and (Test-Path "storage_optimizer.exe")) {
    Write-Host "`n[SUCCESS] Compiled storage_optimizer.exe successfully!" -ForegroundColor Green
    $size = (Get-Item "storage_optimizer.exe").Length / 1MB
    Write-Host "Binary Location: $engineDir\storage_optimizer.exe"
    Write-Host ("Binary Size    : {0:N2} MB" -f $size)
} else {
    Write-Host "`n[ERROR] Compilation failed. Ensure g++ (MinGW-w64) is installed in your PATH." -ForegroundColor Red
}
