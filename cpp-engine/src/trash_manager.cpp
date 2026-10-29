#include "trash_manager.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace storage_optimizer {

namespace {

int64_t get_now_epoch() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string escape_json(const std::string& input) {
    std::ostringstream ss;
    for (char c : input) {
        if (c == '"') ss << "\\\"";
        else if (c == '\\') ss << "\\\\";
        else ss << c;
    }
    return ss.str();
}

} // namespace

std::string TrashedItem::status(int64_t now_ts) const {
    if (now_ts == 0) now_ts = get_now_epoch();
    return TrashManager::format_time_remaining(seconds_remaining(now_ts));
}

TrashManager::TrashManager(std::filesystem::path trash_dir)
    : trash_dir_(std::move(trash_dir)) {
    manifest_path_ = trash_dir_ / "trash_manifest.json";
    
    std::error_code ec;
    if (!std::filesystem::exists(trash_dir_, ec)) {
        std::filesystem::create_directories(trash_dir_, ec);
    }

    load_manifest();
    purge_expired();
}

std::string TrashManager::retention_to_string(TrashRetention r) {
    switch (r) {
        case TrashRetention::ONE_DAY: return "1 Day";
        case TrashRetention::ONE_WEEK: return "1 Week";
        case TrashRetention::ONE_MONTH: return "1 Month";
        case TrashRetention::NEVER: return "Permanent (No Expiry)";
    }
    return "1 Month";
}

TrashRetention TrashManager::parse_retention(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (s.find("day") != std::string::npos || s == "1d") return TrashRetention::ONE_DAY;
    if (s.find("week") != std::string::npos || s == "1w") return TrashRetention::ONE_WEEK;
    if (s.find("month") != std::string::npos || s == "1m") return TrashRetention::ONE_MONTH;
    return TrashRetention::ONE_MONTH;
}

int64_t TrashManager::calculate_expiry(int64_t from_time, TrashRetention retention) {
    switch (retention) {
        case TrashRetention::ONE_DAY: return from_time + (24 * 3600);
        case TrashRetention::ONE_WEEK: return from_time + (7 * 24 * 3600);
        case TrashRetention::ONE_MONTH: return from_time + (30 * 24 * 3600);
        case TrashRetention::NEVER: return 0;
    }
    return from_time + (30 * 24 * 3600);
}

std::string TrashManager::format_time_remaining(int64_t seconds) {
    if (seconds <= 0) return "Expired (Ready to purge)";
    int64_t days = seconds / (24 * 3600);
    int64_t hours = (seconds % (24 * 3600)) / 3600;
    int64_t mins = (seconds % 3600) / 60;

    if (days > 0) return std::to_string(days) + "d " + std::to_string(hours) + "h remaining";
    if (hours > 0) return std::to_string(hours) + "h " + std::to_string(mins) + "m remaining";
    return std::to_string(mins) + "m remaining";
}

bool TrashManager::move_to_trash(
    const std::filesystem::path& file_path, 
    TrashRetention retention,
    const std::string& sha256
) {
    std::error_code ec;
    if (!std::filesystem::exists(file_path, ec) || !std::filesystem::is_regular_file(file_path, ec)) {
        std::cerr << "File does not exist or is not a regular file: " << file_path << "\n";
        return false;
    }

    if (!std::filesystem::exists(trash_dir_, ec)) {
        std::filesystem::create_directories(trash_dir_, ec);
    }

    int64_t now_ts = get_now_epoch();
    std::uintmax_t size = std::filesystem::file_size(file_path, ec);

    std::string item_id = "trash_" + std::to_string(now_ts) + "_" + std::to_string(items_.size() + 1);
    std::string ext = file_path.extension().string();
    std::string trash_filename = item_id + ext;
    std::filesystem::path dest_path = trash_dir_ / trash_filename;

    std::filesystem::rename(file_path, dest_path, ec);
    if (ec) {
        // Fallback to copy & remove if cross-drive
        ec.clear();
        std::filesystem::copy_file(file_path, dest_path, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            std::cerr << "Failed to move file to trash: " << ec.message() << "\n";
            return false;
        }
        std::filesystem::remove(file_path, ec);
    }

    TrashedItem item;
    item.id = item_id;
    item.original_path = std::filesystem::absolute(file_path).string();
    item.trash_filename = trash_filename;
    item.display_name = file_path.filename().string();
    item.size_bytes = size;
    item.sha256_hash = sha256;
    item.trashed_at = now_ts;
    item.expires_at = calculate_expiry(now_ts, retention);
    item.retention_name = retention_to_string(retention);

    items_.push_back(std::move(item));
    save_manifest();

    std::cout << " [TRASHED] Moved to Smart Trash: " << file_path.filename().string() 
              << " (Expires in " << item.retention_name << ")\n";
    return true;
}

