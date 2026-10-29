#pragma once

#include <engine/core/error.hpp>
#include <engine/core/math.hpp>

#include <cstddef>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace engine::core {

enum class FieldType : std::uint8_t { Boolean, Integer, Number, String, Vec3, Quat };

struct FieldDescriptor {
    std::string name;
    std::string description;
    FieldType type{FieldType::String};
    bool required{true};
    nlohmann::json default_value;
    std::function<nlohmann::json(const void*)> to_json;
    std::function<Result<void>(void*, const nlohmann::json&)> from_json;
};

class ComponentDescriptor {
public:
    [[nodiscard]] const std::string& name() const noexcept;
    [[nodiscard]] const std::vector<FieldDescriptor>& fields() const noexcept;
    [[nodiscard]] const FieldDescriptor* find_field(std::string_view name) const noexcept;

    [[nodiscard]] nlohmann::json to_json(const void* component) const;
    [[nodiscard]] Result<void> from_json(void* component, const nlohmann::json& value) const;
    [[nodiscard]] nlohmann::json schema() const;

private:
    std::string name_;
    std::vector<FieldDescriptor> fields_;

    friend class ReflectionRegistry;
    template <typename T> friend class ComponentBuilder;
};

class ReflectionRegistry;

template <typename T> class ComponentBuilder {
public:
    ComponentBuilder(ReflectionRegistry& registry, std::string name);
    ~ComponentBuilder() = default;

    ComponentBuilder(const ComponentBuilder&) = delete;
    ComponentBuilder& operator=(const ComponentBuilder&) = delete;
    ComponentBuilder(ComponentBuilder&&) = default;
    ComponentBuilder& operator=(ComponentBuilder&&) = delete;

    template <typename Member>
    ComponentBuilder& field(std::string name, Member T::* member, std::string description = {}) {
        FieldDescriptor descriptor;
        descriptor.name = std::move(name);
        descriptor.description = std::move(description);
        descriptor.type = field_type<Member>();
        descriptor.required = true;

        const T defaults{};
        descriptor.default_value = encode_value(defaults.*member);
        descriptor.to_json = [member](const void* component) {
            return encode_value(static_cast<const T*>(component)->*member);
        };
        descriptor.from_json = [member](void* component,
                                        const nlohmann::json& value) -> Result<void> {
            auto decoded = decode_value<Member>(value);
            if (!decoded) {
                return tl::make_unexpected(decoded.error());
            }
            static_cast<T*>(component)->*member = std::move(*decoded);
            return {};
        };
        fields_.push_back(std::move(descriptor));
        return *this;
    }

    void build();

private:
    template <typename Member> [[nodiscard]] static constexpr FieldType field_type() {
        if constexpr (std::is_same_v<Member, bool>) {
            return FieldType::Boolean;
        } else if constexpr (std::is_same_v<Member, int>) {
            return FieldType::Integer;
        } else if constexpr (std::is_same_v<Member, float> || std::is_same_v<Member, double>) {
            return FieldType::Number;
        } else if constexpr (std::is_same_v<Member, std::string>) {
            return FieldType::String;
        } else if constexpr (std::is_same_v<Member, Vec3>) {
            return FieldType::Vec3;
        } else if constexpr (std::is_same_v<Member, Quat>) {
            return FieldType::Quat;
        } else {
            static_assert(std::is_same_v<Member, void>, "unsupported reflected field type");
        }
    }

    template <typename Member>
    [[nodiscard]] static nlohmann::json encode_value(const Member& value) {
        if constexpr (std::is_same_v<Member, Vec3>) {
            return nlohmann::json::array({value.x, value.y, value.z});
        } else if constexpr (std::is_same_v<Member, Quat>) {
            return nlohmann::json::array({value.x, value.y, value.z, value.w});
        } else {
            return nlohmann::json(value);
        }
    }

    template <typename Member>
    [[nodiscard]] static Result<Member> decode_value(const nlohmann::json& value) {
        if constexpr (std::is_same_v<Member, bool>) {
            if (!value.is_boolean()) {
                return make_failure<Member>(
                    make_error(ErrorCode::Validation, "expected a boolean"));
            }
            return value.get<bool>();
        } else if constexpr (std::is_same_v<Member, int>) {
            if (!value.is_number_integer()) {
                return make_failure<Member>(
                    make_error(ErrorCode::Validation, "expected an integer"));
            }
            return value.get<int>();
        } else if constexpr (std::is_same_v<Member, float> || std::is_same_v<Member, double>) {
            if (!value.is_number()) {
                return make_failure<Member>(make_error(ErrorCode::Validation, "expected a number"));
            }
            return static_cast<Member>(value.get<double>());
        } else if constexpr (std::is_same_v<Member, std::string>) {
            if (!value.is_string()) {
                return make_failure<Member>(make_error(ErrorCode::Validation, "expected a string"));
            }
            return value.get<std::string>();
        } else if constexpr (std::is_same_v<Member, Vec3>) {
            if (!value.is_array() || value.size() != 3) {
                return make_failure<Member>(
                    make_error(ErrorCode::Validation, "expected an array of three numbers"));
            }
            Vec3 result;
            for (std::size_t index = 0; index < 3; ++index) {
                if (!value[index].is_number()) {
                    return make_failure<Member>(
                        make_error(ErrorCode::Validation, "expected an array of three numbers"));
                }
            }
            result.x = value[0].get<float>();
            result.y = value[1].get<float>();
            result.z = value[2].get<float>();
            return result;
        } else if constexpr (std::is_same_v<Member, Quat>) {
            if (!value.is_array() || value.size() != 4) {
                return make_failure<Member>(
                    make_error(ErrorCode::Validation, "expected an array of four numbers"));
            }
            Quat result;
            for (std::size_t index = 0; index < 4; ++index) {
                if (!value[index].is_number()) {
                    return make_failure<Member>(
                        make_error(ErrorCode::Validation, "expected an array of four numbers"));
                }
            }
            result.x = value[0].get<float>();
            result.y = value[1].get<float>();
            result.z = value[2].get<float>();
            result.w = value[3].get<float>();
            return result;
        } else {
            static_assert(std::is_same_v<Member, void>, "unsupported reflected field type");
        }
    }

    ReflectionRegistry* registry_;
    std::string name_;
    std::vector<FieldDescriptor> fields_;
    bool built_{false};
};

class ReflectionRegistry {
public:
    [[nodiscard]] const ComponentDescriptor* find(std::string_view name) const noexcept;
    [[nodiscard]] std::vector<std::string> names() const;
    [[nodiscard]] Result<nlohmann::json> schema(std::string_view name) const;
    [[nodiscard]] nlohmann::json schemas() const;

    template <typename T> [[nodiscard]] ComponentBuilder<T> component(std::string name) {
        return ComponentBuilder<T>(*this, std::move(name));
    }

    void register_component(ComponentDescriptor descriptor);

private:
    std::unordered_map<std::string, ComponentDescriptor> components_;
};

template <typename T>
ComponentBuilder<T>::ComponentBuilder(ReflectionRegistry& registry, std::string name)
    : registry_(&registry), name_(std::move(name)) {}

template <typename T> void ComponentBuilder<T>::build() {
    if (built_) {
        return;
    }
    ComponentDescriptor descriptor;
    descriptor.name_ = name_;
    descriptor.fields_ = fields_;
    registry_->register_component(std::move(descriptor));
    built_ = true;
}

} // namespace engine::core
