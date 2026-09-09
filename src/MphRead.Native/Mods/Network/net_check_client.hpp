#pragma once

#include "Mods/Network/net_feature_check.hpp"
#include "Mods/Network/net_transport.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <ostream>
#include <span>
#include <string>

namespace fruityprime::net {

// Portable options for the native counterpart of
// MphRead/Mods/Network/NetCheckClient.cs.  The managed implementation owns a
// hidden OpenTK window; the native port keeps the check independent of a
// window toolkit and drives the same network-facing observation boundary.
struct NetCheckClientOptions {
    std::string name = "FruityPrime";
    std::uint8_t hunter = 0;
    int seconds = 5;
    NetworkConditions network;
    double spectate_at_seconds = -1.0;
    double rejoin_at_seconds = -1.0;
    bool send_chat = true;

    // An optional sink lets the command-line owner write the exact packet
    // stream into a demo without making the network library depend on the
    // demo writer library.
    std::function<void(std::uint32_t, std::span<const std::uint8_t>)>
        packet_sink;
};

struct NetCheckClientReport {
    bool connected = false;
    bool passed = false;
    bool features_passed = false;
    int feature_failures = 0;
    int frames = 0;
    double elapsed_seconds = 0.0;
    int local_slot = -1;
    std::size_t intents_sent = 0;
    std::size_t snapshots_received = 0;
    std::size_t rosters_received = 0;
    std::size_t match_states_received = 0;
    std::size_t chats_received = 0;
    std::size_t spectating_frames = 0;
    int spectate_started_frame = -1;
    int rejoined_frame = -1;
    std::array<std::size_t, NetConfig::SlotCapacity>
        remote_spectating_frames{};
    NetFeatureCheck::ReportResult feature_report;
    std::string error;
};

// Run a fixed-step, script-driven network client and print the same
// attribution-oriented report as the managed harness.  It returns a report
// rather than an exit code so callers can preserve the feature table and
// packet diagnostics for CI, demo recording, or a future renderer frontend.
[[nodiscard]] NetCheckClientReport run_net_check_client(
    Endpoint server, const NetCheckClientOptions& options,
    std::ostream& output);

} // namespace fruityprime::net
