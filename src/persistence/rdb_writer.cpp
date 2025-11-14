#include "rdb_writer.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <cstring>
#include <ctime>
#include <filesystem>

#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

const std::string RdbWriter::MAGIC_NUMBER = "REDIS";

bool RdbWriter::SaveToFile(const std::string& filename, const SwissTableStore& store,
                           CompressionType compression) {
    return SaveDatabaseToFile(filename, store, 0, compression);
}

bool RdbWriter::SaveDatabaseToFile(const std::string& filename, const SwissTableStore& store,
                                  int db_number, CompressionType compression) {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Reset statistics
    last_stats_ = SaveStats{};
    last_stats_.compression_used = compression;

    // Ensure directory exists
    if (!EnsureDirectoryExists(filename)) {
        std::cerr << "Failed to create directory for: " << filename << std::endl;
        return false;
    }

    FILE* file = fopen(filename.c_str(), "wb");
    if (!file) {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return false;
    }

    bool success = true;

    try {
        // Write RDB header
        if (!WriteMagicNumber(file)) {
            throw std::runtime_error("Failed to write magic number");
        }

        if (!WriteVersion(file)) {
            throw std::runtime_error("Failed to write version");
        }

        // Write database selector and data
        if (!WriteSelector(file, db_number)) {
            throw std::runtime_error("Failed to write database selector");
        }

        // Get all keys from the store
        auto keys = store.Keys();
        last_stats_.keys_saved = keys.size();

        if (!WriteResizeTable(file, keys.size())) {
            throw std::runtime_error("Failed to write resize table");
        }

        // Write all key-value pairs
        for (const auto& key : keys) {
            auto value = store.Get(key);
            if (value.has_value()) {
                if (!WriteKeyValuePair(file, key, value.value())) {
                    throw std::runtime_error("Failed to write key-value pair for key: " + key);
                }
            }
        }

        // Write end of file marker
        if (!WriteEndOfFile(file)) {
            throw std::runtime_error("Failed to write end of file");
        }

        // Calculate and write checksum
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        if (file_size > 0) {
            std::vector<uint8_t> file_data(file_size);
            fseek(file, 0, SEEK_SET);
            size_t read_size = fread(file_data.data(), 1, file_size, file);
            if (read_size == static_cast<size_t>(file_size)) {
                if (!WriteChecksum(file, file_data)) {
                    throw std::runtime_error("Failed to write checksum");
                }
            }
        }

        // Update statistics
        last_stats_.total_size = static_cast<size_t>(file_size);
        auto end_time = std::chrono::high_resolution_clock::now();
        last_stats_.save_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        std::cout << "Successfully saved " << last_stats_.keys_saved << " keys to "
                  << filename << " (" << last_stats_.total_size << " bytes) in "
                  << last_stats_.save_time.count() << "ms" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error during RDB save: " << e.what() << std::endl;
        success = false;
    }

    fclose(file);

    // Remove partial file on failure
    if (!success && fs::exists(filename)) {
        fs::remove(filename);
    }

    return success;
}

bool RdbWriter::WriteMagicNumber(FILE* file) {
    size_t written = fwrite(MAGIC_NUMBER.c_str(), 1, MAGIC_NUMBER.length(), file);
    return written == MAGIC_NUMBER.length();
}

bool RdbWriter::WriteVersion(FILE* file) {
    uint32_t version_be = htobe32(RDB_VERSION);
    size_t written = fwrite(&version_be, sizeof(uint32_t), 1, file);
    return written == 1;
}

bool RdbWriter::WriteSelector(FILE* file, int db_number) {
    // Redis RDB uses specific byte codes for database selection
    uint8_t select_code = 0xFE;  // SELECTDB opcode
    size_t written = fwrite(&select_code, sizeof(uint8_t), 1, file);
    if (written != 1) return false;

    return WriteLength(file, db_number);
}

bool RdbWriter::WriteResizeTable(FILE* file, size_t table_size) {
    // Redis RDB includes hash table size information
    // For simplicity, we'll write the same size for both tables
    return WriteLength(file, table_size) && WriteLength(file, table_size);
}

bool RdbWriter::WriteKeyValuePair(FILE* file, const std::string& key, const std::string& value) {
    // Redis RDB uses specific encodings for different value types
    uint8_t value_type = 0;  // String value type

    size_t written = fwrite(&value_type, sizeof(uint8_t), 1, file);
    if (written != 1) return false;

    // Write key
    if (!WriteString(file, key)) {
        return false;
    }

    // Write value
    if (!WriteString(file, value)) {
        return false;
    }

    return true;
}

bool RdbWriter::WriteEndOfFile(FILE* file) {
    uint8_t eof_code = 0xFF;  // EOF opcode
    size_t written = fwrite(&eof_code, sizeof(uint8_t), 1, file);
    return written == 1;
}

