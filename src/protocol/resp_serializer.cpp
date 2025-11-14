#include "resp_serializer.h"

std::string RespSerializer::SerializeSimpleString(const std::string& value) {
    return "+" + AppendCRLF(value);
}

std::string RespSerializer::SerializeBulkString(const std::string& value) {
    if (value.empty()) {
        return "$0\r\n\r\n";
    }
    return "$" + std::to_string(value.length()) + "\r\n" + value + "\r\n";
}

std::string RespSerializer::SerializeInteger(int64_t value) {
    return ":" + std::to_string(value) + "\r\n";
}

std::string RespSerializer::SerializeArray(const std::vector<std::string>& values) {
    std::string result = "*" + std::to_string(values.size()) + "\r\n";
    for (const auto& value : values) {
        result += SerializeBulkString(value);
    }
    return result;
}

std::string RespSerializer::SerializeError(const std::string& message) {
    return "-" + message + "\r\n";
}

std::string RespSerializer::SerializeNull() {
    return "$-1\r\n";
}

std::string RespSerializer::SerializeOK() {
    return "+OK\r\n";
}

std::string RespSerializer::SerializePong() {
    return "+PONG\r\n";
}

std::string RespSerializer::SerializeZero() {
    return ":0\r\n";
}

std::string RespSerializer::SerializeOne() {
    return ":1\r\n";
}

std::string RespSerializer::SerializeNil() {
    return "$-1\r\n";
}

std::string RespSerializer::SerializeStatus(const std::string& status) {
    return "+" + status + "\r\n";
}

std::string RespSerializer::SerializeIntegerStatus(int64_t value) {
    return ":" + std::to_string(value) + "\r\n";
}

std::string RespSerializer::SerializeWrongArgsError(const std::string& command) {
    return "-ERR wrong number of arguments for '" + command + "' command\r\n";
}

std::string RespSerializer::SerializeUnknownCommandError(const std::string& command) {
    return "-ERR unknown command '" + command + "'\r\n";
}

std::string RespSerializer::SerializeIntegerError() {
    return "-ERR value is not an integer or out of range\r\n";
}

std::string RespSerializer::SerializeSyntaxError() {
    return "-ERR syntax error\r\n";
}

std::string RespSerializer::AppendCRLF(const std::string& data) {
    return data + "\r\n";
}