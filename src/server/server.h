#pragma once

#include "io_uring_handler.h"
#include <memory>
#include <string>
#include <signal.h>
#include <atomic>

class Server {
private:
    std::unique_ptr<IoUringHandler> io_handler_;
    std::atomic<bool> running_;
    uint16_t port_;
    std::string bind_address_;
    std::string config_file_;

    // Signal handling
    static Server* instance_;
    static void SignalHandler(int signal);

public:
    Server();
    ~Server();

    // Configuration
    bool SetPort(uint16_t port);
    bool SetBindAddress(const std::string& address);
    bool SetConfigFile(const std::string& config_file);

    // Lifecycle
    bool Start();
    void Stop();
    void Wait();

    // Status
    bool IsRunning() const { return running_.load(); }

    // Statistics
    uint64_t GetTotalConnections() const;
    uint64_t GetActiveConnections() const;
    uint64_t GetTotalOperations() const;

private:
    bool ParseConfigFile();
    void SetupSignalHandlers();
    void PrintStartupInfo();
};