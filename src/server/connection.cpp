#include "connection.h"
#include "../storage/command_executor.h"
#include "../protocol/resp_parser.h"
#include <unistd.h>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

Connection::Connection(int fd, CommandExecutor* executor, const std::string& client_address)
    : fd_(fd), state_(State::READING), read_offset_(0), write_offset_(0),
      executor_(executor), client_address_(client_address),
      bytes_received_(0), bytes_sent_(0), commands_processed_(0) {

    parser_ = std::make_unique<RespParser>();
    read_buffer_.resize(4096);  // Initial read buffer size
    write_buffer_.clear();

    executor_->IncrementConnections();
}

Connection::~Connection() {
    Close();
}

bool Connection::HandleReadComplete(ssize_t bytes_read) {
    if (bytes_read <= 0) {
        // Connection closed or error
        return false;
    }

    bytes_received_ += bytes_read;
    read_offset_ += bytes_read;

    // Process the received data
    std::string data(read_buffer_.data(), read_offset_);
    auto commands = ParseCommands(data);

    if (!commands.empty()) {
        commands_processed_ += commands.size();

        // Execute each command and collect responses
        std::string combined_response;
        for (const auto& command : commands) {
            std::string response = executor_->ExecuteCommand(command);
            combined_response += response;
        }

        if (!combined_response.empty()) {
            QueueWrite(combined_response);
            state_ = State::WRITING;
        }
    }

    // Reset read buffer for next read
    if (read_offset_ >= read_buffer_.size() / 2) {
        // Buffer is getting full, move remaining data to beginning
        size_t remaining_data = read_offset_;
        if (remaining_data > 0) {
            std::memmove(read_buffer_.data(), read_buffer_.data(), remaining_data);
        }
        read_offset_ = 0;
    }

    return true;
}

bool Connection::HandleWriteComplete(ssize_t bytes_written) {
    if (bytes_written <= 0) {
        // Write error
        return false;
    }

    bytes_sent_ += bytes_written;
    write_offset_ += bytes_written;

    if (write_offset_ >= write_buffer_.size()) {
        // All data written
        ResetWriteBuffer();
        state_ = State::READING;
    }

    return true;
}

void Connection::QueueWrite(const std::string& response) {
    if (response.empty()) {
        return;
    }

    // Append to write buffer
    write_buffer_.insert(write_buffer_.end(), response.begin(), response.end());
    write_offset_ = 0;  // Reset write offset
}

void Connection::Close() {
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
    state_ = State::CLOSING;
}

struct io_uring_sqe* Connection::PrepareRead(struct io_uring* ring) {
    if (state_ != State::READING || fd_ < 0) {
        return nullptr;
    }

    // Ensure we have space to read
    if (read_offset_ >= read_buffer_.size() - 1024) {
        ResizeReadBuffer(read_buffer_.size() * 2);
    }

    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
    if (sqe) {
        char* read_ptr = read_buffer_.data() + read_offset_;
        size_t read_size = read_buffer_.size() - read_offset_;

        io_uring_prep_read(sqe, fd_, read_ptr, read_size, 0);
        io_uring_sqe_set_data(sqe, this);
    }

    return sqe;
}

struct io_uring_sqe* Connection::PrepareWrite(struct io_uring* ring) {
    if (state_ != State::WRITING || fd_ < 0 || write_buffer_.empty()) {
        return nullptr;
    }

    struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
    if (sqe) {
        char* write_ptr = write_buffer_.data() + write_offset_;
        size_t write_size = write_buffer_.size() - write_offset_;

        io_uring_prep_write(sqe, fd_, write_ptr, write_size, 0);
        io_uring_sqe_set_data(sqe, this);
    }

    return sqe;
}

void Connection::ResizeReadBuffer(size_t new_size) {
    if (new_size > read_buffer_.size()) {
        read_buffer_.resize(new_size);
    }
}

void Connection::ResizeWriteBuffer(size_t new_size) {
    if (new_size > write_buffer_.size()) {
        write_buffer_.resize(new_size);
    }
}

std::vector<std::string> Connection::ParseCommands(const std::string& data) {
    std::vector<std::string> commands;

    // Simple RESP command parsing
    // This is a simplified implementation - a full implementation would handle
    // partial commands and more complex array structures
    size_t pos = 0;

    while (pos < data.length()) {
        if (data[pos] == '*') {
            // Array command
            size_t array_end = data.find("\r\n", pos);
            if (array_end == std::string::npos) break;

            // Parse array length (simplified)
            std::string length_str = data.substr(pos + 1, array_end - pos - 1);
            int array_length = 0;
            try {
                array_length = std::stoi(length_str);
            } catch (...) {
                break;
            }

            // Build command string
            std::string command;
            size_t current_pos = array_end + 2;

            for (int i = 0; i < array_length && current_pos < data.length(); ++i) {
                if (current_pos + 1 < data.length() && data[current_pos] == '$') {
                    size_t len_end = data.find("\r\n", current_pos);
                    if (len_end == std::string::npos) break;

                    std::string item_length = data.substr(current_pos + 1, len_end - current_pos - 1);
                    int item_len = 0;
                    try {
                        item_len = std::stoi(item_length);
                    } catch (...) {
                        break;
                    }

                    size_t item_start = len_end + 2;
                    if (item_start + item_len <= data.length()) {
                        std::string item = data.substr(item_start, item_len);
                        if (!command.empty()) command += " ";
                        command += item;
                        current_pos = item_start + item_len + 2; // Skip CRLF
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }

            if (!command.empty()) {
                commands.push_back(command);
                pos = current_pos;
            } else {
                break;
            }
        } else {
            // Simple string command (for debugging)
            size_t cmd_end = data.find("\r\n", pos);
            if (cmd_end == std::string::npos) break;

            std::string command = data.substr(pos, cmd_end - pos);
            if (!command.empty()) {
                commands.push_back(command);
            }
            pos = cmd_end + 2;
        }
    }

    return commands;
}

void Connection::ResetWriteBuffer() {
    write_buffer_.clear();
    write_offset_ = 0;
}