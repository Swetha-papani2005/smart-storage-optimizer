# PowerShell Script to Generate Synthetic Test Data for Duplicate Detection

$targetDir = "$PSScriptRoot\..\test-sandbox"
if (Test-Path $targetDir) {
    Remove-Item -Recurse -Force $targetDir
}

New-Item -ItemType Directory -Path "$targetDir\docs" -Force | Out-Null
New-Item -ItemType Directory -Path "$targetDir\backup\docs" -Force | Out-Null
New-Item -ItemType Directory -Path "$targetDir\images" -Force | Out-Null
New-Item -ItemType Directory -Path "$targetDir\downloads\images" -Force | Out-Null
New-Item -ItemType Directory -Path "$targetDir\code\v1" -Force | Out-Null
New-Item -ItemType Directory -Path "$targetDir\code\v2" -Force | Out-Null
New-Item -ItemType Directory -Path "$targetDir\collision_test" -Force | Out-Null

Write-Host "Creating sample duplicate files..." -ForegroundColor Cyan

# 1. Duplicate Group A: 3 Identical Documents (each ~5 KB)
$docContent = "CONFIDENTIAL ANNUAL REPORT 2026`n" + ("Lorem ipsum dolor sit amet, consectetur adipiscing elit. " * 80)
$docContent | Out-File -FilePath "$targetDir\docs\annual_report.txt" -Encoding utf8 -NoNewline
$docContent | Out-File -FilePath "$targetDir\backup\docs\annual_report_backup.txt" -Encoding utf8 -NoNewline
$docContent | Out-File -FilePath "$targetDir\docs\annual_report_copy.txt" -Encoding utf8 -NoNewline

# Set different timestamps to test KEEP_OLDEST vs KEEP_NEWEST
(Get-Item "$targetDir\docs\annual_report.txt").LastWriteTime = (Get-Date).AddDays(-30)
(Get-Item "$targetDir\backup\docs\annual_report_backup.txt").LastWriteTime = (Get-Date).AddDays(-5)
(Get-Item "$targetDir\docs\annual_report_copy.txt").LastWriteTime = (Get-Date).AddDays(-1)

# 2. Duplicate Group B: 2 Identical PNG binary files (~12 KB)
$imageBytes = New-Object byte[] 12288
for ($i = 0; $i -lt $imageBytes.Length; $i++) { $imageBytes[$i] = ($i * 7) % 256 }
[System.IO.File]::WriteAllBytes("$targetDir\images\avatar.png", $imageBytes)
[System.IO.File]::WriteAllBytes("$targetDir\downloads\images\avatar (1).png", $imageBytes)

# 3. Duplicate Group C: 2 Identical Source Code files (~3 KB)
$codeContent = "// Optimized C++ Algorithm`n#include <iostream>`nint main() { return 0; }`n" + ("// padding comment line`n" * 100)
$codeContent | Out-File -FilePath "$targetDir\code\v1\engine.cpp" -Encoding utf8 -NoNewline
$codeContent | Out-File -FilePath "$targetDir\code\v2\engine.cpp" -Encoding utf8 -NoNewline

# 4. Critical Edge Case: 2 Files with IDENTICAL Byte Size (e.g. exactly 2048 bytes), but DIFFERENT data!
$collisionA = New-Object byte[] 2048
$collisionB = New-Object byte[] 2048
for ($i = 0; $i -lt 2048; $i++) {
    $collisionA[$i] = 65 # 'A'
    $collisionB[$i] = 66 # 'B'
}
[System.IO.File]::WriteAllBytes("$targetDir\collision_test\fileA_same_size.bin", $collisionA)
[System.IO.File]::WriteAllBytes("$targetDir\collision_test\fileB_same_size.bin", $collisionB)

# 5. Unique Files (never duplicates)
"Unique note A" | Out-File -FilePath "$targetDir\docs\unique_note.txt" -Encoding utf8
"Unique script B" | Out-File -FilePath "$targetDir\code\script.py" -Encoding utf8

Write-Host "Test sandbox successfully created at: $targetDir" -ForegroundColor Green
