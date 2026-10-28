#include <engine/core/log.hpp>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

namespace engine::core {
namespace {

[[nodiscard]] spdlog::level::level_enum to_spdlog(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Trace:
        return spdlog::level::trace;
    case LogLevel::Debug:
        return spdlog::level::debug;
    case LogLevel::Info:
        return spdlog::level::info;
    case LogLevel::Warn:
        return spdlog::level::warn;
    case LogLevel::Error:
        return spdlog::level::err;
    case LogLevel::Critical:
        return spdlog::level::critical;
    }
    return spdlog::level::info;
}

[[nodiscard]] std::string timestamp_utc() {
    const auto now = std::chrono::system_clock::now();
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) %
        std::chrono::milliseconds(1000);
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    const std::tm* utc = std::gmtime(&time);
    if (utc == nullptr) {
        return "1970-01-01T00:00:00.000Z";
    }

    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", utc);

    std::ostringstream output;
    output << buffer << '.' << std::setw(3) << std::setfill('0')
           << static_cast<int>(milliseconds.count()) << 'Z';
    return output.str();
}

[[nodiscard]] std::string thread_id_string() {
    std::ostringstream output;
    output << std::this_thread::get_id();
    return output.str();
}

} // namespace

struct Log::Impl {
    LogLevel level{LogLevel::Info};
    std::unique_ptr<spdlog::logger> console;
    std::unique_ptr<spdlog::logger> json;
    std::mutex mutex;
};

Log::Log(const LogOptions& options) : impl_(std::make_unique<Impl>()) {
    impl_->level = options.level;

    if (options.console) {
        auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
        impl_->console = std::make_unique<spdlog::logger>("engine.console", sink);
        impl_->console->set_pattern("%v");
        impl_->console->set_level(to_spdlog(options.level));
    }

    if (!options.json_file.empty()) {
        auto sink =
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(options.json_file.string(), true);
        impl_->json = std::make_unique<spdlog::logger>("engine.json", sink);
        impl_->json->set_pattern("%v");
        impl_->json->set_level(to_spdlog(options.level));
    }
}

Log::~Log() {
    if (!impl_) {
        return;
    }
    if (impl_->console) {
        impl_->console->flush();
    }
    if (impl_->json) {
        impl_->json->flush();
    }
}

void Log::log(LogLevel level, std::string_view category, std::string_view message,
              nlohmann::json fields) {
    if (!impl_ || level < impl_->level) {
        return;
    }
    if (!impl_->console && !impl_->json) {
        return;
    }

    std::scoped_lock lock(impl_->mutex);
    const std::string timestamp = timestamp_utc();

    if (impl_->console) {
        std::string line = timestamp;
        line += " [";
        line += log_level_name(level);
        line += "] ";
        line += std::string(category);
        line += ": ";
        line += std::string(message);
        if (!fields.empty()) {
            line += ' ';
            line += fields.dump();
        }
        impl_->console->log(to_spdlog(level), "{}", line);
        impl_->console->flush();
    }

    if (impl_->json) {
        nlohmann::json record{
            {"ts", timestamp},
            {"level", log_level_name(level)},
            {"category", std::string(category)},
            {"message", std::string(message)},
            {"thread", thread_id_string()},
            {"fields", std::move(fields)},
        };
        impl_->json->log(to_spdlog(level), "{}", record.dump());
        impl_->json->flush();
    }
}

void Log::trace(std::string_view category, std::string_view message, nlohmann::json fields) {
    log(LogLevel::Trace, category, message, std::move(fields));
}

void Log::debug(std::string_view category, std::string_view message, nlohmann::json fields) {
    log(LogLevel::Debug, category, message, std::move(fields));
}

void Log::info(std::string_view category, std::string_view message, nlohmann::json fields) {
    log(LogLevel::Info, category, message, std::move(fields));
}

void Log::warn(std::string_view category, std::string_view message, nlohmann::json fields) {
    log(LogLevel::Warn, category, message, std::move(fields));
}

void Log::error(std::string_view category, std::string_view message, nlohmann::json fields) {
    log(LogLevel::Error, category, message, std::move(fields));
}

void Log::critical(std::string_view category, std::string_view message, nlohmann::json fields) {
    log(LogLevel::Critical, category, message, std::move(fields));
}

LogLevel Log::level() const noexcept {
    return impl_ ? impl_->level : LogLevel::Info;
}

const char* log_level_name(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Trace:
        return "trace";
    case LogLevel::Debug:
        return "debug";
    case LogLevel::Info:
        return "info";
    case LogLevel::Warn:
        return "warn";
    case LogLevel::Error:
        return "error";
    case LogLevel::Critical:
        return "critical";
    }
    return "info";
}

} // namespace engine::core
