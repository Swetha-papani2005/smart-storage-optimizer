#include "duplicate_detector.hpp"
#include <iostream>
#include <map>
#include <algorithm>

namespace storage_optimizer {

std::vector<DuplicateGroup> DuplicateDetector::detect_duplicates(
    std::vector<FileMetadata>& files,
    std::function<void(const std::string& phase, size_t current, size_t total)> progress_cb
) {
    stats_ = DetectionStats();
    stats_.total_files_checked = files.size();

    std::vector<DuplicateGroup> duplicate_groups;
    if (files.empty()) {
        return duplicate_groups;
    }

    // =========================================================================
    // STAGE 1: File Size Partitioning (O(N) with 0 Disk Hashing I/O)
    // Files with unique byte sizes can never be duplicates.
    // =========================================================================
    if (progress_cb) progress_cb("Stage 1/3: Size Partitioning", 0, files.size());

    std::unordered_map<std::uintmax_t, std::vector<FileMetadata*>> size_buckets;
    for (auto& file : files) {
        size_buckets[file.size_bytes].push_back(&file);
    }
    stats_.unique_sizes_count = size_buckets.size();

    std::vector<FileMetadata*> size_candidates;
    for (auto& [size, bucket] : size_buckets) {
        if (bucket.size() > 1) {
            for (auto* file_ptr : bucket) {
                size_candidates.push_back(file_ptr);
            }
        }
    }
    stats_.candidate_files_after_size_filter = size_candidates.size();

    if (size_candidates.empty()) {
        if (progress_cb) progress_cb("Completed: No duplicate candidates found by size.", 1, 1);
        return duplicate_groups;
    }

    // =========================================================================
    // STAGE 2: 4 KB Header Pre-Hashing
    // Reads only the first 4096 bytes. Eliminates files with identical sizes
    // but different header formats without reading the whole file.
    // =========================================================================
    if (progress_cb) progress_cb("Stage 2/3: Header 4KB Pre-Hashing", 0, size_candidates.size());

    // Group key: size_bytes string + "_" + partial_hash
    std::unordered_map<std::string, std::vector<FileMetadata*>> partial_buckets;
    size_t processed_partial = 0;

    for (auto* file_ptr : size_candidates) {
        // Fast 4KB chunk hash
        file_ptr->partial_hash = SHA256::hash_file(file_ptr->full_path, 4096);
        std::string bucket_key = std::to_string(file_ptr->size_bytes) + "_" + file_ptr->partial_hash;
        partial_buckets[bucket_key].push_back(file_ptr);

        processed_partial++;
        if (progress_cb && (processed_partial % 20 == 0 || processed_partial == size_candidates.size())) {
            progress_cb("Stage 2/3: Header 4KB Pre-Hashing", processed_partial, size_candidates.size());
        }
    }

    std::vector<FileMetadata*> full_hash_candidates;
    for (auto& [key, bucket] : partial_buckets) {
        if (bucket.size() > 1) {
            for (auto* file_ptr : bucket) {
                full_hash_candidates.push_back(file_ptr);
            }
        }
    }
    stats_.candidate_files_after_partial_hash = full_hash_candidates.size();

    if (full_hash_candidates.empty()) {
        if (progress_cb) progress_cb("Completed: No duplicate candidates after header hashing.", 1, 1);
        return duplicate_groups;
    }

    // =========================================================================
    // STAGE 3: Full Cryptographic SHA-256 Stream Hashing
    // Only executed on candidates with matching sizes and matching 4KB headers.
    // =========================================================================
    if (progress_cb) progress_cb("Stage 3/3: Full SHA-256 Hashing", 0, full_hash_candidates.size());

    std::unordered_map<std::string, std::vector<FileMetadata*>> full_hash_buckets;
    size_t processed_full = 0;

    for (auto* file_ptr : full_hash_candidates) {
        // Optimization: If file size is <= 4096 bytes, the partial hash is already the full hash!
        if (file_ptr->size_bytes <= 4096) {
            file_ptr->full_hash = file_ptr->partial_hash;
        } else {
            file_ptr->full_hash = SHA256::hash_file(file_ptr->full_path, 0);
            stats_.full_hashes_computed++;
        }

        if (!file_ptr->full_hash.empty()) {
            full_hash_buckets[file_ptr->full_hash].push_back(file_ptr);
        }

        processed_full++;
        if (progress_cb && (processed_full % 10 == 0 || processed_full == full_hash_candidates.size())) {
            progress_cb("Stage 3/3: Full SHA-256 Hashing", processed_full, full_hash_candidates.size());
        }
    }

    // =========================================================================
    // Assemble Verified Duplicate Groups
    // =========================================================================
    for (auto& [hash, bucket] : full_hash_buckets) {
        if (bucket.size() > 1) {
            DuplicateGroup group;
            group.sha256_hash = hash;
            group.file_size = bucket.front()->size_bytes;
            group.category = bucket.front()->category;

            for (auto* file_ptr : bucket) {
                group.files.push_back(*file_ptr);
            }

            stats_.duplicate_groups_found++;
            stats_.duplicate_files_found += group.files.size();
            stats_.total_wasted_bytes += group.wasted_bytes();

            duplicate_groups.push_back(std::move(group));
        }
    }

    // Sort duplicate groups by wasted space in descending order (highest impact first)
    std::sort(duplicate_groups.begin(), duplicate_groups.end(), [](const DuplicateGroup& a, const DuplicateGroup& b) {
        return a.wasted_bytes() > b.wasted_bytes();
    });

    if (progress_cb) {
        progress_cb("Completed duplicate detection pipeline.", duplicate_groups.size(), duplicate_groups.size());
    }

    return duplicate_groups;
}

} // namespace storage_optimizer
