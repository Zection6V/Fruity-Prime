#pragma once

#include "Mods/Network/net_protocol.hpp"

#include <array>
#include <cstdint>
#include <limits>

namespace fruityprime::players {

struct ScanTarget {
    std::uint16_t scan_id = 0;
    std::uint16_t alternate_scan_id = 0;
    std::uint32_t entity_id = 0;
    net::Vec3 position;
    float distance = 0.0F;
    float center_distance = std::numeric_limits<float>::max();
    float screen_x = 0.0F;
    float screen_y = 0.0F;
    float scale = 0.0F;
    float scan_time = 2.0F;
    std::int32_t category = 0;
    bool dim = false;
    bool already_logged = false;
};

enum class Visor : std::uint8_t {
    Combat,
    Scan
};

// State owned by the PlayerEntity.cs partial in PlayerScan.cs. Projection,
// collision and drawing stay with the scene/HUD host; the visor and scan state
// machine below follows the managed ordering and cartridge scan-entry time.
class PlayerScan final {
public:
    static constexpr std::array<std::array<std::uint16_t, 4>, 8> ScanIds{{
        {{0, 0, 0, 0}}, {{232, 233, 532, 233}},
        {{236, 237, 534, 237}}, {{230, 231, 531, 231}},
        {{228, 229, 530, 229}}, {{234, 235, 533, 235}},
        {{238, 239, 535, 239}}, {{225, 225, 224, 225}}
    }};

    void set_combat_visor() noexcept;
    void reset_combat_visor() noexcept;
    void set_scan_visor(bool silent = false) noexcept;
    void update(float frame_seconds, const ScanTarget* target,
                bool dialog_pause = false) noexcept;
    void update_target(const ScanTarget* target) noexcept;
    void update_scanning(bool scanning) noexcept;
    void update_state(float frame_seconds, bool dialog_pause = false) noexcept;
    void begin_scan() noexcept;
    void cancel_scan() noexcept;
    void complete_scan() noexcept;
    void after_scan() noexcept;

    [[nodiscard]] Visor visor() const noexcept { return visor_; }
    [[nodiscard]] bool scanning() const noexcept { return scanning_; }
    [[nodiscard]] bool scan_complete() const noexcept { return complete_; }
    [[nodiscard]] float scan_seconds() const noexcept { return scan_seconds_; }
    [[nodiscard]] float scan_time() const noexcept { return scan_time_; }
    [[nodiscard]] bool show_dialog_confirm() const noexcept {
        return show_dialog_confirm_;
    }
    [[nodiscard]] int visor_message_id() const noexcept {
        return visor_message_id_;
    }
    [[nodiscard]] float visor_message_time() const noexcept {
        return visor_message_time_;
    }
    [[nodiscard]] const ScanTarget& target() const noexcept { return target_; }

private:
    Visor visor_ = Visor::Combat;
    bool scanning_ = false;
    bool scan_input_down_ = false;
    bool complete_ = false;
    bool silent_visor_switch_ = false;
    bool show_dialog_confirm_ = false;
    float scan_seconds_ = 0.0F;
    float scan_time_ = 0.0F;
    float visor_message_time_ = 1.0F;
    float visor_message_timer_ = 1.0F;
    int visor_message_id_ = 0;
    std::uint32_t scanning_entity_id_ = 0;
    ScanTarget target_;
};

} // namespace fruityprime::players
