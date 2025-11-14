#include "resp_parser.h"
#include <algorithm>
#include <stdexcept>
#include <cstring>

RespParser::RespParser() : state_(ParseState::EXPECTING_TYPE), expected_length_(0), parsed_elements_(0) {}

std::unique_ptr<RespParser::ParseResult> RespParser::Parse(const char* data, size_t length) {
    size_t consumed = 0;

    while (consumed < length && !current_result_) {
        if (state_ == ParseState::EXPECTING_TYPE) {
            if (length > 0) {
                switch (data[consumed]) {
                    case '+':
                        current_result_ = ParseSimpleString(data + consumed + 1, length - consumed - 1, consumed);
                        break;
                    case '$':
                        current_result_ = ParseBulkString(data + consumed + 1, length - consumed - 1, consumed);
                        break;
                    case ':':
                        current_result_ = ParseInteger(data + consumed + 1, length - consumed - 1, consumed);
                        break;
                    case '-':
                        current_result_ = ParseError(data + consumed + 1, length - consumed - 1, consumed);
                        break;
                    case '*':
                        current_result_ = ParseArray(data + consumed + 1, length - consumed - 1, consumed);
                        break;
                    default:
                        // Invalid RESP type
                        auto error = std::make_unique<ParseResult>();
                        error->type = ParseResult::ERROR;
                        error->value = "Invalid RESP protocol type";
                        error->complete = true;
                        return error;
                }
                consumed++; // Consume the type character
            } else {
                break;
            }
        } else {
            // Should not reach here in normal operation
            break;
        }
    }

    if (current_result_) {
        current_result_->bytes_consumed = consumed;
    }

    return std::move(current_result_);
}

std::unique_ptr<RespParser::ParseResult> RespParser::ParseSimpleString(const char* data, size_t length, size_t& consumed) {
    auto result = std::make_unique<ParseResult>();
    result->type = ParseResult::SIMPLE_STRING;

    size_t crlf_pos = FindCRLF(data, length);
    if (crlf_pos == std::string::npos) {
        // Incomplete data
        result->complete = false;
        consumed = 0;
        return result;
    }

    result->value.assign(data, crlf_pos);
    result->complete = true;
    consumed = crlf_pos + 2; // +2 for \r\n

    return result;
}

std::unique_ptr<RespParser::ParseResult> RespParser::ParseBulkString(const char* data, size_t length, size_t& consumed) {
    auto result = std::make_unique<ParseResult>();
    result->type = ParseResult::BULK_STRING;

    size_t crlf_pos = FindCRLF(data, length);
    if (crlf_pos == std::string::npos) {
        // Incomplete length
        result->complete = false;
        consumed = 0;
        return result;
    }

    // Parse length
    std::string length_str(data, crlf_pos);
    try {
        expected_length_ = std::stoull(length_str);
    } catch (const std::exception&) {
        auto error = std::make_unique<ParseResult>();
        error->type = ParseResult::ERROR;
        error->value = "Invalid bulk string length";
        error->complete = true;
        consumed = crlf_pos + 2;
        return error;
    }

    if (expected_length_ == static_cast<size_t>(-1)) {
        // Null bulk string
        result->type = ParseResult::NULL_VALUE;
        result->complete = true;
        consumed = crlf_pos + 2;
        return result;
    }

    size_t total_needed = crlf_pos + 2 + expected_length_ + 2; // type + length + CRLF + data + CRLF
    if (length < total_needed) {
        // Incomplete data
        result->complete = false;
        consumed = 0;
        return result;
    }

    const char* string_start = data + crlf_pos + 2;
    result->value.assign(string_start, expected_length_);
    result->complete = true;
    consumed = total_needed;

    return result;
}

std::unique_ptr<RespParser::ParseResult> RespParser::ParseInteger(const char* data, size_t length, size_t& consumed) {
    auto result = std::make_unique<ParseResult>();
    result->type = ParseResult::INTEGER;

    size_t crlf_pos = FindCRLF(data, length);
    if (crlf_pos == std::string::npos) {
        // Incomplete data
        result->complete = false;
        consumed = 0;
        return result;
    }

    result->value.assign(data, crlf_pos);
    result->complete = true;
    consumed = crlf_pos + 2;

    return result;
}

std::unique_ptr<RespParser::ParseResult> RespParser::ParseError(const char* data, size_t length, size_t& consumed) {
    auto result = std::make_unique<ParseResult>();
    result->type = ParseResult::ERROR;

    size_t crlf_pos = FindCRLF(data, length);
    if (crlf_pos == std::string::npos) {
        // Incomplete data
        result->complete = false;
        consumed = 0;
        return result;
    }

    result->value.assign(data, crlf_pos);
    result->complete = true;
    consumed = crlf_pos + 2;

    return result;
}

std::unique_ptr<RespParser::ParseResult> RespParser::ParseArray(const char* data, size_t length, size_t& consumed) {
    auto result = std::make_unique<ParseResult>();
    result->type = ParseResult::ARRAY;

    size_t crlf_pos = FindCRLF(data, length);
    if (crlf_pos == std::string::npos) {
        // Incomplete length
        result->complete = false;
        consumed = 0;
        return result;
    }

    // Parse array length
    std::string length_str(data, crlf_pos);
    try {
        expected_length_ = std::stoull(length_str);
    } catch (const std::exception&) {
        auto error = std::make_unique<ParseResult>();
        error->type = ParseResult::ERROR;
        error->value = "Invalid array length";
        error->complete = true;
        consumed = crlf_pos + 2;
        return error;
    }

    if (expected_length_ == 0) {
        // Empty array
        result->complete = true;
        consumed = crlf_pos + 2;
        return result;
    }

    // For simplicity, we'll implement basic array parsing
    // In a full implementation, we'd need recursive parsing
    // For now, return a simple array marker
    result->complete = true;
    consumed = crlf_pos + 2;

    // Note: Full array parsing implementation would require recursive state management
    // This is a simplified version that marks arrays as complete after reading length

    return result;
}

size_t RespParser::FindCRLF(const char* data, size_t length) const {
    for (size_t i = 0; i < length - 1; ++i) {
        if (data[i] == '\r' && data[i + 1] == '\n') {
            return i;
        }
    }
    return std::string::npos;
}

void RespParser::Reset() {
    state_ = ParseState::EXPECTING_TYPE;
    current_result_.reset();
    expected_length_ = 0;
    parsed_elements_ = 0;
    buffer_.clear();
}