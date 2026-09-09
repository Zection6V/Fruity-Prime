#include "Mods/Launcher/Gui/launcher_input_rows.hpp"

#include <array>
#include <algorithm>
#include <utility>

namespace fruityprime::launcher::gui {
namespace {

constexpr std::array<input::GamepadButtons, 16> kButtons{{
    input::GamepadButtons::A,
    input::GamepadButtons::B,
    input::GamepadButtons::X,
    input::GamepadButtons::Y,
    input::GamepadButtons::LeftBumper,
    input::GamepadButtons::RightBumper,
    input::GamepadButtons::Back,
    input::GamepadButtons::Start,
    input::GamepadButtons::LeftThumb,
    input::GamepadButtons::RightThumb,
    input::GamepadButtons::DpadUp,
    input::GamepadButtons::DpadRight,
    input::GamepadButtons::DpadDown,
    input::GamepadButtons::DpadLeft,
    input::GamepadButtons::LeftTrigger,
    input::GamepadButtons::RightTrigger,
}};

} // namespace

PadRow::PadRow(input::PadAction action, double label_width)
    : action_(action), label_width_(label_width) {}

std::string PadRow::label() const {
    return input::pad_bindings::name(action_);
}

PadRow::Buttons PadRow::binding() const noexcept {
    return input::pad_bindings::get(action_);
}

std::string PadRow::description() const {
    return input::pad_bindings::describe(binding());
}

RowRect PadRow::box(double bounds_width,
                    double bounds_height) const noexcept {
    return RowRect{label_width_, BoxY,
                   std::max(MinimumBoxWidth, bounds_width - label_width_
                       - BoxMargin),
                   bounds_height - BoxHeightInset};
}

void PadRow::set_rebound_handler(ReboundHandler handler) {
    rebound_handler_ = std::move(handler);
}

void PadRow::begin_listening(Buttons current_buttons) noexcept {
    listening_ = true;
    baseline_ = static_cast<std::uint16_t>(current_buttons);
}

bool PadRow::pointer_press(double x, double y, double bounds_width,
                           Buttons current_buttons, double bounds_height) {
    if (!listening_ && box(bounds_width, bounds_height).contains(x, y)) {
        begin_listening(current_buttons);
    }
    // The managed control marks every pointer press handled, including a
    // click on the label while it is not listening.
    return true;
}

bool PadRow::key_press(PlatformKey key, Buttons current_buttons) {
    if (!listening_) {
        if (key == PlatformKey::Enter || key == PlatformKey::Space) {
            begin_listening(current_buttons);
            return true;
        }
        return false;
    }

    if (key == PlatformKey::Escape) {
        done();
        return true;
    }
    if (key == PlatformKey::Back || key == PlatformKey::Delete) {
        input::pad_bindings::set(action_, Buttons::None);
        done();
        return true;
    }
    // While listening, all keyboard input belongs to this row. A pad sample
    // is accepted only from poll(), which keeps the desktop and Android
    // event paths equivalent.
    return true;
}

bool PadRow::poll(Buttons current_buttons) {
    if (!listening_) {
        return false;
    }
    const std::uint16_t current = static_cast<std::uint16_t>(current_buttons);
    const std::uint16_t pressed = current
        & static_cast<std::uint16_t>(~baseline_);
    // A held button stops shielding as soon as it is released. It can then be
    // chosen by a later press, matching PadRow.cs's baseline update.
    baseline_ = current & baseline_;
    if (pressed == 0) {
        return false;
    }

    // Enum.GetValues<GamepadButtons>() in the managed row yields the single
    // bits in declaration order. Choose the first one if a trigger or a
    // controller event reports several new bits in the same sample.
    for (const Buttons button : kButtons) {
        const auto button_bits = static_cast<std::uint16_t>(button);
        if ((pressed & button_bits) == button_bits) {
            input::pad_bindings::set(action_, button);
            done();
            return true;
        }
    }
    return false;
}

void PadRow::lost_focus() {
    if (listening_) {
        done();
    }
}

void PadRow::detached() noexcept {
    listening_ = false;
    baseline_ = 0;
}

void PadRow::done() {
    listening_ = false;
    baseline_ = 0;
    if (rebound_handler_) {
        rebound_handler_();
    }
}

} // namespace fruityprime::launcher::gui
