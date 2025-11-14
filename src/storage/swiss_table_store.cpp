#include "swiss_table_store.h"
#include <absl/strings/numbers.h>
#include <absl/strings/string_view.h>
#include <sstream>
#include <algorithm>

bool SwissTableStore::Set(const std::string& key, const std::string& value) {
    std::unique_lock lock(mutex_);
    data_[key] = value;
    return true;
}

std::optional<std::string> SwissTableStore::Get(const std::string& key) const {
    std::shared_lock lock(mutex_);
    auto it = data_.find(key);
    if (it != data_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool SwissTableStore::Delete(const std::string& key) {
    std::unique_lock lock(mutex_);
    return data_.erase(key) > 0;
}

bool SwissTableStore::Exists(const std::string& key) const {
    std::shared_lock lock(mutex_);
    return data_.find(key) != data_.end();
}

int64_t SwissTableStore::Increment(const std::string& key, int64_t delta) {
    std::unique_lock lock(mutex_);

    auto it = data_.find(key);
    if (it != data_.end()) {
        // Key exists, try to parse and increment
        auto current_value = ParseInt64(it->second);
        if (current_value.has_value()) {
            // Check for overflow/underflow
            if ((delta > 0 && current_value.value() > INT64_MAX - delta) ||
                (delta < 0 && current_value.value() < INT64_MIN - delta)) {
                // Overflow/underflow occurred, wrap around like Redis
                int64_t new_value = static_cast<int64_t>(
                    (static_cast<uint64_t>(current_value.value()) + static_cast<uint64_t>(delta)) & 0xFFFFFFFFFFFFFFFF);
                it->second = Int64ToString(new_value);
                return new_value;
            }

            int64_t new_value = current_value.value() + delta;
            it->second = Int64ToString(new_value);
            return new_value;
        } else {
            // Existing value is not a number, this is an error
            throw std::invalid_argument("Value is not an integer");
        }
    } else {
        // Key doesn't exist, create it with delta value
        it = data_.emplace(key, Int64ToString(delta)).first;
        return delta;
    }
}

size_t SwissTableStore::Size() const {
    std::shared_lock lock(mutex_);
    return data_.size();
}

void SwissTableStore::Clear() {
    std::unique_lock lock(mutex_);
    data_.clear();
}

std::vector<std::string> SwissTableStore::Keys() const {
    std::shared_lock lock(mutex_);
    std::vector<std::string> keys;
    keys.reserve(data_.size());

    for (const auto& pair : data_) {
        keys.push_back(pair.first);
    }

    return keys;
}

size_t SwissTableStore::MemoryUsage() const {
    std::shared_lock lock(mutex_);

    // Estimate memory usage
    size_t usage = sizeof(SwissTableStore);
    usage += sizeof(absl::flat_hash_map<std::string, std::string>);

    // Add size of keys and values
    for (const auto& pair : data_) {
        usage += pair.first.size() + pair.second.size();
        // Add overhead for map nodes (estimated)
        usage += sizeof(std::string) * 2 + 16; // approximate node overhead
    }

    return usage;
}

bool SwissTableStore::SetBatch(const std::vector<std::pair<std::string, std::string>>& key_values) {
    std::unique_lock lock(mutex_);
    for (const auto& kv : key_values) {
        data_[kv.first] = kv.second;
    }
    return true;
}

std::vector<std::optional<std::string>> SwissTableStore::GetBatch(const std::vector<std::string>& keys) const {
    std::shared_lock lock(mutex_);
    std::vector<std::optional<std::string>> results;
    results.reserve(keys.size());

    for (const auto& key : keys) {
        auto it = data_.find(key);
        if (it != data_.end()) {
            results.push_back(it->second);
        } else {
            results.push_back(std::nullopt);
        }
    }

    return results;
}

std::optional<int64_t> SwissTableStore::ParseInt64(const std::string& str) const {
    int64_t value;
    if (absl::SimpleAtoi(str, &value)) {
        return value;
    }
    return std::nullopt;
}

std::string SwissTableStore::Int64ToString(int64_t value) const {
    return std::to_string(value);
}