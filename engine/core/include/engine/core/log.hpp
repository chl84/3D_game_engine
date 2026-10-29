#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace engine::core {

enum class LogLevel { Trace, Debug, Info, Warn, Error, Critical };

struct LogOptions {
    LogLevel level{LogLevel::Info};
    bool console{true};
    std::filesystem::path json_file;
};

class Log {
public:
    explicit Log(const LogOptions& options);
    ~Log();

    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    Log(Log&&) = delete;
    Log& operator=(Log&&) = delete;

    void log(LogLevel level, std::string_view category, std::string_view message,
             nlohmann::json fields = nlohmann::json::object());

    void trace(std::string_view category, std::string_view message,
               nlohmann::json fields = nlohmann::json::object());
    void debug(std::string_view category, std::string_view message,
               nlohmann::json fields = nlohmann::json::object());
    void info(std::string_view category, std::string_view message,
              nlohmann::json fields = nlohmann::json::object());
    void warn(std::string_view category, std::string_view message,
              nlohmann::json fields = nlohmann::json::object());
    void error(std::string_view category, std::string_view message,
               nlohmann::json fields = nlohmann::json::object());
    void critical(std::string_view category, std::string_view message,
                  nlohmann::json fields = nlohmann::json::object());

    [[nodiscard]] LogLevel level() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] const char* log_level_name(LogLevel level) noexcept;

[[nodiscard]] std::string format_timestamp_utc(std::chrono::system_clock::time_point time);

} // namespace engine::core
