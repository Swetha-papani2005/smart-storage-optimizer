#include "metadata_collector.hpp"
#include "duplicate_detector.hpp"
#include "waste_analyzer.hpp"
#include "reporter.hpp"
#include "trash_manager.hpp"

#include <iostream>
#include <string>
#include <chrono>
#include <filesystem>

using namespace storage_optimizer;

void print_help(const char* prog_name) {
    std::cout << "======================================================================\n";
    std::cout << " SMART STORAGE OPTIMIZATION & DUPLICATE FILE DETECTION SYSTEM (C++17) \n";
    std::cout << "======================================================================\n";
    std::cout << "Usage: " << prog_name << " [options]\n\n";
    std::cout << "Scanning & Detection Options:\n";
    std::cout << "  -s, --scan <dir>           Directory path to scan (default: current directory)\n";
    std::cout << "  --strategy <strat>         Retention strategy: OLDEST (default), NEWEST, SHORTEST\n";
    std::cout << "  -j, --json <filepath>      Path to export JSON audit report (default: storage_audit_report.json)\n";
    std::cout << "  -g, --script <filepath>    Path to generate PowerShell cleanup script (default: cleanup_duplicates.ps1)\n";
    std::cout << "  --min-size <bytes>         Ignore files smaller than <bytes> (default: 1)\n";
    std::cout << "  --include-hidden           Include hidden files and folders\n\n";
    std::cout << "Smart Trash & Recovery Options:\n";
    std::cout << "  --trash-duplicates         Move detected duplicates directly to Smart Trash\n";
    std::cout << "  --retention <policy>       Trash retention timer: 1day, 1week, 1month (default: 1month)\n";
    std::cout << "  --trash-dir <path>         Path to smart trash directory (default: smart_trash)\n";
    std::cout << "  --list-trash               Display all items currently in Smart Trash and time remaining\n";
    std::cout << "  --restore <id>             Restore a trashed item back to its original location\n";
    std::cout << "  --purge-expired            Permanently delete expired files from Smart Trash\n";
    std::cout << "  --empty-trash              Permanently delete all items from Smart Trash\n";
    std::cout << "  -h, --help                 Display this help message\n";
    std::cout << "======================================================================\n";
}

