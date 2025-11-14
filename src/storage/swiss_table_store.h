#pragma once

#include <absl/container/flat_hash_map.h>
#include <string>
#include <optional>
#include <vector>
#include <shared_mutex>
#include <cstdint>

class SwissTableStore {
private:
    absl::flat_hash_map<std::string, std::string> data_;
    mutable std::shared_mutex mutex_; // Read-write lock for thread safety

public:
    // Core operations
    bool Set(const std::string& key, const std::string& value);
    std::optional<std::string> Get(const std::string& key) const;
    bool Delete(const std::string& key);
    bool Exists(const std::string& key) const;

    // Atomic operations
    int64_t Increment(const std::string& key, int64_t delta = 1);

    // Utility operations
    size_t Size() const;
    void Clear();

    // Iteration support
    std::vector<std::string> Keys() const;

    // Memory usage statistics
    size_t MemoryUsage() const;

    // Batch operations for performance
    bool SetBatch(const std::vector<std::pair<std::string, std::string>>& key_values);
    std::vector<std::optional<std::string>> GetBatch(const std::vector<std::string>& keys) const;

private:
    // Helper method for string to integer conversion with error handling
    std::optional<int64_t> ParseInt64(const std::string& str) const;
    std::string Int64ToString(int64_t value) const;
};