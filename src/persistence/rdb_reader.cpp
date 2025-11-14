#include "rdb_reader.h"
#include <chrono>
#include <iostream>
#include <cstring>

RdbReader::LoadResult RdbReader::LoadFromFile(const std::string& filename, SwissTableStore& store) {
    return LoadDatabaseFromFile(filename, store, 0);
}

RdbReader::LoadResult RdbReader::LoadDatabaseFromFile(const std::string& filename, SwissTableStore& store,
                                                     int target_db_number) {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Reset statistics
    last_stats_ = LoadStats{};

    FILE* file = fopen(filename.c_str(), "rb");
    if (!file) {
        return LoadResult::FILE_NOT_FOUND;
    }

    LoadResult result = LoadResult::SUCCESS;

    try {
        // Read and verify RDB header
        if (!ReadMagicNumber(file)) {
            result = LoadResult::INVALID_FORMAT;
            throw std::runtime_error("Invalid RDB magic number");
        }

        if (!ReadVersion(file)) {
            result = LoadResult::VERSION_MISMATCH;
            throw std::runtime_error("Unsupported RDB version");
        }

        // Read file data for checksum verification
        std::vector<uint8_t> file_data = ReadFileData(file);
        if (file_data.empty()) {
            result = LoadResult::IO_ERROR;
            throw std::runtime_error("Failed to read file data");
        }

        // Reset file position for reading database data
        fseek(file, 0, SEEK_SET);

        // Skip header (magic number + version)
        fseek(file, 9, SEEK_SET);

        // Read database content
        if (!ReadDatabase(file, store, target_db_number)) {
            result = LoadResult::CORRUPTED_DATA;
            throw std::runtime_error("Failed to read database content");
        }

        // Verify checksum
        if (!VerifyChecksum(file, file_data)) {
            result = LoadResult::CHECKSUM_ERROR;
            throw std::runtime_error("Checksum verification failed");
        }

        // Update statistics
        fseek(file, 0, SEEK_END);
        last_stats_.total_size = static_cast<size_t>(ftell(file));
        auto end_time = std::chrono::high_resolution_clock::now();
        last_stats_.load_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        std::cout << "Successfully loaded " << last_stats_.keys_loaded << " keys from "
                  << filename << " (" << last_stats_.total_size << " bytes) in "
                  << last_stats_.load_time.count() << "ms" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error during RDB load: " << e.what() << std::endl;
        // Store remains unchanged on error
    }

    fclose(file);
    return result;
}

RdbReader::LoadResult RdbReader::ValidateFile(const std::string& filename) {
    SwissTableStore dummy_store;  // Dummy store for validation
    return LoadFromFile(filename, dummy_store);
}

bool RdbReader::ReadMagicNumber(FILE* file) {
    char magic[6];
    size_t read = fread(magic, 1, 5, file);  // "REDIS" is 5 characters
    if (read != 5) {
        return false;
    }

    magic[5] = '\0';
    return strcmp(magic, "REDIS") == 0;
}

bool RdbReader::ReadVersion(FILE* file) {
    uint32_t version_be;
    size_t read = fread(&version_be, sizeof(uint32_t), 1, file);
    if (read != 1) {
        return false;
    }

    uint32_t version = be32toh(version_be);
    last_stats_.rdb_version = version;

    return (version >= SUPPORTED_VERSION_MIN && version <= SUPPORTED_VERSION_MAX);
}

bool RdbReader::ReadDatabase(FILE* file, SwissTableStore& store, int target_db_number) {
    while (!IsEndOfFile(file)) {
        uint8_t opcode;
        size_t read = fread(&opcode, sizeof(uint8_t), 1, file);
        if (read != 1) {
            break;
        }

        if (opcode == 0xFF) {
            // End of file
            break;
        } else if (opcode == 0xFE) {
            // Database selector
            int db_number = static_cast<int>(ReadLength(file));
            if (db_number != target_db_number) {
                // Skip this database (for future multi-database support)
                continue;
            }

            // Skip resize table information
            ReadLength(file);  // Hash table size
            ReadLength(file);  // Expire table size

        } else if (opcode == 0) {
            // String value
            if (!ReadKeyValuePair(file, store)) {
                return false;
            }
        } else {
            // Unsupported value type, skip
            std::cerr << "Warning: Unsupported value type: " << static_cast<int>(opcode) << std::endl;
            return false;
        }
    }

    return true;
}

bool RdbReader::ReadKeyValuePair(FILE* file, SwissTableStore& store) {
    // Read key
    std::string key = ReadString(file);
    if (key.empty() && IsEndOfFile(file)) {
        return false;
    }

    // Read value
    std::string value = ReadString(file);

    // Store the key-value pair
    store.Set(key, value);
    last_stats_.keys_loaded++;

    return true;
}

