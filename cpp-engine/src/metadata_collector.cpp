#include "metadata_collector.hpp"
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace storage_optimizer {

namespace {

std::string to_lower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return str;
}

} // namespace

MetadataCollector::MetadataCollector(ScanFilterOptions options)
    : options_(std::move(options)) {
    // Default exclusions if empty
    if (options_.exclude_dirs.empty()) {
        options_.exclude_dirs = {
            ".git", ".svn", "node_modules", "$RECYCLE.BIN", "System Volume Information"
        };
    }
}

std::string MetadataCollector::determine_category(const std::string& ext_raw) {
    std::string ext = to_lower(ext_raw);
    if (ext.empty()) return "Other";

    if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".gif" || 
        ext == ".bmp" || ext == ".webp" || ext == ".svg" || ext == ".ico" || ext == ".tiff") {
        return "Images";
    }
    if (ext == ".pdf" || ext == ".docx" || ext == ".doc" || ext == ".xlsx" || 
        ext == ".pptx" || ext == ".txt" || ext == ".md" || ext == ".csv" || ext == ".rtf") {
        return "Documents";
    }
    if (ext == ".mp4" || ext == ".mkv" || ext == ".avi" || ext == ".mov" || 
        ext == ".wmv" || ext == ".flv" || ext == ".webm") {
        return "Videos";
    }
    if (ext == ".mp3" || ext == ".wav" || ext == ".flac" || ext == ".aac" || 
        ext == ".ogg" || ext == ".m4a") {
        return "Audio";
    }
    if (ext == ".zip" || ext == ".tar" || ext == ".gz" || ext == ".7z" || 
        ext == ".rar" || ext == ".bz2" || ext == ".xz") {
        return "Archives";
    }
    if (ext == ".cpp" || ext == ".c" || ext == ".hpp" || ext == ".h" || 
        ext == ".py" || ext == ".js" || ext == ".ts" || ext == ".html" || 
        ext == ".css" || ext == ".json" || ext == ".java" || ext == ".rs" || 
        ext == ".go" || ext == ".sh" || ext == ".bat" || ext == ".ps1") {
        return "Code";
    }

    return "Other";
}

std::string MetadataCollector::format_file_time(std::filesystem::file_time_type ftime) {
    try {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
        std::tm tm_buf;
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm_buf, &cftime);
#else
        localtime_r(&cftime, &tm_buf);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    } catch (...) {
        return "Unknown";
    }
}

std::string MetadataCollector::format_bytes(std::uintmax_t bytes) {
    constexpr double KB = 1024.0;
    constexpr double MB = KB * 1024.0;
    constexpr double GB = MB * 1024.0;
    constexpr double TB = GB * 1024.0;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    if (bytes >= TB) {
        oss << (bytes / TB) << " TB";
    } else if (bytes >= GB) {
        oss << (bytes / GB) << " GB";
    } else if (bytes >= MB) {
        oss << (bytes / MB) << " MB";
    } else if (bytes >= KB) {
        oss << (bytes / KB) << " KB";
    } else {
        oss << bytes << " Bytes";
    }
    return oss.str();
}

bool MetadataCollector::should_skip_directory(const std::filesystem::path& dir_path) const {
    std::string dirname = dir_path.filename().string();
    if (!options_.include_hidden && !dirname.empty() && dirname[0] == '.' && dirname != ".") {
        return true;
    }
    for (const auto& excluded : options_.exclude_dirs) {
        if (dirname == excluded) {
            return true;
        }
    }
    return false;
}

bool MetadataCollector::should_skip_file(const std::filesystem::path& file_path, std::uintmax_t size) const {
    if (size < options_.min_size_bytes) {
        return true;
    }
    if (options_.max_size_bytes > 0 && size > options_.max_size_bytes) {
        return true;
    }
    if (!options_.include_hidden) {
        std::string fname = file_path.filename().string();
        if (!fname.empty() && fname[0] == '.') {
            return true;
        }
    }
    return false;
}

std::vector<FileMetadata> MetadataCollector::scan_directory(
    const std::filesystem::path& root_path,
    std::function<void(size_t files_scanned, const std::string& current_file)> progress_cb
) {
    std::vector<FileMetadata> collected_files;
    std::error_code ec;

    if (!std::filesystem::exists(root_path, ec) || !std::filesystem::is_directory(root_path, ec)) {
        std::cerr << "Error: Target directory does not exist or is not a directory: " << root_path << "\n";
        return collected_files;
    }

    auto dir_options = std::filesystem::directory_options::skip_permission_denied;
    if (options_.follow_symlinks) {
        dir_options |= std::filesystem::directory_options::follow_directory_symlink;
    }

    std::filesystem::recursive_directory_iterator iter(root_path, dir_options, ec);
    std::filesystem::recursive_directory_iterator end_iter;

    if (ec) {
        std::cerr << "Error creating directory iterator: " << ec.message() << "\n";
        return collected_files;
    }

    size_t scanned_counter = 0;

    for (; iter != end_iter; iter.increment(ec)) {
        if (ec) {
            // Permission or access error on sub-item, skip and continue
            ec.clear();
            continue;
        }

        const auto& entry = *iter;
        std::error_code entry_ec;

        if (entry.is_directory(entry_ec)) {
            if (should_skip_directory(entry.path())) {
                iter.disable_recursion_pending();
            }
            continue;
        }

        if (!entry.is_regular_file(entry_ec)) {
            continue;
        }

        std::uintmax_t size = entry.file_size(entry_ec);
        if (entry_ec) {
            continue;
        }

        if (should_skip_file(entry.path(), size)) {
            continue;
        }

        FileMetadata meta;
        meta.full_path = std::filesystem::canonical(entry.path(), entry_ec);
        if (entry_ec) {
            meta.full_path = entry.path().lexically_normal();
        }
        
        meta.relative_path = std::filesystem::relative(meta.full_path, root_path, entry_ec);
        if (entry_ec) {
            meta.relative_path = meta.full_path.filename();
        }

        meta.filename = meta.full_path.filename().string();
        meta.extension = meta.full_path.extension().string();
        meta.category = determine_category(meta.extension);
        meta.size_bytes = size;
        meta.last_write_time = entry.last_write_time(entry_ec);
        meta.formatted_time = format_file_time(meta.last_write_time);

        collected_files.push_back(std::move(meta));
        scanned_counter++;

        if (progress_cb && (scanned_counter % 25 == 0 || scanned_counter == 1)) {
            progress_cb(scanned_counter, collected_files.back().filename);
        }
    }

    if (progress_cb) {
        progress_cb(scanned_counter, "Done scanning directory.");
    }

    return collected_files;
}

} // namespace storage_optimizer
