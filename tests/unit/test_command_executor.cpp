#include <gtest/gtest.h>
#include "../../src/storage/swiss_table_store.h"
#include "../../src/storage/command_executor.h"
#include <vector>

class CommandExecutorTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto store = std::make_unique<SwissTableStore>();
        executor_ = std::make_unique<CommandExecutor>(std::move(store));
    }

    void TearDown() override {
        executor_.reset();
    }

    std::string ExecuteCommand(const std::vector<std::string>& args) {
        return executor_->ExecuteCommand(args);
    }

    std::unique_ptr<CommandExecutor> executor_;
};

TEST_F(CommandExecutorTest, GetCommand) {
    // GET on non-existent key
    std::string response = ExecuteCommand({"GET", "nonexistent"});
    EXPECT_EQ(response, "$-1\r\n");

    // SET then GET
    ExecuteCommand({"SET", "key", "value"});
    response = ExecuteCommand({"GET", "key"});
    EXPECT_EQ(response, "$5\r\nvalue\r\n");
}

TEST_F(CommandExecutorTest, SetCommand) {
    // SET with correct arguments
    std::string response = ExecuteCommand({"SET", "key", "value"});
    EXPECT_EQ(response, "+OK\r\n");

    // Verify value was set
    response = ExecuteCommand({"GET", "key"});
    EXPECT_EQ(response, "$5\r\nvalue\r\n");
}

TEST_F(CommandExecutorTest, SetCommandErrors) {
    // SET with wrong number of arguments
    std::string response = ExecuteCommand({"SET", "only_key"});
    EXPECT_TRUE(response.find("wrong number of arguments") != std::string::npos);

    response = ExecuteCommand({"SET", "key", "value", "extra"});
    EXPECT_TRUE(response.find("wrong number of arguments") != std::string::npos);

    // SET with no arguments
    response = ExecuteCommand({"SET"});
    EXPECT_TRUE(response.find("wrong number of arguments") != std::string::npos);
}

TEST_F(CommandExecutorTest, DeleteCommand) {
    // Delete non-existent key
    std::string response = ExecuteCommand({"DEL", "nonexistent"});
    EXPECT_EQ(response, ":0\r\n");

    // SET then DELETE
    ExecuteCommand({"SET", "key", "value"});
    response = ExecuteCommand({"DEL", "key"});
    EXPECT_EQ(response, ":1\r\n");

    // Verify key was deleted
    response = ExecuteCommand({"GET", "key"});
    EXPECT_EQ(response, "$-1\r\n");
}

TEST_F(CommandExecutorTest, DeleteMultipleKeys) {
    ExecuteCommand({"SET", "key1", "value1"});
    ExecuteCommand({"SET", "key2", "value2"});
    ExecuteCommand({"SET", "key3", "value3"});

    // Delete multiple keys
    std::string response = ExecuteCommand({"DEL", "key1", "key2", "nonexistent"});
    EXPECT_EQ(response, ":2\r\n");  // Only 2 keys should be deleted

    // Verify remaining key
    response = ExecuteCommand({"GET", "key3"});
    EXPECT_EQ(response, "$6\r\nvalue3\r\n");
}

TEST_F(CommandExecutorTest, ExistsCommand) {
    // EXISTS on non-existent key
    std::string response = ExecuteCommand({"EXISTS", "nonexistent"});
    EXPECT_EQ(response, ":0\r\n");

    // SET then EXISTS
    ExecuteCommand({"SET", "key", "value"});
    response = ExecuteCommand({"EXISTS", "key"});
    EXPECT_EQ(response, ":1\r\n");

    // EXISTS on multiple keys
    ExecuteCommand({"SET", "key2", "value2"});
    response = ExecuteCommand({"EXISTS", "key", "key2", "nonexistent"});
    EXPECT_EQ(response, ":2\r\n");
}

TEST_F(CommandExecutorTest, IncrementCommands) {
    // INCR on non-existent key
    std::string response = ExecuteCommand({"INCR", "counter"});
    EXPECT_EQ(response, ":1\r\n");

    // INCR on existing key
    response = ExecuteCommand({"INCR", "counter"});
    EXPECT_EQ(response, ":2\r\n");

    // DECR
    response = ExecuteCommand({"DECR", "counter"});
    EXPECT_EQ(response, ":1\r\n");
}

TEST_F(CommandExecutorTest, IncrementByCommand) {
    // INCRBY on non-existent key
    std::string response = ExecuteCommand({"INCRBY", "counter", "5"});
    EXPECT_EQ(response, ":5\r\n");

    // INCRBY on existing key
    response = ExecuteCommand({"INCRBY", "counter", "3"});
    EXPECT_EQ(response, ":8\r\n");

    // Negative increment
    response = ExecuteCommand({"INCRBY", "counter", "-2"});
    EXPECT_EQ(response, ":6\r\n");
}

TEST_F(CommandExecutorTest, DecrementByCommand) {
    // DECRBY on non-existent key
    std::string response = ExecuteCommand({"DECRBY", "counter", "3"});
    EXPECT_EQ(response, ":-3\r\n");

    // DECRBY on existing key
    response = ExecuteCommand({"DECRBY", "counter", "1"});
    EXPECT_EQ(response, ":-4\r\n");
}

