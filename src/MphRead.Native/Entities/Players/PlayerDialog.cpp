// Native counterpart of src/MphRead/Entities/Players/PlayerDialog.cs.
#include "PlayerDialog.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace fruityprime::players {

void PlayerDialog::show(DialogType type, std::string message,
                        float duration_seconds, EventType event,
                        PromptType prompt) noexcept {
    type_ = type;
    message_ = std::move(message);
    timer_ = std::max(0.0F, duration_seconds);
    character_timer_ = 0.0F;
    event_ = event;
    prompt_ = prompt;
    confirm_state_ = type == DialogType::YesNo
        ? ConfirmState::No : ConfirmState::Okay;
    page_ = 0;
    rebuild_pages();
}

void PlayerDialog::close() noexcept {
    type_ = DialogType::None;
    message_.clear();
    timer_ = 0.0F;
    character_timer_ = 0.0F;
    page_ = 0;
    pages_ = 0;
}

void PlayerDialog::update(float frame_seconds) noexcept {
    if (type_ == DialogType::None) {
        return;
    }
    const float seconds = std::max(0.0F, frame_seconds);
    if (timer_ > 0.0F) {
        timer_ = std::max(0.0F, timer_ - seconds);
        if (timer_ == 0.0F && type_ == DialogType::Overlay) {
            close();
            return;
        }
    }
    character_timer_ += seconds;
}

void PlayerDialog::confirm() noexcept {
    if (type_ == DialogType::YesNo) {
        confirm_state_ = confirm_state_ == ConfirmState::Yes
            ? ConfirmState::No : ConfirmState::Yes;
    } else if (type_ == DialogType::Okay || type_ == DialogType::Event
               || type_ == DialogType::Scan) {
        next_page();
    }
}

void PlayerDialog::cancel() noexcept {
    if (type_ == DialogType::YesNo) {
        confirm_state_ = ConfirmState::No;
    } else {
        close();
    }
}

void PlayerDialog::next_page() noexcept {
    if (pages_ == 0 || page_ + 1 >= pages_) {
        close();
        return;
    }
    ++page_;
    character_timer_ = 0.0F;
}

void PlayerDialog::rebuild_pages() noexcept {
    page_offsets_.fill(message_.size());
    pages_ = message_.empty() ? 0 : 1;
    std::size_t line_count = 0;
    for (std::size_t index = 0; index < message_.size(); ++index) {
        if (message_[index] != '\n') {
            continue;
        }
        ++line_count;
        if (line_count < 3) {
            continue;
        }
        if (pages_ < page_offsets_.size()) {
            page_offsets_[pages_] = index + 1;
            ++pages_;
        }
        line_count = 0;
    }
    pages_ = std::min<std::uint32_t>(
        pages_, static_cast<std::uint32_t>(page_offsets_.size()));
}

bool PlayerDialog::show_dialog(
    const DialogContext& context, DialogType type, std::string message,
    float duration_seconds, EventType event, PromptType prompt) noexcept {
    if (!context.initialized || !context.single_player
        || !context.is_main_player) {
        return false;
    }
    silent_visor_switch_ = false;
    // The overlay and HUD forms are notices rather than prompts: they do not
    // take the visor and cannot be answered.
    if (type == DialogType::Overlay || type == DialogType::Hud) {
        show(type, std::move(message), duration_seconds, event, prompt);
        return true;
    }
    if (type == DialogType::Okay || type == DialogType::YesNo
        || type == DialogType::Scan || type == DialogType::Event) {
        // A prompt needs the combat visor, so the scan visor is put away and
        // remembered -- closing the dialog is what brings it back.
        if (context.scan_visor && type != DialogType::Scan) {
            silent_visor_switch_ = true;
        }
        show(type, std::move(message), duration_seconds, event, prompt);
        return true;
    }
    // Anything else closes what was showing rather than doing nothing.
    static_cast<void>(close_dialogs(context));
    return false;
}

bool PlayerDialog::close_dialogs(const DialogContext& context) noexcept {
    if (!context.initialized || !context.single_player
        || !context.is_main_player) {
        return false;
    }
    close();
    const bool restore_visor = silent_visor_switch_;
    silent_visor_switch_ = false;
    return restore_visor;
}

} // namespace fruityprime::players
