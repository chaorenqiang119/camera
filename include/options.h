#pragma once

#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string>

struct Options {
    bool show_image = true;
    bool help = false;
    uint32_t camera_index = 0; // 0: first supported device
    uint64_t frame_limit = 0; // 0: unlimited
    std::string log_file = "logs/daheng.log";
    std::string log_level = "debug";
};

inline Options ParseOptions(int argc, char* argv[]) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help" || argument == "-h") options.help = true;
        else if (argument == "--no-display") options.show_image = false;
        else {
            if (argument != "--frames" && argument != "--camera-index" &&
                argument != "--log-file" && argument != "--log-level")
                throw std::invalid_argument("Unknown option: " + argument);
            if (++i >= argc) throw std::invalid_argument("Missing value for " + argument);
            const std::string value = argv[i];
            if (argument == "--log-file") {
                if (value.empty()) throw std::invalid_argument("Log file path must not be empty");
                options.log_file = value;
            } else if (argument == "--log-level") {
                if (value != "trace" && value != "debug" && value != "info" &&
                    value != "warn" && value != "error" && value != "critical")
                    throw std::invalid_argument("Invalid log level: " + value);
                options.log_level = value;
            } else {
                uint64_t number = 0;
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || value.empty())
                    throw std::invalid_argument("Invalid integer for " + argument + ": " + value);
                if (argument == "--frames") options.frame_limit = number;
                else {
                    if (number == 0 || number > UINT32_MAX)
                        throw std::invalid_argument("Camera index must be in [1, 4294967295]");
                    options.camera_index = static_cast<uint32_t>(number);
                }
            }
        }
    }
    return options;
}

inline constexpr const char* kUsage =
    "Usage: camera_demo [--no-display] [--frames N] [--camera-index N]\n"
    "                   [--log-file PATH] [--log-level LEVEL] [--help]\n"
    "Defaults: display enabled, unlimited frames, first USB3/GigE camera.\n"
    "LEVEL: trace, debug, info, warn, error, critical.\n"
    "Keys: e/d exposure, a/q gain, s/w gamma, ESC exit; Ctrl+C also exits.";
