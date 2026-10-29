# ⚡ Smart Storage Optimization and Duplicate File Detection System

[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/17)
[![SHA-256](https://img.shields.io/badge/Security-SHA--256-blue.svg)](https://csrc.nist.gov/publications/detail/fips/180-2/final)
[![Firebase](https://img.shields.io/badge/Backend-Firebase_Firestore_%26_Hosting-FFCA28?logo=firebase&logoColor=black)](https://firebase.google.com/)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![CI/CD](https://github.com/Swetha-papani2005/smart-storage-optimizer/actions/workflows/ci.yml/badge.svg)](https://github.com/Swetha-papani2005/smart-storage-optimizer/actions)

An enterprise-grade, high-throughput storage optimization system designed in **C++17** and integrated with **Firebase** and an **interactive Web Dashboard**. The system rapidly traverses file systems, detects bit-identical files using a collision-free **three-tier SHA-256 hashing pipeline**, calculates wasted disk capacity, and provides safe, rollback-ready cleanup strategies.

---

## 📌 Project Architecture

```
                       ┌──────────────────────────────────────────────┐
                       │   SMART STORAGE OPTIMIZER (C++17 ENGINE)     │
                       └──────────────────────┬───────────────────────┘
                                              │
         ┌────────────────────────────────────┼────────────────────────────────────┐
         ▼                                    ▼                                    ▼
   [ MODULE 1 ]                         [ MODULE 2 ]                         [ MODULE 3 ]
 FILE SCANNING &                     DUPLICATE DETECTION                 STORAGE WASTE ANALYSIS
METADATA COLLECTION                     USING SHA-256                  & CLEANUP RECOMMENDATIONS
 • Recursive filesystem traversal     • 3-Tier Hashing Pipeline          • Wasted bytes computation
 • Canonical path resolution          • Tier 1: Exact size filter        • Strategy: Keep Oldest/Newest
 • Size, timestamp & type filters     • Tier 2: 4KB Header pre-hash      • Safe rollback / quarantine
 • Non-blocking permission handling   • Tier 3: Full SHA-256 streaming   • JSON report & script generator
         │                                    │                                    │
         └────────────────────────────────────┼────────────────────────────────────┘
                                              │
                                     [ OUTPUT REPORTS ]
                                 • storage_audit_report.json
                                 • cleanup_duplicates.ps1
                                              │
                     ┌────────────────────────┴────────────────────────┐
                     ▼                                                 ▼
        [ FIREBASE WEB DASHBOARD ]                          [ GITHUB REPOSITORY & CI ]
 • In-Browser SHA-256 Web Crypto Scanner            • Multi-platform GitHub Actions CI
 • Storage Waste Distribution Charts                • Automated C++ build & test verification
 • Interactive Duplicate Cluster Resolver           • Automatic GitHub Pages deployment
 • Real-time Firestore Cloud Sync                   • Complete version-controlled source
```

---

## 🛠️ The Three Core Modules

### 🔹 Module 1: File Scanning and Metadata Collection
* **Filesystem Traversal**: Leverages modern `std::filesystem::recursive_directory_iterator` with `std::filesystem::directory_options::skip_permission_denied` to navigate nested directory trees safely without crashing on protected system files.
* **Metadata Extraction**: Gathers canonical full path, filename, extension, exact byte size (`uintmax_t`), last modified time (`file_time_type`), and categorizes files into *Images, Documents, Videos, Audio, Code, Archives, or Other*.
* **Intelligent Filtering**: Filters out zero-byte files, symlinks (preventing infinite recursion loops), and excluded system directories (`.git`, `node_modules`, `System Volume Information`).

### 🔹 Module 2: Duplicate File Detection using SHA-256
Computing cryptographic hashes on every file on a large hard drive creates an immense I/O bottleneck. To guarantee high performance, this module uses a **3-Tier Optimization Pipeline**:
1. **Tier 1 (Size Partitioning)**: Files are grouped into a hash table by exact byte size. Files with unique sizes are eliminated immediately ($O(N)$ with zero hashing I/O). This eliminates **85–95%** of files instantly.
2. **Tier 2 (4KB Chunk Pre-Hashing)**: Files with matching sizes have only their first 4 KB read and hashed. Files with differing headers are discarded without reading the entire file.
3. **Tier 3 (Full SHA-256 Cryptographic Streaming)**: Remaining candidate files are streamed in 64 KB memory chunks through a self-contained, FIPS 180-2 compliant SHA-256 engine. This ensures collision-free duplicate identification ($p_{\text{collision}} \approx 2^{-256}$) without external dependencies like OpenSSL.

### 🔹 Module 3: Storage Waste Analysis & Cleanup Recommendations
* **Storage Waste Calculation**: For each duplicate group $G$ containing $N$ identical files of size $S$:
  $$\text{Wasted Storage} = (N - 1) \times S$$
* **Intelligent Retention Strategies**:
  * `KEEP_OLDEST`: Preserves the original file created earliest; targets newer redundant copies for removal.
  * `KEEP_NEWEST`: Preserves the latest revised copy; targets older snapshots.
  * `KEEP_SHORTEST_PATH`: Preserves files residing closest to root or cleanest folder hierarchy.
* **Safe Cleanup Output**:
  * Emits `storage_audit_report.json` containing complete metrics and duplicate clusters.
  * Generates an automated, rollback-safe PowerShell script (`cleanup_duplicates.ps1`) and batch file (`cleanup_duplicates.bat`) equipped with **Dry-Run** and **Quarantine** modes (moving files to a quarantine folder instead of permanent deletion).

---

## 🚀 Getting Started

### Prerequisites
* **C++ Compiler**: MinGW-w64 `g++` (GCC 8.0+) or MSVC supporting C++17.
* **CMake**: Version 3.15+ (optional).
* **Node.js**: v18+ (for Web Dashboard & Firebase deployment).

---

### Compiling and Running the C++ Engine

#### Option A: 1-Click PowerShell Build
```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1
```

#### Option B: Direct g++ Compilation (Static & Portable)
```bash
cd cpp-engine
g++ -std=c++17 -O2 -static -static-libgcc -static-libstdc++ -Iinclude src/sha256.cpp src/metadata_collector.cpp src/duplicate_detector.cpp src/waste_analyzer.cpp src/reporter.cpp src/main.cpp -o storage_optimizer.exe
```

#### Option C: CMake Build
```bash
cd cpp-engine
cmake -B build
cmake --build build --config Release
```

---

### CLI Usage & Examples

```bash
# Scan a specific directory
.\storage_optimizer.exe -s "C:\MyFiles"

# Scan with KEEP_NEWEST strategy and export custom JSON report
.\storage_optimizer.exe -s "D:\Projects" --strategy NEWEST -j audit.json -g clean.ps1

# Display help and all available options
.\storage_optimizer.exe --help
```

#### Sample Terminal Output:
```
======================================================================
 SMART STORAGE OPTIMIZATION & DUPLICATE FILE DETECTION SYSTEM
======================================================================
Target Directory: C:\Users\DELL\Desktop\smart-storage-optimizer\test-sandbox
Retention Rule  : KEEP_OLDEST

>> [MODULE 1] Scanning directory & collecting file metadata...
   Total regular files discovered: 11

>> [MODULE 2] Detecting duplicates via 3-tier SHA-256 optimization pipeline...
   • Unique file sizes observed     : 6
   • Candidates matching same size  : 9
   • Candidates matching 4KB header : 7
   • Full SHA-256 stream hashes run : 5
   • Verified duplicate groups      : 3
   • Redundant duplicate files      : 7

>> [MODULE 3] Analyzing storage waste & computing cleanup recommendations...

======================================================================
    SMART STORAGE OPTIMIZATION & DUPLICATE FILE DETECTION REPORT      
======================================================================
 Target Directory   : test-sandbox
 Scan Timestamp     : 2026-10-29 11:52:45
 Retention Strategy : KEEP_OLDEST
 Total Files Scanned: 11
 Total Scanned Space: 46.33 KB
----------------------------------------------------------------------
 Duplicate Groups   : 3
 Redundant Files    : 7
 Recoverable Space  : 23.39 KB
 Waste Ratio        : 50.49 %
======================================================================
```

---

## 🌐 Web Dashboard & Firebase Deployment

The web dashboard provides an interactive UI for storage visualization and allows users to:
1. **Drag-and-Drop folders/files directly into the browser** to run the 3-stage SHA-256 algorithm via the native browser Web Crypto API without installing software.
2. **Import C++ JSON audit reports** to visualize metrics, charts, and duplicate groups.
3. **Sync scan records to Firebase Firestore** for audit tracking across devices.
4. **Download safe cleanup scripts** customized to the chosen retention strategy.

### Local Testing of Web Dashboard
```bash
cd web-dashboard
npx.cmd serve public -p 5000
```
Open [http://localhost:5000](http://localhost:5000) in your browser.

### Deploying to Firebase Hosting
```bash
cd web-dashboard

# 1. Login to Firebase
cmd.exe /c "npx.cmd --yes firebase-tools login"

# 2. Deploy to Firebase Hosting
cmd.exe /c "npx.cmd --yes firebase-tools deploy --only hosting"
```
Once deployed, Firebase outputs your live URL:
`https://smart-storage-optimizer.web.app`

---

## 📦 Deploying to GitHub Repository

To create and deploy this project to your GitHub account:

```bash
# 1. Initialize Git and stage all files
git init
git add .
git commit -m "feat: initial commit of Smart Storage Optimization & Duplicate Detection System"

# 2. Rename branch to main
git branch -M main

# 3. Add your remote GitHub repository URL
git remote add origin https://github.com/Swetha-papani2005/smart-storage-optimizer.git

# 4. Push to GitHub
git push -u origin main
```

*(If the remote repository is not yet created on GitHub, create a new empty repository named `smart-storage-optimizer` at https://github.com/new and then run step 4).*

---

## 📂 Project Directory Structure

```
smart-storage-optimizer/
├── .github/
│   └── workflows/
│       └── ci.yml                   # Automated GitHub Actions CI/CD pipeline
├── cpp-engine/
│   ├── CMakeLists.txt               # CMake build configuration
│   ├── include/
│   │   ├── duplicate_detector.hpp   # Module 2: 3-tier SHA-256 duplicate detection
│   │   ├── metadata_collector.hpp   # Module 1: Recursive directory scanner & metadata
│   │   ├── reporter.hpp             # CLI, JSON, and PowerShell script generator
│   │   ├── sha256.hpp               # Cryptographic SHA-256 hashing engine
│   │   └── waste_analyzer.hpp       # Module 3: Storage waste & cleanup recommendations
│   └── src/
│       ├── duplicate_detector.cpp
│       ├── main.cpp                 # CLI entry point
│       ├── metadata_collector.cpp
│       ├── reporter.cpp
│       ├── sha256.cpp
│       └── waste_analyzer.cpp
├── web-dashboard/
│   ├── .firebaserc                  # Firebase project identifier
│   ├── firebase.json                # Firebase Hosting and Firestore configuration
│   ├── firestore.rules              # Firestore security rules
│   ├── firestore.indexes.json
│   ├── package.json
│   └── public/
│       ├── index.html               # Modern responsive dashboard
│       ├── style.css                # Sleek dark-mode developer UI
│       └── src/
│           ├── app.js               # UI controller and report visualizer
│           ├── firebase-config.js   # Firebase SDK & Firestore persistence
│           └── scanner.js           # Browser-side Web Crypto SHA-256 engine
├── scripts/
│   ├── build.ps1                    # 1-click C++ build script
│   ├── deploy_firebase.ps1          # Firebase deployment script
│   └── generate_test_data.ps1       # Generates sample duplicate test files
├── .gitignore
├── LICENSE                          # MIT License
└── README.md                        # Documentation
```

---

## 🛡️ License

This project is licensed under the [MIT License](LICENSE).
