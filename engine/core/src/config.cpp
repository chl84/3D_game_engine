#include <engine/core/config.hpp>

#include <fstream>
#include <sstream>
#include <utility>

namespace engine::core {
namespace {

void merge_into(nlohmann::json& base, const nlohmann::json& overlay) {
    if (!base.is_object() || !overlay.is_object()) {
        base = overlay;
        return;
    }

    for (const auto& [key, value] : overlay.items()) {
        auto existing = base.find(key);
        if (existing != base.end() && existing->is_object() && value.is_object()) {
            merge_into(*existing, value);
        } else {
            base[key] = value;
        }
    }
}

} // namespace

Config::Config(nlohmann::json data) : data_(std::move(data)) {}

Result<Config> Config::from_json(nlohmann::json data) {
    if (!data.is_object()) {
        return make_failure<Config>(
            make_error(ErrorCode::Validation, "Configuration root must be a JSON object."));
    }
    return Config(std::move(data));
}

Result<Config> Config::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return make_failure<Config>(make_error(ErrorCode::Io, "Cannot open configuration file.", {},
                                               {{"path", path.string()}}));
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    try {
        return from_json(nlohmann::json::parse(buffer.str()));
    } catch (const std::exception& error) {
        return make_failure<Config>(make_error(ErrorCode::Validation,
                                               "Configuration file is not valid JSON.",
                                               error.what(), {{"path", path.string()}}));
    }
}

void Config::merge(const Config& overlay) {
    merge_into(data_, overlay.data_);
}

const nlohmann::json& Config::data() const noexcept {
    return data_;
}

nlohmann::json Config::to_json() const {
    return data_;
}

bool Config::contains(std::string_view pointer) const {
    try {
        return data_.contains(nlohmann::json::json_pointer{std::string(pointer)});
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace engine::core
