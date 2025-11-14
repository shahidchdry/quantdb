#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <chrono>
#include <atomic>

class Logger {
public:
    enum class LogLevel {
        DEBUG = 0,
        INFO = 1,
        WARN = 2,
        ERROR = 3,
        FATAL = 4
    };

    struct LogConfig {
        LogLevel min_level = LogLevel::INFO;
        std::string file_path;
        size_t max_file_size = 100 * 1024 * 1024;  // 100MB
        size_t max_files = 5;
        bool console_output = true;
        bool timestamp_format = true;  // Include timestamp in logs
        bool thread_id = false;        // Include thread ID in logs
    };

private:
    static std::unique_ptr<Logger> instance_;
    static std::mutex instance_mutex_;

    std::mutex log_mutex_;
    std::ofstream log_file_;
    LogConfig config_;
    std::atomic<uint64_t> log_count_{0};
    std::chrono::steady_clock::time_point start_time_;

    Logger() : start_time_(std::chrono::steady_clock::now()) {}

public:
    // Singleton pattern
    static Logger& GetInstance();
    static void Initialize(const LogConfig& config);
    static void Shutdown();

    // Delete copy constructor and assignment operator
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Configuration
    void SetConfig(const LogConfig& config);
    const LogConfig& GetConfig() const { return config_; }

    // Logging methods
    void Debug(const std::string& message);
    void Info(const std::string& message);
    void Warn(const std::string& message);
    void Error(const std::string& message);
    void Fatal(const std::string& message);

    // Template methods for formatted logging
    template<typename... Args>
    void Debugf(const std::string& format, Args... args);

    template<typename... Args>
    void Infof(const std::string& format, Args... args);

    template<typename... Args>
    void Warnf(const std::string& format, Args... args);

    template<typename... Args>
    void Errorf(const std::string& format, Args... args);

    template<typename... Args>
    void Fatalf(const std::string& format, Args... args);

    // Performance logging
    void LogOperation(const std::string& operation,
                     std::chrono::microseconds duration);

    // Statistics
    uint64_t GetLogCount() const { return log_count_.load(); }
    std::chrono::milliseconds GetUptime() const;

    // Static convenience methods
    static void SetLevel(LogLevel level) { GetInstance().SetLevel(level); }
    static void Debug(const std::string& message) { GetInstance().Debug(message); }
    static void Info(const std::string& message) { GetInstance().Info(message); }
    static void Warn(const std::string& message) { GetInstance().Warn(message); }
    static void Error(const std::string& message) { GetInstance().Error(message); }
    static void Fatal(const std::string& message) { GetInstance().Fatal(message); }

private:
    void LogInternal(LogLevel level, const std::string& message);
    void WriteToFile(LogLevel level, const std::string& formatted_message);
    void WriteToConsole(LogLevel level, const std::string& formatted_message);
    void RotateLogFile();

    std::string FormatMessage(LogLevel level, const std::string& message) const;
    std::string FormatTimestamp() const;
    std::string FormatThreadId() const;
    std::string LevelToString(LogLevel level) const;

    void SetLevel(LogLevel level) { config_.min_level = level; }

    // Template implementation helper
    template<typename... Args>
    std::string FormatString(const std::string& format, Args... args);
};