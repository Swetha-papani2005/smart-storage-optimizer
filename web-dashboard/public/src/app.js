import { BrowserDuplicateScanner } from './scanner.js';
import { FirebaseBackendService } from './firebase-config.js';

class App {
  constructor() {
    this.firebase = new FirebaseBackendService();
    this.currentReport = null;
    this.currentStrategy = 'KEEP_OLDEST';
    this.init();
  }

  async init() {
    await this.firebase.init();
    this.bindEvents();
    this.renderHistory();

    // Load sample report initially if available
    this.loadSampleData();
  }

  bindEvents() {
    const dropZone = document.getElementById('dropZone');
    const folderInput = document.getElementById('folderInput');
    const filesInput = document.getElementById('filesInput');
    const jsonInput = document.getElementById('jsonInput');
    const strategySelect = document.getElementById('strategySelect');
    const exportScriptBtn = document.getElementById('exportScriptBtn');
    const exportJsonBtn = document.getElementById('exportJsonBtn');
    const syncFirebaseBtn = document.getElementById('syncFirebaseBtn');

    // Drag and drop
    dropZone.addEventListener('dragover', (e) => {
      e.preventDefault();
      dropZone.classList.add('dragover');
    });

    dropZone.addEventListener('dragleave', () => {
      dropZone.classList.remove('dragover');
    });

    dropZone.addEventListener('drop', async (e) => {
      e.preventDefault();
      dropZone.classList.remove('dragover');
      if (e.dataTransfer.files && e.dataTransfer.files.length > 0) {
        await this.processFiles(e.dataTransfer.files);
      }
    });

    folderInput.addEventListener('change', async (e) => {
      if (e.target.files && e.target.files.length > 0) {
        await this.processFiles(e.target.files);
      }
    });

    filesInput.addEventListener('change', async (e) => {
      if (e.target.files && e.target.files.length > 0) {
        await this.processFiles(e.target.files);
      }
    });

    jsonInput.addEventListener('change', (e) => {
      const file = e.target.files[0];
      if (file) {
        const reader = new FileReader();
        reader.onload = (evt) => {
          try {
            const parsed = JSON.parse(evt.target.result);
            this.setReport(parsed);
          } catch (err) {
            alert("Invalid JSON audit file: " + err.message);
          }
        };
        reader.readAsText(file);
      }
    });

    strategySelect.addEventListener('change', (e) => {
      this.currentStrategy = e.target.value;
      this.recalculateStrategy();
    });

    exportScriptBtn.addEventListener('click', () => this.downloadCleanupScript());
    exportJsonBtn.addEventListener('click', () => this.downloadJsonReport());
    syncFirebaseBtn.addEventListener('click', () => this.saveToFirebase());
  }

  async processFiles(fileList) {
    const progressBox = document.getElementById('progressBox');
    const progressText = document.getElementById('progressText');
    progressBox.style.display = 'block';

    const scanner = new BrowserDuplicateScanner((msg) => {
      progressText.textContent = msg;
    });

    try {
      const report = await scanner.scanFiles(fileList, this.currentStrategy);
      progressBox.style.display = 'none';
      this.setReport(report);
    } catch (err) {
      progressBox.style.display = 'none';
      alert("Error scanning files: " + err.message);
    }
  }

  setReport(report) {
    this.currentReport = report;
    this.renderStats();
    this.renderCategoryBars();
    this.renderClusters();
    document.getElementById('reportSection').classList.remove('hidden');
  }

