// Browser-side SHA-256 Duplicate Detector Engine (Mirrors the C++ Multi-Stage Pipeline)

export class BrowserDuplicateScanner {
  constructor(onProgress) {
    this.onProgress = onProgress || (() => {});
  }

  formatBytes(bytes) {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
  }

  determineCategory(filename) {
    const ext = filename.slice(((filename.lastIndexOf(".") - 1) >>> 0) + 2).toLowerCase();
    if (!ext) return 'Other';

    if (['jpg', 'jpeg', 'png', 'gif', 'bmp', 'webp', 'svg', 'ico', 'tiff'].includes(ext)) return 'Images';
    if (['pdf', 'docx', 'doc', 'xlsx', 'pptx', 'txt', 'md', 'csv', 'rtf'].includes(ext)) return 'Documents';
    if (['mp4', 'mkv', 'avi', 'mov', 'wmv', 'flv', 'webm'].includes(ext)) return 'Videos';
    if (['mp3', 'wav', 'flac', 'aac', 'ogg', 'm4a'].includes(ext)) return 'Audio';
    if (['zip', 'tar', 'gz', '7z', 'rar', 'bz2'].includes(ext)) return 'Archives';
    if (['cpp', 'c', 'hpp', 'h', 'py', 'js', 'ts', 'html', 'css', 'json', 'java', 'go', 'rs', 'sh', 'bat', 'ps1'].includes(ext)) return 'Code';

    return 'Other';
  }

  async computeSHA256(file, maxBytes = 0) {
    let blob = file;
    if (maxBytes > 0 && file.size > maxBytes) {
      blob = file.slice(0, maxBytes);
    }
    const buffer = await blob.arrayBuffer();
    const digest = await crypto.subtle.digest('SHA-256', buffer);
    const hashArray = Array.from(new Uint8Array(digest));
    return hashArray.map(b => b.toString(16).padStart(2, '0')).join('');
  }

  async scanFiles(fileList, strategy = 'KEEP_OLDEST') {
    const rawFiles = Array.from(fileList).filter(f => f.size > 0);
    const totalFiles = rawFiles.length;

    this.onProgress('Module 1: Scanning file metadata...', 1, 3);

    const metadataList = rawFiles.map(f => {
      const path = f.webkitRelativePath || f.name;
      const lastMod = new Date(f.lastModified).toISOString().replace('T', ' ').substring(0, 19);
      return {
        fileObj: f,
        filename: f.name,
        full_path: path,
        size_bytes: f.size,
        category: this.determineCategory(f.name),
        extension: '.' + (f.name.split('.').pop() || ''),
        formatted_time: lastMod,
        timestamp_ms: f.lastModified
      };
    });

    // -------------------------------------------------------------------------
    // STAGE 1: Size Partitioning
    // -------------------------------------------------------------------------
    this.onProgress('Module 2: Stage 1/3 - Size partitioning...', 1, 3);
    const sizeMap = new Map();
    for (const item of metadataList) {
      if (!sizeMap.has(item.size_bytes)) sizeMap.set(item.size_bytes, []);
      sizeMap.get(item.size_bytes).push(item);
    }

    const sizeCandidates = [];
    for (const [size, list] of sizeMap.entries()) {
      if (list.length > 1) {
        sizeCandidates.push(...list);
      }
    }

    if (sizeCandidates.length === 0) {
      return this.buildReport([], metadataList, strategy);
    }

    // -------------------------------------------------------------------------
    // STAGE 2: 4KB Header Pre-Hashing
    // -------------------------------------------------------------------------
    this.onProgress(`Module 2: Stage 2/3 - 4KB Header hashing (${sizeCandidates.length} files)...`, 2, 3);
    const headerMap = new Map();
    for (let i = 0; i < sizeCandidates.length; i++) {
      const item = sizeCandidates[i];
      const partialHash = await this.computeSHA256(item.fileObj, 4096);
      item.partial_hash = partialHash;
      const key = `${item.size_bytes}_${partialHash}`;
      if (!headerMap.has(key)) headerMap.set(key, []);
      headerMap.get(key).push(item);
    }

    const fullCandidates = [];
    for (const [key, list] of headerMap.entries()) {
      if (list.length > 1) {
        fullCandidates.push(...list);
      }
    }

    // -------------------------------------------------------------------------
    // STAGE 3: Full SHA-256 Hashing
    // -------------------------------------------------------------------------
    this.onProgress(`Module 2: Stage 3/3 - Full SHA-256 hashing (${fullCandidates.length} candidates)...`, 3, 3);
    const fullHashMap = new Map();
    for (let i = 0; i < fullCandidates.length; i++) {
      const item = fullCandidates[i];
      if (item.size_bytes <= 4096) {
        item.full_hash = item.partial_hash;
      } else {
        item.full_hash = await this.computeSHA256(item.fileObj, 0);
      }
      if (!fullHashMap.has(item.full_hash)) fullHashMap.set(item.full_hash, []);
      fullHashMap.get(item.full_hash).push(item);
    }

    // Assemble Duplicate Groups
    const duplicateGroups = [];
    for (const [hash, list] of fullHashMap.entries()) {
      if (list.length > 1) {
        const singleSize = list[0].size_bytes;
        const wasted = singleSize * (list.length - 1);
        duplicateGroups.push({
          sha256_hash: hash,
          file_size: singleSize,
          file_size_formatted: this.formatBytes(singleSize),
          category: list[0].category,
          wasted_bytes: wasted,
          wasted_formatted: this.formatBytes(wasted),
          files: list.map(x => ({
            filename: x.filename,
            full_path: x.full_path,
            extension: x.extension,
            category: x.category,
            size_bytes: x.size_bytes,
            formatted_time: x.formatted_time,
            timestamp_ms: x.timestamp_ms
          }))
        });
      }
    }

    duplicateGroups.sort((a, b) => b.wasted_bytes - a.wasted_bytes);
    return this.buildReport(duplicateGroups, metadataList, strategy);
  }

