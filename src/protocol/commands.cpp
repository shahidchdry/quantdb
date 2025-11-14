#include "commands.h"
#include <algorithm>
#include <cctype>

const std::unordered_map<std::string, CommandRegistry::CommandInfo> CommandRegistry::commands_ = {
    {"GET",    {CommandType::GET,    "GET",    1, 1, "Get the value of a key"}},
    {"SET",    {CommandType::SET,    "SET",    2, 2, "Set the value of a key"}},
    {"DEL",    {CommandType::DEL,    "DEL",    1, 0, "Delete a key"}},
    {"EXISTS", {CommandType::EXISTS, "EXISTS", 1, 0, "Check if key exists"}},
    {"INCR",   {CommandType::INCR,   "INCR",   1, 1, "Increment the integer value of a key by one"}},
    {"DECR",   {CommandType::DECR,   "DECR",   1, 1, "Decrement the integer value of a key by one"}},
    {"INCRBY", {CommandType::INCRBY, "INCRBY", 2, 2, "Increment the integer value of a key by the given number"}},
    {"DECRBY", {CommandType::DECRBY, "DECRBY", 2, 2, "Decrement the integer value of a key by the given number"}},
    {"PING",   {CommandType::PING,   "PING",   0, 0, "Ping the server"}},
    {"INFO",   {CommandType::INFO,   "INFO",   0, 0, "Get information and statistics about the server"}}
};

CommandRegistry::CommandType CommandRegistry::ParseCommand(const std::string& command_name) {
    std::string upper_name = command_name;
    std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(), ::toupper);

    auto it = commands_.find(upper_name);
    if (it != commands_.end()) {
        return it->second.type;
    }

    return CommandType::UNKNOWN;
}

const CommandRegistry::CommandInfo* CommandRegistry::GetCommandInfo(const std::string& command_name) {
    std::string upper_name = command_name;
    std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(), ::toupper);

    auto it = commands_.find(upper_name);
    if (it != commands_.end()) {
        return &it->second;
    }

    return nullptr;
}

std::vector<std::string> CommandRegistry::GetAllCommands() {
    std::vector<std::string> result;
    result.reserve(commands_.size());

    for (const auto& pair : commands_) {
        result.push_back(pair.first);
    }

    return result;
}

bool CommandRegistry::IsValidCommand(const std::string& command_name) {
    std::string upper_name = command_name;
    std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(), ::toupper);

    return commands_.find(upper_name) != commands_.end();
}

bool CommandRegistry::ValidateArgs(const CommandInfo& info, size_t arg_count) {
    if (arg_count < info.min_args) {
        return false;
    }

    if (info.max_args > 0 && arg_count > info.max_args) {
        return false;
    }

    return true;
}