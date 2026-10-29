const http = require('http');
const fs = require('fs');
const path = require('path');
const { execFile } = require('child_process');

const PORT = 3000;
const PUBLIC_DIR = path.join(__dirname, 'public');
const ROOT_DIR = path.join(__dirname, '..');
const TRASH_DIR = path.join(ROOT_DIR, 'smart_trash');
const TRASH_MANIFEST = path.join(TRASH_DIR, 'trash_manifest.json');
const CPP_ENGINE = path.join(ROOT_DIR, 'cpp-engine', 'storage_optimizer.exe');

// Ensure trash folder exists
if (!fs.existsSync(TRASH_DIR)) {
  fs.mkdirSync(TRASH_DIR, { recursive: true });
}

function getNowEpoch() {
  return Math.floor(Date.now() / 1000);
}

function calculateExpiry(fromEpoch, retention) {
  const norm = (retention || '').toLowerCase();
  if (norm.includes('day') || norm === '1d') return fromEpoch + (24 * 3600);
  if (norm.includes('week') || norm === '1w') return fromEpoch + (7 * 24 * 3600);
  return fromEpoch + (30 * 24 * 3600); // Default 1 month
}

function formatRemaining(seconds) {
  if (seconds <= 0) return 'Expired (Ready to purge)';
  const days = Math.floor(seconds / (24 * 3600));
  const hours = Math.floor((seconds % (24 * 3600)) / 3600);
  const mins = Math.floor((seconds % 3600) / 60);
  if (days > 0) return `${days}d ${hours}h remaining`;
  if (hours > 0) return `${hours}h ${mins}m remaining`;
  return `${mins}m remaining`;
}

function loadTrashManifest() {
  if (!fs.existsSync(TRASH_MANIFEST)) return { items: [] };
  try {
    const raw = fs.readFileSync(TRASH_MANIFEST, 'utf8');
    return JSON.parse(raw);
  } catch {
    return { items: [] };
  }
}

function saveTrashManifest(manifest) {
  fs.writeFileSync(TRASH_MANIFEST, JSON.stringify(manifest, null, 2), 'utf8');
}

function purgeExpiredTrash() {
  const manifest = loadTrashManifest();
  const now = getNowEpoch();
  const retained = [];
  let purgedCount = 0;

  for (const item of manifest.items) {
    if (item.expires_at > 0 && now >= item.expires_at) {
      const fpath = path.join(TRASH_DIR, item.trash_filename);
      if (fs.existsSync(fpath)) {
        try { fs.unlinkSync(fpath); } catch (e) { console.error('Purge error:', e); }
      }
      purgedCount++;
      console.log(`[AUTO-PURGE] Expired file deleted: ${item.display_name}`);
    } else {
      retained.push(item);
    }
  }

  if (purgedCount > 0) {
    manifest.items = retained;
    saveTrashManifest(manifest);
  }
  return purgedCount;
}

// Auto purge check on launch and every hour
purgeExpiredTrash();
setInterval(purgeExpiredTrash, 60 * 60 * 1000);

/**
 * Smart Path Resolver:
 * Maps browser-relative paths (e.g. "nami/nami 1.pptx") to their true absolute location on Windows.
 */
function resolveRealPath(rawPath) {
  if (!rawPath) return null;
  // If absolute and exists
  if (path.isAbsolute(rawPath) && fs.existsSync(rawPath)) {
    return rawPath;
  }

  const userProfile = process.env.USERPROFILE || 'C:\\Users\\DELL';
  const desktopDir = path.join(userProfile, 'Desktop');
  const downloadsDir = path.join(userProfile, 'Downloads');
  const documentsDir = path.join(userProfile, 'Documents');

  const candidates = [
    rawPath,
    path.join(desktopDir, rawPath),
    path.join(downloadsDir, rawPath),
    path.join(documentsDir, rawPath),
    path.join(userProfile, rawPath),
    path.join(ROOT_DIR, rawPath),
    path.join(process.cwd(), rawPath)
  ];

  for (const c of candidates) {
    if (fs.existsSync(c)) {
      return path.resolve(c);
    }
  }

  // Handle slash differences
  const normalized = rawPath.replace(/\\/g, '/');
  const parts = normalized.split('/');
  if (parts.length > 0) {
    const topFolder = parts[0];
    const subPath = parts.slice(1).join(path.sep);

    for (const base of [desktopDir, downloadsDir, documentsDir, userProfile]) {
      const folderCandidate = path.join(base, topFolder);
      if (fs.existsSync(folderCandidate)) {
        const fullCandidate = subPath ? path.join(folderCandidate, subPath) : folderCandidate;
        if (fs.existsSync(fullCandidate)) {
          return path.resolve(fullCandidate);
        }
      }
    }
  }

  return null;
}

const MIME_TYPES = {
  '.html': 'text/html; charset=UTF-8',
  '.css': 'text/css; charset=UTF-8',
  '.js': 'application/javascript; charset=UTF-8',
  '.json': 'application/json; charset=UTF-8',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.svg': 'image/svg+xml',
  '.ico': 'image/x-icon',
  '.txt': 'text/plain; charset=UTF-8'
};

