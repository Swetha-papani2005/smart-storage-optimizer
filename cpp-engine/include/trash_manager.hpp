#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>
#include <chrono>

namespace storage_optimizer {

enum class TrashRetention {
    ONE_DAY,    // 24 Hours
    ONE_WEEK,   // 7 Days
    ONE_MONTH,  // 30 Days
    NEVER       // Keep indefinitely
};

struct TrashedItem {
    std::string id;
    std::string original_path;
    std::string trash_filename;
    std::string display_name;
    std::uintmax_t size_bytes = 0;
    std::string sha256_hash;
    int64_t trashed_at = 0;     // Unix timestamp seconds
    int64_t expires_at = 0;     // Unix timestamp seconds
    std::string retention_name; // "1 Day", "1 Week", "1 Month"

    bool is_expired(int64_t now_ts) const {
        return expires_at > 0 && now_ts >= expires_at;
    }

    int64_t seconds_remaining(int64_t now_ts) const {
        return expires_at > now_ts ? (expires_at - now_ts) : 0;
    }

    std::string status(int64_t now_ts = 0) const;
};

class TrashManager {
public:
    explicit TrashManager(std::filesystem::path trash_dir = "smart_trash");

    // Move file to smart trash with retention policy
    bool move_to_trash(
        const std::filesystem::path& file_path, 
        TrashRetention retention = TrashRetention::ONE_MONTH,
        const std::string& sha256 = ""
    );

    // Restore trashed file back to its original path
    bool restore_item(const std::string& item_id);

    // Purge expired files from trash automatically
    size_t purge_expired();

    // List all items currently in trash
    std::vector<TrashedItem> get_items() const { return items_; }

    // Permanently remove specific item
    bool delete_permanently(const std::string& item_id);

    // Empty entire trash
    void empty_trash();

    static std::string retention_to_string(TrashRetention r);
    static TrashRetention parse_retention(const std::string& str);
    static int64_t calculate_expiry(int64_t from_time, TrashRetention retention);
    static std::string format_time_remaining(int64_t seconds);

    std::filesystem::path trash_dir() const { return trash_dir_; }

private:
    void load_manifest();
    void save_manifest();

    std::filesystem::path trash_dir_;
    std::filesystem::path manifest_path_;
    std::vector<TrashedItem> items_;
};

} // namespace storage_optimizer
