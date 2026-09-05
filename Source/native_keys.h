#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace qoh_keys {

// QOH99 SE reads/writes this raw little-endian structure at 0x004851E0 /
// 0x00485390. The active profile is a DWORD at offset 0. Each profile has
// four players, but this launcher edits only players 1 and 2.
inline constexpr std::size_t kConfigSize = 0x1F6C;
inline constexpr std::size_t kProfileCount = 5;
inline constexpr std::size_t kProfileStride = 0x2FC;
inline constexpr std::size_t kKeyTableOffset = 0x180;
inline constexpr std::size_t kPlayerStride = 0x60;
inline constexpr std::size_t kRecordStride = 12;
inline constexpr std::size_t kPlayerCount = 2;
inline constexpr std::size_t kActionCount = 8;

struct KeyBinding {
    std::uint32_t kind; // 0: keyboard; 1..4: joystick device.
    std::uint32_t code; // Keyboard: DirectInput scan code; joystick: bit mask.
};

inline bool operator==(const KeyBinding& a, const KeyBinding& b) {
    return a.kind == b.kind && a.code == b.code;
}

inline bool operator!=(const KeyBinding& a, const KeyBinding& b) {
    return !(a == b);
}

using KeyBindings = std::array<std::array<KeyBinding, kActionCount>, kPlayerCount>;

// Order: up, down, left, right, A, B, C, D. Defaults come from the original
// executable's initialization routine at 0x00484D70 (DIK, not virtual keys).
inline constexpr KeyBindings kDefaultKeys{{
    {{{0, 0xC8}, {0, 0xD0}, {0, 0xCB}, {0, 0xCD},
      {0, 0x2C}, {0, 0x2D}, {0, 0x2E}, {0, 0x2F}}},
    {{{0, 0x17}, {0, 0x25}, {0, 0x24}, {0, 0x26},
      {0, 0x1E}, {0, 0x1F}, {0, 0x20}, {0, 0x21}}}
}};

namespace detail {

inline std::uint32_t Read32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) |
        (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
        (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
        (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

inline void Write32(std::vector<std::uint8_t>& bytes, std::size_t offset,
                    std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
    }
}

inline std::size_t Validate(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() == 0x1E34) {
        throw std::runtime_error("Older QOHcnf.key format (7732 bytes) is not supported. Save it with the QOH99 SE configuration tool first.");
    }
    if (bytes.size() != kConfigSize) {
        throw std::runtime_error("Unsupported QOHcnf.key size; expected exactly 8044 bytes.");
    }
    const auto profile = Read32(bytes, 0);
    if (profile >= kProfileCount) {
        throw std::runtime_error("Invalid QOHcnf.key active profile; expected 0 through 4.");
    }
    return kKeyTableOffset + static_cast<std::size_t>(profile) * kProfileStride;
}

} // namespace detail

inline KeyBindings DecodeKeys(const std::vector<std::uint8_t>& bytes) {
    const auto table = detail::Validate(bytes);
    KeyBindings bindings{};
    for (std::size_t player = 0; player < kPlayerCount; ++player) {
        for (std::size_t action = 0; action < kActionCount; ++action) {
            const auto offset = table + player * kPlayerStride + action * kRecordStride;
            bindings[player][action] = {detail::Read32(bytes, offset),
                                        detail::Read32(bytes, offset + 4)};
        }
    }
    return bindings;
}

inline std::vector<std::uint8_t> EncodeKeys(const std::vector<std::uint8_t>& original,
                                          const KeyBindings& bindings) {
    const auto table = detail::Validate(original);
    const auto previous = DecodeKeys(original);
    auto result = original;
    for (std::size_t player = 0; player < kPlayerCount; ++player) {
        for (std::size_t action = 0; action < kActionCount; ++action) {
            const auto& binding = bindings[player][action];
            if (binding == previous[player][action]) {
                continue;
            }
            if (binding.kind != 0 || binding.code == 0 || binding.code > 0xFF) {
                throw std::runtime_error("Changed bindings must be keyboard keys with a DirectInput scan code from 1 through 255.");
            }
            const auto offset = table + player * kPlayerStride + action * kRecordStride;
            detail::Write32(result, offset, binding.kind);
            detail::Write32(result, offset + 4, binding.code);
            // +8 is a persisted native function pointer. Leave it untouched:
            // QOH rebuilds these callbacks from device kinds at 0x00484B50.
        }
    }
    return result;
}

} // namespace qoh_keys
