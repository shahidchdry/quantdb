#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <liburing.h>

class CommandExecutor;
class RespParser;

class Connection {
public:
    enum class State {
        CONNECTING,
        READING,
        PROCESSING,
        WRITING,
        CLOSING
    };

private:
    int fd_;
    State state_;
    std::vector<char> read_buffer_;
    std::vector<char> write_buffer_;
    size_t read_offset_;
    size_t write_offset_;
    std::unique_ptr<RespParser> parser_;
    CommandExecutor* executor_;
    std::string client_address_;

    // Statistics
    uint64_t bytes_received_;
    uint64_t bytes_sent_;
    uint64_t commands_processed_;

public:
    Connection(int fd, CommandExecutor* executor, const std::string& client_address = "");
    ~Connection();

    // Getters
    int GetFd() const { return fd_; }
    State GetState() const { return state_; }
    const std::string& GetClientAddress() const { return client_address_; }

    // Operations
    bool HandleReadComplete(ssize_t bytes_read);
    bool HandleWriteComplete(ssize_t bytes_written);
    void QueueWrite(const std::string& response);
    void Close();

    // io_uring operations
    struct io_uring_sqe* PrepareRead(struct io_uring* ring);
    struct io_uring_sqe* PrepareWrite(struct io_uring* ring);

    // Buffer management
    void ResizeReadBuffer(size_t new_size);
    void ResizeWriteBuffer(size_t new_size);

    // Statistics
    uint64_t GetBytesReceived() const { return bytes_received_; }
    uint64_t GetBytesSent() const { return bytes_sent_; }
    uint64_t GetCommandsProcessed() const { return commands_processed_; }

    // State management
    void SetState(State state) { state_ = state; }

private:
    bool ProcessCommands();
    void ResetWriteBuffer();
    std::vector<std::string> ParseCommands(const std::string& data);
};