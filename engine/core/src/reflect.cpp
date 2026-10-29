#include <engine/core/reflect.hpp>

#include <algorithm>
#include <utility>

namespace engine::core {
namespace {

[[nodiscard]] nlohmann::json schema_for_type(FieldType type) {
    switch (type) {
    case FieldType::Boolean:
        return {{"type", "boolean"}};
    case FieldType::Integer:
        return {{"type", "integer"}};
    case FieldType::Number:
        return {{"type", "number"}};
    case FieldType::String:
        return {{"type", "string"}};
    case FieldType::Vec3:
    case FieldType::Vec3d:
        return {
            {"type", "array"},
            {"items", {{"type", "number"}}},
            {"minItems", 3},
            {"maxItems", 3},
        };
    case FieldType::Quat:
        return {
            {"type", "array"},
            {"items", {{"type", "number"}}},
            {"minItems", 4},
            {"maxItems", 4},
        };
    }
    return {{"type", "string"}};
}

} // namespace

const std::string& ComponentDescriptor::name() const noexcept {
    return name_;
}

const std::vector<FieldDescriptor>& ComponentDescriptor::fields() const noexcept {
    return fields_;
}

const FieldDescriptor* ComponentDescriptor::find_field(std::string_view name) const noexcept {
    const auto iterator =
        std::ranges::find_if(fields_, [name](const auto& field) { return field.name == name; });
    return iterator == fields_.end() ? nullptr : &*iterator;
}

nlohmann::json ComponentDescriptor::to_json(const void* component) const {
    nlohmann::json result = nlohmann::json::object();
    for (const auto& field : fields_) {
        result[field.name] = field.to_json(component);
    }
    return result;
}

Result<void> ComponentDescriptor::from_json(void* component, const nlohmann::json& value) const {
    if (!value.is_object()) {
        return make_failure<void>(make_error(ErrorCode::Validation,
                                             "component value must be an object", {},
                                             {{"component", name_}}));
    }

    for (auto iterator = value.begin(); iterator != value.end(); ++iterator) {
        if (find_field(iterator.key()) == nullptr) {
            return make_failure<void>(
                make_error(ErrorCode::Validation, "unknown component field",
                           "Check the component schema for valid field names.",
                           {{"component", name_}, {"field", iterator.key()}}));
        }
    }

    for (const auto& field : fields_) {
        const auto iterator = value.find(field.name);
        if (iterator == value.end()) {
            continue;
        }
        auto result = field.from_json(component, *iterator);
        if (!result) {
            Error error = std::move(result.error());
            error.details["component"] = name_;
            error.details["field"] = field.name;
            return make_failure<void>(std::move(error));
        }
    }
    return {};
}

nlohmann::json ComponentDescriptor::schema() const {
    nlohmann::json properties = nlohmann::json::object();
    nlohmann::json required = nlohmann::json::array();

    for (const auto& field : fields_) {
        auto property = schema_for_type(field.type);
        property["default"] = field.default_value;
        if (!field.description.empty()) {
            property["description"] = field.description;
        }
        properties[field.name] = std::move(property);
        if (field.required) {
            required.push_back(field.name);
        }
    }

    return {
        {"$schema", "http://json-schema.org/draft-07/schema#"},
        {"$id", "https://engine.local/schemas/components/" + name_ + ".schema.json"},
        {"title", name_},
        {"type", "object"},
        {"additionalProperties", false},
        {"properties", std::move(properties)},
        {"required", std::move(required)},
    };
}

const ComponentDescriptor* ReflectionRegistry::find(std::string_view name) const noexcept {
    const auto iterator = components_.find(std::string(name));
    return iterator == components_.end() ? nullptr : &iterator->second;
}

std::vector<std::string> ReflectionRegistry::names() const {
    std::vector<std::string> result;
    result.reserve(components_.size());
    for (const auto& [name, descriptor] : components_) {
        (void)descriptor;
        result.push_back(name);
    }
    std::ranges::sort(result);
    return result;
}

Result<nlohmann::json> ReflectionRegistry::schema(std::string_view name) const {
    const ComponentDescriptor* descriptor = find(name);
    if (descriptor == nullptr) {
        return make_failure<nlohmann::json>(make_error(ErrorCode::NotFound, "unknown component", {},
                                                       {{"component", std::string(name)}}));
    }
    return descriptor->schema();
}

nlohmann::json ReflectionRegistry::schemas() const {
    nlohmann::json result = nlohmann::json::object();
    for (const auto& name : names()) {
        result[name] = components_.at(name).schema();
    }
    return result;
}

void ReflectionRegistry::register_component(ComponentDescriptor descriptor) {
    std::string name = descriptor.name();
    components_.insert_or_assign(std::move(name), std::move(descriptor));
}

} // namespace engine::core
