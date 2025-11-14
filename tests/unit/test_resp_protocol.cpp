#include <gtest/gtest.h>
#include "../../src/protocol/resp_parser.h"
#include "../../src/protocol/resp_serializer.h"

class RespProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser_ = std::make_unique<RespParser>();
    }

    void TearDown() override {
        parser_.reset();
    }

    std::unique_ptr<RespParser> parser_;
};

TEST_F(RespProtocolTest, SimpleStringParsing) {
    const char* data = "+OK\r\n";
    auto result = parser_->Parse(data, strlen(data));

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->type, RespParser::ParseResult::SIMPLE_STRING);
    EXPECT_TRUE(result->complete);
    EXPECT_EQ(result->value, "OK");
}

TEST_F(RespProtocolTest, BulkStringParsing) {
    const char* data = "$5\r\nhello\r\n";
    auto result = parser_->Parse(data, strlen(data));

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->type, RespParser::ParseResult::BULK_STRING);
    EXPECT_TRUE(result->complete);
    EXPECT_EQ(result->value, "hello");
}

TEST_F(RespProtocolTest, NullBulkStringParsing) {
    const char* data = "$-1\r\n";
    auto result = parser_->Parse(data, strlen(data));

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->type, RespParser::ParseResult::NULL_VALUE);
    EXPECT_TRUE(result->complete);
}

TEST_F(RespProtocolTest, EmptyBulkStringParsing) {
    const char* data = "$0\r\n\r\n";
    auto result = parser_->Parse(data, strlen(data));

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->type, RespParser::ParseResult::BULK_STRING);
    EXPECT_TRUE(result->complete);
    EXPECT_EQ(result->value, "");
}

TEST_F(RespProtocolTest, IntegerParsing) {
    const char* data = ":42\r\n";
    auto result = parser_->Parse(data, strlen(data));

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->type, RespParser::ParseResult::INTEGER);
    EXPECT_TRUE(result->complete);
    EXPECT_EQ(result->value, "42");
}

TEST_F(RespProtocolTest, NegativeIntegerParsing) {
    const char* data = ":-123\r\n";
    auto result = parser_->Parse(data, strlen(data));

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->type, RespParser::ParseResult::INTEGER);
    EXPECT_TRUE(result->complete);
    EXPECT_EQ(result->value, "-123");
}

TEST_F(RespProtocolTest, ErrorParsing) {
    const char* data = "-Error message\r\n";
    auto result = parser_->Parse(data, strlen(data));

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->type, RespParser::ParseResult::ERROR);
    EXPECT_TRUE(result->complete);
    EXPECT_EQ(result->value, "Error message");
}

TEST_F(RespProtocolTest, ArrayParsing) {
    const char* data = "*2\r\n$3\r\nGET\r\n$3\r\nkey\r\n";
    auto result = parser_->Parse(data, strlen(data));

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->type, RespParser::ParseResult::ARRAY);
    EXPECT_TRUE(result->complete);
    // Note: Array parsing is simplified in this implementation
}

TEST_F(RespProtocolTest, IncompleteDataParsing) {
    const char* data = "+OK";  // Missing \r\n
    auto result = parser_->Parse(data, strlen(data));

    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->complete);
}

TEST_F(RespProtocolTest, InvalidRESPType) {
    const char* data = "Xinvalid\r\n";
    auto result = parser_->Parse(data, strlen(data));

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->type, RespParser::ParseResult::ERROR);
    EXPECT_TRUE(result->complete);
    EXPECT_EQ(result->value, "Invalid RESP protocol type");
}

// Serializer Tests
TEST_F(RespProtocolTest, SimpleStringSerialization) {
    std::string serialized = RespSerializer::SerializeSimpleString("OK");
    EXPECT_EQ(serialized, "+OK\r\n");
}

TEST_F(RespProtocolTest, BulkStringSerialization) {
    std::string serialized = RespSerializer::SerializeBulkString("hello");
    EXPECT_EQ(serialized, "$5\r\nhello\r\n");
}

TEST_F(RespProtocolTest, EmptyBulkStringSerialization) {
    std::string serialized = RespSerializer::SerializeBulkString("");
    EXPECT_EQ(serialized, "$0\r\n\r\n");
}

