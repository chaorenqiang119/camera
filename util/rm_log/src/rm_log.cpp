#include "rm_log.h"

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/async.h"
#include <stdexcept>

namespace utils {
namespace {
constexpr const char* kPattern =
    "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] [%s %!:%#] %v";

spdlog::level::level_enum ParseLevel(const std::string& name) {
    // spdlog 1.12 returns 'off' for unknown names, so validate explicitly.
    if (name != "trace" && name != "debug" && name != "info" && name != "warn" &&
        name != "warning" && name != "error" && name != "err" &&
        name != "critical" && name != "off")
        throw std::invalid_argument("Invalid log level: " + name);
    return spdlog::level::from_str(name);
}
}

std::shared_ptr<spdlog::logger> RMLOG::getLogger() {
    // Initialize on the main thread before starting other threads.
    if (!logger_sptr_) {
        auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
        logger_sptr_ = std::make_shared<spdlog::logger>("rm_log_fallback", sink);
        logger_sptr_->set_pattern(kPattern);
        logger_sptr_->set_level(spdlog::level::trace);
    }
    return logger_sptr_;
}

void RMLOG::Close() {
    if (logger_sptr_) logger_sptr_->flush();
    // Drain the asynchronous queue and join its worker before releasing sinks.
    spdlog::shutdown();
    logger_sptr_.reset();
}

void RMLOG::Init(const std::string& path, const std::string& console_level,
                 const std::string& file_level, const std::string& log_level) {
    const auto console = ParseLevel(console_level);
    const auto file = ParseLevel(file_level);
    const auto overall = ParseLevel(log_level);
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(console);
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(path, 5 * 1024 * 1024, 3);
    file_sink->set_level(file);
    Close();
    spdlog::init_thread_pool(4096, 1);
    spdlog::sinks_init_list sinks = {file_sink, console_sink};
    logger_sptr_ = std::make_shared<spdlog::async_logger>(
        "rm_log", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
    logger_sptr_->set_level(overall);
    logger_sptr_->flush_on(spdlog::level::warn);
    logger_sptr_->set_pattern(kPattern);
}

void RMLOG::SetLevel(const std::string& log_level) {
    getLogger()->set_level(ParseLevel(log_level));
}
}