bool RdbWriter::WriteChecksum(FILE* file, const std::vector<uint8_t>& data) {
    uint64_t checksum = CalculateCRC64(data.data(), data.size());
    uint64_t checksum_be = htobe64(checksum);

    size_t written = fwrite(&checksum_be, sizeof(uint64_t), 1, file);
    return written == 1;
}

bool RdbWriter::WriteLength(FILE* file, size_t length) {
    if (length < (1 << 6)) {
        // 6-bit length
        uint8_t byte = static_cast<uint8_t>(length);
        size_t written = fwrite(&byte, sizeof(uint8_t), 1, file);
        return written == 1;
    } else if (length < (1 << 14)) {
        // 14-bit length
        uint16_t value = static_cast<uint16_t>((length << 2) | 0x01);
        uint16_t value_be = htobe16(value);
        size_t written = fwrite(&value_be, sizeof(uint16_t), 1, file);
        return written == 1;
    } else {
        // 32-bit length
        uint8_t header = 0x02;
        size_t written = fwrite(&header, sizeof(uint8_t), 1, file);
        if (written != 1) return false;

        uint32_t length_be = htobe32(static_cast<uint32_t>(length));
        written = fwrite(&length_be, sizeof(uint32_t), 1, file);
        return written == 1;
    }
}

bool RdbWriter::WriteString(FILE* file, const std::string& str) {
    if (str.length() < (1 << 6)) {
        // Use integer encoding for small strings
        uint8_t header = 0xC0 | static_cast<uint8_t>(str.length());
        size_t written = fwrite(&header, sizeof(uint8_t), 1, file);
        if (written != 1) return false;
    } else {
        // Use length-encoded string
        if (!WriteLength(file, str.length())) {
            return false;
        }
    }

    size_t written = fwrite(str.data(), 1, str.length(), file);
    return written == str.length();
}

bool RdbWriter::WriteInteger(FILE* file, int64_t value) {
    if (value >= 0 && value <= 12) {
        // Special encoding for very small integers
        uint8_t encoded = static_cast<uint8_t>(value | 0xC0);
        size_t written = fwrite(&encoded, sizeof(uint8_t), 1, file);
        return written == 1;
    } else if (value >= INT8_MIN && value <= INT8_MAX) {
        // 8-bit integer
        uint8_t header = 0xC3;
        int8_t int_val = static_cast<int8_t>(value);
        size_t written = fwrite(&header, sizeof(uint8_t), 1, file);
        if (written != 1) return false;
        written = fwrite(&int_val, sizeof(int8_t), 1, file);
        return written == 1;
    } else if (value >= INT16_MIN && value <= INT16_MAX) {
        // 16-bit integer
        uint8_t header = 0xC2;
        int16_t int_val = htobe16(static_cast<int16_t>(value));
        size_t written = fwrite(&header, sizeof(uint8_t), 1, file);
        if (written != 1) return false;
        written = fwrite(&int_val, sizeof(int16_t), 1, file);
        return written == 1;
    } else if (value >= INT32_MIN && value <= INT32_MAX) {
        // 32-bit integer
        uint8_t header = 0xC1;
        int32_t int_val = htobe32(static_cast<int32_t>(value));
        size_t written = fwrite(&header, sizeof(uint8_t), 1, file);
        if (written != 1) return false;
        written = fwrite(&int_val, sizeof(int32_t), 1, file);
        return written == 1;
    } else {
        // 64-bit integer
        uint8_t header = 0xC4;
        int64_t int_val = htobe64(value);
        size_t written = fwrite(&header, sizeof(uint8_t), 1, file);
        if (written != 1) return false;
        written = fwrite(&int_val, sizeof(int64_t), 1, file);
        return written == 1;
    }
}

std::vector<uint8_t> RdbWriter::CompressData(const std::vector<uint8_t>& data,
                                            CompressionType type) {
    // Compression not implemented in this version
    // This is a placeholder for future compression support
    (void)data;
    (void)type;
    return data;
}

std::vector<uint8_t> RdbWriter::DecompressData(const std::vector<uint8_t>& data,
                                              CompressionType type) {
    // Compression not implemented in this version
    (void)data;
    (void)type;
    return data;
}

uint64_t RdbWriter::CalculateCRC64(const uint8_t* data, size_t length) {
    // Simple CRC64 implementation
    // Redis uses a specific CRC64 polynomial, but for simplicity,
    // we'll use a basic checksum here
    uint64_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0x42F0E1EBA9EA3693ULL;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

std::string RdbWriter::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

bool RdbWriter::EnsureDirectoryExists(const std::string& filepath) {
    fs::path file_path(filepath);
    fs::path dir_path = file_path.parent_path();

    if (!dir_path.empty() && !fs::exists(dir_path)) {
        std::error_code ec;
        if (!fs::create_directories(dir_path, ec)) {
            return false;
        }
    }

    return true;
}