  recalculateStrategy() {
    if (!this.currentReport || !this.currentReport.duplicate_groups) return;

    const strat = this.currentStrategy;
    const recommendations = [];

    for (const group of this.currentReport.duplicate_groups) {
      let original = group.files[0];
      for (let i = 1; i < group.files.length; i++) {
        const candidate = group.files[i];
        if (strat === 'KEEP_OLDEST') {
          if (candidate.formatted_time < original.formatted_time) original = candidate;
        } else if (strat === 'KEEP_NEWEST') {
          if (candidate.formatted_time > original.formatted_time) original = candidate;
        } else if (strat === 'KEEP_SHORTEST_PATH') {
          if (candidate.full_path.length < original.full_path.length) original = candidate;
        }
      }

      const toDelete = group.files.filter(f => f.full_path !== original.full_path);
      recommendations.push({
        sha256_hash: group.sha256_hash,
        original_to_keep: original,
        duplicates_to_delete: toDelete,
        recoverable_bytes: group.wasted_bytes,
        reason: `Preserved ${original.filename} (${strat}); frees ${toDelete.length} copies.`
      });
    }

    this.currentReport.strategy_used = strat;
    this.currentReport.recommendations = recommendations;
    this.renderClusters();
  }

  renderStats() {
    const r = this.currentReport;
    document.getElementById('statTotalFiles').textContent = r.total_files_scanned;
    document.getElementById('statTotalScanned').textContent = r.total_scanned_formatted;
    document.getElementById('statDuplicateFiles').textContent = r.duplicate_files_count;
    document.getElementById('statWastedSpace').textContent = r.total_wasted_formatted;
    document.getElementById('statWastePercent').textContent = `${r.waste_percentage}%`;
    document.getElementById('statClusters').textContent = r.duplicate_groups_count;
  }

  renderCategoryBars() {
    const container = document.getElementById('categoryBars');
    container.innerHTML = '';

    const cats = this.currentReport.waste_by_category || {};
    const totalWasted = this.currentReport.total_wasted_bytes || 1;

    const entries = Object.entries(cats).sort((a, b) => b[1] - a[1]);
    if (entries.length === 0) {
      container.innerHTML = '<p class="drop-subtext">No wasted storage across categories.</p>';
      return;
    }

    for (const [cat, bytes] of entries) {
      const pct = Math.min(100, Math.round((bytes / totalWasted) * 100));
      const formatted = this.formatBytes(bytes);

      const div = document.createElement('div');
      div.className = 'cat-item';
      div.innerHTML = `
        <div class="cat-header">
          <span><strong>${cat}</strong> (${formatted})</span>
          <span>${pct}% of waste</span>
        </div>
        <div class="cat-bar-bg">
          <div class="cat-bar-fill" style="width: ${pct}%"></div>
        </div>
      `;
      container.appendChild(div);
    }
  }

  renderClusters() {
    const container = document.getElementById('clustersList');
    container.innerHTML = '';

    const recs = this.currentReport.recommendations || [];
    if (recs.length === 0) {
      container.innerHTML = '<p class="drop-subtext">No duplicate file clusters discovered.</p>';
      return;
    }

    recs.forEach((rec, idx) => {
      const box = document.createElement('div');
      box.className = 'cluster-box';

      const orig = rec.original_to_keep;
      let filesHtml = `
        <div class="file-row keep">
          <div style="display:flex; align-items:center;">
            <span class="tag-keep">KEEP (ORIGINAL)</span>
            <span class="file-path">${orig.full_path}</span>
          </div>
          <span class="file-time">${orig.formatted_time || ''}</span>
        </div>
      `;

      rec.duplicates_to_delete.forEach(dup => {
        filesHtml += `
          <div class="file-row">
            <div style="display:flex; align-items:center;">
              <span class="tag-remove">REMOVE COPY</span>
              <span class="file-path">${dup.full_path}</span>
            </div>
            <span class="file-time">${dup.formatted_time || ''}</span>
          </div>
        `;
      });

      box.innerHTML = `
        <div class="cluster-header">
          <div>
            <strong>Cluster #${idx + 1}</strong>
            <span style="margin-left:0.5rem; color:var(--text-muted); font-size:0.85rem;">
              Recoverable: <span style="color:var(--accent-rose); font-weight:700;">${this.formatBytes(rec.recoverable_bytes)}</span>
            </span>
          </div>
          <span class="hash-badge">SHA-256: ${rec.sha256_hash.substring(0, 16)}...</span>
        </div>
        <div class="file-list">${filesHtml}</div>
      `;
      container.appendChild(box);
    });
  }