bool RdbReader::VerifyChecksum(FILE* file, const std::vector<uint8_t>& file_data) {
    // Move to end of file to read checksum
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);

    if (file_size < 8) {
        return false;  // File is too small to have a checksum
    }

    // Read checksum from end of file
    fseek(file, -8, SEEK_END);
    uint64_t stored_checksum_be;
    size_t read = fread(&stored_checksum_be, sizeof(uint64_t), 1, file);
    if (read != 1) {
        return false;
    }

    uint64_t stored_checksum = be64toh(stored_checksum_be);

    // Calculate checksum of all data except the checksum itself
    size_t data_size = file_data.size() - 8;
    uint64_t calculated_checksum = CalculateCRC64(file_data.data(), data_size);

    return stored_checksum == calculated_checksum;
}

size_t RdbReader::ReadLength(FILE* file) {
    uint8_t first_byte;
    size_t read = fread(&first_byte, sizeof(uint8_t), 1, file);
    if (read != 1) {
        return 0;
    }

    // Check the 2 MSB to determine encoding
    if ((first_byte & 0xC0) == 0x00) {
        // 6-bit length
        return first_byte & 0x3F;
    } else if ((first_byte & 0xC0) == 0x40) {
        // 14-bit length
        uint8_t second_byte;
        read = fread(&second_byte, sizeof(uint8_t), 1, file);
        if (read != 1) {
            return 0;
        }
        return ((first_byte & 0x3F) << 8) | second_byte;
    } else if ((first_byte & 0xC0) == 0x80) {
        // 32-bit length
        uint8_t buffer[4];
        read = fread(buffer, 1, 4, file);
        if (read != 4) {
            return 0;
        }
        uint32_t length_be;
        memcpy(&length_be, buffer, 4);
        return be32toh(length_be);
    } else {
        // Special encoding (not used for lengths in this implementation)
        return 0;
    }
}

std::string RdbReader::ReadString(FILE* file, size_t expected_length) {
    size_t length;
    if (expected_length > 0) {
        length = expected_length;
    } else {
        length = ReadLength(file);
    }

    if (length == 0) {
        return "";
    }

    if (length > 1024 * 1024) {  // 1MB limit for safety
        std::cerr << "Warning: Very large string encountered: " << length << " bytes" << std::endl;
    }

    std::vector<char> buffer(length);
    size_t read = fread(buffer.data(), 1, length, file);
    if (read != length) {
        return "";
    }

    return std::string(buffer.begin(), buffer.end());
}

int64_t RdbReader::ReadInteger(FILE* file) {
    uint8_t encoding;
    size_t read = fread(&encoding, sizeof(uint8_t), 1, file);
    if (read != 1) {
        return 0;
    }

    if (encoding == 0xC3) {
        // 8-bit integer
        int8_t value;
        read = fread(&value, sizeof(int8_t), 1, file);
        return read == 1 ? value : 0;
    } else if (encoding == 0xC2) {
        // 16-bit integer
        int16_t value_be;
        read = fread(&value_be, sizeof(int16_t), 1, file);
        return read == 1 ? be16toh(value_be) : 0;
    } else if (encoding == 0xC1) {
        // 32-bit integer
        int32_t value_be;
        read = fread(&value_be, sizeof(int32_t), 1, file);
        return read == 1 ? be32toh(value_be) : 0;
    } else if (encoding == 0xC4) {
        // 64-bit integer
        int64_t value_be;
        read = fread(&value_be, sizeof(int64_t), 1, file);
        return read == 1 ? be64toh(value_be) : 0;
    } else if ((encoding & 0xC0) == 0xC0) {
        // Small integer (0-12)
        return static_cast<int64_t>(encoding & 0x3F);
    } else {
        // Not an integer encoding
        fseek(file, -1, SEEK_CUR);  // Put the byte back
        return 0;
    }
}

bool RdbReader::IsEndOfFile(FILE* file) {
    int current_pos = ftell(file);
    fseek(file, 0, SEEK_END);
    int end_pos = ftell(file);
    fseek(file, current_pos, SEEK_SET);
    return current_pos >= end_pos;
}

uint64_t RdbReader::CalculateCRC64(const uint8_t* data, size_t length) {
    // Same implementation as in RdbWriter
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

std::vector<uint8_t> RdbReader::ReadFileData(FILE* file) {
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> data(file_size);
    size_t read = fread(data.data(), 1, file_size, file);

    if (read != static_cast<size_t>(file_size)) {
        return std::vector<uint8_t>();
    }

    return data;
}