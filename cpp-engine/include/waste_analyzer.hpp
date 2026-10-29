#pragma once

#include "duplicate_detector.hpp"
#include <vector>
#include <string>
#include <map>
#include <cstdint>

namespace storage_optimizer {

enum class CleanupStrategy {
    KEEP_OLDEST,
    KEEP_NEWEST,
    KEEP_SHORTEST_PATH,
    KEEP_LONGEST_PATH
};

struct CleanupRecommendation {
    std::string sha256_hash;
    FileMetadata original_to_keep;
    std::vector<FileMetadata> duplicates_to_delete;
    std::uintmax_t recoverable_bytes = 0;
    std::string reason;
};

struct WasteAnalysisReport {
    std::string scan_root;
    std::string timestamp;
    CleanupStrategy strategy_used = CleanupStrategy::KEEP_OLDEST;
    std::string strategy_name;

    size_t total_files_scanned = 0;
    std::uintmax_t total_scanned_bytes = 0;
    
    size_t duplicate_files_count = 0;
    size_t duplicate_groups_count = 0;
    std::uintmax_t total_wasted_bytes = 0;
    double waste_percentage = 0.0;

    std::map<std::string, std::uintmax_t> waste_by_category;
    std::map<std::string, size_t> duplicate_count_by_category;
    std::map<std::string, std::uintmax_t> waste_by_extension;

    std::vector<DuplicateGroup> duplicate_groups;
    std::vector<CleanupRecommendation> recommendations;
};

class WasteAnalyzer {
public:
    explicit WasteAnalyzer(CleanupStrategy strategy = CleanupStrategy::KEEP_OLDEST);

    WasteAnalysisReport analyze(
        const std::filesystem::path& root_path,
        const std::vector<FileMetadata>& all_files,
        const std::vector<DuplicateGroup>& duplicates
    );

    static std::string strategy_to_string(CleanupStrategy strategy);
    static CleanupStrategy parse_strategy(const std::string& str);

private:
    FileMetadata select_original_to_keep(const std::vector<FileMetadata>& files) const;

    CleanupStrategy strategy_;
};

} // namespace storage_optimizer
