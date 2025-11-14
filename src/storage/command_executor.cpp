#include "command_executor.h"
#include <absl/strings/numbers.h>
#include <sstream>
#include <iomanip>

CommandExecutor::CommandExecutor(std::unique_ptr<SwissTableStore> store)
    : store_(std::move(store)), operations_count_(0), total_connections_(0) {}

std::string CommandExecutor::ExecuteCommand(const std::vector<std::string>& args) {
    if (args.empty()) {
        return SerializeError("Empty command");
    }

    operations_count_++;

    const std::string& command = args[0];
    auto* cmd_info = CommandRegistry::GetCommandInfo(command);

    if (!cmd_info) {
        return RespSerializer::SerializeUnknownCommandError(command);
    }

    // Validate arguments
    if (!CommandRegistry::ValidateArgs(*cmd_info, args.size() - 1)) {
        return SerializeWrongArgsError(command);
    }

    try {
        switch (cmd_info->type) {
            case CommandRegistry::CommandType::GET:
                return HandleGet(args);
            case CommandRegistry::CommandType::SET:
                return HandleSet(args);
            case CommandRegistry::CommandType::DEL:
                return HandleDel(args);
            case CommandRegistry::CommandType::EXISTS:
                return HandleExists(args);
            case CommandRegistry::CommandType::INCR:
                return HandleIncr(args);
            case CommandRegistry::CommandType::DECR:
                return HandleDecr(args);
            case CommandRegistry::CommandType::INCRBY:
                return HandleIncrBy(args);
            case CommandRegistry::CommandType::DECRBY:
                return HandleDecrBy(args);
            case CommandRegistry::CommandType::PING:
                return HandlePing(args);
            case CommandRegistry::CommandType::INFO:
                return HandleInfo(args);
            default:
                return SerializeError("Command not implemented");
        }
    } catch (const std::exception& e) {
        return SerializeError(std::string("Internal error: ") + e.what());
    }
}

std::string CommandExecutor::HandleGet(const std::vector<std::string>& args) {
    if (args.size() != 2) {
        return SerializeWrongArgsError("GET");
    }

    auto result = store_->Get(args[1]);
    if (result.has_value()) {
        return RespSerializer::SerializeBulkString(result.value());
    } else {
        return RespSerializer::SerializeNull();
    }
}

std::string CommandExecutor::HandleSet(const std::vector<std::string>& args) {
    if (args.size() != 3) {
        return SerializeWrongArgsError("SET");
    }

    bool success = store_->Set(args[1], args[2]);
    return success ? RespSerializer::SerializeOK() : SerializeError("Failed to set value");
}

std::string CommandExecutor::HandleDel(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        return SerializeWrongArgsError("DEL");
    }

    int deleted_count = 0;
    for (size_t i = 1; i < args.size(); ++i) {
        if (store_->Delete(args[i])) {
            deleted_count++;
        }
    }

    return RespSerializer::SerializeInteger(deleted_count);
}

std::string CommandExecutor::HandleExists(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        return SerializeWrongArgsError("EXISTS");
    }

    int exists_count = 0;
    for (size_t i = 1; i < args.size(); ++i) {
        if (store_->Exists(args[i])) {
            exists_count++;
        }
    }

    return RespSerializer::SerializeInteger(exists_count);
}

std::string CommandExecutor::HandleIncr(const std::vector<std::string>& args) {
    if (args.size() != 2) {
        return SerializeWrongArgsError("INCR");
    }

    try {
        int64_t result = store_->Increment(args[1], 1);
        return RespSerializer::SerializeInteger(result);
    } catch (const std::invalid_argument&) {
        return RespSerializer::SerializeIntegerError();
    }
}

std::string CommandExecutor::HandleDecr(const std::vector<std::string>& args) {
    if (args.size() != 2) {
        return SerializeWrongArgsError("DECR");
    }

    try {
        int64_t result = store_->Increment(args[1], -1);
        return RespSerializer::SerializeInteger(result);
    } catch (const std::invalid_argument&) {
        return RespSerializer::SerializeIntegerError();
    }
}

std::string CommandExecutor::HandleIncrBy(const std::vector<std::string>& args) {
    if (args.size() != 3) {
        return SerializeWrongArgsError("INCRBY");
    }

    auto delta = ParseInt64(args[2]);
    if (!delta.has_value()) {
        return RespSerializer::SerializeIntegerError();
    }

    try {
        int64_t result = store_->Increment(args[1], delta.value());
        return RespSerializer::SerializeInteger(result);
    } catch (const std::invalid_argument&) {
        return RespSerializer::SerializeIntegerError();
    }
}

std::string CommandExecutor::HandleDecrBy(const std::vector<std::string>& args) {
    if (args.size() != 3) {
        return SerializeWrongArgsError("DECRBY");
    }

    auto delta = ParseInt64(args[2]);
    if (!delta.has_value()) {
        return RespSerializer::SerializeIntegerError();
    }

    try {
        int64_t result = store_->Increment(args[1], -delta.value());
        return RespSerializer::SerializeInteger(result);
    } catch (const std::invalid_argument&) {
        return RespSerializer::SerializeIntegerError();
    }
}

std::string CommandExecutor::HandlePing(const std::vector<std::string>& args) {
    if (args.size() == 1) {
        return RespSerializer::SerializePong();
    } else if (args.size() == 2) {
        // Redis supports PING with message argument
        return RespSerializer::SerializeBulkString(args[1]);
    } else {
        return SerializeWrongArgsError("PING");
    }
}

std::string CommandExecutor::HandleInfo(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        return SerializeWrongArgsError("INFO");
    }

    std::ostringstream info;
    info << "# Server\r\n";
    info << "redis_version:7.0.0\r\n";
    info << "redis_mode:standalone\r\n";
    info << "os:Linux\r\n";
    info << "arch_bits:64\r\n";
    info << "process_id:" << getpid() << "\r\n";
    info << "tcp_port:6379\r\n";
    info << "uptime_in_seconds:0\r\n";
    info << "uptime_in_days:0\r\n";

    info << "# Clients\r\n";
    info << "connected_clients:" << total_connections_ << "\r\n";

    info << "# Memory\r\n";
    info << "used_memory:" << GetMemoryUsage() << "\r\n";
    info << "used_memory_human:" << std::fixed << std::setprecision(2)
        << (GetMemoryUsage() / 1024.0 / 1024.0) << "M\r\n";

    info << "# Persistence\r\n";
    info << "loading:0\r\n";

    info << "# Stats\r\n";
    info << "total_connections_received:" << total_connections_ << "\r\n";
    info << "total_commands_processed:" << operations_count_ << "\r\n";
    info << "keyspace_hits:0\r\n";
    info << "keyspace_misses:0\r\n";

    info << "# Keyspace\r\n";
    info << "db0:keys=" << store_->Size() << ",expires=0,avg_ttl=0\r\n";

    return RespSerializer::SerializeBulkString(info.str());
}

std::string CommandExecutor::SerializeError(const std::string& message) {
    return RespSerializer::SerializeError(message);
}

std::string CommandExecutor::SerializeWrongArgsError(const std::string& command) {
    return RespSerializer::SerializeWrongArgsError(command);
}

std::optional<int64_t> CommandExecutor::ParseInt64(const std::string& str) const {
    int64_t value;
    if (absl::SimpleAtoi(str, &value)) {
        return value;
    }
    return std::nullopt;
}

size_t CommandExecutor::GetMemoryUsage() const {
    size_t usage = sizeof(CommandExecutor);
    if (store_) {
        usage += store_->MemoryUsage();
    }
    return usage;
}