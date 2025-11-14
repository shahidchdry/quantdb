#pragma once

#include <string>
#include <variant>
#include <chrono>

class StringValue {
public:
    enum class Type {
        PLAIN_STRING,
        INTEGER,
        EXPIRING_STRING
    };

private:
    std::variant<std::string, int64_t> value_;
    Type type_;
    std::chrono::system_clock::time_point expiry_time_;
    bool has_expiry_;

public:
    StringValue();
    explicit StringValue(const std::string& value);
    explicit StringValue(int64_t value);
    StringValue(const std::string& value, std::chrono::milliseconds ttl);

    Type GetType() const { return type_; }
    bool IsExpired() const;
    bool HasExpiry() const { return has_expiry_; }

    std::string AsString() const;
    std::optional<int64_t> AsInteger() const;

    void SetExpiry(std::chrono::milliseconds ttl);
    std::chrono::milliseconds TTL() const;
    void ClearExpiry();

    // For serialization
    size_t SerializedSize() const;
    void Serialize(uint8_t* buffer) const;
    static StringValue Deserialize(const uint8_t* buffer, size_t size);
};