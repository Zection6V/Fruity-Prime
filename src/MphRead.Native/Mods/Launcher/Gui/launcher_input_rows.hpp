#pragma once

#include "Mods/Input/input.hpp"
#include "Mods/Launcher/Gui/launcher_rows.hpp"
#include "Mods/Network/pad_bindings.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace fruityprime::launcher::gui {

// These values are the toolkit-neutral edge of KeyRow.  An Avalonia, Win32,
// or Android head maps its own key event to PlatformKey once; the binding and
// listening state below then have one behavior on every platform.
enum class PlatformKey : std::uint16_t {
    Unknown,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    D0,
    D1,
    D2,
    D3,
    D4,
    D5,
    D6,
    D7,
    D8,
    D9,
    NumPad0,
    NumPad1,
    NumPad2,
    NumPad3,
    NumPad4,
    NumPad5,
    NumPad6,
    NumPad7,
    NumPad8,
    NumPad9,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    Space,
    Tab,
    Enter,
    Escape,
    Back,
    Delete,
    LeftShift,
    RightShift,
    LeftCtrl,
    RightCtrl,
    LeftAlt,
    RightAlt,
    Left,
    Right,
    Up,
    Down,
    Insert,
    Home,
    End,
    PageUp,
    PageDown,
    CapsLock,
    OemMinus,
    OemPlus,
    OemOpenBrackets,
    OemCloseBrackets,
    OemSemicolon,
    OemQuotes,
    OemComma,
    OemPeriod,
    OemQuestion,
    OemBackslash,
    OemPipe,
    OemTilde,
    Add,
    Subtract,
    Multiply,
    Divide,
};

enum class KeyCode : std::uint16_t {
    Unknown,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    D0,
    D1,
    D2,
    D3,
    D4,
    D5,
    D6,
    D7,
    D8,
    D9,
    KeyPad0,
    KeyPad1,
    KeyPad2,
    KeyPad3,
    KeyPad4,
    KeyPad5,
    KeyPad6,
    KeyPad7,
    KeyPad8,
    KeyPad9,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    Space,
    Tab,
    Enter,
    LeftShift,
    RightShift,
    LeftControl,
    RightControl,
    LeftAlt,
    RightAlt,
    Left,
    Right,
    Up,
    Down,
    Insert,
    Home,
    End,
    PageUp,
    PageDown,
    CapsLock,
    Minus,
    Equal,
    LeftBracket,
    RightBracket,
    Semicolon,
    Apostrophe,
    Comma,
    Period,
    Slash,
    Backslash,
    GraveAccent,
    KeyPadAdd,
    KeyPadSubtract,
    KeyPadMultiply,
    KeyPadDivide,
};

enum class BindingType : std::uint8_t {
    Key,
    Mouse,
    ScrollUp,
    ScrollDown,
};

enum class MouseButton : std::uint8_t {
    Left,
    Right,
    Middle,
    Button4,
    Button5,
    Button6,
    Button7,
    Button8,
};

struct KeyBinding final {
    BindingType type = BindingType::Key;
    KeyCode key = KeyCode::Unknown;
    MouseButton mouse = MouseButton::Left;

    friend bool operator==(const KeyBinding&, const KeyBinding&) = default;
};

[[nodiscard]] std::optional<KeyCode> translate_key(
    PlatformKey key) noexcept;
[[nodiscard]] std::string key_name(KeyCode key);
[[nodiscard]] std::string describe_binding(const KeyBinding& binding);
[[nodiscard]] std::string action_name(std::string_view property_name);

class KeyRow final {
public:
    static constexpr double Height = 32.0;
    static constexpr double DefaultLabelWidth = 160.0;
    static constexpr double BoxY = 2.0;
    static constexpr double BoxMargin = 4.0;
    static constexpr double BoxHeightInset = 4.0;
    static constexpr double MinimumBoxWidth = 60.0;

    using ReboundHandler = std::function<void(const KeyBinding&)>;

