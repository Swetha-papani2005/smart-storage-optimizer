#include "reporter.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace storage_optimizer {

std::string Reporter::escape_json_string(const std::string& input) {
    std::ostringstream ss;
    for (char c : input) {
        switch (c) {
            case '"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    ss << c;
                }
        }
    }
    return ss.str();
}

void Reporter::print_cli_summary(const WasteAnalysisReport& report) {
    std::cout << "\n";
    std::cout << "======================================================================\n";
    std::cout << "    SMART STORAGE OPTIMIZATION & DUPLICATE FILE DETECTION REPORT      \n";
    std::cout << "======================================================================\n";
    std::cout << " Target Directory   : " << report.scan_root << "\n";
    std::cout << " Scan Timestamp     : " << report.timestamp << "\n";
    std::cout << " Retention Strategy : " << report.strategy_name << "\n";
    std::cout << " Total Files Scanned: " << report.total_files_scanned << "\n";
    std::cout << " Total Scanned Space: " << MetadataCollector::format_bytes(report.total_scanned_bytes) << "\n";
    std::cout << "----------------------------------------------------------------------\n";
    std::cout << " Duplicate Groups   : " << report.duplicate_groups_count << "\n";
    std::cout << " Redundant Files    : " << report.duplicate_files_count << "\n";
    std::cout << " Recoverable Space  : " << MetadataCollector::format_bytes(report.total_wasted_bytes) << "\n";
    std::cout << " Waste Ratio        : " << std::fixed << std::setprecision(2) << report.waste_percentage << " %\n";
    std::cout << "======================================================================\n";

    if (!report.waste_by_category.empty()) {
        std::cout << "\n[ STORAGE WASTE BY FILE CATEGORY ]\n";
        for (const auto& [category, wasted_bytes] : report.waste_by_category) {
            std::cout << "  • " << std::left << std::setw(15) << category 
                      << ": " << std::right << std::setw(12) << MetadataCollector::format_bytes(wasted_bytes) << "\n";
        }
    }

    if (!report.duplicate_groups.empty()) {
        std::cout << "\n[ TOP DUPLICATE FILE CLUSTERS ]\n";
        size_t show_count = std::min(report.duplicate_groups.size(), static_cast<size_t>(5));
        for (size_t i = 0; i < show_count; ++i) {
            const auto& group = report.duplicate_groups[i];
            std::cout << "\nCluster #" << (i + 1) << " | SHA-256: " << group.sha256_hash.substr(0, 16) << "..."
                      << " | Single Size: " << MetadataCollector::format_bytes(group.file_size)
                      << " | Wasted: " << MetadataCollector::format_bytes(group.wasted_bytes()) << "\n";
            for (size_t j = 0; j < group.files.size(); ++j) {
                const auto& f = group.files[j];
                std::cout << "   [" << (j + 1) << "] " << f.full_path.string() 
                          << " (Modified: " << f.formatted_time << ")\n";
            }
        }
        if (report.duplicate_groups.size() > show_count) {
            std::cout << "\n  ... and " << (report.duplicate_groups.size() - show_count) << " more duplicate clusters.\n";
        }
    } else {
        std::cout << "\nNo duplicate files found! Storage is clean and optimized.\n";
    }

    std::cout << "\n======================================================================\n";
}

