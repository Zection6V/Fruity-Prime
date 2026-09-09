#include "Mods/Network/net_log.hpp"

#include "GameState.hpp"
#include "Entities/gameplay.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace fruityprime::net {
namespace {

[[nodiscard]] std::tm local_time(std::time_t value) noexcept {
    std::tm result{};
#ifdef _WIN32
    localtime_s(&result, &value);
#else
    localtime_r(&value, &result);
#endif
    return result;
}

[[nodiscard]] std::string vector_text(Vec3 value) {
    std::ostringstream text;
    text << '(' << std::fixed << std::setprecision(2)
         << value.x << ',' << value.y << ',' << value.z << ')';
    return text.str();
}

[[nodiscard]] float vector_distance(Vec3 left, Vec3 right) noexcept {
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    const float z = left.z - right.z;
    return std::sqrt(x * x + y * y + z * z);
}

[[nodiscard]] bool has_flag(const PlayerState& player,
                            std::uint8_t flag) noexcept {
    return (player.flags & flag) != 0;
}

} // namespace

NetLog::NetLog() noexcept : interval_(read_interval()) {}

NetLog::~NetLog() {
    close();
}

double NetLog::read_interval() noexcept {
    const char* value = std::getenv("MPHREAD_NETLOG_INTERVAL");
    if (value == nullptr || *value == '\0') {
        return 1.0;
    }
    char* end = nullptr;
    const double parsed = std::strtod(value, &end);
    if (end == value || *end != '\0' || !std::isfinite(parsed)
        || parsed <= 0.0 || parsed > 10.0) {
        return 1.0;
    }
    return parsed;
}

std::string NetLog::safe_name(std::string_view name) {
    std::string result;
    result.reserve(name.size());
    for (const unsigned char value : name) {
        result.push_back(std::isalnum(value) != 0 ? static_cast<char>(value)
                                                  : '_');
    }
    return result;
}

std::string NetLog::timestamp(bool milliseconds) {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    const std::tm current = local_time(time);
    std::ostringstream text;
    text << std::put_time(&current, "%H:%M:%S");
    if (milliseconds) {
        const auto millis = std::chrono::duration_cast<
            std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        text << '.' << std::setfill('0') << std::setw(3) << millis.count();
    }
    return text.str();
}

bool NetLog::open(const std::filesystem::path& base_directory,
                  std::string_view client_name) noexcept {
    std::lock_guard lock(mutex_);
    if (failed_ || output_.is_open()) {
        return output_.is_open();
    }
    try {
        path_ = base_directory / ("netlog-" + safe_name(client_name) + ".txt");
        output_.open(path_, std::ios::out | std::ios::trunc);
        if (!output_) {
            path_.clear();
            failed_ = true;
            return false;
        }
        output_ << "=== MphRead net log for \"" << client_name << "\" ===\n";
        output_ << "started " << timestamp(false) << '\n';
        output_.flush();
        last_write_ = 0.0;
        return true;
    } catch (...) {
        failed_ = true;
        path_.clear();
        if (output_.is_open()) {
            output_.close();
        }
        return false;
    }
}

void NetLog::close() noexcept {
    std::lock_guard lock(mutex_);
    if (output_.is_open()) {
        output_.flush();
        output_.close();
    }
    last_write_ = 0.0;
}

bool NetLog::enabled() const noexcept {
    std::lock_guard lock(mutex_);
    return output_.is_open();
}

void NetLog::line(std::string_view text) noexcept {
    std::lock_guard lock(mutex_);
    if (!output_.is_open()) {
        return;
    }
    try {
        output_ << text << '\n';
        output_.flush();
    } catch (...) {
        // A full or disconnected drive must never stop a match.
    }
}

void NetLog::event(std::string_view message) noexcept {
    std::ostringstream text;
    text << '[' << timestamp(true) << "] EVENT  " << message;
    line(text.str());
}

void NetLog::collision_range(int slot, std::string_view label,
                             Vec3 previous, Vec3 current) noexcept {
    std::ostringstream text;
    text << "collision " << label << " slot=" << slot
         << " prev=" << vector_text(previous)
         << " cur=" << vector_text(current)
         << " delta=" << std::fixed << std::setprecision(2)
         << vector_distance(previous, current);
    event(text.str());
}

void NetLog::snapshot(double time, const SnapshotContext& context) noexcept {
    if (context.game_state == nullptr) {
        return;
    }
    {
        std::lock_guard lock(mutex_);
        if (!output_.is_open() || time - last_write_ < interval_) {
            return;
        }
        last_write_ = time;
    }

    const game::State& state = *context.game_state;
    std::ostringstream header;
    header << '[' << timestamp(true) << "] STATE  role=" << context.role
           << " slot=" << context.local_slot
           << " authority=" << (context.authority ? '1' : '0')
           << " main=" << context.main_player
           << " mode=" << static_cast<int>(state.mode)
           << " matchTime=" << std::fixed << std::setprecision(1)
           << state.match_time
           << " matchState=" << static_cast<int>(state.match_state)
           << " goal=" << state.point_goal;
    if (!state.room_name.empty()) {
        header << " map=" << state.room_name;
    }
    if (context.server_match != nullptr) {
        header << " serverTime=" << std::fixed << std::setprecision(1)
               << context.server_match->time_remaining
               << " serverMap=" << context.server_match->room_key
               << " serverPeers=" << static_cast<int>(
                   context.server_match->player_count);
    }
    line(header.str());

    for (std::size_t slot = 0; slot < NetConfig::SlotCapacity; ++slot) {
        const bool has_entity = context.session != nullptr
            && context.session->has_player(static_cast<std::uint8_t>(slot));
        if (!has_entity) {
            std::ostringstream absent;
            absent << "           slot " << slot << ": (no entity)";
            line(absent.str());
            continue;
        }
        const auto& player = context.session->player(
            static_cast<std::uint8_t>(slot));
        const bool occupied = context.occupied[slot];
        const bool active = has_flag(player, PlayerState::FlagActive);
        if (!active && !occupied) {
            continue;
        }
        std::ostringstream detail;
        detail << "           slot " << slot << ": name="
               << std::left << std::setw(10) << state.nicknames[slot]
               << std::right
               << " occupied=" << (occupied ? 'y' : 'n')
               << " active=" << (active ? 'y' : 'n')
               << " spawned=" << (has_flag(player, PlayerState::FlagSpawned)
                                     ? 'y' : 'n')
               << " bot=native"
               << " hp=" << std::setw(3) << player.health
               << " score=" << state.points[slot] << '/' << state.team_points[slot]
               << 'p' << ' ' << state.kills[slot] << 'k'
               << state.deaths[slot] << 'd'
               << " respawnTimer=" << context.session->respawn_ticks(
                   static_cast<std::uint8_t>(slot))
               << " pos=" << vector_text(player.position)
               << " form=" << (has_flag(player, PlayerState::FlagSpectating)
                                    ? "spectating"
                                    : has_flag(player, PlayerState::FlagAltForm)
                                        ? "alt" : "biped")
               << " nodeRef=native-session"
               << " inScene=native-session"
               << " stateValid=" << (context.state_valid[slot] ? 'y' : 'n')
               << " intentValid=" << (context.intent_valid[slot] ? 'y' : 'n')
               << " ping=" << context.pings[slot];
        line(detail.str());
    }
}

} // namespace fruityprime::net
