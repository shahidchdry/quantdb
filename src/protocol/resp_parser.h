#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>

class RespParser {
public:
    struct ParseResult {
        enum Type {
            SIMPLE_STRING,
            BULK_STRING,
            INTEGER,
            ARRAY,
            ERROR,
            NULL_VALUE
        };

        Type type;
        std::string value;
        std::vector<std::unique_ptr<ParseResult>> array_values;
        bool complete = false;
        size_t bytes_consumed = 0;
    };

private:
    enum class ParseState {
        EXPECTING_TYPE,
        PARSING_SIMPLE_STRING,
        PARSING_BULK_STRING_LENGTH,
        PARSING_BULK_STRING_DATA,
        PARSING_INTEGER,
        PARSING_ERROR,
        PARSING_ARRAY_LENGTH,
        PARSING_ARRAY_ELEMENTS
    };

    ParseState state_;
    std::unique_ptr<ParseResult> current_result_;
    size_t expected_length_;
    size_t parsed_elements_;
    std::string buffer_;

public:
    RespParser();

    // Parse data from buffer
    std::unique_ptr<ParseResult> Parse(const char* data, size_t length);

    // Reset parser state
    void Reset();

    // Check if parser is ready for new data
    bool IsReady() const { return state_ == ParseState::EXPECTING_TYPE; }

private:
    std::unique_ptr<ParseResult> ParseSimpleString(const char* data, size_t length, size_t& consumed);
    std::unique_ptr<ParseResult> ParseBulkString(const char* data, size_t length, size_t& consumed);
    std::unique_ptr<ParseResult> ParseInteger(const char* data, size_t length, size_t& consumed);
    std::unique_ptr<ParseResult> ParseError(const char* data, size_t length, size_t& consumed);
    std::unique_ptr<ParseResult> ParseArray(const char* data, size_t length, size_t& consumed);

    size_t FindCRLF(const char* data, size_t length) const;
};