bool Reporter::export_json(const WasteAnalysisReport& report, const std::filesystem::path& output_path) {
    std::ofstream out(output_path);
    if (!out.is_open()) {
        std::cerr << "Failed to open output JSON file: " << output_path << "\n";
        return false;
    }

    out << "{\n";
    out << "  \"system\": \"Smart Storage Optimizer and Duplicate File Detection\",\n";
    out << "  \"version\": \"1.0.0\",\n";
    out << "  \"scan_root\": \"" << escape_json_string(report.scan_root) << "\",\n";
    out << "  \"timestamp\": \"" << escape_json_string(report.timestamp) << "\",\n";
    out << "  \"strategy_used\": \"" << report.strategy_name << "\",\n";
    out << "  \"total_files_scanned\": " << report.total_files_scanned << ",\n";
    out << "  \"total_scanned_bytes\": " << report.total_scanned_bytes << ",\n";
    out << "  \"total_scanned_formatted\": \"" << MetadataCollector::format_bytes(report.total_scanned_bytes) << "\",\n";
    out << "  \"duplicate_groups_count\": " << report.duplicate_groups_count << ",\n";
    out << "  \"duplicate_files_count\": " << report.duplicate_files_count << ",\n";
    out << "  \"total_wasted_bytes\": " << report.total_wasted_bytes << ",\n";
    out << "  \"total_wasted_formatted\": \"" << MetadataCollector::format_bytes(report.total_wasted_bytes) << "\",\n";
    out << "  \"waste_percentage\": " << std::fixed << std::setprecision(2) << report.waste_percentage << ",\n";

    // Waste by category
    out << "  \"waste_by_category\": {\n";
    size_t cat_idx = 0;
    for (auto it = report.waste_by_category.begin(); it != report.waste_by_category.end(); ++it, ++cat_idx) {
        out << "    \"" << escape_json_string(it->first) << "\": " << it->second;
        if (cat_idx + 1 < report.waste_by_category.size()) out << ",";
        out << "\n";
    }
    out << "  },\n";

    // Duplicate groups
    out << "  \"duplicate_groups\": [\n";
    for (size_t i = 0; i < report.duplicate_groups.size(); ++i) {
        const auto& group = report.duplicate_groups[i];
        out << "    {\n";
        out << "      \"sha256_hash\": \"" << group.sha256_hash << "\",\n";
        out << "      \"file_size\": " << group.file_size << ",\n";
        out << "      \"file_size_formatted\": \"" << MetadataCollector::format_bytes(group.file_size) << "\",\n";
        out << "      \"category\": \"" << escape_json_string(group.category) << "\",\n";
        out << "      \"wasted_bytes\": " << group.wasted_bytes() << ",\n";
        out << "      \"wasted_formatted\": \"" << MetadataCollector::format_bytes(group.wasted_bytes()) << "\",\n";
        out << "      \"files\": [\n";
        for (size_t j = 0; j < group.files.size(); ++j) {
            const auto& f = group.files[j];
            out << "        {\n";
            out << "          \"filename\": \"" << escape_json_string(f.filename) << "\",\n";
            out << "          \"full_path\": \"" << escape_json_string(f.full_path.string()) << "\",\n";
            out << "          \"extension\": \"" << escape_json_string(f.extension) << "\",\n";
            out << "          \"category\": \"" << escape_json_string(f.category) << "\",\n";
            out << "          \"size_bytes\": " << f.size_bytes << ",\n";
            out << "          \"formatted_time\": \"" << escape_json_string(f.formatted_time) << "\"\n";
            out << "        }" << (j + 1 < group.files.size() ? "," : "") << "\n";
        }
        out << "      ]\n";
        out << "    }" << (i + 1 < report.duplicate_groups.size() ? "," : "") << "\n";
    }
    out << "  ],\n";

    // Recommendations
    out << "  \"recommendations\": [\n";
    for (size_t i = 0; i < report.recommendations.size(); ++i) {
        const auto& rec = report.recommendations[i];
        out << "    {\n";
        out << "      \"sha256_hash\": \"" << rec.sha256_hash << "\",\n";
        out << "      \"reason\": \"" << escape_json_string(rec.reason) << "\",\n";
        out << "      \"recoverable_bytes\": " << rec.recoverable_bytes << ",\n";
        out << "      \"original_to_keep\": {\n";
        out << "        \"filename\": \"" << escape_json_string(rec.original_to_keep.filename) << "\",\n";
        out << "        \"full_path\": \"" << escape_json_string(rec.original_to_keep.full_path.string()) << "\",\n";
        out << "        \"formatted_time\": \"" << escape_json_string(rec.original_to_keep.formatted_time) << "\"\n";
        out << "      },\n";
        out << "      \"duplicates_to_delete\": [\n";
        for (size_t j = 0; j < rec.duplicates_to_delete.size(); ++j) {
            const auto& d = rec.duplicates_to_delete[j];
            out << "        {\n";
            out << "          \"filename\": \"" << escape_json_string(d.filename) << "\",\n";
            out << "          \"full_path\": \"" << escape_json_string(d.full_path.string()) << "\",\n";
            out << "          \"formatted_time\": \"" << escape_json_string(d.formatted_time) << "\"\n";
            out << "        }" << (j + 1 < rec.duplicates_to_delete.size() ? "," : "") << "\n";
        }
        out << "      ]\n";
        out << "    }" << (i + 1 < report.recommendations.size() ? "," : "") << "\n";
    }
    out << "  ]\n";
    out << "}\n";

    return true;
}

