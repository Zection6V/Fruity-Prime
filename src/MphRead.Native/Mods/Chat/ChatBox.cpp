#include "Mods/chat.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace fruityprime::chat {
namespace {

[[nodiscard]] bool blank(std::string_view value) noexcept {
    return value.find_first_not_of(" \t\r\n") == std::string_view::npos;
}

void trim(std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        value.clear();
        return;
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    value = value.substr(first, last - first + 1);
}

} // namespace

void Log::clear() {
    lines_.clear();
    compose_.clear();
    composing_ = false;
    swallow_opening_text_ = false;
    just_closed_ = false;
}

void Log::add(std::string name, std::string text, std::uint8_t kind,
              double now) {
    if (blank(text)) {
        return;
    }
    lines_.push_back(Line{std::move(name), std::move(text), kind, now});
    if (lines_.size() > 64) {
        lines_.erase(lines_.begin(), lines_.end() - 64);
    }
}

void Log::receive(const net::ChatPacket& packet, double now) {
    if (packet.kind == net::ChatPacket::KindSystem) {
        add({}, packet.text, packet.kind, now);
        return;
    }
    const std::string name = packet.name.empty()
        ? "Player" + std::to_string(packet.slot)
        : packet.name;
    add(name, packet.text, packet.kind, now);
}

void Log::open(bool swallow_opening_text) {
    if (composing_) {
        return;
    }
    compose_.clear();
    composing_ = true;
    swallow_opening_text_ = swallow_opening_text;
    just_closed_ = false;
}

void Log::cancel() {
    compose_.clear();
    composing_ = false;
    swallow_opening_text_ = false;
    just_closed_ = true;
}

std::string Log::submit() {
    std::string text = compose_;
    trim(text);
    compose_.clear();
    composing_ = false;
    swallow_opening_text_ = false;
    just_closed_ = true;
    if (text.size() > MaxLength) {
        text.resize(MaxLength);
    }
    return text;
}

void Log::handle_text(int code_point) {
    if (!composing_) {
        return;
    }
    if (swallow_opening_text_) {
        swallow_opening_text_ = false;
        return;
    }
    if (code_point < 32 || code_point > 126) {
        return;
    }
    if (compose_.size() < MaxLength) {
        compose_.push_back(static_cast<char>(code_point));
    }
}

void Log::backspace() {
    if (composing_ && !compose_.empty()) {
        compose_.pop_back();
    }
}

bool Log::consume_just_closed() noexcept {
    if (!just_closed_) {
        return false;
    }
    just_closed_ = false;
    return true;
}

std::vector<VisibleLine> Log::visible(double now) const {
    std::vector<VisibleLine> result;
    for (auto it = lines_.rbegin();
         it != lines_.rend() && result.size() < VisibleLines; ++it) {
        const double age = std::max(0.0, now - it->arrived_at);
        if (age >= HoldSeconds) {
            break;
        }
        const float alpha = age > HoldSeconds - FadeSeconds
            ? static_cast<float>((HoldSeconds - age) / FadeSeconds)
            : 1.0F;
        result.push_back(VisibleLine{*it, std::clamp(alpha, 0.0F, 1.0F)});
    }
    std::reverse(result.begin(), result.end());
    return result;
}

} // namespace fruityprime::chat
