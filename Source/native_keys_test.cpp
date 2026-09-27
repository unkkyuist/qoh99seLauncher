#include "native_keys.h"

#include <functional>
#include <iostream>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void Throws(const std::function<void()>& operation, const char* messageFragment) {
    try {
        operation();
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find(messageFragment) != std::string::npos,
                "Validation failure did not identify the expected problem.");
        return;
    }
    throw std::runtime_error("Expected a validation failure.");
}

void Put32(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes.at(at + i) = static_cast<std::uint8_t>(value >> (8 * i));
    }
}

std::vector<std::uint8_t> Fixture(std::uint32_t profile) {
    std::vector<std::uint8_t> bytes(qoh_keys::kConfigSize);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<std::uint8_t>((i * 73 + 19) & 255);
    }
    Put32(bytes, 0, profile);
    const auto base = 0x180 + profile * 0x2FC;
    for (std::size_t player = 0; player < 2; ++player) {
        for (std::size_t action = 0; action < 8; ++action) {
            const auto record = base + player * 0x60 + action * 12;
            Put32(bytes, record, 0);
            Put32(bytes, record + 4, qoh_keys::kDefaultKeys[player][action].code);
            Put32(bytes, record + 8, 0x00484B00);
        }
    }
    return bytes;
}

void RunTests() {
    // Independent file offsets: DWORD header + each 0x2FC-byte profile + 0x6C.
    const std::size_t fullscreenOffsets[] = {0x70,0x36C,0x668,0x964,0xC60};
    for (std::uint32_t profile=0; profile<5; ++profile) {
        auto bytes=Fixture(profile);
        Put32(bytes,fullscreenOffsets[profile],0);
        auto expected=bytes;
        Put32(expected,fullscreenOffsets[profile],1);
        Require(qoh_keys::EnableNativeFullscreen(bytes)==expected,
                "Fullscreen normalization changed keys or another profile.");
        Require(qoh_keys::EnableNativeFullscreen(expected)==expected,
                "An already fullscreen profile was changed.");
        Put32(bytes,fullscreenOffsets[profile],2);
        Throws([&] { qoh_keys::EnableNativeFullscreen(bytes); }, "native display");
    }
    Throws([] { qoh_keys::EnableNativeFullscreen(std::vector<std::uint8_t>(7732)); }, "Older");
    for (std::uint32_t profile = 0; profile < 5; ++profile) {
        const auto bytes = Fixture(profile);
        Require(qoh_keys::DecodeKeys(bytes) == qoh_keys::kDefaultKeys,
                "Decoded the wrong profile, player, action, or scan code.");
        Require(qoh_keys::EncodeKeys(bytes, qoh_keys::DecodeKeys(bytes)) == bytes,
                "An unchanged configuration must round-trip byte-for-byte.");
    }

    auto original = Fixture(1);
    // Use literal documented file offsets as an independent layout check.
    // Profile 1 / P1 / Up: 0x180 + 0x2FC = 0x47C.
    // Profile 1 / P2 / D:  0x47C + 0x60 + 7*12 = 0x530.
    Put32(original, 0x47C, 2);
    Put32(original, 0x480, 0x10000);
    Put32(original, 0x484, 0x00484B20);
    auto keys = qoh_keys::DecodeKeys(original);
    Require(keys[0][0] == qoh_keys::KeyBinding{2, 0x10000},
            "Joystick binding must be decoded without converting it.");
    keys[1][7] = {0, 0x39}; // Space, changing exactly the P2 D scan code.
    const auto modified = qoh_keys::EncodeKeys(original, keys);
    auto expected = original;
    Put32(expected, 0x534, 0x39);
    Require(modified == expected,
            "Changing P2 D touched another row, profile, callback, or unrelated byte.");
    Require(original[0x534] == 0x21, "Encoding mutated the original buffer.");

    // Rebinding an existing joystick action to a key changes only its two
    // binding DWORDs; the existing joystick callback stays byte-identical.
    keys = qoh_keys::DecodeKeys(original);
    keys[0][0] = {0, 0x11}; // W
    expected = original;
    Put32(expected, 0x47C, 0);
    Put32(expected, 0x480, 0x11);
    Require(qoh_keys::EncodeKeys(original, keys) == expected,
            "Joystick-to-keyboard edit failed to preserve the callback or other bytes.");

    Throws([] { qoh_keys::DecodeKeys({}); }, "8044");
    Throws([] { qoh_keys::DecodeKeys(std::vector<std::uint8_t>(0x1E34)); }, "Older");
    Throws([] { qoh_keys::DecodeKeys(std::vector<std::uint8_t>(8045)); }, "8044");
    for (const auto invalidProfile : {5U, 0xFFFFFFFFU}) {
        auto invalid = original;
        Put32(invalid, 0, invalidProfile);
        Throws([&] { qoh_keys::DecodeKeys(invalid); }, "active profile");
        Throws([&] { qoh_keys::EncodeKeys(invalid, keys); }, "active profile");
    }
    for (const auto invalidBinding : {qoh_keys::KeyBinding{1, 1},
                                      qoh_keys::KeyBinding{0, 0},
                                      qoh_keys::KeyBinding{0, 256}}) {
        auto invalid = qoh_keys::DecodeKeys(original);
        invalid[1][0] = invalidBinding;
        Throws([&] { qoh_keys::EncodeKeys(original, invalid); }, "scan code");
    }
    Require(qoh_keys::EncodeKeys(original, qoh_keys::DecodeKeys(original)) == original,
            "Untouched joystick configuration did not round-trip.");
}

} // namespace

int main() {
    try {
        RunTests();
        std::cout << "PASS: native QOH keys - all profiles, offsets, preservation, joystick, and validation\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
