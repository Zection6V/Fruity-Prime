#include "Mods/Launcher/Gui/launcher_input_rows.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace fruityprime::launcher::gui {
namespace {

template <typename Enum>
constexpr int enum_value(Enum value) noexcept {
    return static_cast<int>(value);
}

std::string humanize(std::string raw) {
    if (raw.size() == 2 && raw.front() == 'D'
        && std::isdigit(static_cast<unsigned char>(raw[1]))) {
        return std::string(1, raw[1]);
    }
    std::string result;
    result.reserve(raw.size() + 4);
    for (std::size_t index = 0; index < raw.size(); ++index) {
        const unsigned char character = static_cast<unsigned char>(raw[index]);
        if (index > 0 && std::isupper(character)
            && !std::isupper(static_cast<unsigned char>(raw[index - 1]))) {
            result.push_back(' ');
            result.push_back(static_cast<char>(std::tolower(character)));
        } else {
            result.push_back(static_cast<char>(character));
        }
    }
    return result;
}

std::string raw_key_name(KeyCode key) {
    const int value = enum_value(key);
    if (value >= enum_value(KeyCode::A)
        && value <= enum_value(KeyCode::Z)) {
        return std::string(1, static_cast<char>('A' + value
            - enum_value(KeyCode::A)));
    }
    if (value >= enum_value(KeyCode::D0)
        && value <= enum_value(KeyCode::D9)) {
        return "D" + std::to_string(value - enum_value(KeyCode::D0));
    }
    if (value >= enum_value(KeyCode::KeyPad0)
        && value <= enum_value(KeyCode::KeyPad9)) {
        return "KeyPad" + std::to_string(value - enum_value(KeyCode::KeyPad0));
    }
    if (value >= enum_value(KeyCode::F1)
        && value <= enum_value(KeyCode::F12)) {
        return "F" + std::to_string(value - enum_value(KeyCode::F1) + 1);
    }
    switch (key) {
    case KeyCode::Space: return "Space";
    case KeyCode::Tab: return "Tab";
    case KeyCode::Enter: return "Enter";
    case KeyCode::LeftShift: return "LeftShift";
    case KeyCode::RightShift: return "RightShift";
    case KeyCode::LeftControl: return "LeftControl";
    case KeyCode::RightControl: return "RightControl";
    case KeyCode::LeftAlt: return "LeftAlt";
    case KeyCode::RightAlt: return "RightAlt";
    case KeyCode::Left: return "Left";
    case KeyCode::Right: return "Right";
    case KeyCode::Up: return "Up";
    case KeyCode::Down: return "Down";
    case KeyCode::Insert: return "Insert";
    case KeyCode::Home: return "Home";
    case KeyCode::End: return "End";
    case KeyCode::PageUp: return "PageUp";
    case KeyCode::PageDown: return "PageDown";
    case KeyCode::CapsLock: return "CapsLock";
    case KeyCode::Minus: return "Minus";
    case KeyCode::Equal: return "Equal";
    case KeyCode::LeftBracket: return "LeftBracket";
    case KeyCode::RightBracket: return "RightBracket";
    case KeyCode::Semicolon: return "Semicolon";
    case KeyCode::Apostrophe: return "Apostrophe";
    case KeyCode::Comma: return "Comma";
    case KeyCode::Period: return "Period";
    case KeyCode::Slash: return "Slash";
    case KeyCode::Backslash: return "Backslash";
    case KeyCode::GraveAccent: return "GraveAccent";
    case KeyCode::KeyPadAdd: return "KeyPadAdd";
    case KeyCode::KeyPadSubtract: return "KeyPadSubtract";
    case KeyCode::KeyPadMultiply: return "KeyPadMultiply";
    case KeyCode::KeyPadDivide: return "KeyPadDivide";
    case KeyCode::Unknown: break;
    default: break;
    }
    return {};
}

} // namespace

