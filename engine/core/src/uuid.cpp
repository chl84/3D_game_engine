#include <engine/core/uuid.hpp>

#include <algorithm>
#include <array>
#include <ostream>
#include <random>
#include <string>
#include <utility>

namespace engine::core {
namespace {

constexpr char kHexDigits[] = "0123456789abcdef";

[[nodiscard]] int hex_value(char c) noexcept {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return (c - 'a') + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return (c - 'A') + 10;
    }
    return -1;
}

[[nodiscard]] std::uint64_t random_u64() {
    thread_local std::mt19937_64 generator{std::random_device{}()};
    return generator();
}

void set_version_and_variant(Uuid::Bytes& bytes) noexcept {
    bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0FU) | 0x40U);
    bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3FU) | 0x80U);
}

[[nodiscard]] bool valid_hyphen_position(std::size_t index) noexcept {
    return index == 8 || index == 13 || index == 18 || index == 23;
}

} // namespace

Uuid::Uuid(Bytes bytes) noexcept : bytes_(bytes) {}

Uuid Uuid::generate() {
    Bytes bytes{};
    for (std::size_t offset = 0; offset < bytes.size(); offset += 8) {
        const std::uint64_t value = random_u64();
        for (std::size_t index = 0; index < 8; ++index) {
            const std::size_t shift = (7U - index) * 8U;
            bytes[offset + index] = static_cast<std::uint8_t>((value >> shift) & 0xFFU);
        }
    }
    set_version_and_variant(bytes);
    return Uuid(bytes);
}

Result<Uuid> Uuid::parse(std::string_view text) {
    if (text.size() != 36) {
        return make_failure<Uuid>(make_error(ErrorCode::Validation, "Invalid UUID string.",
                                             "Expected the 8-4-4-4-12 hexadecimal form.",
                                             {{"uuid", std::string(text)}}));
    }

    Bytes bytes{};
    std::size_t byte_index = 0;
    int high_nibble = -1;

    for (std::size_t index = 0; index < text.size(); ++index) {
        if (valid_hyphen_position(index)) {
            if (text[index] != '-') {
                return make_failure<Uuid>(make_error(ErrorCode::Validation, "Invalid UUID string.",
                                                     "Expected a hyphen at this position.",
                                                     {{"uuid", std::string(text)}}));
            }
            continue;
        }

        const int value = hex_value(text[index]);
        if (value < 0) {
            return make_failure<Uuid>(make_error(ErrorCode::Validation, "Invalid UUID string.",
                                                 "Expected hexadecimal digits.",
                                                 {{"uuid", std::string(text)}}));
        }

        if (high_nibble < 0) {
            high_nibble = value;
        } else {
            if (byte_index >= bytes.size()) {
                return make_failure<Uuid>(make_error(ErrorCode::Validation, "Invalid UUID string.",
                                                     "Too many hexadecimal digits.",
                                                     {{"uuid", std::string(text)}}));
            }
            const auto high = static_cast<unsigned>(high_nibble);
            const auto low = static_cast<unsigned>(value);
            bytes[byte_index] = static_cast<std::uint8_t>((high << 4U) | low);
            ++byte_index;
            high_nibble = -1;
        }
    }

    if (byte_index != bytes.size() || high_nibble >= 0) {
        return make_failure<Uuid>(make_error(ErrorCode::Validation, "Invalid UUID string.",
                                             "Expected exactly 16 bytes of hexadecimal data.",
                                             {{"uuid", std::string(text)}}));
    }

    return Uuid(bytes);
}

std::string Uuid::to_string() const {
    std::string result;
    result.reserve(36);

    for (std::size_t index = 0; index < bytes_.size(); ++index) {
        if (index == 4 || index == 6 || index == 8 || index == 10) {
            result.push_back('-');
        }
        const std::uint8_t byte = bytes_[index];
        result.push_back(kHexDigits[byte >> 4U]);
        result.push_back(kHexDigits[byte & 0x0FU]);
    }

    return result;
}

const Uuid::Bytes& Uuid::bytes() const noexcept {
    return bytes_;
}

bool Uuid::is_nil() const noexcept {
    return std::ranges::all_of(bytes_, [](std::uint8_t byte) { return byte == 0; });
}

std::size_t Uuid::hash() const noexcept {
    std::size_t result = 1469598103934665603ULL;
    for (const std::uint8_t byte : bytes_) {
        result ^= static_cast<std::size_t>(byte);
        result *= 1099511628211ULL;
    }
    return result;
}

UuidGenerator::UuidGenerator(std::uint64_t seed, std::uint64_t stream) noexcept
    : state_(seed ^ (stream + 0x9E3779B97F4A7C15ULL)) {}

std::uint64_t UuidGenerator::next_word() noexcept {
    state_ += 0x9E3779B97F4A7C15ULL;
    std::uint64_t value = state_;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

Uuid UuidGenerator::next() noexcept {
    const std::uint64_t high = next_word();
    const std::uint64_t low = next_word();

    Uuid::Bytes bytes{};
    for (std::size_t index = 0; index < 8; ++index) {
        const std::size_t shift = (7U - index) * 8U;
        bytes[index] = static_cast<std::uint8_t>((high >> shift) & 0xFFU);
        bytes[8 + index] = static_cast<std::uint8_t>((low >> shift) & 0xFFU);
    }
    set_version_and_variant(bytes);
    return Uuid(bytes);
}

std::ostream& operator<<(std::ostream& out, const Uuid& uuid) {
    return out << uuid.to_string();
}

} // namespace engine::core
