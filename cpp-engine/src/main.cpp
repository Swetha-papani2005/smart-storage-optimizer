#include "metadata_collector.hpp"
#include "duplicate_detector.hpp"
#include "waste_analyzer.hpp"
#include "reporter.hpp"

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
    std::cout << "Options:\n";
    std::cout << "  -s, --scan <dir>           Directory path to scan (default: current directory)\n";
    std::cout << "  --strategy <strat>         Retention strategy: OLDEST (default), NEWEST, SHORTEST\n";
    std::cout << "  -j, --json <filepath>      Path to export JSON audit report (default: storage_audit_report.json)\n";
    std::cout << "  -g, --script <filepath>    Path to generate PowerShell cleanup script (default: cleanup_duplicates.ps1)\n";
    std::cout << "  --quarantine <dir>         Directory to quarantine duplicates (default: ./quarantine)\n";
    std::cout << "  --min-size <bytes>         Ignore files smaller than <bytes> (default: 1)\n";
    std::cout << "  --include-hidden           Include hidden files and folders\n";
    std::cout << "  -h, --help                 Display this help message\n\n";
    std::cout << "Modules:\n";
    std::cout << "  Module 1: File Scanning & Metadata Collection (std::filesystem)\n";
    std::cout << "  Module 2: Duplicate File Detection using SHA-256 (3-tier pipeline)\n";
    std::cout << "  Module 3: Storage Waste Analysis & Cleanup Recommendations\n";
    std::cout << "======================================================================\n";
}

int main(int argc, char* argv[]) {
    std::filesystem::path target_dir = ".";
    CleanupStrategy strategy = CleanupStrategy::KEEP_OLDEST;
    std::filesystem::path json_output = "storage_audit_report.json";
    std::filesystem::path script_output = "cleanup_duplicates.ps1";
    std::filesystem::path quarantine_dir = "quarantine";
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
        } else if (arg == "--quarantine" && i + 1 < argc) {
            quarantine_dir = argv[++i];
        } else if (arg == "--min-size" && i + 1 < argc) {
            scan_opts.min_size_bytes = std::stoull(argv[++i]);
        } else if (arg == "--include-hidden") {
            scan_opts.include_hidden = true;
        } else {
            // If argument is not an option flag, treat as scan path
            if (arg[0] != '-') {
                target_dir = arg;
            }
        }
    }

    std::cout << "======================================================================\n";
    std::cout << " SMART STORAGE OPTIMIZATION & DUPLICATE FILE DETECTION SYSTEM\n";
    std::cout << "======================================================================\n";
    std::cout << "Target Directory: " << std::filesystem::absolute(target_dir).string() << "\n";
    std::cout << "Retention Rule  : " << WasteAnalyzer::strategy_to_string(strategy) << "\n\n";

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
    if (Reporter::generate_cleanup_script_ps1(report, script_output, quarantine_dir)) {
        std::cout << " [+] Generated cleanup script    : " << std::filesystem::absolute(script_output).string() << "\n";
    }

    std::cout << "\n Processing completed in " << std::fixed << std::setprecision(2) << elapsed_ms << " ms.\n";
    std::cout << "======================================================================\n";

    return 0;
}
