#pragma once

#include <string>
#include <vector>
#include <optional>

class RespSerializer {
public:
    // Serialize individual RESP types
    static std::string SerializeSimpleString(const std::string& value);
    static std::string SerializeBulkString(const std::string& value);
    static std::string SerializeInteger(int64_t value);
    static std::string SerializeArray(const std::vector<std::string>& values);
    static std::string SerializeError(const std::string& message);
    static std::string SerializeNull();

    // Convenience methods for common responses
    static std::string SerializeOK();
    static std::string SerializePong();
    static std::string SerializeZero();
    static std::string SerializeOne();
    static std::string SerializeNil();

    // Serialize status responses
    static std::string SerializeStatus(const std::string& status);
    static std::string SerializeIntegerStatus(int64_t value);

    // Serialize error responses with error codes
    static std::string SerializeWrongArgsError(const std::string& command);
    static std::string SerializeUnknownCommandError(const std::string& command);
    static std::string SerializeIntegerError();
    static std::string SerializeSyntaxError();

private:
    static std::string AppendCRLF(const std::string& data);
};