bool TrashManager::restore_item(const std::string& item_id) {
    auto it = std::find_if(items_.begin(), items_.end(), [&](const TrashedItem& x) {
        return x.id == item_id;
    });

    if (it == items_.end()) {
        std::cerr << "Error: Trashed item ID not found: " << item_id << "\n";
        return false;
    }

    std::error_code ec;
    std::filesystem::path src_path = trash_dir_ / it->trash_filename;
    std::filesystem::path orig_path(it->original_path);

    if (!std::filesystem::exists(src_path, ec)) {
        std::cerr << "Error: Physical file missing in trash directory: " << src_path << "\n";
        return false;
    }

    // Ensure parent directory exists for restoration
    std::filesystem::path parent = orig_path.parent_path();
    if (!std::filesystem::exists(parent, ec)) {
        std::filesystem::create_directories(parent, ec);
    }

    // Move file back
    std::filesystem::rename(src_path, orig_path, ec);
    if (ec) {
        ec.clear();
        std::filesystem::copy_file(src_path, orig_path, std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::remove(src_path, ec);
    }

    std::cout << " [RESTORED] Restored file to original location: " << orig_path.string() << "\n";

    items_.erase(it);
    save_manifest();
    return true;
}

size_t TrashManager::purge_expired() {
    int64_t now_ts = get_now_epoch();
    size_t purged_count = 0;
    std::error_code ec;

    auto it = items_.begin();
    while (it != items_.end()) {
        if (it->is_expired(now_ts)) {
            std::filesystem::path fpath = trash_dir_ / it->trash_filename;
            if (std::filesystem::exists(fpath, ec)) {
                std::filesystem::remove(fpath, ec);
            }
            std::cout << " [AUTO-PURGED] Expired trash file deleted permanently: " 
                      << it->display_name << "\n";
            it = items_.erase(it);
            purged_count++;
        } else {
            ++it;
        }
    }

    if (purged_count > 0) {
        save_manifest();
    }
    return purged_count;
}

bool TrashManager::delete_permanently(const std::string& item_id) {
    auto it = std::find_if(items_.begin(), items_.end(), [&](const TrashedItem& x) {
        return x.id == item_id;
    });

    if (it == items_.end()) return false;

    std::error_code ec;
    std::filesystem::path fpath = trash_dir_ / it->trash_filename;
    if (std::filesystem::exists(fpath, ec)) {
        std::filesystem::remove(fpath, ec);
    }

    items_.erase(it);
    save_manifest();
    return true;
}

void TrashManager::empty_trash() {
    std::error_code ec;
    for (const auto& item : items_) {
        std::filesystem::path fpath = trash_dir_ / item.trash_filename;
        std::filesystem::remove(fpath, ec);
    }
    items_.clear();
    save_manifest();
    std::cout << "Smart Trash emptied completely.\n";
}

void TrashManager::save_manifest() {
    std::ofstream out(manifest_path_);
    if (!out.is_open()) return;

    int64_t now_ts = get_now_epoch();

    out << "{\n  \"items\": [\n";
    for (size_t i = 0; i < items_.size(); ++i) {
        const auto& it = items_[i];
        out << "    {\n";
        out << "      \"id\": \"" << escape_json(it.id) << "\",\n";
        out << "      \"original_path\": \"" << escape_json(it.original_path) << "\",\n";
        out << "      \"trash_filename\": \"" << escape_json(it.trash_filename) << "\",\n";
        out << "      \"display_name\": \"" << escape_json(it.display_name) << "\",\n";
        out << "      \"size_bytes\": " << it.size_bytes << ",\n";
        out << "      \"sha256_hash\": \"" << it.sha256_hash << "\",\n";
        out << "      \"trashed_at\": " << it.trashed_at << ",\n";
        out << "      \"expires_at\": " << it.expires_at << ",\n";
        out << "      \"retention_name\": \"" << it.retention_name << "\",\n";
        out << "      \"seconds_remaining\": " << it.seconds_remaining(now_ts) << ",\n";
        out << "      \"status\": \"" << format_time_remaining(it.seconds_remaining(now_ts)) << "\"\n";
        out << "    }" << (i + 1 < items_.size() ? "," : "") << "\n";
    }
    out << "  ]\n}\n";
}

void TrashManager::load_manifest() {
    items_.clear();
    std::ifstream in(manifest_path_);
    if (!in.is_open()) return;

    // Simple line-based JSON reader for portability without 3rd party parser
    std::string line;
    TrashedItem cur;
    bool in_item = false;

    while (std::getline(in, line)) {
        if (line.find("\"id\":") != std::string::npos) {
            in_item = true;
            cur = TrashedItem();
            auto q1 = line.find('"', line.find(':') + 1);
            auto q2 = line.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) {
                cur.id = line.substr(q1 + 1, q2 - q1 - 1);
            }
        } else if (in_item && line.find("\"original_path\":") != std::string::npos) {
            auto q1 = line.find('"', line.find(':') + 1);
            auto q2 = line.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) {
                cur.original_path = line.substr(q1 + 1, q2 - q1 - 1);
            }
        } else if (in_item && line.find("\"trash_filename\":") != std::string::npos) {
            auto q1 = line.find('"', line.find(':') + 1);
            auto q2 = line.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) {
                cur.trash_filename = line.substr(q1 + 1, q2 - q1 - 1);
            }
        } else if (in_item && line.find("\"display_name\":") != std::string::npos) {
            auto q1 = line.find('"', line.find(':') + 1);
            auto q2 = line.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) {
                cur.display_name = line.substr(q1 + 1, q2 - q1 - 1);
            }
        } else if (in_item && line.find("\"size_bytes\":") != std::string::npos) {
            size_t col = line.find(':');
            cur.size_bytes = std::stoull(line.substr(col + 1));
        } else if (in_item && line.find("\"sha256_hash\":") != std::string::npos) {
            auto q1 = line.find('"', line.find(':') + 1);
            auto q2 = line.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) {
                cur.sha256_hash = line.substr(q1 + 1, q2 - q1 - 1);
            }
        } else if (in_item && line.find("\"trashed_at\":") != std::string::npos) {
            size_t col = line.find(':');
            cur.trashed_at = std::stoll(line.substr(col + 1));
        } else if (in_item && line.find("\"expires_at\":") != std::string::npos) {
            size_t col = line.find(':');
            cur.expires_at = std::stoll(line.substr(col + 1));
        } else if (in_item && line.find("\"retention_name\":") != std::string::npos) {
            auto q1 = line.find('"', line.find(':') + 1);
            auto q2 = line.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) {
                cur.retention_name = line.substr(q1 + 1, q2 - q1 - 1);
            }
        } else if (in_item && line.find('}') != std::string::npos) {
            if (!cur.id.empty()) {
                items_.push_back(std::move(cur));
            }
            in_item = false;
        }
    }
}

} // namespace storage_optimizer
