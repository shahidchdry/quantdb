#pragma once

#include "../storage/swiss_table_store.h"
#include <string>
#include <fstream>
#include <vector>

class RdbReader {
public:
    enum class LoadResult {
        SUCCESS = 0,
        FILE_NOT_FOUND = 1,
        INVALID_FORMAT = 2,
        VERSION_MISMATCH = 3,
        CHECKSUM_ERROR = 4,
        CORRUPTED_DATA = 5,
        IO_ERROR = 6
    };

private:
    static const uint32_t SUPPORTED_VERSION_MIN = 1;
    static const uint32_t SUPPORTED_VERSION_MAX = 9;

public:
    RdbReader() = default;
    ~RdbReader() = default;

    // Load database from RDB file
    LoadResult LoadFromFile(const std::string& filename, SwissTableStore& store);

    // Load specific database (for future multi-database support)
    LoadResult LoadDatabaseFromFile(const std::string& filename, SwissTableStore& store,
                                   int target_db_number = 0);

    // Get load statistics
    struct LoadStats {
        size_t keys_loaded = 0;
        size_t total_size = 0;
        std::chrono::milliseconds load_time{0};
        uint32_t rdb_version = 0;
    };

    const LoadStats& GetLastLoadStats() const { return last_stats_; }

    // Validate RDB file without loading
    LoadResult ValidateFile(const std::string& filename);

private:
    LoadStats last_stats_;

    // Core RDB reading functions
    bool ReadMagicNumber(FILE* file);
    bool ReadVersion(FILE* file);
    bool ReadDatabase(FILE* file, SwissTableStore& store, int target_db_number);
    bool ReadKeyValuePair(FILE* file, SwissTableStore& store);
    bool VerifyChecksum(FILE* file, const std::vector<uint8_t>& data);

    // RDB value decoding
    size_t ReadLength(FILE* file);
    std::string ReadString(FILE* file, size_t expected_length = 0);
    int64_t ReadInteger(FILE* file);

    // Error handling
    std::string GetLoadResultString(LoadResult result);

    // Utility functions
    bool IsEndOfFile(FILE* file);
    uint64_t CalculateCRC64(const uint8_t* data, size_t length);
    std::vector<uint8_t> ReadFileData(FILE* file);
};