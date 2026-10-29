// Firebase Backend Integration (Firestore & Storage / Sync)

// Config placeholder - users can fill their own Firebase configuration or connect via CLI
export const firebaseConfig = {
  apiKey: "AIzaSyDummyKeyForStorageOptimizerProject",
  authDomain: "smart-storage-optimizer.firebaseapp.com",
  projectId: "smart-storage-optimizer",
  storageBucket: "smart-storage-optimizer.appspot.com",
  messagingSenderId: "123456789012",
  appId: "1:123456789012:web:abcdef1234567890"
};

export class FirebaseBackendService {
  constructor() {
    this.isOnline = false;
    this.localStorageKey = 'smart_storage_scan_history';
  }

  async init() {
    // If Firebase SDK is loaded on window
    if (window.firebase && window.firebase.apps && !window.firebase.apps.length) {
      try {
        window.firebase.initializeApp(firebaseConfig);
        this.db = window.firebase.firestore ? window.firebase.firestore() : null;
        this.isOnline = true;
        console.log("Firebase backend initialized successfully.");
      } catch (err) {
        console.warn("Firebase initialized with local fallback:", err.message);
      }
    }
  }

  async saveScanReport(report) {
    const record = {
      id: 'scan_' + Date.now(),
      savedAt: new Date().toISOString(),
      report: report
    };

    // Save to LocalStorage for offline persistence
    const history = this.getScanHistory();
    history.unshift(record);
    if (history.length > 20) history.pop();
    localStorage.setItem(this.localStorageKey, JSON.stringify(history));

    // If online Firebase DB is configured, sync to Firestore
    if (this.isOnline && this.db) {
      try {
        await this.db.collection('scan_reports').add(record);
        console.log("Report synced to Firebase Firestore!");
      } catch (e) {
        console.warn("Could not sync to remote Firestore, stored locally:", e);
      }
    }
    return record;
  }

  getScanHistory() {
    try {
      const data = localStorage.getItem(this.localStorageKey);
      return data ? JSON.parse(data) : [];
    } catch {
      return [];
    }
  }

  clearScanHistory() {
    localStorage.removeItem(this.localStorageKey);
  }
}
