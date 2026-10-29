#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <utility>

#include <engine/core/error.hpp>

namespace engine::core {

class Config {
public:
    Config() = default;
    explicit Config(nlohmann::json data);

    [[nodiscard]] static Result<Config> from_json(nlohmann::json data);
    [[nodiscard]] static Result<Config> load(const std::filesystem::path& path);

    void merge(const Config& overlay);

    [[nodiscard]] const nlohmann::json& data() const noexcept;
    [[nodiscard]] nlohmann::json to_json() const;
    [[nodiscard]] bool contains(std::string_view pointer) const;

    template <typename T> [[nodiscard]] Result<T> get(std::string_view pointer) const;

    template <typename T> [[nodiscard]] T get_or(std::string_view pointer, T fallback) const;

private:
    nlohmann::json data_ = nlohmann::json::object();
};

template <typename T> Result<T> Config::get(std::string_view pointer) const {
    try {
        const nlohmann::json::json_pointer json_pointer{std::string(pointer)};
        if (!data_.contains(json_pointer)) {
            return make_failure<T>(make_error(ErrorCode::NotFound, "Configuration key not found.",
                                              {}, {{"pointer", std::string(pointer)}}));
        }
        return data_.at(json_pointer).get<T>();
    } catch (const std::exception& error) {
        return make_failure<T>(make_error(ErrorCode::Validation, "Configuration value is invalid.",
                                          error.what(), {{"pointer", std::string(pointer)}}));
    }
}

template <typename T> T Config::get_or(std::string_view pointer, T fallback) const {
    const auto result = get<T>(pointer);
    if (result) {
        return *result;
    }
    return std::move(fallback);
}

} // namespace engine::core
