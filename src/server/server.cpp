#include "server.h"
#include <iostream>
#include <csignal>
#include <unistd.h>
#include <sys/types.h>
#include <thread>
#include <chrono>

Server* Server::instance_ = nullptr;

Server::Server() : running_(false), port_(6379), bind_address_("0.0.0.0") {
    instance_ = this;
}

Server::~Server() {
    Stop();
    instance_ = nullptr;
}

bool Server::SetPort(uint16_t port) {
    if (running_.load()) {
        std::cerr << "Cannot change port while server is running" << std::endl;
        return false;
    }
    port_ = port;
    return true;
}

bool Server::SetBindAddress(const std::string& address) {
    if (running_.load()) {
        std::cerr << "Cannot change bind address while server is running" << std::endl;
        return false;
    }
    bind_address_ = address;
    return true;
}

bool Server::SetConfigFile(const std::string& config_file) {
    if (running_.load()) {
        std::cerr << "Cannot change config file while server is running" << std::endl;
        return false;
    }
    config_file_ = config_file;
    return true;
}

bool Server::Start() {
    if (running_.load()) {
        std::cerr << "Server is already running" << std::endl;
        return false;
    }

    // Parse config file if provided
    if (!config_file_.empty()) {
        if (!ParseConfigFile()) {
            std::cerr << "Failed to parse config file: " << config_file_ << std::endl;
            return false;
        }
    }

    // Setup signal handlers
    SetupSignalHandlers();

    // Initialize io_uring handler
    io_handler_ = std::make_unique<IoUringHandler>();
    if (!io_handler_->Initialize(port_, bind_address_)) {
        std::cerr << "Failed to initialize io_uring handler" << std::endl;
        return false;
    }

    running_.store(true);
    PrintStartupInfo();

    // Start the event loop
    io_handler_->Run();

    return true;
}

void Server::Stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    if (io_handler_) {
        io_handler_->Stop();
        io_handler_.reset();
    }

    std::cout << "Server stopped" << std::endl;
}

void Server::Wait() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

uint64_t Server::GetTotalConnections() const {
    return io_handler_ ? io_handler_->GetTotalConnections() : 0;
}

uint64_t Server::GetActiveConnections() const {
    return io_handler_ ? io_handler_->GetActiveConnections() : 0;
}

uint64_t Server::GetTotalOperations() const {
    return io_handler_ ? io_handler_->GetTotalOperations() : 0;
}

bool Server::ParseConfigFile() {
    // Simplified config parsing - in a real implementation,
    // this would use a YAML library
    std::cout << "Config file parsing not yet implemented: " << config_file_ << std::endl;
    return true;
}

void Server::SetupSignalHandlers() {
    struct sigaction sa;
    sa.sa_handler = SignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGPIPE, &sa, nullptr); // Ignore SIGPIPE
}

void Server::SignalHandler(int signal) {
    if (instance_) {
        switch (signal) {
            case SIGINT:
            case SIGTERM:
                std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
                instance_->Stop();
                break;
            case SIGPIPE:
                // Ignore SIGPIPE - handle errors through return codes
                break;
            default:
                std::cout << "Received signal " << signal << std::endl;
                break;
        }
    }
}

void Server::PrintStartupInfo() {
    std::cout << std::endl;
    std::cout << "=================================================" << std::endl;
    std::cout << "QuantDB - High Performance Redis Alternative" << std::endl;
    std::cout << "=================================================" << std::endl;
    std::cout << "Version: 1.0.0" << std::endl;
    std::cout << "Storage Engine: Swiss Tables" << std::endl;
    std::cout << "Network I/O: io_uring" << std::endl;
    std::cout << "Protocol: RESP" << std::endl;
    std::cout << std::endl;
    std::cout << "Server Configuration:" << std::endl;
    std::cout << "  Bind Address: " << bind_address_ << std::endl;
    std::cout << "  Port: " << port_ << std::endl;
    if (!config_file_.empty()) {
        std::cout << "  Config File: " << config_file_ << std::endl;
    }
    std::cout << std::endl;
    std::cout << "Ready to accept connections..." << std::endl;
    std::cout << "=================================================" << std::endl;
    std::cout << std::endl;
}