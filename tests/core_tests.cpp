#include "options.h"
#include "rm_log.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
Options Parse(std::vector<std::string> arguments) {
    std::vector<char*> argv;
    for (auto& argument : arguments) argv.push_back(argument.data());
    return ParseOptions(static_cast<int>(argv.size()), argv.data());
}
void Reject(std::vector<std::string> arguments) {
    bool rejected = false;
    try { Parse(std::move(arguments)); }
    catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, "Invalid arguments accepted");
}
}

int main() {
    try {
        auto options = Parse({"camera_demo"});
        Require(options.show_image && options.frame_limit == 0 && options.camera_index == 0,
                "Default options changed");
        options = Parse({"camera_demo", "--no-display", "--frames", "100", "--camera-index", "2",
                         "--log-level", "trace", "--log-file", "nested/log.txt"});
        Require(!options.show_image && options.frame_limit == 100 && options.camera_index == 2 &&
                options.log_level == "trace" && options.log_file == "nested/log.txt", "Option parsing failed");
        Require(Parse({"camera_demo", "--help"}).help, "Help parsing failed");
        Reject({"camera_demo", "--frames"});
        Reject({"camera_demo", "--frames", "-1"});
        Reject({"camera_demo", "--frames", "12x"});
        Reject({"camera_demo", "--frames", "18446744073709551616"});
        Reject({"camera_demo", "--camera-index", "0"});
        Reject({"camera_demo", "--camera-index", "4294967296"});
        Reject({"camera_demo", "--log-level", "typo"});
        Reject({"camera_demo", "--log-file", ""});
        Reject({"camera_demo", "--unknown"});

        RM_LOG_INFO("Fallback before initialization works");
        utils::RMLOG::instance().Close();
        std::filesystem::create_directories("test-output");
        const auto path = std::filesystem::path("test-output/rm_log.log");
        // The test owns this single file, not the surrounding directory.
        std::filesystem::remove(path);
        INIT_LOG(path.string(), "off", "trace", "trace");
        RM_LOG_TRACE("trace marker");
        RM_LOG_DEBUG("debug marker");
        RM_LOG_INFO("info marker");
        RM_LOG_WARN("warn marker");
        RM_LOG_ERROR("error marker");
        RM_LOG_CRITICAL("critical marker");
        for (int i = 0; i < 4500; ++i) RM_LOG_DEBUG("queue item {}", i);
        SET_LOG_LEVEL("info");
        RM_LOG_DEBUG("filtered marker");
        RM_LOG_INFO("final marker");
        bool invalid_level_rejected = false;
        try { SET_LOG_LEVEL("typo"); }
        catch (const std::invalid_argument&) { invalid_level_rejected = true; }
        Require(invalid_level_rejected, "Invalid level silently disabled logging");
        utils::RMLOG::instance().Close();
        std::ifstream file(path);
        const std::string contents{std::istreambuf_iterator<char>(file), {}};
        for (const char* marker : {"trace marker", "debug marker", "info marker", "warn marker",
                                  "error marker", "critical marker", "queue item 0", "queue item 4499",
                                  "final marker", "core_tests.cpp main:"})
            Require(contents.find(marker) != std::string::npos, "Missing log message or source location");
        Require(contents.find("filtered marker") == std::string::npos, "Log level filtering failed");
        std::size_t count = 0, position = 0;
        while ((position = contents.find("queue item ", position)) != std::string::npos) {
            ++count;
            ++position;
        }
        Require(count == 4500, "Asynchronous shutdown lost messages");
        bool invalid_path_rejected = false;
        try { INIT_LOG(path.string() + "/child.log", "info", "info", "info"); }
        catch (const std::exception&) { invalid_path_rejected = true; }
        Require(invalid_path_rejected, "Invalid file path accepted");
        RM_LOG_INFO("Fallback after initialization failure works; core tests passed");
        utils::RMLOG::instance().Close();
        return 0;
    } catch (const std::exception& error) {
        RM_LOG_ERROR("Core tests failed: {}", error.what());
        utils::RMLOG::instance().Close();
        return 1;
    }
}
