#pragma once

#include "metadata_collector.hpp"
#include "sha256.hpp"
#include <vector>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <functional>

namespace storage_optimizer {

struct DuplicateGroup {
    std::string sha256_hash;
    std::uintmax_t file_size = 0;
    std::string category;
    std::vector<FileMetadata> files;

    std::uintmax_t wasted_bytes() const {
        if (files.size() <= 1) return 0;
        return file_size * (files.size() - 1);
    }
};

struct DetectionStats {
    size_t total_files_checked = 0;
    size_t unique_sizes_count = 0;
    size_t candidate_files_after_size_filter = 0;
    size_t candidate_files_after_partial_hash = 0;
    size_t full_hashes_computed = 0;
    size_t duplicate_groups_found = 0;
    size_t duplicate_files_found = 0;
    std::uintmax_t total_wasted_bytes = 0;
};

class DuplicateDetector {
public:
    DuplicateDetector() = default;

    // Detects duplicates through the 3-stage optimization pipeline:
    // Size Partitioning -> 4KB Chunk Pre-hash -> Full SHA-256 Digest
    std::vector<DuplicateGroup> detect_duplicates(
        std::vector<FileMetadata>& files,
        std::function<void(const std::string& phase, size_t current, size_t total)> progress_cb = nullptr
    );

    const DetectionStats& stats() const { return stats_; }

private:
    DetectionStats stats_;
};

} // namespace storage_optimizer
