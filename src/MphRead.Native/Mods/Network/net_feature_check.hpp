#pragma once

#include "Metadata/metadata.hpp"
#include "Mods/Network/net_protocol.hpp"
#include "Mods/Network/net_test_script.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <ostream>
#include <string>
#include <vector>

namespace fruityprime::net {

// Renderer-independent counterpart of MphRead/Mods/Network/NetFeatureCheck.cs.
// The managed checker reads PlayerEntity and Scene directly.  The native
// version receives one compact observation frame from the frontend so the
// accounting remains usable by the headless client, the Win32 renderer, and
// replay tooling without coupling this report to a window toolkit.
class NetFeatureCheck final {
public:
    static constexpr std::uint8_t NoWeapon = 0xff;
    static constexpr float TeleportStep = 9.0F;
    static constexpr int LaunchGraceFrames = 30;

    struct PlayerObservation {
        bool present = false;
        bool active = false;
        bool spawned = false;
        bool alt_form = false;
        bool zoomed = false;
        bool frozen = false;
        bool spectating = false;
        bool double_damage = false;
        bool alt_attack_pressed = false;
        std::uint8_t hunter = static_cast<std::uint8_t>(
            metadata::Hunter::Samus);
        std::uint8_t current_weapon = NoWeapon;
        std::uint16_t health = 0;
        Vec3 position;
        Vec3 facing{0.0F, 0.0F, 1.0F};
        // Number of jump-pad/teleporter events reported for this slot so far.
        int world_events = 0;
    };

    struct Frame {
        int local_slot = 0;
        std::uint32_t net_frame = 0;
        TestPhase phase = TestPhase::Idle;
        bool authority = false;
        int items_now = 0;
        std::array<int, NetConfig::SlotCapacity> beams{};
        std::array<int, NetConfig::SlotCapacity> bombs{};
        std::array<int, NetConfig::SlotCapacity> halfturrets{};
        std::array<int, NetConfig::SlotCapacity> fired_totals{};
        std::array<PlayerObservation, NetConfig::SlotCapacity> players{};
        std::array<PlayerState, NetConfig::SlotCapacity> remote_states{};
        std::array<bool, NetConfig::SlotCapacity> remote_state_valid{};
    };

    struct Record {
        int spawned_frames = 0;
        int moved_frames = 0;
        double travelled = 0.0;
        float min_y = std::numeric_limits<float>::max();
        float max_y = std::numeric_limits<float>::lowest();
        double facing_degrees = 0.0;
        int beam_frames = 0;
        int shots_fired = 0;
        int last_fired_total = 0;
        int bomb_frames = 0;
        int halfturret_frames = 0;
        int alt_form_frames = 0;
        int alt_form_in_morph_phase = 0;
        int biped_in_unmorph_phase = 0;
        int weapon_changes = 0;
        int alt_attack_presses = 0;
        int damage_events = 0;
        int damage_in_alt_form = 0;
        int deaths = 0;
        int zoom_frames = 0;
        int frozen_frames = 0;
        int spectating_frames = 0;
        int double_damage_frames = 0;
        Vec3 last_position;
        Vec3 last_facing{0.0F, 0.0F, 1.0F};
        Vec3 last_frame_position;
        bool have_frame_previous = false;
        bool have_previous = false;
        int last_health = -1;
        std::uint8_t last_weapon = NoWeapon;
        bool was_alive = false;
        int form_disagree_frames = 0;
        int form_disagree_run = 0;
        int worst_form_disagree_run = 0;
        std::string worst_form_context;
        double worst_position_gap = 0.0;
        double worst_step = 0.0;
        int teleports = 0;
        bool ever_compared = false;
        int frames_since_respawn = 0;
        int frames_since_launch = 0;
        int last_world_events = 0;
        metadata::Hunter hunter = metadata::Hunter::Samus;
    };

    struct FeatureResult {
        std::string feature;
        double mine = 0.0;
        double seen = 0.0;
        double needed = 0.0;
        bool applicable = true;
        bool tested = false;
        bool ok = false;
        bool pairwise = false;
        std::string verdict;
    };

    struct PairResult {
        std::uint8_t slot = 0;
        metadata::Hunter hunter = metadata::Hunter::Samus;
        bool ever_compared = false;
        int form_disagree_frames = 0;
        int worst_form_disagree_run = 0;
        double worst_position_gap = 0.0;
        double worst_step = 0.0;
        std::string worst_form_context;
        int failures = 0;
        std::vector<FeatureResult> features;
    };

    struct ReportResult {
        bool any_remote = false;
        int failures = 0;
        std::vector<PairResult> pairs;
    };

    NetFeatureCheck() noexcept;

    void reset() noexcept;
    void observe(const Frame& frame);

    [[nodiscard]] const std::array<Record, NetConfig::SlotCapacity>&
    records() const noexcept { return records_; }
    [[nodiscard]] int items_now() const noexcept { return items_now_; }
    [[nodiscard]] int items_picked_up() const noexcept {
        return items_picked_up_;
    }
    [[nodiscard]] int item_samples() const noexcept { return item_samples_; }
    [[nodiscard]] long long item_total() const noexcept { return item_total_; }
    [[nodiscard]] int local_slot() const noexcept { return local_slot_; }
    [[nodiscard]] const std::map<int, std::string>& boards() const noexcept {
        return boards_;
    }

    // The caller supplies names because the native checker deliberately has
    // no dependency on process-wide UI/GameState storage.
    [[nodiscard]] ReportResult report(
        const std::array<std::string, NetConfig::SlotCapacity>& names) const;
    void print_report(
        std::ostream& output,
        const std::array<std::string, NetConfig::SlotCapacity>& names) const;

    // Keep the scoreboard series at the same server-clock marks as C#.
    void sample_scoreboard(
        int server_second,
        const std::array<std::string, NetConfig::SlotCapacity>& names,
        const std::array<bool, NetConfig::SlotCapacity>& present,
        const std::array<std::int32_t, NetConfig::SlotCapacity>& kills,
        const std::array<std::int32_t, NetConfig::SlotCapacity>& deaths,
        const std::array<std::int32_t, NetConfig::SlotCapacity>& points);

private:
    static bool lays_bombs(metadata::Hunter hunter) noexcept;
    static double height(const Record& record) noexcept;
    static const char* phase_name(TestPhase phase) noexcept;
    static std::string hunter_name(metadata::Hunter hunter);

    std::array<Record, NetConfig::SlotCapacity> records_{};
    int item_samples_ = 0;
    long long item_total_ = 0;
    int items_now_ = 0;
    int items_picked_up_ = 0;
    int last_item_count_ = -1;
    std::map<TestPhase, int> phase_frames_;
    std::map<int, std::string> boards_;
    int local_slot_ = 0;
};

} // namespace fruityprime::net
