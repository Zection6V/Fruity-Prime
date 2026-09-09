// Native counterpart of src/MphRead/Entities/Players/PlayerScan.cs.
#include "PlayerScan.hpp"

#include <algorithm>

namespace fruityprime::players {

void PlayerScan::set_combat_visor() noexcept {
    if (visor_ != Visor::Scan) {
        return;
    }
    visor_ = Visor::Combat;
    scanning_ = false;
    complete_ = false;
    visor_message_timer_ = visor_message_time_ = 1.0F;
    visor_message_id_ = 108;
    scan_seconds_ = 0.0F;
    scan_time_ = 0.0F;
    scanning_entity_id_ = 0;
    target_ = {};
}

void PlayerScan::reset_combat_visor() noexcept {
    if (visor_ != Visor::Scan) {
        return;
    }
    silent_visor_switch_ = true;
    set_combat_visor();
    silent_visor_switch_ = false;
}

void PlayerScan::set_scan_visor(bool silent) noexcept {
    if (visor_ == Visor::Scan) {
        return;
    }
    silent_visor_switch_ = silent;
    visor_ = Visor::Scan;
    visor_message_timer_ = visor_message_time_ = 1.0F;
    visor_message_id_ = 107;
    scanning_ = false;
    complete_ = false;
    show_dialog_confirm_ = false;
    silent_visor_switch_ = false;
}

void PlayerScan::update(float frame_seconds, const ScanTarget* target,
                        bool dialog_pause) noexcept {
    update_target(target);
    update_scanning(scan_input_down_);
    update_state(frame_seconds, dialog_pause);
}

void PlayerScan::update_target(const ScanTarget* target) noexcept {
    if (target == nullptr) {
        target_ = {};
        scanning_ = false;
        return;
    }

    const std::uint32_t identity = target->entity_id != 0
        ? target->entity_id : static_cast<std::uint32_t>(target->scan_id);
    if (identity != scanning_entity_id_) {
        complete_ = false;
        show_dialog_confirm_ = false;
        scan_seconds_ = 0.0F;
        scan_time_ = std::max(0.0F, target->scan_time);
        scanning_entity_id_ = identity;
        scanning_ = false;
    }
    target_ = *target;
}

void PlayerScan::update_scanning(bool scanning) noexcept {
    scan_input_down_ = scanning;
    if (!scanning) {
        scanning_ = false;
    } else if (target_.scan_id != 0 && target_.distance < 12.0F) {
        scanning_ = true;
    } else if (target_.scan_id != 0 && visor_message_timer_ == 0.0F) {
        visor_message_id_ = 118;
        visor_message_timer_ = visor_message_time_ = 2.0F;
    }
}

void PlayerScan::update_state(float frame_seconds, bool dialog_pause) noexcept {
    if (dialog_pause) {
        return;
    }
    const float elapsed = std::max(0.0F, frame_seconds);
    visor_message_timer_ = std::max(0.0F, visor_message_timer_ - elapsed);
    if (target_.scan_id == 0) {
        scanning_ = false;
    }
    if (!scanning_ || complete_) {
        return;
    }
    if (scan_seconds_ < scan_time_) {
        if (target_.already_logged) {
            scan_seconds_ = scan_time_;
            show_dialog_confirm_ = true;
        } else {
            scan_seconds_ += elapsed;
        }
    } else if (target_.scan_id != 0) {
        complete_ = true;
        scanning_ = false;
    }
}

void PlayerScan::begin_scan() noexcept {
    if (visor_ != Visor::Scan) {
        set_scan_visor();
    }
    scan_input_down_ = true;
    update_scanning(true);
}

void PlayerScan::cancel_scan() noexcept {
    scan_input_down_ = false;
    scanning_ = false;
    complete_ = false;
    scan_seconds_ = 0.0F;
    show_dialog_confirm_ = false;
}

void PlayerScan::complete_scan() noexcept {
    scanning_ = false;
    complete_ = true;
    scan_seconds_ = scan_time_;
}

void PlayerScan::after_scan() noexcept {
    scan_input_down_ = false;
    scanning_ = false;
    complete_ = false;
    show_dialog_confirm_ = false;
    scan_seconds_ = 0.0F;
    scan_time_ = 0.0F;
    scanning_entity_id_ = 0;
    target_ = {};
}

} // namespace fruityprime::players
