#include "io_uring_handler.h"
#include "../storage/swiss_table_store.h"
#include "../storage/command_executor.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <errno.h>

IoUringHandler::IoUringHandler()
    : listen_fd_(-1), state_(State::STOPPED), port_(6379),
      bind_address_("0.0.0.0"), queue_entries_(1024),
      total_connections_(0), active_connections_(0), total_operations_(0) {}

IoUringHandler::~IoUringHandler() {
    Stop();
}

bool IoUringHandler::Initialize(uint16_t port, const std::string& bind_address,
                                size_t queue_entries) {
    if (state_.load() != State::STOPPED) {
        return false;
    }

    state_.store(State::STARTING);
    port_ = port;
    bind_address_ = bind_address;
    queue_entries_ = queue_entries;

    // Initialize io_uring
    int ret = io_uring_queue_init(queue_entries, &ring_, 0);
    if (ret != 0) {
        std::cerr << "Failed to initialize io_uring: " << ret << std::endl;
        state_.store(State::STOPPED);
        return false;
    }

    // Initialize storage and executor
    auto store = std::make_unique<SwissTableStore>();
    executor_ = std::make_unique<CommandExecutor>(std::move(store));

    // Setup listening socket
    if (!SetupListenSocket()) {
        io_uring_queue_exit(&ring_);
        state_.store(State::STOPPED);
        return false;
    }

    state_.store(State::RUNNING);
    return true;
}

void IoUringHandler::Run() {
    if (state_.load() != State::RUNNING) {
        return;
    }

    // Start event loop in a separate thread
    event_loop_thread_ = std::thread(&IoUringHandler::EventLoop, this);
}

void IoUringHandler::Stop() {
    if (state_.load() == State::STOPPED) {
        return;
    }

    state_.store(State::STOPPING);

    // Close listening socket
    if (listen_fd_ >= 0) {
        close(listen_fd_);
        listen_fd_ = -1;
    }

    // Close all connections
    connections_.clear();

    // Wait for event loop thread to finish
    if (event_loop_thread_.joinable()) {
        event_loop_thread_.join();
    }

    // Cleanup io_uring
    io_uring_queue_exit(&ring_);
    state_.store(State::STOPPED);
}

