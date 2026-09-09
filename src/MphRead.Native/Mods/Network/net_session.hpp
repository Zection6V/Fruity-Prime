#pragma once

#include "Mods/Network/net_client.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::net {

enum class NetRole {
    Offline,
    Client,
    Playback
};

// Wire/session state shared by the game hooks and the headless network
// client.  The dedicated relay owns the Host-side peer table; this class owns
// the client-facing state that NetSession.cs exposes to the scene each frame.
class NetSession final {
public:
    NetSession() noexcept = default;
    NetSession(const NetSession&) = delete;
    NetSession& operator=(const NetSession&) = delete;
    ~NetSession();

    [[nodiscard]] bool start_client(
        Endpoint server, NetworkConditions conditions = {},
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1500));
    void stop() noexcept;

    void set_player(std::uint8_t hunter, std::string_view name);
    void identify();
    [[nodiscard]] std::vector<ReceivedPacket> update(double time_seconds);

    void send_intent(IntentState intent);
    void send_match_end();
    void send_chat(std::string_view text);
    void send(PacketType type, std::span<const std::uint8_t> payload = {});

    [[nodiscard]] NetRole role() const noexcept { return role_; }
    [[nodiscard]] bool active() const noexcept {
        return role_ != NetRole::Offline;
    }
    [[nodiscard]] bool is_client() const noexcept {
        return role_ == NetRole::Client;
    }
    [[nodiscard]] bool is_authority() const noexcept { return authority_; }
    [[nodiscard]] int local_slot() const noexcept {
        return client_ == nullptr ? -1 : client_->local_slot();
    }
    [[nodiscard]] std::uint32_t net_frame() const noexcept { return net_frame_; }
    [[nodiscard]] std::uint32_t last_snapshot_frame() const noexcept {
        return last_snapshot_frame_;
    }
    [[nodiscard]] std::string_view last_error() const noexcept {
        return last_error_;
    }
    [[nodiscard]] bool refused() const noexcept { return refused_; }
    [[nodiscard]] const RefusedPacket& refused_reason() const noexcept {
        return refused_reason_;
    }
    [[nodiscard]] std::optional<MatchStatePacket> server_match() const;
    [[nodiscard]] const RosterPacket& roster() const noexcept { return roster_; }
    [[nodiscard]] bool roster_valid() const noexcept { return roster_valid_; }

    [[nodiscard]] const std::array<PlayerState, NetConfig::SlotCapacity>&
    remote_states() const noexcept { return remote_states_; }
    [[nodiscard]] const std::array<bool, NetConfig::SlotCapacity>&
    remote_state_valid() const noexcept { return remote_state_valid_; }
    [[nodiscard]] const std::array<IntentState, NetConfig::SlotCapacity>&
    remote_intents() const noexcept { return remote_intents_; }
    [[nodiscard]] const std::array<bool, NetConfig::SlotCapacity>&
    remote_intent_valid() const noexcept { return remote_intent_valid_; }
    [[nodiscard]] const std::array<std::uint32_t, NetConfig::SlotCapacity>&
    remote_intent_arrived() const noexcept { return remote_intent_arrived_; }
    [[nodiscard]] std::uint32_t remote_intent_age(int slot) const noexcept;
    [[nodiscard]] const std::array<bool, NetConfig::SlotCapacity>&
    slot_occupied() const noexcept { return slot_occupied_; }
    [[nodiscard]] const std::array<std::uint8_t, NetConfig::SlotCapacity>&
    slot_hunters() const noexcept { return slot_hunters_; }
    [[nodiscard]] const std::array<std::uint16_t, NetConfig::SlotCapacity>&
    slot_ping() const noexcept { return slot_ping_; }

    void forget_slot(int slot) noexcept;
    void note_states_applied() noexcept { ++states_applied_; }
    [[nodiscard]] std::uint64_t snapshots_received() const noexcept {
        return snapshots_received_;
    }
    [[nodiscard]] std::uint64_t snapshots_sent() const noexcept {
        return snapshots_sent_;
    }
    [[nodiscard]] std::uint64_t states_applied() const noexcept {
        return states_applied_;
    }
    [[nodiscard]] std::uint64_t intents_received() const noexcept {
        return intents_received_;
    }
    [[nodiscard]] std::uint64_t snapshots_out_of_order() const noexcept {
        return snapshots_out_of_order_;
    }
    [[nodiscard]] std::uint64_t intents_out_of_order() const noexcept {
        return intents_out_of_order_;
    }
    [[nodiscard]] int reannouncements() const noexcept {
        return reannouncements_;
    }
    [[nodiscard]] double longest_server_silence() const noexcept {
        return longest_server_silence_;
    }

private:
    void reset_state() noexcept;
    void retain_packet(const ReceivedPacket& packet, double time_seconds);
    void retain_roster(const RosterPacket& roster) noexcept;
    void retain_match_state(const MatchStatePacket& match) noexcept;
    void retain_snapshot(const SnapshotPacket& snapshot);
    void retain_slot_intent(std::span<const std::uint8_t> payload);
    void reannounce();

    std::unique_ptr<NetClient> client_;
    NetRole role_ = NetRole::Offline;
    std::string player_name_ = "Player";
    std::uint8_t local_hunter_ = 0;
    std::uint32_t net_frame_ = 0;
    std::uint32_t last_snapshot_frame_ = 0;
    std::array<std::uint32_t, NetConfig::SlotCapacity> last_slot_intent_frame_{};
    std::array<std::uint32_t, NetConfig::SlotCapacity> remote_intent_arrived_{};
    std::array<PlayerState, NetConfig::SlotCapacity> remote_states_{};
    std::array<bool, NetConfig::SlotCapacity> remote_state_valid_{};
    std::array<IntentState, NetConfig::SlotCapacity> remote_intents_{};
    std::array<bool, NetConfig::SlotCapacity> remote_intent_valid_{};
    std::array<bool, NetConfig::SlotCapacity> slot_occupied_{};
    std::array<std::uint8_t, NetConfig::SlotCapacity> slot_hunters_{};
    std::array<std::uint16_t, NetConfig::SlotCapacity> slot_ping_{};
    RosterPacket roster_{};
    MatchStatePacket server_match_{};
    bool roster_valid_ = false;
    bool server_match_valid_ = false;
    bool authority_ = false;
    bool refused_ = false;
    RefusedPacket refused_reason_{};
    std::string last_error_;
    double last_server_packet_time_ = 0.0;
    double longest_server_silence_ = 0.0;
    int reannouncements_ = 0;
    int late_snapshot_run_ = 0;
    int snapshot_stream_resets_ = 0;
    std::uint64_t snapshots_received_ = 0;
    std::uint64_t snapshots_sent_ = 0;
    std::uint64_t states_applied_ = 0;
    std::uint64_t intents_received_ = 0;
    std::uint64_t snapshots_out_of_order_ = 0;
    std::uint64_t intents_out_of_order_ = 0;
};

} // namespace fruityprime::net
