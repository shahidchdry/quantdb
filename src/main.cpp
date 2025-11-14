#include "server/server.h"
#include <iostream>
#include <getopt.h>
#include <string>

void PrintUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -p, --port <port>        Set server port (default: 6379)" << std::endl;
    std::cout << "  -b, --bind <address>     Set bind address (default: 0.0.0.0)" << std::endl;
    std::cout << "  -c, --config <file>      Load configuration from file" << std::endl;
    std::cout << "  -h, --help               Show this help message" << std::endl;
    std::cout << "  -v, --version            Show version information" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << " -p 6380 -b 127.0.0.1" << std::endl;
    std::cout << "  " << program_name << " --config quantdb.yaml" << std::endl;
}

void PrintVersion() {
    std::cout << "QuantDB version 1.0.0" << std::endl;
    std::cout << "High-performance Redis alternative using Swiss Tables" << std::endl;
}

int main(int argc, char* argv[]) {
    Server server;
    uint16_t port = 6379;
    std::string bind_address = "0.0.0.0";
    std::string config_file;

    // Parse command line arguments
    static struct option long_options[] = {
        {"port", required_argument, 0, 'p'},
        {"bind", required_argument, 0, 'b'},
        {"config", required_argument, 0, 'c'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int c;

    while ((c = getopt_long(argc, argv, "p:b:c:hv", long_options, &option_index)) != -1) {
        switch (c) {
            case 'p':
                try {
                    int port_num = std::stoi(optarg);
                    if (port_num <= 0 || port_num > 65535) {
                        std::cerr << "Error: Port must be between 1 and 65535" << std::endl;
                        return 1;
                    }
                    port = static_cast<uint16_t>(port_num);
                } catch (const std::exception& e) {
                    std::cerr << "Error: Invalid port number: " << optarg << std::endl;
                    return 1;
                }
                break;

            case 'b':
                bind_address = optarg;
                break;

            case 'c':
                config_file = optarg;
                break;

            case 'h':
                PrintUsage(argv[0]);
                return 0;

            case 'v':
                PrintVersion();
                return 0;

            case '?':
                std::cerr << "Use -h or --help for usage information" << std::endl;
                return 1;

            default:
                break;
        }
    }

    // Configure server
    server.SetPort(port);
    server.SetBindAddress(bind_address);
    if (!config_file.empty()) {
        server.SetConfigFile(config_file);
    }

    // Start the server
    if (!server.Start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    // Wait for server to finish (via signal)
    server.Wait();

    return 0;
}