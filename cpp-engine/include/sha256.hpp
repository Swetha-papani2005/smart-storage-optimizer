#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>

namespace storage_optimizer {

class SHA256 {
public:
    SHA256();
    void update(const uint8_t* data, size_t length);
    void update(const std::string& data);
    std::string finalize();

    // Stream-hashes a file. If max_bytes > 0, hashes only up to max_bytes (useful for fast pre-filtering)
    static std::string hash_file(const std::filesystem::path& file_path, std::uintmax_t max_bytes = 0);
    static std::string hash_data(const uint8_t* data, size_t length);

private:
    void transform(const uint8_t* chunk);

    uint32_t state_[8];
    uint64_t bit_count_;
    uint8_t buffer_[64];
    size_t buffer_len_;
};

} // namespace storage_optimizer
