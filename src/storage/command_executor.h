#pragma once

#include "swiss_table_store.h"
#include "../protocol/commands.h"
#include "../protocol/resp_serializer.h"
#include <memory>
#include <string>
#include <vector>

class CommandExecutor {
private:
    std::unique_ptr<SwissTableStore> store_;
    uint64_t operations_count_;
    uint64_t total_connections_;

public:
    explicit CommandExecutor(std::unique_ptr<SwissTableStore> store);
    ~CommandExecutor() = default;

    // Execute a command and return the RESP response
    std::string ExecuteCommand(const std::vector<std::string>& args);

    // Get statistics
    uint64_t GetOperationsCount() const { return operations_count_; }
    uint64_t GetTotalConnections() const { return total_connections_; }
    void IncrementConnections() { total_connections_++; }

    // Get memory usage
    size_t GetMemoryUsage() const;

    // Store operations for direct access
    SwissTableStore* GetStore() { return store_.get(); }

private:
    // Command handlers
    std::string HandleGet(const std::vector<std::string>& args);
    std::string HandleSet(const std::vector<std::string>& args);
    std::string HandleDel(const std::vector<std::string>& args);
    std::string HandleExists(const std::vector<std::string>& args);
    std::string HandleIncr(const std::vector<std::string>& args);
    std::string HandleDecr(const std::vector<std::string>& args);
    std::string HandleIncrBy(const std::vector<std::string>& args);
    std::string HandleDecrBy(const std::vector<std::string>& args);
    std::string HandlePing(const std::vector<std::string>& args);
    std::string HandleInfo(const std::vector<std::string>& args);

    // Error handling
    std::string SerializeError(const std::string& message);
    std::string SerializeWrongArgsError(const std::string& command);

    // Helper functions
    std::optional<int64_t> ParseInt64(const std::string& str) const;
};