TEST_F(CommandExecutorTest, IncrementCommandErrors) {
    // INCR on non-numeric value
    ExecuteCommand({"SET", "text_key", "hello"});
    std::string response = ExecuteCommand({"INCR", "text_key"});
    EXPECT_TRUE(response.find("not an integer") != std::string::npos);

    // INCRBY with non-numeric delta
    response = ExecuteCommand({"INCRBY", "counter", "not_a_number"});
    EXPECT_TRUE(response.find("not an integer") != std::string::npos);

    // Wrong number of arguments
    response = ExecuteCommand({"INCRBY", "counter"});
    EXPECT_TRUE(response.find("wrong number of arguments") != std::string::npos);

    response = ExecuteCommand({"INCRBY"});
    EXPECT_TRUE(response.find("wrong number of arguments") != std::string::npos);
}

TEST_F(CommandExecutorTest, PingCommand) {
    // PING without arguments
    std::string response = ExecuteCommand({"PING"});
    EXPECT_EQ(response, "+PONG\r\n");

    // PING with argument
    response = ExecuteCommand({"PING", "hello"});
    EXPECT_EQ(response, "$5\r\nhello\r\n");

    // PING with too many arguments
    response = ExecuteCommand({"PING", "arg1", "arg2"});
    EXPECT_TRUE(response.find("wrong number of arguments") != std::string::npos);
}

TEST_F(CommandExecutorTest, InfoCommand) {
    std::string response = ExecuteCommand({"INFO"});

    // Should be a bulk string response
    EXPECT_TRUE(response.find("$") == 0);

    // Should contain server information
    EXPECT_TRUE(response.find("redis_version") != std::string::npos);
    EXPECT_TRUE(response.find("quantdb") != std::string::npos || response.find("redis_mode") != std::string::npos);
}

TEST_F(CommandExecutorTest, UnknownCommand) {
    std::string response = ExecuteCommand({"UNKNOWN_COMMAND", "arg"});
    EXPECT_TRUE(response.find("unknown command") != std::string::npos);
    EXPECT_TRUE(response.find("UNKNOWN_COMMAND") != std::string::npos);
}

TEST_F(CommandExecutorTest, EmptyCommand) {
    std::string response = ExecuteCommand({});
    EXPECT_TRUE(response.find("Empty command") != std::string::npos);
}

TEST_F(CommandExecutorTest, LargeValues) {
    // Test with large values
    std::string large_value(1024 * 1024, 'x');  // 1MB

    ExecuteCommand({"SET", "large_key", large_value});
    std::string response = ExecuteCommand({"GET", "large_key"});

    // Should start with bulk string marker for large value
    EXPECT_TRUE(response.find("$1048576") == 0);  // 1MB = 1048576 bytes
}

TEST_F(CommandExecutorTest, SpecialCharacters) {
    // Test with special characters in keys and values
    std::string special_key = "key with spaces & symbols!@#$%^&*()";
    std::string special_value = "value with \n newlines \r carriage returns";

    ExecuteCommand({"SET", special_key, special_value});
    std::string response = ExecuteCommand({"GET", special_key});

    // Should be a bulk string response (starts with $ and length)
    EXPECT_TRUE(response.find("$") == 0);
    EXPECT_TRUE(response.find("\r\n") != std::string::npos);
}

TEST_F(CommandExecutorTest, Statistics) {
    // Initial statistics should be zero
    EXPECT_EQ(executor_->GetOperationsCount(), 0);
    EXPECT_EQ(executor_->GetTotalConnections(), 0);

    // Execute some commands
    ExecuteCommand({"SET", "key1", "value1"});
    ExecuteCommand({"GET", "key1"});
    ExecuteCommand({"PING"});

    // Operations count should increase
    EXPECT_GT(executor_->GetOperationsCount(), 0);

    // Increment connections
    executor_->IncrementConnections();
    EXPECT_GT(executor_->GetTotalConnections(), 0);
}

TEST_F(CommandExecutorTest, EdgeCases) {
    // Empty key
    std::string response = ExecuteCommand({"SET", "", "value"});
    EXPECT_EQ(response, "+OK\r\n");

    response = ExecuteCommand({"GET", ""});
    EXPECT_EQ(response, "$5\r\nvalue\r\n");

    // Empty value
    response = ExecuteCommand({"SET", "empty_value", ""});
    EXPECT_EQ(response, "+OK\r\n");

    response = ExecuteCommand({"GET", "empty_value"});
    EXPECT_EQ(response, "$0\r\n\r\n");

    // Numeric edge cases
    ExecuteCommand({"SET", "zero", "0"});
    response = ExecuteCommand({"INCR", "zero"});
    EXPECT_EQ(response, ":1\r\n");

    ExecuteCommand({"SET", "negative", "-5"});
    response = ExecuteCommand({"INCR", "negative"});
    EXPECT_EQ(response, ":-4\r\n");
}