  buildReport(duplicateGroups, allFiles, strategy) {
    const totalScannedBytes = allFiles.reduce((acc, f) => acc + f.size_bytes, 0);
    const totalWastedBytes = duplicateGroups.reduce((acc, g) => acc + g.wasted_bytes, 0);
    const duplicateFilesCount = duplicateGroups.reduce((acc, g) => acc + g.files.length, 0);

    const wasteByCategory = {};
    for (const group of duplicateGroups) {
      wasteByCategory[group.category] = (wasteByCategory[group.category] || 0) + group.wasted_bytes;
    }

    // Module 3: Recommendations based on Strategy
    const recommendations = [];
    for (const group of duplicateGroups) {
      let original = group.files[0];
      for (let i = 1; i < group.files.length; i++) {
        const candidate = group.files[i];
        if (strategy === 'KEEP_OLDEST' && candidate.timestamp_ms < original.timestamp_ms) {
          original = candidate;
        } else if (strategy === 'KEEP_NEWEST' && candidate.timestamp_ms > original.timestamp_ms) {
          original = candidate;
        } else if (strategy === 'KEEP_SHORTEST_PATH' && candidate.full_path.length < original.full_path.length) {
          original = candidate;
        }
      }

      const duplicatesToDelete = group.files.filter(f => f.full_path !== original.full_path);
      recommendations.push({
        sha256_hash: group.sha256_hash,
        original_to_keep: original,
        duplicates_to_delete: duplicatesToDelete,
        recoverable_bytes: group.wasted_bytes,
        reason: `Preserved ${original.filename} (${strategy}); safely frees ${duplicatesToDelete.length} duplicates.`
      });
    }

    return {
      system: "Smart Storage Optimizer and Duplicate File Detection",
      version: "1.0.0",
      scan_root: "Browser File Selection",
      timestamp: new Date().toISOString().replace('T', ' ').substring(0, 19),
      strategy_used: strategy,
      total_files_scanned: allFiles.length,
      total_scanned_bytes: totalScannedBytes,
      total_scanned_formatted: this.formatBytes(totalScannedBytes),
      duplicate_groups_count: duplicateGroups.length,
      duplicate_files_count: duplicateFilesCount,
      total_wasted_bytes: totalWastedBytes,
      total_wasted_formatted: this.formatBytes(totalWastedBytes),
      waste_percentage: totalScannedBytes > 0 ? ((totalWastedBytes / totalScannedBytes) * 100).toFixed(2) : 0,
      waste_by_category: wasteByCategory,
      duplicate_groups: duplicateGroups,
      recommendations: recommendations
    };
  }
}