function readBodyJson(req) {
  return new Promise((resolve, reject) => {
    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
      try {
        resolve(body ? JSON.parse(body) : {});
      } catch (e) {
        reject(e);
      }
    });
    req.on('error', reject);
  });
}

const server = http.createServer(async (req, res) => {
  const parsedUrl = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
  const pathname = parsedUrl.pathname;

  // CORS headers
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, DELETE, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

  if (req.method === 'OPTIONS') {
    res.writeHead(204);
    res.end();
    return;
  }

  // API Endpoints
  if (pathname.startsWith('/api/')) {
    res.setHeader('Content-Type', 'application/json');

    // 0. POST /api/scan-native (Trigger C++ Engine Scan from UI)
    if (req.method === 'POST' && pathname === '/api/scan-native') {
      try {
        const body = await readBodyJson(req);
        let folderPath = body.folderPath || '';
        const strategy = body.strategy || 'KEEP_OLDEST';

        if (!folderPath) {
          folderPath = path.join(process.env.USERPROFILE || 'C:\\Users\\DELL', 'Desktop');
        }

        // Check if path exists
        if (!fs.existsSync(folderPath)) {
          res.writeHead(400);
          res.end(JSON.stringify({ error: `Directory does not exist: ${folderPath}` }));
          return;
        }

        const reportPath = path.join(PUBLIC_DIR, 'storage_audit_report.json');
        const args = ['-s', folderPath, '--strategy', strategy, '-j', reportPath];

        execFile(CPP_ENGINE, args, (err, stdout, stderr) => {
          if (err && !fs.existsSync(reportPath)) {
            res.writeHead(500);
            res.end(JSON.stringify({ error: 'C++ Scan execution failed: ' + (stderr || err.message) }));
            return;
          }

          try {
            const reportData = JSON.parse(fs.readFileSync(reportPath, 'utf8'));
            res.writeHead(200);
            res.end(JSON.stringify({ success: true, report: reportData }));
          } catch (readErr) {
            res.writeHead(500);
            res.end(JSON.stringify({ error: 'Failed to read generated report: ' + readErr.message }));
          }
        });
      } catch (err) {
        res.writeHead(500);
        res.end(JSON.stringify({ error: err.message }));
      }
      return;
    }

    // 1. GET /api/trash
    if (req.method === 'GET' && pathname === '/api/trash') {
      const manifest = loadTrashManifest();
      const now = getNowEpoch();
      const items = manifest.items.map(it => ({
        ...it,
        seconds_remaining: it.expires_at > now ? (it.expires_at - now) : 0,
        status: formatRemaining(it.expires_at > now ? (it.expires_at - now) : 0)
      }));
      res.writeHead(200);
      res.end(JSON.stringify({ success: true, items }));
      return;
    }

    // 2. POST /api/trash (Move files to Smart Trash with Smart Path Resolution)
    if (req.method === 'POST' && pathname === '/api/trash') {
      try {
        const body = await readBodyJson(req);
        const { filePaths, retention } = body;
        if (!filePaths || !Array.isArray(filePaths) || filePaths.length === 0) {
          res.writeHead(400);
          res.end(JSON.stringify({ error: 'filePaths array is required' }));
          return;
        }

        const manifest = loadTrashManifest();
        const now = getNowEpoch();
        const expiry = calculateExpiry(now, retention);
        const retentionName = (retention && retention.toLowerCase().includes('day')) ? '1 Day'
          : (retention && retention.toLowerCase().includes('week')) ? '1 Week' : '1 Month';

        const movedItems = [];
        const failedPaths = [];

        for (const rawPath of filePaths) {
          const resolvedPath = resolveRealPath(rawPath);
          if (!resolvedPath || !fs.existsSync(resolvedPath)) {
            console.warn(`Could not resolve real path on disk for: ${rawPath}`);
            failedPaths.push(rawPath);
            continue;
          }

          const stats = fs.statSync(resolvedPath);
          const ext = path.extname(resolvedPath);
          const basename = path.basename(resolvedPath);
          const itemId = 'trash_' + now + '_' + Math.random().toString(36).substring(2, 8);
          const trashFilename = itemId + ext;
          const destPath = path.join(TRASH_DIR, trashFilename);

          try {
            fs.renameSync(resolvedPath, destPath);
          } catch (renameErr) {
            // Cross-device fallback
            fs.copyFileSync(resolvedPath, destPath);
            fs.unlinkSync(resolvedPath);
          }

          const item = {
            id: itemId,
            original_path: path.resolve(resolvedPath),
            trash_filename: trashFilename,
            display_name: basename,
            size_bytes: stats.size,
            trashed_at: now,
            expires_at: expiry,
            retention_name: retentionName
          };

          manifest.items.push(item);
          movedItems.push(item);
          console.log(`[TRASHED] Moved to Smart Trash: ${resolvedPath} -> ${trashFilename}`);
        }

        saveTrashManifest(manifest);

        if (movedItems.length === 0 && failedPaths.length > 0) {
          res.writeHead(404);
          res.end(JSON.stringify({
            error: `Could not locate file on disk: "${failedPaths[0]}". Please run scan using the C++ scan input bar or specify full path.`,
            failed_paths: failedPaths
          }));
          return;
        }

        res.writeHead(200);
        res.end(JSON.stringify({
          success: true,
          moved_count: movedItems.length,
          failed_count: failedPaths.length,
          retention: retentionName,
          items: movedItems
        }));
      } catch (err) {
        res.writeHead(500);
        res.end(JSON.stringify({ error: err.message }));
      }
      return;
    }

    // 3. POST /api/trash/restore (Recover a file back)
    if (req.method === 'POST' && pathname === '/api/trash/restore') {
      try {
        const body = await readBodyJson(req);
        const { id } = body;
        const manifest = loadTrashManifest();
        const idx = manifest.items.findIndex(x => x.id === id);

        if (idx === -1) {
          res.writeHead(404);
          res.end(JSON.stringify({ error: 'Trashed item not found' }));
          return;
        }

        const item = manifest.items[idx];
        const srcPath = path.join(TRASH_DIR, item.trash_filename);
        if (!fs.existsSync(srcPath)) {
          res.writeHead(404);
          res.end(JSON.stringify({ error: 'Physical file missing in trash folder' }));
          return;
        }

        const parentDir = path.dirname(item.original_path);
        if (!fs.existsSync(parentDir)) {
          fs.mkdirSync(parentDir, { recursive: true });
        }

        try {
          fs.renameSync(srcPath, item.original_path);
        } catch {
          fs.copyFileSync(srcPath, item.original_path);
          fs.unlinkSync(srcPath);
        }

        manifest.items.splice(idx, 1);
        saveTrashManifest(manifest);

        console.log(`[RESTORED] ${item.display_name} -> ${item.original_path}`);
        res.writeHead(200);
        res.end(JSON.stringify({ success: true, restored: item }));
      } catch (err) {
        res.writeHead(500);
        res.end(JSON.stringify({ error: err.message }));
      }
      return;
    }

    // 4. POST /api/trash/purge (Purge expired or empty all)
    if (req.method === 'POST' && pathname === '/api/trash/purge') {
      try {
        const body = await readBodyJson(req);
        const manifest = loadTrashManifest();

        if (body.all) {
          for (const item of manifest.items) {
            const fpath = path.join(TRASH_DIR, item.trash_filename);
            if (fs.existsSync(fpath)) {
              try { fs.unlinkSync(fpath); } catch (e) {}
            }
          }
          manifest.items = [];
          saveTrashManifest(manifest);
          res.writeHead(200);
          res.end(JSON.stringify({ success: true, message: 'All trash emptied permanently.' }));
          return;
        }

        const purged = purgeExpiredTrash();
        res.writeHead(200);
        res.end(JSON.stringify({ success: true, purged_count: purged }));
      } catch (err) {
        res.writeHead(500);
        res.end(JSON.stringify({ error: err.message }));
      }
      return;
    }

    // 5. POST /api/trash/delete (Delete single item permanently)
    if (req.method === 'POST' && pathname === '/api/trash/delete') {
      try {
        const body = await readBodyJson(req);
        const { id } = body;
        const manifest = loadTrashManifest();
        const idx = manifest.items.findIndex(x => x.id === id);

        if (idx !== -1) {
          const item = manifest.items[idx];
          const fpath = path.join(TRASH_DIR, item.trash_filename);
          if (fs.existsSync(fpath)) {
            try { fs.unlinkSync(fpath); } catch (e) {}
          }
          manifest.items.splice(idx, 1);
          saveTrashManifest(manifest);
        }

        res.writeHead(200);
        res.end(JSON.stringify({ success: true }));
      } catch (err) {
        res.writeHead(500);
        res.end(JSON.stringify({ error: err.message }));
      }
      return;
    }

    res.writeHead(404);
    res.end(JSON.stringify({ error: 'Endpoint not found' }));
    return;
  }

  // Static File Serving
  let safeUrl = pathname;
  if (safeUrl === '/') safeUrl = '/index.html';

  const filePath = path.join(PUBLIC_DIR, safeUrl);

  if (!filePath.startsWith(PUBLIC_DIR)) {
    res.writeHead(403, { 'Content-Type': 'text/plain' });
    res.end('403 Forbidden');
    return;
  }

  fs.stat(filePath, (err, stats) => {
    if (err || !stats.isFile()) {
      res.writeHead(404, { 'Content-Type': 'text/plain' });
      res.end('404 Not Found');
      return;
    }

    const ext = path.extname(filePath).toLowerCase();
    const contentType = MIME_TYPES[ext] || 'application/octet-stream';

    res.writeHead(200, { 'Content-Type': contentType });
    fs.createReadStream(filePath).pipe(res);
  });
});

server.listen(PORT, () => {
  console.log('======================================================================');
  console.log('  SMART STORAGE OPTIMIZER & DUPLICATE FILE DETECTION DASHBOARD');
  console.log('======================================================================');
  console.log(`  Localhost Server Running at: http://localhost:${PORT}`);
  console.log(`  Smart Trash Directory:       ${TRASH_DIR}`);
  console.log('======================================================================');
});
