#pragma once

#include "Mods/Network/net_protocol.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::chat {

struct Line {
    std::string name;
    std::string text;
    std::uint8_t kind = net::ChatPacket::KindSay;
    double arrived_at = 0.0;
};

struct VisibleLine {
    Line line;
    float alpha = 1.0F;
};

// Frontend-independent chat state. Network code supplies received packets;
// the Win32/Android frontends own the actual send operation and key events.
class Log {
public:
    static constexpr std::size_t VisibleLines = 3;
    static constexpr std::size_t MaxLength = 80;
    static constexpr double HoldSeconds = 10.0;
    static constexpr double FadeSeconds = 1.0;

    void clear();
    void add(std::string name, std::string text, std::uint8_t kind,
             double now);
    void receive(const net::ChatPacket& packet, double now);

    void open(bool swallow_opening_text = true);
    void cancel();
    [[nodiscard]] std::string submit();
    void handle_text(int code_point);
    void backspace();

    [[nodiscard]] bool composing() const noexcept { return composing_; }
    [[nodiscard]] std::string_view compose_text() const noexcept {
        return compose_;
    }
    [[nodiscard]] bool consume_just_closed() noexcept;
    [[nodiscard]] std::vector<VisibleLine> visible(double now) const;

private:
    std::vector<Line> lines_;
    std::string compose_;
    bool composing_ = false;
    bool swallow_opening_text_ = false;
    bool just_closed_ = false;
};

} // namespace fruityprime::chat
