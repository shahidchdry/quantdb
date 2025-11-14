#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

// Command registry and definitions
class CommandRegistry {
public:
    enum class CommandType {
        GET,
        SET,
        DEL,
        EXISTS,
        INCR,
        DECR,
        INCRBY,
        DECRBY,
        PING,
        INFO,
        UNKNOWN
    };

    struct CommandInfo {
        CommandType type;
        std::string name;
        size_t min_args;
        size_t max_args;  // 0 means unlimited
        std::string description;
    };

private:
    static const std::unordered_map<std::string, CommandInfo> commands_;

public:
    static CommandType ParseCommand(const std::string& command_name);
    static const CommandInfo* GetCommandInfo(const std::string& command_name);
    static std::vector<std::string> GetAllCommands();
    static bool IsValidCommand(const std::string& command_name);

    // Command argument validation
    static bool ValidateArgs(const CommandInfo& info, size_t arg_count);
};

// Command result types
struct CommandResult {
    enum class Type {
        STRING,
        INTEGER,
        ARRAY,
        STATUS,
        ERROR,
        NULL_VALUE
    };

    Type type;
    std::string string_value;
    int64_t integer_value;
    std::vector<std::string> array_values;
    bool success = true;

    static CommandResult String(const std::string& value) {
        CommandResult result;
        result.type = Type::STRING;
        result.string_value = value;
        return result;
    }

    static CommandResult Integer(int64_t value) {
        CommandResult result;
        result.type = Type::INTEGER;
        result.integer_value = value;
        return result;
    }

    static CommandResult Array(const std::vector<std::string>& values) {
        CommandResult result;
        result.type = Type::ARRAY;
        result.array_values = values;
        return result;
    }

    static CommandResult Status(const std::string& status) {
        CommandResult result;
        result.type = Type::STATUS;
        result.string_value = status;
        return result;
    }

    static CommandResult Error(const std::string& error) {
        CommandResult result;
        result.type = Type::ERROR;
        result.string_value = error;
        result.success = false;
        return result;
    }

    static CommandResult Null() {
        CommandResult result;
        result.type = Type::NULL_VALUE;
        return result;
    }
};