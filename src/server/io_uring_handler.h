#pragma once

#include "connection.h"
#include <liburing.h>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>

class CommandExecutor;

class IoUringHandler {
public:
    enum class State {
        STOPPED,
        STARTING,
        RUNNING,
        STOPPING
    };

private:
    struct io_uring ring_;
    int listen_fd_;
    std::vector<std::unique_ptr<Connection>> connections_;
    std::unique_ptr<CommandExecutor> executor_;
    std::atomic<State> state_;
    uint16_t port_;
    std::string bind_address_;
    size_t queue_entries_;
    std::thread event_loop_thread_;

    // Statistics
    std::atomic<uint64_t> total_connections_;
    std::atomic<uint64_t> active_connections_;
    std::atomic<uint64_t> total_operations_;

public:
    IoUringHandler();
    ~IoUringHandler();

    bool Initialize(uint16_t port = 6379, const std::string& bind_address = "0.0.0.0",
                    size_t queue_entries = 1024);
    void Run();
    void Stop();

    // Statistics
    uint64_t GetTotalConnections() const { return total_connections_.load(); }
    uint64_t GetActiveConnections() const { return active_connections_.load(); }
    uint64_t GetTotalOperations() const { return total_operations_.load(); }
    State GetState() const { return state_.load(); }

private:
    bool SetupListenSocket();
    void EventLoop();
    void HandleEvent();
    void HandleAccept();
    void HandleRead(struct io_uring_cqe* cqe);
    void HandleWrite(struct io_uring_cqe* cqe);
    void HandleConnectionClose(Connection* conn);

    Connection* GetConnectionByFd(int fd);
    void RemoveConnection(Connection* conn);

    // Utility functions
    std::string GetClientAddress(struct sockaddr_in* addr);
    void SetSocketOptions(int fd);
};