std::optional<KeyCode> translate_key(PlatformKey key) noexcept {
    const int value = enum_value(key);
    const auto range = [value](PlatformKey first, PlatformKey last,
                               KeyCode target_first)
        -> std::optional<KeyCode> {
        if (value < enum_value(first) || value > enum_value(last)) {
            return std::nullopt;
        }
        return static_cast<KeyCode>(enum_value(target_first)
            + value - enum_value(first));
    };
    if (const auto letters = range(PlatformKey::A, PlatformKey::Z,
                                   KeyCode::A); letters.has_value()) {
        return letters;
    }
    if (const auto digits = range(PlatformKey::D0, PlatformKey::D9,
                                  KeyCode::D0); digits.has_value()) {
        return digits;
    }
    if (const auto keypad = range(PlatformKey::NumPad0, PlatformKey::NumPad9,
                                  KeyCode::KeyPad0); keypad.has_value()) {
        return keypad;
    }
    if (const auto function = range(PlatformKey::F1, PlatformKey::F12,
                                    KeyCode::F1); function.has_value()) {
        return function;
    }
    switch (key) {
    case PlatformKey::Space: return KeyCode::Space;
    case PlatformKey::Tab: return KeyCode::Tab;
    case PlatformKey::Enter: return KeyCode::Enter;
    case PlatformKey::LeftShift: return KeyCode::LeftShift;
    case PlatformKey::RightShift: return KeyCode::RightShift;
    case PlatformKey::LeftCtrl: return KeyCode::LeftControl;
    case PlatformKey::RightCtrl: return KeyCode::RightControl;
    case PlatformKey::LeftAlt: return KeyCode::LeftAlt;
    case PlatformKey::RightAlt: return KeyCode::RightAlt;
    case PlatformKey::Left: return KeyCode::Left;
    case PlatformKey::Right: return KeyCode::Right;
    case PlatformKey::Up: return KeyCode::Up;
    case PlatformKey::Down: return KeyCode::Down;
    case PlatformKey::Insert: return KeyCode::Insert;
    case PlatformKey::Home: return KeyCode::Home;
    case PlatformKey::End: return KeyCode::End;
    case PlatformKey::PageUp: return KeyCode::PageUp;
    case PlatformKey::PageDown: return KeyCode::PageDown;
    case PlatformKey::CapsLock: return KeyCode::CapsLock;
    case PlatformKey::OemMinus: return KeyCode::Minus;
    case PlatformKey::OemPlus: return KeyCode::Equal;
    case PlatformKey::OemOpenBrackets: return KeyCode::LeftBracket;
    case PlatformKey::OemCloseBrackets: return KeyCode::RightBracket;
    case PlatformKey::OemSemicolon: return KeyCode::Semicolon;
    case PlatformKey::OemQuotes: return KeyCode::Apostrophe;
    case PlatformKey::OemComma: return KeyCode::Comma;
    case PlatformKey::OemPeriod: return KeyCode::Period;
    case PlatformKey::OemQuestion: return KeyCode::Slash;
    case PlatformKey::OemBackslash:
    case PlatformKey::OemPipe: return KeyCode::Backslash;
    case PlatformKey::OemTilde: return KeyCode::GraveAccent;
    case PlatformKey::Add: return KeyCode::KeyPadAdd;
    case PlatformKey::Subtract: return KeyCode::KeyPadSubtract;
    case PlatformKey::Multiply: return KeyCode::KeyPadMultiply;
    case PlatformKey::Divide: return KeyCode::KeyPadDivide;
    case PlatformKey::Unknown:
    case PlatformKey::Escape:
    case PlatformKey::Back:
    case PlatformKey::Delete: break;
    default: break;
    }
    return std::nullopt;
}

std::string key_name(KeyCode key) {
    if (key == KeyCode::Unknown) {
        return "unbound";
    }
    return humanize(raw_key_name(key));
}

std::string describe_binding(const KeyBinding& binding) {
    switch (binding.type) {
    case BindingType::Mouse:
        switch (binding.mouse) {
        case MouseButton::Left: return "Mouse left";
        case MouseButton::Right: return "Mouse right";
        case MouseButton::Middle: return "Mouse middle";
        default:
            return "Mouse "
                + std::to_string(static_cast<int>(binding.mouse) + 1);
        }
    case BindingType::ScrollUp: return "Scroll up";
    case BindingType::ScrollDown: return "Scroll down";
    case BindingType::Key: return key_name(binding.key);
    }
    return "unbound";
}

std::string action_name(std::string_view property_name) {
    std::string name(property_name);
    if (name == "Pause") {
        name = "Scoreboard";
    } else if (name == "RolltLeft") {
        name = "Roll left";
    }
    return humanize(std::move(name));
}

KeyRow::KeyRow(std::string action, KeyBinding binding, double label_width)
    : action_(std::move(action)), binding_(binding), label_width_(label_width) {}

RowRect KeyRow::box(double bounds_width,
                    double bounds_height) const noexcept {
    return RowRect{label_width_, BoxY,
                   std::max(MinimumBoxWidth, bounds_width - label_width_
                       - BoxMargin),
                   bounds_height - BoxHeightInset};
}

void KeyRow::set_rebound_handler(ReboundHandler handler) {
    rebound_handler_ = std::move(handler);
}

bool KeyRow::pointer_press(double x, double y, double bounds_width,
                            std::optional<MouseButton> button,
                            double bounds_height) {
    if (!listening_) {
        if (box(bounds_width, bounds_height).contains(x, y)) {
            listen();
        }
        return true;
    }
    if (button.has_value()) {
        rebind_mouse(*button);
        done();
    }
    return true;
}

bool KeyRow::pointer_wheel(double delta_y) {
    if (!listening_ || delta_y == 0.0) {
        return false;
    }
    rebind_scroll(delta_y > 0.0);
    done();
    return true;
}

bool KeyRow::key_press(PlatformKey key) {
    if (!listening_) {
        if (key == PlatformKey::Enter || key == PlatformKey::Space) {
            listen();
            return true;
        }
        return false;
    }

    // Once listening, every key is consumed so focus navigation and closing
    // cannot steal a candidate binding from the row.
    if (key == PlatformKey::Escape) {
        done();
        return true;
    }
    if (key == PlatformKey::Back || key == PlatformKey::Delete) {
        rebind_key(KeyCode::Unknown);
        done();
        return true;
    }
    if (const auto translated = translate_key(key); translated.has_value()) {
        rebind_key(*translated);
        done();
    }
    return true;
}

void KeyRow::lost_focus() noexcept {
    listening_ = false;
}

void KeyRow::done() {
    listening_ = false;
    if (rebound_handler_) {
        rebound_handler_(binding_);
    }
}

void KeyRow::rebind_key(KeyCode key) {
    binding_.type = BindingType::Key;
    binding_.key = key;
}

void KeyRow::rebind_mouse(MouseButton button) {
    binding_.type = BindingType::Mouse;
    binding_.key = KeyCode::Unknown;
    binding_.mouse = button;
}

void KeyRow::rebind_scroll(bool up) {
    binding_.type = up ? BindingType::ScrollUp : BindingType::ScrollDown;
    binding_.key = KeyCode::Unknown;
    binding_.mouse = MouseButton::Left;
}

} // namespace fruityprime::launcher::gui
