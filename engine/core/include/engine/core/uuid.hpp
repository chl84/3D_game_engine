#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>
#include <string_view>

#include <engine/core/error.hpp>

namespace engine::core {

class Uuid {
public:
    using Bytes = std::array<std::uint8_t, 16>;

    Uuid() noexcept = default;
    explicit Uuid(Bytes bytes) noexcept;

    [[nodiscard]] static Uuid generate();
    [[nodiscard]] static Result<Uuid> parse(std::string_view text);

    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] const Bytes& bytes() const noexcept;
    [[nodiscard]] bool is_nil() const noexcept;
    [[nodiscard]] std::size_t hash() const noexcept;

    friend bool operator==(const Uuid&, const Uuid&) noexcept = default;
    friend auto operator<=>(const Uuid&, const Uuid&) noexcept = default;

private:
    Bytes bytes_{};
};

class UuidGenerator {
public:
    explicit UuidGenerator(std::uint64_t seed, std::uint64_t stream = 0) noexcept;

    [[nodiscard]] Uuid next() noexcept;

private:
    [[nodiscard]] std::uint64_t next_word() noexcept;

    std::uint64_t state_;
};

std::ostream& operator<<(std::ostream& out, const Uuid& uuid);

} // namespace engine::core

namespace std {

template <> struct hash<engine::core::Uuid> {
    [[nodiscard]] std::size_t operator()(const engine::core::Uuid& uuid) const noexcept {
        return uuid.hash();
    }
};

} // namespace std
