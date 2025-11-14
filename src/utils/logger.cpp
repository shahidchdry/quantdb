#include "logger.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <thread>
#include <filesystem>

// Add compatibility header for older compilers
#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

std::unique_ptr<Logger> Logger::instance_;
std::mutex Logger::instance_mutex_;

Logger& Logger::GetInstance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) {
        instance_ = std::unique_ptr<Logger>(new Logger());
    }
    return *instance_;
}

void Logger::Initialize(const LogConfig& config) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) {
        instance_ = std::unique_ptr<Logger>(new Logger());
    }
    instance_->SetConfig(config);
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (instance_) {
        if (instance_->log_file_.is_open()) {
            instance_->log_file_.close();
        }
        instance_.reset();
    }
}

void Logger::SetConfig(const LogConfig& config) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    config_ = config;

    // Close existing log file
    if (log_file_.is_open()) {
        log_file_.close();
    }

    // Open new log file if specified
    if (!config_.file_path.empty()) {
        // Create directory if it doesn't exist
        std::filesystem::path file_path(config_.file_path);
        std::filesystem::path dir_path = file_path.parent_path();
        if (!dir_path.empty() && !std::filesystem::exists(dir_path)) {
            std::filesystem::create_directories(dir_path);
        }

        log_file_.open(config_.file_path, std::ios::app);
        if (!log_file_.is_open()) {
            std::cerr << "Failed to open log file: " << config_.file_path << std::endl;
        }
    }
}

void Logger::Debug(const std::string& message) {
    if (LogLevel::DEBUG >= config_.min_level) {
        LogInternal(LogLevel::DEBUG, message);
    }
}

void Logger::Info(const std::string& message) {
    if (LogLevel::INFO >= config_.min_level) {
        LogInternal(LogLevel::INFO, message);
    }
}

void Logger::Warn(const std::string& message) {
    if (LogLevel::WARN >= config_.min_level) {
        LogInternal(LogLevel::WARN, message);
    }
}

void Logger::Error(const std::string& message) {
    if (LogLevel::ERROR >= config_.min_level) {
        LogInternal(LogLevel::ERROR, message);
    }
}

void Logger::Fatal(const std::string& message) {
    if (LogLevel::FATAL >= config_.min_level) {
        LogInternal(LogLevel::FATAL, message);
    }
}

void Logger::LogOperation(const std::string& operation, std::chrono::microseconds duration) {
    if (LogLevel::DEBUG >= config_.min_level) {
        std::ostringstream oss;
        oss << "Operation '" << operation << "' completed in "
            << duration.count() << " microseconds";
        LogInternal(LogLevel::DEBUG, oss.str());
    }
}

std::chrono::milliseconds Logger::GetUptime() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
}

void Logger::LogInternal(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    log_count_++;

    std::string formatted = FormatMessage(level, message);

    if (config_.console_output) {
        WriteToConsole(level, formatted);
    }

    if (log_file_.is_open()) {
        WriteToFile(level, formatted);
    }
}

void Logger::WriteToFile(LogLevel level, const std::string& formatted_message) {
    if (log_file_.is_open()) {
        log_file_ << formatted_message << std::endl;
        log_file_.flush();

        // Check if we need to rotate the log file
        if (log_file_.tellp() > 0 && static_cast<size_t>(log_file_.tellp()) > config_.max_file_size) {
            RotateLogFile();
        }
    }
}

void Logger::WriteToConsole(LogLevel level, const std::string& formatted_message) {
    // Use colors for console output
    const char* color_code = "";
    switch (level) {
        case LogLevel::DEBUG: color_code = "\033[36m"; break;    // Cyan
        case LogLevel::INFO:  color_code = "\033[32m"; break;    // Green
        case LogLevel::WARN:  color_code = "\033[33m"; break;    // Yellow
        case LogLevel::ERROR: color_code = "\033[31m"; break;    // Red
        case LogLevel::FATAL: color_code = "\033[35m"; break;    // Magenta
        default: break;
    }

    std::cout << color_code << formatted_message << "\033[0m" << std::endl;
}

void Logger::RotateLogFile() {
    if (config_.file_path.empty() || !log_file_.is_open()) {
        return;
    }

    log_file_.close();

    // Rotate existing log files
    for (int i = static_cast<int>(config_.max_files) - 1; i > 0; --i) {
        std::string old_file = config_.file_path + "." + std::to_string(i);
        std::string new_file;

        if (i == static_cast<int>(config_.max_files) - 1) {
            // Delete the oldest log file
            std::filesystem::remove(old_file);
        } else {
            new_file = config_.file_path + "." + std::to_string(i + 1);
            std::filesystem::rename(old_file, new_file);
        }
    }

    // Move current log file to .1
    std::string backup_file = config_.file_path + ".1";
    std::filesystem::rename(config_.file_path, backup_file);

    // Create new log file
    log_file_.open(config_.file_path, std::ios::app);
}

std::string Logger::FormatMessage(LogLevel level, const std::string& message) const {
    std::ostringstream oss;

    if (config_.timestamp_format) {
        oss << "[" << FormatTimestamp() << "] ";
    }

    oss << "[" << LevelToString(level) << "] ";

    if (config_.thread_id) {
        oss << "[" << FormatThreadId() << "] ";
    }

    oss << message;

    return oss.str();
}

std::string Logger::FormatTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}

std::string Logger::FormatThreadId() const {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return oss.str();
}

std::string Logger::LevelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

template<typename... Args>
std::string Logger::FormatString(const std::string& format, Args... args) {
    int size = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
    if (size <= 0) {
        return format;
    }

    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1);
}

// Template method implementations
template<typename... Args>
void Logger::Debugf(const std::string& format, Args... args) {
    Debug(FormatString(format, args...));
}

template<typename... Args>
void Logger::Infof(const std::string& format, Args... args) {
    Info(FormatString(format, args...));
}

template<typename... Args>
void Logger::Warnf(const std::string& format, Args... args) {
    Warn(FormatString(format, args...));
}

template<typename... Args>
void Logger::Errorf(const std::string& format, Args... args) {
    Error(FormatString(format, args...));
}

template<typename... Args>
void Logger::Fatalf(const std::string& format, Args... args) {
    Fatal(FormatString(format, args...));
}

// Explicit template instantiations
template void Logger::Debugf<>(const std::string& format);
template void Logger::Infof<>(const std::string& format);
template void Logger::Warnf<>(const std::string& format);
template void Logger::Errorf<>(const std::string& format);
template void Logger::Fatalf<>(const std::string& format);

template void Logger::Debugf<int>(const std::string& format, int);
template void Logger::Infof<int>(const std::string& format, int);
template void Logger::Warnf<int>(const std::string& format, int);
template void Logger::Errorf<int>(const std::string& format, int);
template void Logger::Fatalf<int>(const std::string& format, int);

template void Logger::Debugf<const char*>(const std::string& format, const char*);
template void Logger::Infof<const char*>(const std::string& format, const char*);
template void Logger::Warnf<const char*>(const std::string& format, const char*);
template void Logger::Errorf<const char*>(const std::string& format, const char*);
template void Logger::Fatalf<const char*>(const std::string& format, const char*);