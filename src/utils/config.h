#pragma once

#include <string>
#include <cstdint>

class Config {
public:
    struct ServerConfig {
        uint16_t port = 6379;
        std::string bind_address = "0.0.0.0";
        size_t max_connections = 10000;
        size_t io_uring_queue_depth = 1024;
        bool tcp_keepalive = true;
    };

    struct StorageConfig {
        size_t max_memory = 1024 * 1024 * 1024;  // 1GB
        std::string eviction_policy = "noeviction";  // Future: lru, lfu
        size_t initial_capacity = 1000;
    };

    struct PersistenceConfig {
        bool enabled = false;
        uint32_t save_interval = 300;  // seconds
        std::string rdb_file = "quantdb.rdb";
        bool compression = false;
        bool auto_save = true;
    };

    struct LoggingConfig {
        std::string level = "INFO";
        std::string file = "";
        size_t max_size = 100 * 1024 * 1024;  // 100MB
        size_t max_files = 5;
        bool console_output = true;
    };

    struct PerformanceConfig {
        size_t worker_threads = 4;
        bool cpu_affinity = false;
        size_t read_buffer_size = 64 * 1024;  // 64KB
        size_t write_buffer_size = 64 * 1024;  // 64KB
    };

private:
    ServerConfig server_;
    StorageConfig storage_;
    PersistenceConfig persistence_;
    LoggingConfig logging_;
    PerformanceConfig performance_;

public:
    Config() = default;

    // Configuration loading
    bool LoadFromFile(const std::string& filename);
    bool LoadFromString(const std::string& yaml_content);
    void SetDefaults();

    // Accessors
    const ServerConfig& server() const { return server_; }
    const StorageConfig& storage() const { return storage_; }
    const PersistenceConfig& persistence() const { return persistence_; }
    const LoggingConfig& logging() const { return logging_; }
    const PerformanceConfig& performance() const { return performance_; }

    // Mutators (for runtime configuration)
    ServerConfig& mutable_server() { return server_; }
    StorageConfig& mutable_storage() { return storage_; }
    PersistenceConfig& mutable_persistence() { return persistence_; }
    LoggingConfig& mutable_logging() { return logging_; }
    PerformanceConfig& mutable_performance() { return performance_; }

    // Validation
    bool Validate() const;
    std::string GetValidationErrors() const;

    // Serialization
    std::string ToString() const;
    bool SaveToFile(const std::string& filename) const;

private:
    // Simple YAML parsing helpers
    bool ParseYAMLValue(const std::string& line, std::string& key, std::string& value);
    std::string Trim(const std::string& str) const;
    size_t ParseSize(const std::string& str) const;
    std::string FormatSize(size_t bytes) const;
    LogLevel ParseLogLevel(const std::string& level) const;
    std::string FormatLogLevel(LogLevel level) const;

    enum class LogLevel {
        DEBUG = 0,
        INFO = 1,
        WARN = 2,
        ERROR = 3,
        FATAL = 4
    };
};