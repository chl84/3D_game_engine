#include <engine/core/log.hpp>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
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
    using namespace std::chrono;

    const auto now = system_clock::now();
    const auto since_epoch = now.time_since_epoch();
    const auto day_count = duration_cast<days>(since_epoch);
    const auto time_of_day = since_epoch - day_count;

    const auto hour_count = duration_cast<hours>(time_of_day).count();
    const auto minute_count = duration_cast<minutes>(time_of_day).count() % 60;
    const auto second_count = duration_cast<seconds>(time_of_day).count() % 60;
    const auto millisecond_count = duration_cast<milliseconds>(time_of_day).count() % 1000;

    // Civil date from days since 1970-01-01 (Howard Hinnant's algorithm).
    const std::int64_t z = static_cast<std::int64_t>(day_count.count()) + 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const auto day_of_era = static_cast<unsigned>(z - (era * 146097));
    const auto year_of_era =
        (day_of_era - (day_of_era / 1460) + (day_of_era / 36524) - (day_of_era / 146096)) / 365;
    const std::int64_t year_base = static_cast<std::int64_t>(year_of_era) + (era * 400);
    const int day_of_year = static_cast<int>(
        day_of_era - ((365 * year_of_era) + (year_of_era / 4) - (year_of_era / 100)));
    const int month_part = ((5 * day_of_year) + 2) / 153;
    const int day = day_of_year - (((153 * month_part) + 2) / 5) + 1;
    const int month = month_part + (month_part < 10 ? 3 : -9);
    const int year = static_cast<int>(year_base) + (month <= 2 ? 1 : 0);

    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << year << '-' << std::setw(2) << month << '-'
           << std::setw(2) << day << 'T' << std::setw(2) << static_cast<int>(hour_count) << ':'
           << std::setw(2) << static_cast<int>(minute_count) << ':' << std::setw(2)
           << static_cast<int>(second_count) << '.' << std::setw(3)
           << static_cast<int>(millisecond_count) << 'Z';
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
