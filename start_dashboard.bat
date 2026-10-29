@echo off
title Smart Storage Optimizer - Web Dashboard (Localhost:3000)
cd /d "%~dp0\web-dashboard"

echo ======================================================================
echo    SMART STORAGE OPTIMIZER & DUPLICATE FILE DETECTION DASHBOARD
echo ======================================================================
echo Starting local web server on port 3000...
echo Opening http://localhost:3000 in your browser...
echo.

start http://localhost:3000
node server.js
pause
