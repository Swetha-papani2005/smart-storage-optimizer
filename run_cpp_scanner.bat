@echo off
title Smart Storage Optimizer - C++ Core Engine Runner
cd /d "%~dp0"

echo ======================================================================
echo    SMART STORAGE OPTIMIZATION & DUPLICATE FILE DETECTION (C++17)
echo ======================================================================
echo.
echo Choose an option:
echo   [1] Scan Test Sandbox (Default Sample Files)
echo   [2] Scan Custom Directory
echo   [3] Rebuild C++ Executable
echo.
set /p choice="Enter choice (1-3, default is 1): "

if "%choice%"=="2" goto CUSTOM
if "%choice%"=="3" goto REBUILD
goto DEFAULT

:DEFAULT
echo.
echo [Running] Scanning test-sandbox...
if not exist "test-sandbox" (
    echo Generating sample files first...
    powershell -ExecutionPolicy Bypass -File scripts\generate_test_data.ps1
)
"cpp-engine\storage_optimizer.exe" -s "test-sandbox" -j "storage_audit_report.json" -g "cleanup_duplicates.ps1"
goto COPY_REPORT

:CUSTOM
echo.
set /p scanpath="Enter the directory path to scan (e.g., C:\Users\YourName\Downloads): "
if "%scanpath%"=="" goto DEFAULT
echo.
echo [Running] Scanning "%scanpath%"...
"cpp-engine\storage_optimizer.exe" -s "%scanpath%" -j "storage_audit_report.json" -g "cleanup_duplicates.ps1"
goto COPY_REPORT

:REBUILD
echo.
echo [Compiling] Building C++ engine with g++...
powershell -ExecutionPolicy Bypass -File scripts\build.ps1
pause
exit /b

:COPY_REPORT
echo.
if exist "storage_audit_report.json" (
    copy /y "storage_audit_report.json" "web-dashboard\public\storage_audit_report.json" >nul
    echo [+] Synced audit report to Web Dashboard (storage_audit_report.json)
)
echo.
echo ======================================================================
echo All modules finished. You can now view results in:
echo   1. Terminal report above
echo   2. Web Dashboard at http://localhost:3000
echo   3. PowerShell cleanup script: cleanup_duplicates.ps1
echo ======================================================================
echo.
pause
