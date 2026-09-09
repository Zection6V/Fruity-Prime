#include "Mods/Input/input.hpp"

#include <cstdint>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fruityprime::input {

#ifdef _WIN32
namespace {

template <typename Function>
[[nodiscard]] Function function_from_proc(FARPROC proc) noexcept {
    static_assert(sizeof(Function) == sizeof(FARPROC));
    Function result = nullptr;
    std::memcpy(&result, &proc, sizeof(result));
    return result;
}

} // namespace
#endif

struct Gamepad::Impl {
#ifdef _WIN32
    using GetState = DWORD(WINAPI*)(DWORD, void*);
    HMODULE module = nullptr;
    GetState get_state = nullptr;
#endif
};

Gamepad::Gamepad()
    : impl_(std::make_unique<Impl>()) {
#ifdef _WIN32
    constexpr const char* Libraries[] = {"xinput1_4.dll", "xinput1_3.dll"};
    for (const char* library : Libraries) {
        impl_->module = LoadLibraryA(library);
        if (impl_->module == nullptr) {
            continue;
        }
        impl_->get_state = function_from_proc<Impl::GetState>(
            GetProcAddress(impl_->module, "XInputGetState"));
        if (impl_->get_state != nullptr) {
            break;
        }
        FreeLibrary(impl_->module);
        impl_->module = nullptr;
    }
#endif
}

Gamepad::~Gamepad() {
#ifdef _WIN32
    if (impl_ != nullptr && impl_->module != nullptr) {
        FreeLibrary(impl_->module);
        impl_->module = nullptr;
        impl_->get_state = nullptr;
    }
#endif
}

bool Gamepad::poll(GamepadState& state) noexcept {
    state = {};
#ifdef _WIN32
    if (impl_ == nullptr || impl_->get_state == nullptr) {
        return false;
    }
    struct NativeGamepad {
        std::uint16_t buttons;
        std::uint8_t left_trigger;
        std::uint8_t right_trigger;
        std::int16_t left_x;
        std::int16_t left_y;
        std::int16_t right_x;
        std::int16_t right_y;
    };
    struct NativeState {
        std::uint32_t packet_number;
        NativeGamepad gamepad;
    } native{};
    constexpr DWORD ErrorSuccess = 0;
    bool found = false;
    for (DWORD user = 0; user < 4; ++user) {
        native = {};
        if (impl_->get_state(user, &native) == ErrorSuccess) {
            found = true;
            break;
        }
    }
    if (!found) {
        return false;
    }
    state.connected = true;
    state.name = "XInput gamepad";
    constexpr std::uint16_t RawDpadUp = 0x0001;
    constexpr std::uint16_t RawDpadDown = 0x0002;
    constexpr std::uint16_t RawDpadLeft = 0x0004;
    constexpr std::uint16_t RawDpadRight = 0x0008;
    constexpr std::uint16_t RawStart = 0x0010;
    constexpr std::uint16_t RawBack = 0x0020;
    constexpr std::uint16_t RawLeftThumb = 0x0040;
    constexpr std::uint16_t RawRightThumb = 0x0080;
    constexpr std::uint16_t RawLeftBumper = 0x0100;
    constexpr std::uint16_t RawRightBumper = 0x0200;
    constexpr std::uint16_t RawA = 0x1000;
    constexpr std::uint16_t RawB = 0x2000;
    constexpr std::uint16_t RawX = 0x4000;
    constexpr std::uint16_t RawY = 0x8000;
    const std::uint16_t raw_buttons = native.gamepad.buttons;
    GamepadButtons buttons = GamepadButtons::None;
    const auto add = [&buttons, raw_buttons](std::uint16_t raw,
                                              GamepadButtons canonical) {
        if ((raw_buttons & raw) != 0) {
            buttons |= canonical;
        }
    };
    add(RawA, GamepadButtons::A);
    add(RawB, GamepadButtons::B);
    add(RawX, GamepadButtons::X);
    add(RawY, GamepadButtons::Y);
    add(RawLeftBumper, GamepadButtons::LeftBumper);
    add(RawRightBumper, GamepadButtons::RightBumper);
    add(RawBack, GamepadButtons::Back);
    add(RawStart, GamepadButtons::Start);
    add(RawLeftThumb, GamepadButtons::LeftThumb);
    add(RawRightThumb, GamepadButtons::RightThumb);
    add(RawDpadUp, GamepadButtons::DpadUp);
    add(RawDpadRight, GamepadButtons::DpadRight);
    add(RawDpadDown, GamepadButtons::DpadDown);
    add(RawDpadLeft, GamepadButtons::DpadLeft);
    if (native.gamepad.left_trigger >= 166) {
        buttons |= GamepadButtons::LeftTrigger;
    }
    if (native.gamepad.right_trigger >= 166) {
        buttons |= GamepadButtons::RightTrigger;
    }
    state.buttons = buttons;
    state.left_trigger = native.gamepad.left_trigger;
    state.right_trigger = native.gamepad.right_trigger;
    state.left_x = native.gamepad.left_x;
    state.left_y = native.gamepad.left_y;
    state.right_x = native.gamepad.right_x;
    state.right_y = native.gamepad.right_y;
    return true;
#else
    return false;
#endif
}

} // namespace fruityprime::input