bool Reporter::generate_cleanup_script_ps1(
    const WasteAnalysisReport& report,
    const std::filesystem::path& script_path,
    const std::filesystem::path& quarantine_dir
) {
    std::ofstream out(script_path);
    if (!out.is_open()) return false;

    out << "# =====================================================================\n";
    out << "# Smart Storage Optimizer: Safe Cleanup Script (PowerShell)\n";
    out << "# Generated: " << report.timestamp << "\n";
    out << "# Strategy Used: " << report.strategy_name << "\n";
    out << "# Total Recoverable Space: " << MetadataCollector::format_bytes(report.total_wasted_bytes) << "\n";
    out << "# =====================================================================\n\n";

    out << "param(\n";
    out << "    [switch]$Execute = $false,\n";
    out << "    [switch]$Quarantine = $true,\n";
    out << "    [string]$QuarantinePath = \"" << (quarantine_dir.empty() ? ".\\quarantine" : quarantine_dir.string()) << "\"\n";
    out << ")\n\n";

    out << "Write-Host \"=== SMART STORAGE CLEANUP RUNNER ===\" -ForegroundColor Cyan\n";
    out << "Write-Host \"Mode: $(if ($Execute) { 'LIVE EXECUTION' } else { 'DRY RUN (Simulated)' })\" -ForegroundColor Yellow\n";
    out << "Write-Host \"Action: $(if ($Quarantine) { 'Move to Quarantine folder' } else { 'Permanent Delete' })\"\n\n";

    out << "if ($Execute -and $Quarantine) {\n";
    out << "    if (-not (Test-Path $QuarantinePath)) {\n";
    out << "        New-Item -ItemType Directory -Path $QuarantinePath -Force | Out-Null\n";
    out << "    }\n";
    out << "}\n\n";

    size_t file_index = 0;
    for (const auto& rec : report.recommendations) {
        out << "# Cluster SHA: " << rec.sha256_hash.substr(0, 16) << "...\n";
        out << "# Keeping: " << rec.original_to_keep.full_path.string() << "\n";

        for (const auto& dup : rec.duplicates_to_delete) {
            file_index++;
            out << "$file" << file_index << " = \"" << dup.full_path.string() << "\"\n";
            out << "if (Test-Path $file" << file_index << ") {\n";
            out << "    if ($Execute) {\n";
            out << "        if ($Quarantine) {\n";
            out << "            Move-Item -LiteralPath $file" << file_index << " -Destination $QuarantinePath -Force\n";
            out << "            Write-Host \"[QUARANTINED] $file" << file_index << "\" -ForegroundColor Green\n";
            out << "        } else {\n";
            out << "            Remove-Item -LiteralPath $file" << file_index << " -Force\n";
            out << "            Write-Host \"[DELETED] $file" << file_index << "\" -ForegroundColor Red\n";
            out << "        }\n";
            out << "    } else {\n";
            out << "        Write-Host \"[DRY-RUN] Would remove: $file" << file_index << "\" -ForegroundColor Gray\n";
            out << "    }\n";
            out << "}\n";
        }
        out << "\n";
    }

    out << "Write-Host \"\\nExecution finished. Run with -Execute to apply changes.\" -ForegroundColor Green\n";
    return true;
}

bool Reporter::generate_cleanup_script_bat(
    const WasteAnalysisReport& report,
    const std::filesystem::path& script_path,
    const std::filesystem::path& quarantine_dir
) {
    std::ofstream out(script_path);
    if (!out.is_open()) return false;

    out << "@echo off\n";
    out << "REM =====================================================================\n";
    out << "REM Smart Storage Optimizer: Safe Cleanup Batch Script\n";
    out << "REM Generated: " << report.timestamp << "\n";
    out << "REM Total Recoverable: " << MetadataCollector::format_bytes(report.total_wasted_bytes) << "\n";
    out << "REM =====================================================================\n\n";

    out << "echo ====================================================\n";
    out << "echo       SMART STORAGE CLEANUP - BATCH RUNNER\n";
    out << "echo ====================================================\n";
    out << "echo Dry run simulation mode.\n";
    out << "echo Pass 'EXECUTE' parameter to perform actual deletion.\n\n";

    out << "if \"%1\"==\"EXECUTE\" goto LIVE\n\n";

    out << ":SIMULATION\n";
    out << "echo Running in DRY-RUN mode...\n";
    for (const auto& rec : report.recommendations) {
        for (const auto& dup : rec.duplicates_to_delete) {
            out << "echo [DRY-RUN] Would delete: \"" << dup.full_path.string() << "\"\n";
        }
    }
    out << "echo.\n";
    out << "echo Simulation complete. To delete files, run: %0 EXECUTE\n";
    out << "goto END\n\n";

    out << ":LIVE\n";
    out << "echo Deleting redundant duplicates...\n";
    for (const auto& rec : report.recommendations) {
        for (const auto& dup : rec.duplicates_to_delete) {
            out << "if exist \"" << dup.full_path.string() << "\" del /f /q \"" << dup.full_path.string() << "\"\n";
        }
    }
    out << "echo Cleanup complete.\n\n";

    out << ":END\n";
    return true;
}

} // namespace storage_optimizer
