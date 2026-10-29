#include "waste_analyzer.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace storage_optimizer {

namespace {

std::string get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t time_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm_buf, &time_now);
#else
    localtime_r(&time_now, &tm_buf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace

WasteAnalyzer::WasteAnalyzer(CleanupStrategy strategy)
    : strategy_(strategy) {}

std::string WasteAnalyzer::strategy_to_string(CleanupStrategy strategy) {
    switch (strategy) {
        case CleanupStrategy::KEEP_OLDEST: return "KEEP_OLDEST";
        case CleanupStrategy::KEEP_NEWEST: return "KEEP_NEWEST";
        case CleanupStrategy::KEEP_SHORTEST_PATH: return "KEEP_SHORTEST_PATH";
        case CleanupStrategy::KEEP_LONGEST_PATH: return "KEEP_LONGEST_PATH";
    }
    return "KEEP_OLDEST";
}

CleanupStrategy WasteAnalyzer::parse_strategy(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    if (s == "NEWEST" || s == "KEEP_NEWEST") return CleanupStrategy::KEEP_NEWEST;
    if (s == "SHORTEST" || s == "KEEP_SHORTEST_PATH") return CleanupStrategy::KEEP_SHORTEST_PATH;
    if (s == "LONGEST" || s == "KEEP_LONGEST_PATH") return CleanupStrategy::KEEP_LONGEST_PATH;
    return CleanupStrategy::KEEP_OLDEST;
}

FileMetadata WasteAnalyzer::select_original_to_keep(const std::vector<FileMetadata>& files) const {
    if (files.empty()) return FileMetadata();

    auto best = files.begin();
    for (auto it = files.begin() + 1; it != files.end(); ++it) {
        switch (strategy_) {
            case CleanupStrategy::KEEP_OLDEST:
                if (it->last_write_time < best->last_write_time) {
                    best = it;
                }
                break;
            case CleanupStrategy::KEEP_NEWEST:
                if (it->last_write_time > best->last_write_time) {
                    best = it;
                }
                break;
            case CleanupStrategy::KEEP_SHORTEST_PATH:
                if (it->full_path.string().length() < best->full_path.string().length()) {
                    best = it;
                }
                break;
            case CleanupStrategy::KEEP_LONGEST_PATH:
                if (it->full_path.string().length() > best->full_path.string().length()) {
                    best = it;
                }
                break;
        }
    }
    return *best;
}

WasteAnalysisReport WasteAnalyzer::analyze(
    const std::filesystem::path& root_path,
    const std::vector<FileMetadata>& all_files,
    const std::vector<DuplicateGroup>& duplicates
) {
    WasteAnalysisReport report;
    report.scan_root = root_path.string();
    report.timestamp = get_current_timestamp();
    report.strategy_used = strategy_;
    report.strategy_name = strategy_to_string(strategy_);

    report.total_files_scanned = all_files.size();
    for (const auto& file : all_files) {
        report.total_scanned_bytes += file.size_bytes;
    }

    report.duplicate_groups = duplicates;
    report.duplicate_groups_count = duplicates.size();

    for (const auto& group : duplicates) {
        report.duplicate_files_count += group.files.size();
        std::uintmax_t wasted = group.wasted_bytes();
        report.total_wasted_bytes += wasted;

        report.waste_by_category[group.category] += wasted;
        report.duplicate_count_by_category[group.category] += group.files.size();

        if (!group.files.empty()) {
            std::string ext = group.files.front().extension;
            if (ext.empty()) ext = "[none]";
            report.waste_by_extension[ext] += wasted;
        }

        // Generate cleanup recommendation for this group
        FileMetadata original = select_original_to_keep(group.files);
        CleanupRecommendation rec;
        rec.sha256_hash = group.sha256_hash;
        rec.original_to_keep = original;
        rec.recoverable_bytes = wasted;

        for (const auto& f : group.files) {
            if (f.full_path != original.full_path) {
                rec.duplicates_to_delete.push_back(f);
            }
        }

        std::ostringstream reason_oss;
        reason_oss << "Preserved " << original.filename << " (" << report.strategy_name 
                   << " modified " << original.formatted_time << "); safely removes " 
                   << rec.duplicates_to_delete.size() << " duplicate copy/copies.";
        rec.reason = reason_oss.str();

        report.recommendations.push_back(std::move(rec));
    }

    if (report.total_scanned_bytes > 0) {
        report.waste_percentage = (static_cast<double>(report.total_wasted_bytes) / 
                                   static_cast<double>(report.total_scanned_bytes)) * 100.0;
    }

    return report;
}

} // namespace storage_optimizer
