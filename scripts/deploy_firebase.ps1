# Firebase Hosting & Backend Deployment Helper Script

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Deploying Web Dashboard to Firebase Hosting" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

$webDir = "$PSScriptRoot\..\web-dashboard"
Set-Location $webDir

Write-Host "Step 1: Checking Firebase CLI..." -ForegroundColor Yellow
$firebaseCmd = Get-Command firebase -ErrorAction SilentlyContinue

if (-not $firebaseCmd) {
    Write-Host "Firebase CLI not found globally. Attempting via npx.cmd..." -ForegroundColor Gray
    cmd.exe /c "npx.cmd --yes firebase-tools login"
    cmd.exe /c "npx.cmd --yes firebase-tools deploy --only hosting"
} else {
    Write-Host "Firebase CLI found. Running deploy..." -ForegroundColor Green
    cmd.exe /c "firebase deploy --only hosting"
}

Write-Host "`nDeployment process completed!" -ForegroundColor Green
