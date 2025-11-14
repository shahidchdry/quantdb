#include "string_value.h"
#include <absl/strings/numbers.h>
#include <cstring>

StringValue::StringValue() : type_(Type::PLAIN_STRING), value_(std::string{}), has_expiry_(false) {}

StringValue::StringValue(const std::string& value)
    : type_(Type::PLAIN_STRING), value_(value), has_expiry_(false) {}

StringValue::StringValue(int64_t value)
    : type_(Type::INTEGER), value_(value), has_expiry_(false) {}

StringValue::StringValue(const std::string& value, std::chrono::milliseconds ttl)
    : type_(Type::EXPIRING_STRING), value_(value), has_expiry_(true) {
    expiry_time_ = std::chrono::system_clock::now() + ttl;
}

bool StringValue::IsExpired() const {
    if (!has_expiry_) {
        return false;
    }
    return std::chrono::system_clock::now() >= expiry_time_;
}

std::string StringValue::AsString() const {
    if (IsExpired()) {
        return "";
    }

    if (type_ == Type::INTEGER) {
        return std::to_string(std::get<int64_t>(value_));
    }
    return std::get<std::string>(value_);
}

std::optional<int64_t> StringValue::AsInteger() const {
    if (IsExpired()) {
        return std::nullopt;
    }

    if (type_ == Type::INTEGER) {
        return std::get<int64_t>(value_);
    }

    // Try to parse plain string as integer
    const std::string& str = std::get<std::string>(value_);
    int64_t parsed_value;
    if (absl::SimpleAtoi(str, &parsed_value)) {
        return parsed_value;
    }

    return std::nullopt;
}

void StringValue::SetExpiry(std::chrono::milliseconds ttl) {
    expiry_time_ = std::chrono::system_clock::now() + ttl;
    has_expiry_ = true;
    if (type_ == Type::PLAIN_STRING) {
        type_ = Type::EXPIRING_STRING;
    }
}

std::chrono::milliseconds StringValue::TTL() const {
    if (!has_expiry_) {
        return std::chrono::milliseconds(-1);
    }

    auto now = std::chrono::system_clock::now();
    if (now >= expiry_time_) {
        return std::chrono::milliseconds(0);
    }

    return std::chrono::duration_cast<std::chrono::milliseconds>(expiry_time_ - now);
}

void StringValue::ClearExpiry() {
    has_expiry_ = false;
    if (type_ == Type::EXPIRING_STRING) {
        type_ = Type::PLAIN_STRING;
    }
}

size_t StringValue::SerializedSize() const {
    size_t size = sizeof(Type) + sizeof(bool);
    if (has_expiry_) {
        size += sizeof(int64_t); // expiry time as timestamp
    }

    if (type_ == Type::INTEGER) {
        size += sizeof(int64_t);
    } else {
        const std::string& str = std::get<std::string>(value_);
        size += sizeof(uint32_t) + str.size(); // length + string data
    }

    return size;
}

void StringValue::Serialize(uint8_t* buffer) const {
    size_t offset = 0;

    // Serialize type
    std::memcpy(buffer + offset, &type_, sizeof(Type));
    offset += sizeof(Type);

    // Serialize expiry flag
    std::memcpy(buffer + offset, &has_expiry_, sizeof(bool));
    offset += sizeof(bool);

    // Serialize expiry time if present
    if (has_expiry_) {
        auto timestamp = expiry_time_.time_since_epoch().count();
        std::memcpy(buffer + offset, &timestamp, sizeof(int64_t));
        offset += sizeof(int64_t);
    }

    // Serialize value
    if (type_ == Type::INTEGER) {
        int64_t int_val = std::get<int64_t>(value_);
        std::memcpy(buffer + offset, &int_val, sizeof(int64_t));
    } else {
        const std::string& str = std::get<std::string>(value_);
        uint32_t str_len = static_cast<uint32_t>(str.size());
        std::memcpy(buffer + offset, &str_len, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        std::memcpy(buffer + offset, str.data(), str.size());
    }
}

StringValue StringValue::Deserialize(const uint8_t* buffer, size_t size) {
    size_t offset = 0;

    // Deserialize type
    Type type;
    std::memcpy(&type, buffer + offset, sizeof(Type));
    offset += sizeof(Type);

    // Deserialize expiry flag
    bool has_expiry;
    std::memcpy(&has_expiry, buffer + offset, sizeof(bool));
    offset += sizeof(bool);

    StringValue result;

    if (has_expiry && offset + sizeof(int64_t) <= size) {
        int64_t timestamp;
        std::memcpy(&timestamp, buffer + offset, sizeof(int64_t));
        offset += sizeof(int64_t);
        result.has_expiry_ = true;
        result.expiry_time_ = std::chrono::system_clock::time_point(
            std::chrono::system_clock::duration(timestamp));
    }

    // Deserialize value
    if (type == Type::INTEGER && offset + sizeof(int64_t) <= size) {
        int64_t int_val;
        std::memcpy(&int_val, buffer + offset, sizeof(int64_t));
        result.value_ = int_val;
        result.type_ = Type::INTEGER;
    } else if (offset + sizeof(uint32_t) <= size) {
        uint32_t str_len;
        std::memcpy(&str_len, buffer + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        if (offset + str_len <= size) {
            std::string str(reinterpret_cast<const char*>(buffer + offset), str_len);
            result.value_ = str;
            result.type_ = has_expiry ? Type::EXPIRING_STRING : Type::PLAIN_STRING;
        }
    }

    return result;
}