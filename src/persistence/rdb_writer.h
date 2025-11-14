#pragma once

#include "../storage/swiss_table_store.h"
#include <string>
#include <fstream>

class RdbWriter {
public:
    enum class CompressionType {
        NONE = 0,
        LZ4 = 1,
        ZSTD = 2
    };

private:
    static const uint32_t RDB_VERSION = 9;  // Redis RDB version 9
    static const std::string MAGIC_NUMBER;

public:
    RdbWriter() = default;
    ~RdbWriter() = default;

    // Save database to RDB file
    bool SaveToFile(const std::string& filename, const SwissTableStore& store,
                    CompressionType compression = CompressionType::NONE);

    // Save specific database (for future multi-database support)
    bool SaveDatabaseToFile(const std::string& filename, const SwissTableStore& store,
                           int db_number = 0,
                           CompressionType compression = CompressionType::NONE);

    // Get save statistics
    struct SaveStats {
        size_t keys_saved = 0;
        size_t total_size = 0;
        std::chrono::milliseconds save_time{0};
        CompressionType compression_used = CompressionType::NONE;
        double compression_ratio = 1.0;
    };

    const SaveStats& GetLastSaveStats() const { return last_stats_; }

private:
    SaveStats last_stats_;

    // Core RDB writing functions
    bool WriteMagicNumber(FILE* file);
    bool WriteVersion(FILE* file);
    bool WriteSelector(FILE* file, int db_number);
    bool WriteResizeTable(FILE* file, size_t table_size);
    bool WriteKeyValuePair(FILE* file, const std::string& key, const std::string& value);
    bool WriteEndOfFile(FILE* file);
    bool WriteChecksum(FILE* file, const std::vector<uint8_t>& data);

    // RDB value encoding
    bool WriteLength(FILE* file, size_t length);
    bool WriteString(FILE* file, const std::string& str);
    bool WriteInteger(FILE* file, int64_t value);

    // Compression utilities
    std::vector<uint8_t> CompressData(const std::vector<uint8_t>& data,
                                     CompressionType type);
    std::vector<uint8_t> DecompressData(const std::vector<uint8_t>& data,
                                       CompressionType type);

    // Checksum calculation
    uint64_t CalculateCRC64(const uint8_t* data, size_t length);

    // Utility functions
    std::string GetTimestamp();
    bool EnsureDirectoryExists(const std::string& filepath);
};