TEST_F(RespProtocolTest, IntegerSerialization) {
    std::string serialized = RespSerializer::SerializeInteger(42);
    EXPECT_EQ(serialized, ":42\r\n");
}

TEST_F(RespProtocolTest, NegativeIntegerSerialization) {
    std::string serialized = RespSerializer::SerializeInteger(-123);
    EXPECT_EQ(serialized, ":-123\r\n");
}

TEST_F(RespProtocolTest, ArraySerialization) {
    std::vector<std::string> values = {"GET", "key"};
    std::string serialized = RespSerializer::SerializeArray(values);
    EXPECT_EQ(serialized, "*2\r\n$3\r\nGET\r\n$3\r\nkey\r\n");
}

TEST_F(RespProtocolTest, EmptyArraySerialization) {
    std::vector<std::string> values;
    std::string serialized = RespSerializer::SerializeArray(values);
    EXPECT_EQ(serialized, "*0\r\n");
}

TEST_F(RespProtocolTest, ErrorSerialization) {
    std::string serialized = RespSerializer::SerializeError("Error message");
    EXPECT_EQ(serialized, "-Error message\r\n");
}

TEST_F(RespProtocolTest, NullSerialization) {
    std::string serialized = RespSerializer::SerializeNull();
    EXPECT_EQ(serialized, "$-1\r\n");
}

TEST_F(RespProtocolTest, ConvenienceMethods) {
    EXPECT_EQ(RespSerializer::SerializeOK(), "+OK\r\n");
    EXPECT_EQ(RespSerializer::SerializePong(), "+PONG\r\n");
    EXPECT_EQ(RespSerializer::SerializeZero(), ":0\r\n");
    EXPECT_EQ(RespSerializer::SerializeOne(), ":1\r\n");
    EXPECT_EQ(RespSerializer::SerializeNil(), "$-1\r\n");
}

TEST_F(RespProtocolTest, ErrorConvenienceMethods) {
    EXPECT_EQ(RespSerializer::SerializeWrongArgsError("GET"),
              "-ERR wrong number of arguments for 'GET' command\r\n");
    EXPECT_EQ(RespSerializer::SerializeUnknownCommandError("UNKNOWN"),
              "-ERR unknown command 'UNKNOWN'\r\n");
    EXPECT_EQ(RespSerializer::SerializeIntegerError(),
              "-ERR value is not an integer or out of range\r\n");
    EXPECT_EQ(RespSerializer::SerializeSyntaxError(),
              "-ERR syntax error\r\n");
}

TEST_F(RespProtocolTest, RoundTripTest) {
    // Test simple strings
    std::string original = "hello world";
    std::string serialized = RespSerializer::SerializeBulkString(original);

    parser_->Reset();
    auto result = parser_->Parse(serialized.c_str(), serialized.length());

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->type, RespParser::ParseResult::BULK_STRING);
    EXPECT_EQ(result->value, original);

    // Test integers
    int64_t original_int = -12345;
    serialized = RespSerializer::SerializeInteger(original_int);

    parser_->Reset();
    result = parser_->Parse(serialized.c_str(), serialized.length());

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->type, RespParser::ParseResult::INTEGER);
    EXPECT_EQ(result->value, "-12345");
}

TEST_F(RespProtocolTest, LargeStringSerialization) {
    // Test with a large string (1KB)
    std::string large_string(1024, 'x');
    std::string serialized = RespSerializer::SerializeBulkString(large_string);

    // Verify the length is correct
    std::string expected_prefix = "$1024\r\n";
    EXPECT_TRUE(serialized.find(expected_prefix) == 0);
    EXPECT_TRUE(serialized.find("\r\n", expected_prefix.length()) != std::string::npos);

    // Parse it back
    parser_->Reset();
    auto result = parser_->Parse(serialized.c_str(), serialized.length());

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->type, RespParser::ParseResult::BULK_STRING);
    EXPECT_EQ(result->value, large_string);
}

TEST_F(RespProtocolTest, SpecialCharactersInStrings) {
    std::string special_string = "hello\nworld\r\0test";
    std::string serialized = RespSerializer::SerializeBulkString(special_string);

    parser_->Reset();
    auto result = parser_->Parse(serialized.c_str(), serialized.length());

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->type, RespParser::ParseResult::BULK_STRING);
    EXPECT_EQ(result->value, special_string);
}