int main(int argc, char* argv[]) {
    std::filesystem::path target_dir = "";
    CleanupStrategy strategy = CleanupStrategy::KEEP_OLDEST;
    std::filesystem::path json_output = "storage_audit_report.json";
    std::filesystem::path script_output = "cleanup_duplicates.ps1";
    std::filesystem::path trash_dir = "smart_trash";
    TrashRetention retention = TrashRetention::ONE_MONTH;
    bool auto_trash_duplicates = false;
    bool list_trash_only = false;
    bool purge_expired_only = false;
    bool empty_trash_only = false;
    std::string restore_item_id = "";
    ScanFilterOptions scan_opts;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else if ((arg == "-s" || arg == "--scan") && i + 1 < argc) {
            target_dir = argv[++i];
        } else if (arg == "--strategy" && i + 1 < argc) {
            strategy = WasteAnalyzer::parse_strategy(argv[++i]);
        } else if ((arg == "-j" || arg == "--json") && i + 1 < argc) {
            json_output = argv[++i];
        } else if ((arg == "-g" || arg == "--script") && i + 1 < argc) {
            script_output = argv[++i];
        } else if (arg == "--trash-duplicates") {
            auto_trash_duplicates = true;
        } else if (arg == "--retention" && i + 1 < argc) {
            retention = TrashManager::parse_retention(argv[++i]);
        } else if (arg == "--trash-dir" && i + 1 < argc) {
            trash_dir = argv[++i];
        } else if (arg == "--list-trash") {
            list_trash_only = true;
        } else if (arg == "--restore" && i + 1 < argc) {
            restore_item_id = argv[++i];
        } else if (arg == "--purge-expired") {
            purge_expired_only = true;
        } else if (arg == "--empty-trash") {
            empty_trash_only = true;
        } else if (arg == "--min-size" && i + 1 < argc) {
            scan_opts.min_size_bytes = std::stoull(argv[++i]);
        } else if (arg == "--include-hidden") {
            scan_opts.include_hidden = true;
        } else {
            if (arg[0] != '-') {
                target_dir = arg;
            }
        }
    }

    TrashManager trash_mgr(trash_dir);

    // Handle dedicated trash commands
    if (list_trash_only) {
        std::cout << "\n======================================================================\n";
        std::cout << "                     SMART TRASH & RECOVERY VAULT                     \n";
        std::cout << "======================================================================\n";
        auto items = trash_mgr.get_items();
        if (items.empty()) {
            std::cout << " Smart Trash is empty. No files awaiting recovery.\n";
        } else {
            std::cout << " Total Items in Trash: " << items.size() << "\n\n";
            for (size_t idx = 0; idx < items.size(); ++idx) {
                const auto& item = items[idx];
                std::cout << " [" << (idx + 1) << "] ID: " << item.id << "\n";
                std::cout << "     File     : " << item.display_name << " (" << MetadataCollector::format_bytes(item.size_bytes) << ")\n";
                std::cout << "     Original : " << item.original_path << "\n";
                std::cout << "     Retention: " << item.retention_name << " | Status: " << item.status() << "\n";
                std::cout << "----------------------------------------------------------------------\n";
            }
            std::cout << " To recover a file, run: " << argv[0] << " --restore <item_id>\n";
        }
        std::cout << "======================================================================\n";
        return 0;
    }

    if (!restore_item_id.empty()) {
        std::cout << "Attempting to restore item ID: " << restore_item_id << "...\n";
        if (trash_mgr.restore_item(restore_item_id)) {
            std::cout << "File successfully recovered to original directory!\n";
        } else {
            std::cout << "Restore failed. Verify item ID with --list-trash.\n";
        }
        return 0;
    }

    if (purge_expired_only) {
        size_t purged = trash_mgr.purge_expired();
        std::cout << "Purged " << purged << " expired files from Smart Trash.\n";
        return 0;
    }

    if (empty_trash_only) {
        trash_mgr.empty_trash();
        return 0;
    }

    if (target_dir.empty()) {
        target_dir = ".";
    }

    std::cout << "======================================================================\n";
    std::cout << " SMART STORAGE OPTIMIZATION & DUPLICATE FILE DETECTION SYSTEM\n";
    std::cout << "======================================================================\n";
    std::cout << "Target Directory : " << std::filesystem::absolute(target_dir).string() << "\n";
    std::cout << "Retention Rule   : " << WasteAnalyzer::strategy_to_string(strategy) << "\n";
    std::cout << "Trash Retention  : " << TrashManager::retention_to_string(retention) << "\n\n";

    auto start_time = std::chrono::high_resolution_clock::now();

    // ------------------------------------------------------------------------
    // MODULE 1: File Scanning & Metadata Collection
    // ------------------------------------------------------------------------
    std::cout << ">> [MODULE 1] Scanning directory & collecting file metadata...\n";
    MetadataCollector collector(scan_opts);
    
    auto files = collector.scan_directory(target_dir, [](size_t count, const std::string& current) {
        std::cout << "\r   Scanned " << count << " files... (Current: " << current.substr(0, 30) << ")   " << std::flush;
    });
    std::cout << "\n   Total regular files discovered: " << files.size() << "\n\n";

    if (files.empty()) {
        std::cout << "No matching files found in target directory.\n";
        return 0;
    }

    // ------------------------------------------------------------------------
    // MODULE 2: Duplicate File Detection using SHA-256
    // ------------------------------------------------------------------------
    std::cout << ">> [MODULE 2] Detecting duplicates via 3-tier SHA-256 optimization pipeline...\n";
    DuplicateDetector detector;
    auto duplicates = detector.detect_duplicates(files, [](const std::string& phase, size_t cur, size_t total) {
        std::cout << "\r   [" << phase << "] " << cur << " / " << total << "   " << std::flush;
    });
    std::cout << "\n";

    const auto& stats = detector.stats();
    std::cout << "   • Unique file sizes observed     : " << stats.unique_sizes_count << "\n";
    std::cout << "   • Candidates matching same size  : " << stats.candidate_files_after_size_filter << "\n";
    std::cout << "   • Candidates matching 4KB header : " << stats.candidate_files_after_partial_hash << "\n";
    std::cout << "   • Full SHA-256 stream hashes run : " << stats.full_hashes_computed << "\n";
    std::cout << "   • Verified duplicate groups      : " << stats.duplicate_groups_found << "\n";
    std::cout << "   • Redundant duplicate files      : " << stats.duplicate_files_found << "\n\n";

    // ------------------------------------------------------------------------
    // MODULE 3: Storage Waste Analysis & Cleanup Recommendations
    // ------------------------------------------------------------------------
    std::cout << ">> [MODULE 3] Analyzing storage waste & computing cleanup recommendations...\n";
    WasteAnalyzer analyzer(strategy);
    auto report = analyzer.analyze(target_dir, files, duplicates);

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    // CLI Summary
    Reporter::print_cli_summary(report);

    // Export JSON & Script
    if (Reporter::export_json(report, json_output)) {
        std::cout << " [+] Exported JSON audit report  : " << std::filesystem::absolute(json_output).string() << "\n";
    }
    if (Reporter::generate_cleanup_script_ps1(report, script_output, trash_dir)) {
        std::cout << " [+] Generated cleanup script    : " << std::filesystem::absolute(script_output).string() << "\n";
    }

    // If --trash-duplicates was specified, automatically move duplicates to Smart Trash
    if (auto_trash_duplicates && !report.recommendations.empty()) {
        std::cout << "\n>> Moving redundant duplicates to Smart Trash (Retention: " 
                  << TrashManager::retention_to_string(retention) << ")...\n";
        size_t moved_count = 0;
        for (const auto& rec : report.recommendations) {
            for (const auto& dup : rec.duplicates_to_delete) {
                if (trash_mgr.move_to_trash(dup.full_path, retention, rec.sha256_hash)) {
                    moved_count++;
                }
            }
        }
        std::cout << " [+] Safely moved " << moved_count << " duplicates to " 
                  << std::filesystem::absolute(trash_dir).string() << "\n";
        std::cout << " [+] Files can be restored within " << TrashManager::retention_to_string(retention) 
                  << " before automatic permanent deletion.\n";
    }

    std::cout << "\n Processing completed in " << std::fixed << std::setprecision(2) << elapsed_ms << " ms.\n";
    std::cout << "======================================================================\n";

    return 0;
}
