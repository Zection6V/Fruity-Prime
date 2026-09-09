#pragma once

#include "Mods/Network/net_protocol.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

namespace fruityprime::game {
struct State;
}

namespace fruityprime::gameplay {
class Session;
}

namespace fruityprime::net {

// Per-client network diagnostics.  This is the native counterpart of
// MphRead/Mods/Network/NetLog.cs; it intentionally owns a separate file from
// the general application log so two clients can be compared frame by frame.
class NetLog final {
public:
    struct SnapshotContext {
        const game::State* game_state = nullptr;
        const gameplay::Session* session = nullptr;
        const MatchStatePacket* server_match = nullptr;
        std::string_view role;
        bool authority = false;
        int local_slot = -1;
        int main_player = -1;
        std::array<bool, NetConfig::SlotCapacity> occupied{};
        std::array<std::uint16_t, NetConfig::SlotCapacity> pings{};
        std::array<bool, NetConfig::SlotCapacity> state_valid{};
        std::array<bool, NetConfig::SlotCapacity> intent_valid{};
    };

    NetLog() noexcept;
    NetLog(const NetLog&) = delete;
    NetLog& operator=(const NetLog&) = delete;
    ~NetLog();

    // The file is created beside the executable, matching AppContext.BaseDirectory
    // in the managed implementation.  Failure is deliberately non-fatal.
    [[nodiscard]] bool open(const std::filesystem::path& base_directory,
                            std::string_view client_name) noexcept;
    void close() noexcept;
    [[nodiscard]] bool enabled() const noexcept;
    [[nodiscard]] double interval() const noexcept { return interval_; }

    void event(std::string_view message) noexcept;
    void collision_range(int slot, std::string_view label,
                         Vec3 previous, Vec3 current) noexcept;
    void snapshot(double time, const SnapshotContext& context) noexcept;

private:
    static double read_interval() noexcept;
    static std::string safe_name(std::string_view name);
    static std::string timestamp(bool milliseconds);
    void line(std::string_view text) noexcept;

    mutable std::mutex mutex_;
    std::ofstream output_;
    std::filesystem::path path_;
    double interval_ = 1.0;
    double last_write_ = 0.0;
    bool failed_ = false;
};

} // namespace fruityprime::net
