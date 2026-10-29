#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>
#include <chrono>
#include <functional>

namespace storage_optimizer {

struct FileMetadata {
    std::filesystem::path full_path;
    std::filesystem::path relative_path;
    std::string filename;
    std::string extension;
    std::string category;
    std::uintmax_t size_bytes = 0;
    std::filesystem::file_time_type last_write_time;
    std::string formatted_time;
    
    // Checksum caches
    std::string partial_hash; // First 4KB hash
    std::string full_hash;    // Complete SHA-256 hash
};

struct ScanFilterOptions {
    std::uintmax_t min_size_bytes = 1;       // Default ignore 0-byte files
    std::uintmax_t max_size_bytes = 0;       // 0 means no upper limit
    bool include_hidden = false;             // Skip hidden files/directories (.git, etc.)
    bool follow_symlinks = false;            // Avoid circular symlink traps
    std::vector<std::string> exclude_dirs;   // e.g. "node_modules", ".git", "build"
};

class MetadataCollector {
public:
    explicit MetadataCollector(ScanFilterOptions options = ScanFilterOptions());

    // Recursively scans a directory collecting metadata for all matching regular files
    std::vector<FileMetadata> scan_directory(
        const std::filesystem::path& root_path,
        std::function<void(size_t files_scanned, const std::string& current_file)> progress_cb = nullptr
    );

    static std::string determine_category(const std::string& extension);
    static std::string format_file_time(std::filesystem::file_time_type ftime);
    static std::string format_bytes(std::uintmax_t bytes);

private:
    bool should_skip_directory(const std::filesystem::path& dir_path) const;
    bool should_skip_file(const std::filesystem::path& file_path, std::uintmax_t size) const;

    ScanFilterOptions options_;
};

} // namespace storage_optimizer
