#include "config.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <iomanip>

bool Config::LoadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << filename << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return LoadFromString(buffer.str());
}

bool Config::LoadFromString(const std::string& yaml_content) {
    std::istringstream stream(yaml_content);
    std::string line;
    std::string current_section;

    while (std::getline(stream, line)) {
        line = Trim(line);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Check for section header
        if (line[0] != ' ' && line[0] != '\t' && line.back() == ':') {
            current_section = line.substr(0, line.length() - 1);
            continue;
        }

        // Parse key-value pairs
        std::string key, value;
        if (ParseYAMLValue(line, key, value)) {
            std::string full_key = current_section.empty() ? key : current_section + "." + key;

            // Parse based on section and key
            if (current_section == "server") {
                if (key == "port") server_.port = static_cast<uint16_t>(std::stoul(value));
                else if (key == "bind_address") server_.bind_address = value;
                else if (key == "max_connections") server_.max_connections = std::stoull(value);
                else if (key == "io_uring_queue_depth") server_.io_uring_queue_depth = std::stoull(value);
                else if (key == "tcp_keepalive") server_.tcp_keepalive = (value == "true");
            }
            else if (current_section == "storage") {
                if (key == "max_memory") storage_.max_memory = ParseSize(value);
                else if (key == "eviction_policy") storage_.eviction_policy = value;
                else if (key == "initial_capacity") storage_.initial_capacity = std::stoull(value);
            }
            else if (current_section == "persistence") {
                if (key == "enabled") persistence_.enabled = (value == "true");
                else if (key == "save_interval") persistence_.save_interval = std::stoul(value);
                else if (key == "rdb_file") persistence_.rdb_file = value;
                else if (key == "compression") persistence_.compression = (value == "true");
                else if (key == "auto_save") persistence_.auto_save = (value == "true");
            }
            else if (current_section == "logging") {
                if (key == "level") logging_.level = value;
                else if (key == "file") logging_.file = value;
                else if (key == "max_size") logging_.max_size = ParseSize(value);
                else if (key == "max_files") logging_.max_files = std::stoull(value);
                else if (key == "console_output") logging_.console_output = (value == "true");
            }
            else if (current_section == "performance") {
                if (key == "worker_threads") performance_.worker_threads = std::stoull(value);
                else if (key == "cpu_affinity") performance_.cpu_affinity = (value == "true");
                else if (key == "read_buffer_size") performance_.read_buffer_size = ParseSize(value);
                else if (key == "write_buffer_size") performance_.write_buffer_size = ParseSize(value);
            }
        }
    }

    return Validate();
}

void Config::SetDefaults() {
    server_ = ServerConfig{};
    storage_ = StorageConfig{};
    persistence_ = PersistenceConfig{};
    logging_ = LoggingConfig{};
    performance_ = PerformanceConfig{};
}

bool Config::Validate() const {
    std::string errors = GetValidationErrors();
    return errors.empty();
}

std::string Config::GetValidationErrors() const {
    std::string errors;

    if (server_.port == 0 || server_.port > 65535) {
        errors += "Invalid port number (must be 1-65535)\n";
    }

    if (server_.max_connections == 0) {
        errors += "max_connections must be greater than 0\n";
    }

    if (storage_.max_memory == 0) {
        errors += "max_memory must be greater than 0\n";
    }

    if (performance_.worker_threads == 0) {
        errors += "worker_threads must be greater than 0\n";
    }

    if (persistence_.enabled && persistence_.rdb_file.empty()) {
        errors += "rdb_file cannot be empty when persistence is enabled\n";
    }

    return errors;
}

std::string Config::ToString() const {
    std::ostringstream oss;
    oss << "QuantDB Configuration:\n";
    oss << "=======================\n\n";

    oss << "Server:\n";
    oss << "  port: " << server_.port << "\n";
    oss << "  bind_address: " << server_.bind_address << "\n";
    oss << "  max_connections: " << server_.max_connections << "\n";
    oss << "  io_uring_queue_depth: " << server_.io_uring_queue_depth << "\n";
    oss << "  tcp_keepalive: " << (server_.tcp_keepalive ? "true" : "false") << "\n\n";

    oss << "Storage:\n";
    oss << "  max_memory: " << FormatSize(storage_.max_memory) << "\n";
    oss << "  eviction_policy: " << storage_.eviction_policy << "\n";
    oss << "  initial_capacity: " << storage_.initial_capacity << "\n\n";

    oss << "Persistence:\n";
    oss << "  enabled: " << (persistence_.enabled ? "true" : "false") << "\n";
    oss << "  save_interval: " << persistence_.save_interval << " seconds\n";
    oss << "  rdb_file: " << persistence_.rdb_file << "\n";
    oss << "  compression: " << (persistence_.compression ? "true" : "false") << "\n";
    oss << "  auto_save: " << (persistence_.auto_save ? "true" : "false") << "\n\n";

    oss << "Logging:\n";
    oss << "  level: " << logging_.level << "\n";
    oss << "  file: " << (logging_.file.empty() ? "(console)" : logging_.file) << "\n";
    oss << "  max_size: " << FormatSize(logging_.max_size) << "\n";
    oss << "  max_files: " << logging_.max_files << "\n";
    oss << "  console_output: " << (logging_.console_output ? "true" : "false") << "\n\n";

    oss << "Performance:\n";
    oss << "  worker_threads: " << performance_.worker_threads << "\n";
    oss << "  cpu_affinity: " << (performance_.cpu_affinity ? "true" : "false") << "\n";
    oss << "  read_buffer_size: " << FormatSize(performance_.read_buffer_size) << "\n";
    oss << "  write_buffer_size: " << FormatSize(performance_.write_buffer_size) << "\n";

    return oss.str();
}

bool Config::SaveToFile(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    file << ToString();
    return true;
}

bool Config::ParseYAMLValue(const std::string& line, std::string& key, std::string& value) {
    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos) {
        return false;
    }

    key = Trim(line.substr(0, colon_pos));
    value = Trim(line.substr(colon_pos + 1));

    // Remove quotes from value if present
    if ((value.front() == '"' && value.back() == '"') ||
        (value.front() == '\'' && value.back() == '\'')) {
        value = value.substr(1, value.length() - 2);
    }

    return !key.empty();
}

std::string Config::Trim(const std::string& str) const {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";

    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

size_t Config::ParseSize(const std::string& str) const {
    std::string num_str = Trim(str);

    // Extract the numeric part
    std::string number;
    for (char c : num_str) {
        if (std::isdigit(c) || c == '.') {
            number += c;
        } else {
            break;
        }
    }

    if (number.empty()) {
        return 0;
    }

    size_t multiplier = 1;

    // Determine the unit
    std::string unit = num_str.substr(number.length());
    std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);

    if (unit == "k" || unit == "kb") {
        multiplier = 1024;
    } else if (unit == "m" || unit == "mb") {
        multiplier = 1024 * 1024;
    } else if (unit == "g" || unit == "gb") {
        multiplier = 1024 * 1024 * 1024;
    }

    try {
        double value = std::stod(number);
        return static_cast<size_t>(value * multiplier);
    } catch (...) {
        return 0;
    }
}

std::string Config::FormatSize(size_t bytes) const {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unit_index];
    return oss.str();
}