  async saveToFirebase() {
    if (!this.currentReport) return;
    const res = await this.firebase.saveScanReport(this.currentReport);
    alert("Scan report successfully saved and synced to Firebase history!");
    this.renderHistory();
  }

  renderHistory() {
    const container = document.getElementById('historyList');
    if (!container) return;
    const history = this.firebase.getScanHistory();
    container.innerHTML = '';

    if (history.length === 0) {
      container.innerHTML = '<p class="drop-subtext">No previous scans found.</p>';
      return;
    }

    history.forEach(item => {
      const div = document.createElement('div');
      div.className = 'file-row';
      div.style.cursor = 'pointer';
      div.innerHTML = `
        <div>
          <strong>${item.report.scan_root}</strong> - 
          <span style="color:var(--accent-rose);">${item.report.total_wasted_formatted} recoverable</span>
          <span style="color:var(--text-muted); font-size:0.75rem; margin-left:0.5rem;">(${item.savedAt.substring(0, 19)})</span>
        </div>
        <button class="btn btn-secondary" style="padding:0.2rem 0.6rem; font-size:0.75rem;">Load Report</button>
      `;
      div.addEventListener('click', () => {
        this.setReport(item.report);
      });
      container.appendChild(div);
    });
  }

  downloadCleanupScript() {
    if (!this.currentReport || !this.currentReport.recommendations) return;
    let ps1 = `# Smart Storage Optimizer - Safe Cleanup Script (PowerShell)\n`;
    ps1 += `# Generated by Live Web Dashboard\n# Total Recoverable Space: ${this.currentReport.total_wasted_formatted}\n\n`;
    ps1 += `param([switch]$Execute = $false, [string]$Quarantine = ".\\\\quarantine")\n\n`;
    ps1 += `if ($Execute -and (-not (Test-Path $Quarantine))) { New-Item -ItemType Directory -Path $Quarantine -Force | Out-Null }\n\n`;

    this.currentReport.recommendations.forEach((rec, idx) => {
      ps1 += `# Cluster ${idx + 1} - Preserving: ${rec.original_to_keep.full_path}\n`;
      rec.duplicates_to_delete.forEach(dup => {
        ps1 += `if (Test-Path "${dup.full_path}") {\n`;
        ps1 += `    if ($Execute) { Move-Item -LiteralPath "${dup.full_path}" -Destination $Quarantine -Force; Write-Host "[QUARANTINED] ${dup.filename}" -ForegroundColor Green }\n`;
        ps1 += `    else { Write-Host "[DRY-RUN] Would remove: ${dup.full_path}" -ForegroundColor Gray }\n`;
        ps1 += `}\n`;
      });
      ps1 += `\n`;
    });

    const blob = new Blob([ps1], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'cleanup_duplicates.ps1';
    a.click();
  }

  downloadJsonReport() {
    if (!this.currentReport) return;
    const jsonStr = JSON.stringify(this.currentReport, null, 2);
    const blob = new Blob([jsonStr], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'storage_audit_report.json';
    a.click();
  }

  loadSampleData() {
    // Populate with a default demonstration snapshot
    const sample = {
      system: "Smart Storage Optimizer and Duplicate File Detection",
      version: "1.0.0",
      scan_root: "C:\\Users\\DELL\\Desktop\\smart-storage-optimizer\\test-sandbox",
      timestamp: "2026-10-29 11:52:45",
      strategy_used: "KEEP_OLDEST",
      total_files_scanned: 11,
      total_scanned_bytes: 47445,
      total_scanned_formatted: "46.33 KB",
      duplicate_groups_count: 3,
      duplicate_files_count: 7,
      total_wasted_bytes: 23953,
      total_wasted_formatted: "23.39 KB",
      waste_percentage: 50.49,
      waste_by_category: {
        "Images": 12288,
        "Documents": 9190,
        "Code": 2475
      },
      duplicate_groups: [
        {
          sha256_hash: "b9fb93208c04f9c22a6dac3b1416b5c0d5dcd292b8e99ae0146a40b63f96f610",
          file_size: 12288,
          category: "Images",
          wasted_bytes: 12288,
          files: [
            { filename: "avatar (1).png", full_path: "downloads/images/avatar (1).png", formatted_time: "2026-10-29 11:52:34" },
            { filename: "avatar.png", full_path: "images/avatar.png", formatted_time: "2026-10-29 11:52:34" }
          ]
        },
        {
          sha256_hash: "ece8b79e1835acb6c5792bc984fc2d3112c00195debb11be88263ea94851a219",
          file_size: 4595,
          category: "Documents",
          wasted_bytes: 9190,
          files: [
            { filename: "annual_report.txt", full_path: "docs/annual_report.txt", formatted_time: "2026-09-29 11:52:33" },
            { filename: "annual_report_backup.txt", full_path: "backup/docs/annual_report_backup.txt", formatted_time: "2026-10-24 11:52:33" },
            { filename: "annual_report_copy.txt", full_path: "docs/annual_report_copy.txt", formatted_time: "2026-10-28 11:52:33" }
          ]
        },
        {
          sha256_hash: "b00c6b03dc533c03aaeb02379d20c5d7945d81b490dbff45ea5533fb7c815254",
          file_size: 2475,
          category: "Code",
          wasted_bytes: 2475,
          files: [
            { filename: "engine.cpp", full_path: "code/v1/engine.cpp", formatted_time: "2026-10-29 11:52:34" },
            { filename: "engine.cpp", full_path: "code/v2/engine.cpp", formatted_time: "2026-10-29 11:52:34" }
          ]
        }
      ],
      recommendations: [
        {
          sha256_hash: "b9fb93208c04f9c22a6dac3b1416b5c0d5dcd292b8e99ae0146a40b63f96f610",
          original_to_keep: { filename: "avatar.png", full_path: "images/avatar.png", formatted_time: "2026-10-29 11:52:34" },
          duplicates_to_delete: [{ filename: "avatar (1).png", full_path: "downloads/images/avatar (1).png", formatted_time: "2026-10-29 11:52:34" }],
          recoverable_bytes: 12288
        },
        {
          sha256_hash: "ece8b79e1835acb6c5792bc984fc2d3112c00195debb11be88263ea94851a219",
          original_to_keep: { filename: "annual_report.txt", full_path: "docs/annual_report.txt", formatted_time: "2026-09-29 11:52:33" },
          duplicates_to_delete: [
            { filename: "annual_report_backup.txt", full_path: "backup/docs/annual_report_backup.txt", formatted_time: "2026-10-24 11:52:33" },
            { filename: "annual_report_copy.txt", full_path: "docs/annual_report_copy.txt", formatted_time: "2026-10-28 11:52:33" }
          ],
          recoverable_bytes: 9190
        },
        {
          sha256_hash: "b00c6b03dc533c03aaeb02379d20c5d7945d81b490dbff45ea5533fb7c815254",
          original_to_keep: { filename: "engine.cpp", full_path: "code/v1/engine.cpp", formatted_time: "2026-10-29 11:52:34" },
          duplicates_to_delete: [{ filename: "engine.cpp", full_path: "code/v2/engine.cpp", formatted_time: "2026-10-29 11:52:34" }],
          recoverable_bytes: 2475
        }
      ]
    };

    this.setReport(sample);
  }

  formatBytes(bytes) {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
  }
}

document.addEventListener('DOMContentLoaded', () => {
  new App();
});