bool IoUringHandler::SetupListenSocket() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listen_fd_ < 0) {
        std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }

    SetSocketOptions(listen_fd_);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);

    if (bind_address_ == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, bind_address_.c_str(), &addr.sin_addr) <= 0) {
            std::cerr << "Invalid bind address: " << bind_address_ << std::endl;
            close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }
    }

    if (bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind socket: " << strerror(errno) << std::endl;
        close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    if (listen(listen_fd_, SOMAXCONN) < 0) {
        std::cerr << "Failed to listen on socket: " << strerror(errno) << std::endl;
        close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    // Add accept operation to io_uring
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (sqe) {
        io_uring_prep_accept(sqe, listen_fd_, nullptr, nullptr, 0);
        io_uring_sqe_set_data(sqe, nullptr); // Use nullptr to indicate accept
    }

    std::cout << "Server listening on " << bind_address_ << ":" << port_ << std::endl;
    return true;
}

void IoUringHandler::EventLoop() {
    std::cout << "Event loop started" << std::endl;

    while (state_.load() == State::RUNNING) {
        int ret = io_uring_submit_and_wait(&ring_, 1);
        if (ret < 0) {
            if (state_.load() == State::RUNNING) {
                std::cerr << "io_uring_submit_and_wait failed: " << ret << std::endl;
            }
            break;
        }

        HandleEvent();
    }

    std::cout << "Event loop stopped" << std::endl;
}

void IoUringHandler::HandleEvent() {
    struct io_uring_cqe* cqe;
    unsigned head;
    unsigned processed = 0;

    io_uring_for_each_cqe(&ring_, head, cqe) {
        processed++;

        void* user_data = io_uring_cqe_get_data(cqe);
        int res = cqe->res;

        if (user_data == nullptr) {
            // Accept operation
            if (res >= 0) {
                HandleAccept();
            } else if (res != -ECANCELED) {
                std::cerr << "Accept failed: " << strerror(-res) << std::endl;
            }

            // Queue next accept
            if (state_.load() == State::RUNNING) {
                struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
                if (sqe) {
                    io_uring_prep_accept(sqe, listen_fd_, nullptr, nullptr, 0);
                    io_uring_sqe_set_data(sqe, nullptr);
                }
            }
        } else {
            Connection* conn = static_cast<Connection*>(user_data);
            if (conn) {
                if (res >= 0) {
                    // Read or write operation completed
                    if (conn->GetState() == Connection::State::READING) {
                        HandleRead(cqe);
                    } else if (conn->GetState() == Connection::State::WRITING) {
                        HandleWrite(cqe);
                    }
                } else {
                    // Error or connection closed
                    HandleConnectionClose(conn);
                }
            }
        }

        io_uring_cqe_seen(&ring_, cqe);
    }
}

void IoUringHandler::HandleAccept() {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept4(listen_fd_, (struct sockaddr*)&client_addr, &client_len,
                          SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (client_fd < 0) {
        std::cerr << "Accept failed: " << strerror(errno) << std::endl;
        return;
    }

    std::string client_addr_str = GetClientAddress(&client_addr);

    auto conn = std::make_unique<Connection>(client_fd, executor_.get(), client_addr_str);
    Connection* conn_ptr = conn.get();

    connections_.push_back(std::move(conn));
    total_connections_++;
    active_connections_++;

    // Prepare first read
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (sqe) {
        conn_ptr->PrepareRead(&ring_);
    }

    std::cout << "New connection from " << client_addr_str
              << " (fd: " << client_fd << ")" << std::endl;
}

void IoUringHandler::HandleRead(struct io_uring_cqe* cqe) {
    Connection* conn = static_cast<Connection*>(io_uring_cqe_get_data(cqe));
    if (!conn) return;

    if (!conn->HandleReadComplete(cqe->res)) {
        HandleConnectionClose(conn);
        return;
    }

    total_operations_++;

    // If connection is ready to write, prepare write operation
    if (conn->GetState() == Connection::State::WRITING) {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
        if (sqe) {
            conn->PrepareWrite(&ring_);
        }
    } else if (conn->GetState() == Connection::State::READING) {
        // Prepare next read
        struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
        if (sqe) {
            conn->PrepareRead(&ring_);
        }
    }
}

void IoUringHandler::HandleWrite(struct io_uring_cqe* cqe) {
    Connection* conn = static_cast<Connection*>(io_uring_cqe_get_data(cqe));
    if (!conn) return;

    if (!conn->HandleWriteComplete(cqe->res)) {
        HandleConnectionClose(conn);
        return;
    }

    // If write is complete, prepare next read
    if (conn->GetState() == Connection::State::READING) {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
        if (sqe) {
            conn->PrepareRead(&ring_);
        }
    } else if (conn->GetState() == Connection::State::WRITING) {
        // More data to write
        struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
        if (sqe) {
            conn->PrepareWrite(&ring_);
        }
    }
}

void IoUringHandler::HandleConnectionClose(Connection* conn) {
    if (!conn) return;

    std::cout << "Connection closed from " << conn->GetClientAddress()
              << " (fd: " << conn->GetFd() << ")" << std::endl;

    active_connections_--;
    RemoveConnection(conn);
}

Connection* IoUringHandler::GetConnectionByFd(int fd) {
    for (auto& conn : connections_) {
        if (conn->GetFd() == fd) {
            return conn.get();
        }
    }
    return nullptr;
}

void IoUringHandler::RemoveConnection(Connection* conn) {
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
                      [conn](const std::unique_ptr<Connection>& c) {
                          return c.get() == conn;
                      }),
        connections_.end());
}

std::string IoUringHandler::GetClientAddress(struct sockaddr_in* addr) {
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    return std::string(client_ip) + ":" + std::to_string(ntohs(addr->sin_port));
}

void IoUringHandler::SetSocketOptions(int fd) {
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    // Set TCP_NODELAY to disable Nagle's algorithm
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

    // Set socket buffer sizes
    int buffer_size = 64 * 1024; // 64KB
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size));
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size));
}