    explicit KeyRow(std::string action, KeyBinding binding = {},
                    double label_width = DefaultLabelWidth);

    [[nodiscard]] const std::string& action() const noexcept { return action_; }
    [[nodiscard]] const KeyBinding& binding() const noexcept { return binding_; }
    void set_binding(KeyBinding binding) noexcept { binding_ = binding; }
    [[nodiscard]] double label_width() const noexcept { return label_width_; }
    [[nodiscard]] RowRect box(double bounds_width,
                              double bounds_height = Height) const noexcept;

    void set_rebound_handler(ReboundHandler handler);
    [[nodiscard]] bool listening() const noexcept { return listening_; }
    [[nodiscard]] bool hot() const noexcept { return hot_; }
    void pointer_enter() noexcept { hot_ = true; }
    void pointer_exit() noexcept { hot_ = false; }

    // Returns whether the event was consumed.  The first click on the box
    // starts listening; a later mouse press is the new binding.
    [[nodiscard]] bool pointer_press(
        double x, double y, double bounds_width,
        std::optional<MouseButton> button = std::nullopt,
        double bounds_height = Height);
    [[nodiscard]] bool pointer_wheel(double delta_y);
    [[nodiscard]] bool key_press(PlatformKey key);
    void lost_focus() noexcept;

private:
    void listen() noexcept { listening_ = true; }
    void done();
    void rebind_key(KeyCode key);
    void rebind_mouse(MouseButton button);
    void rebind_scroll(bool up);

    std::string action_;
    KeyBinding binding_{};
    double label_width_ = DefaultLabelWidth;
    bool listening_ = false;
    bool hot_ = false;
    ReboundHandler rebound_handler_;
};

class PadRow final {
public:
    static constexpr double Height = 32.0;
    static constexpr double DefaultLabelWidth = 160.0;
    static constexpr double BoxY = 2.0;
    static constexpr double BoxMargin = 4.0;
    static constexpr double BoxHeightInset = 4.0;
    static constexpr double MinimumBoxWidth = 60.0;

    using Buttons = input::GamepadButtons;
    using ReboundHandler = std::function<void()>;

    explicit PadRow(input::PadAction action,
                    double label_width = DefaultLabelWidth);

    [[nodiscard]] input::PadAction action() const noexcept { return action_; }
    [[nodiscard]] std::string label() const;
    [[nodiscard]] Buttons binding() const noexcept;
    [[nodiscard]] std::string description() const;
    [[nodiscard]] double label_width() const noexcept { return label_width_; }
    [[nodiscard]] RowRect box(double bounds_width,
                              double bounds_height = Height) const noexcept;

    void set_rebound_handler(ReboundHandler handler);
    [[nodiscard]] bool listening() const noexcept { return listening_; }
    [[nodiscard]] bool hot() const noexcept { return hot_; }
    void pointer_enter() noexcept { hot_ = true; }
    void pointer_exit() noexcept { hot_ = false; }

    // The platform head supplies the current sampled buttons when listening
    // begins. This is the baseline shield used by PadRow.cs to ignore a
    // button already held while the row was opened.
    void begin_listening(Buttons current_buttons) noexcept;
    [[nodiscard]] bool pointer_press(
        double x, double y, double bounds_width, Buttons current_buttons,
        double bounds_height = Height);
    [[nodiscard]] bool key_press(PlatformKey key, Buttons current_buttons);
    // Feed one fresh sample while listening. Returns true when a button was
    // accepted and the row left listening mode.
    [[nodiscard]] bool poll(Buttons current_buttons);
    void lost_focus();
    void detached() noexcept;

private:
    void done();

    input::PadAction action_;
    double label_width_ = DefaultLabelWidth;
    bool listening_ = false;
    bool hot_ = false;
    std::uint16_t baseline_ = 0;
    ReboundHandler rebound_handler_;
};

} // namespace fruityprime::launcher::gui
