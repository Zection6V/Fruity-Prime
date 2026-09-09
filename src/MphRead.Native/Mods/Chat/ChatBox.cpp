#include "Mods/Chat/ChatBox.hpp"

#include "Mods/InputSettings.hpp"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <utility>

namespace fruityprime::chat {
namespace {

constexpr std::int64_t HoldMilliseconds = 10'000;
constexpr std::int64_t FadeMilliseconds = 1'000;
constexpr int KeyUnknown = 0;
constexpr int KeyEscape = 256;
constexpr int KeyEnter = 257;
constexpr int KeyBackspace = 259;
constexpr int KeyV = 86;
constexpr int KeyPadEnter = 335;

std::vector<ChatLine> lines;
std::string compose;
std::mutex state_lock;
bool composing = false;
bool swallow_next_char = false;
bool just_closed = false;
int sent = 0;
int received = 0;

detail::RuntimeBindings runtime;
std::mutex runtime_lock;

[[nodiscard]] detail::RuntimeBindings runtime_snapshot() noexcept {
    std::scoped_lock guard(runtime_lock);
    return runtime;
}

[[nodiscard]] std::int64_t tick_count64() noexcept {
    const auto bindings = runtime_snapshot();
    if (bindings.TickCount64 != nullptr) {
        return bindings.TickCount64();
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

[[nodiscard]] bool unicode_white_space(std::uint32_t code_point) noexcept {
    return (code_point >= 0x0009 && code_point <= 0x000D)
        || code_point == 0x0020 || code_point == 0x0085
        || code_point == 0x00A0 || code_point == 0x1680
        || (code_point >= 0x2000 && code_point <= 0x200A)
        || code_point == 0x2028 || code_point == 0x2029
        || code_point == 0x202F || code_point == 0x205F
        || code_point == 0x3000;
}

[[nodiscard]] bool next_utf8(std::string_view value, std::size_t& index,
                             std::uint32_t& code_point) noexcept {
    const auto first = static_cast<unsigned char>(value[index++]);
    if (first < 0x80) {
        code_point = first;
        return true;
    }

    int continuation_count = 0;
    std::uint32_t minimum = 0;
    if (first >= 0xC2 && first <= 0xDF) {
        continuation_count = 1;
        code_point = first & 0x1F;
        minimum = 0x80;
    } else if (first >= 0xE0 && first <= 0xEF) {
        continuation_count = 2;
        code_point = first & 0x0F;
        minimum = 0x800;
    } else if (first >= 0xF0 && first <= 0xF4) {
        continuation_count = 3;
        code_point = first & 0x07;
        minimum = 0x10000;
    } else {
        return false;
    }

    if (index + static_cast<std::size_t>(continuation_count) > value.size()) {
        return false;
    }
    for (int count = 0; count < continuation_count; ++count) {
        const auto next = static_cast<unsigned char>(value[index++]);
        if ((next & 0xC0) != 0x80) {
            return false;
        }
        code_point = (code_point << 6) | (next & 0x3F);
    }
    return code_point >= minimum && code_point <= 0x10FFFF
        && !(code_point >= 0xD800 && code_point <= 0xDFFF);
}

[[nodiscard]] bool null_or_white_space(std::string_view value) noexcept {
    if (value.empty()) {
        return true;
    }
    std::size_t index = 0;
    while (index < value.size()) {
        std::uint32_t code_point = 0;
        if (!next_utf8(value, index, code_point)
            || !unicode_white_space(code_point)) {
            return false;
        }
    }
    return true;
}

void trim_compose(std::string& value) {
    const auto first = value.find_first_not_of(' ');
    if (first == std::string::npos) {
        value.clear();
        return;
    }
    const auto last = value.find_last_not_of(' ');
    value = value.substr(first, last - first + 1);
}

} // namespace

bool ChatBox::Composing() noexcept {
    std::scoped_lock guard(state_lock);
    return composing;
}

bool ChatBox::ConsumeJustClosed() noexcept {
    std::scoped_lock guard(state_lock);
    if (!just_closed) {
        return false;
    }
    just_closed = false;
    return true;
}

int ChatBox::Sent() noexcept {
    std::scoped_lock guard(state_lock);
    return sent;
}

int ChatBox::Received() noexcept {
    std::scoped_lock guard(state_lock);
    return received;
}

std::string ChatBox::ComposeText() {
    std::scoped_lock guard(state_lock);
    return compose;
}

void ChatBox::CollectVisible(std::vector<VisibleChatLine>& into) {
    into.clear();
    const std::int64_t now = tick_count64();
    std::scoped_lock guard(state_lock);
    for (auto it = lines.rbegin();
         it != lines.rend()
             && into.size() < static_cast<std::size_t>(VisibleLines);
         ++it) {
        const std::int64_t age = now - it->ArrivedAt;
        if (age >= HoldMilliseconds) {
            break;
        }
        const float alpha = age > HoldMilliseconds - FadeMilliseconds
            ? static_cast<float>(HoldMilliseconds - age)
                / static_cast<float>(FadeMilliseconds)
            : 1.0F;
        into.push_back(VisibleChatLine{*it, alpha});
    }
    std::reverse(into.begin(), into.end());
}

bool ChatBox::Available() {
    const auto bindings = runtime_snapshot();
    return bindings.SinglePlayer != nullptr && !bindings.SinglePlayer();
}

bool ChatBox::Visible() {
    if (!Available()) {
        return false;
    }
    if (Composing()) {
        return true;
    }
    const std::int64_t now = tick_count64();
    std::scoped_lock guard(state_lock);
    return !lines.empty() && now - lines.back().ArrivedAt < HoldMilliseconds;
}

void ChatBox::Clear() {
    std::scoped_lock guard(state_lock);
    lines.clear();
    compose.clear();
    composing = false;
    swallow_next_char = false;
}

void ChatBox::Receive(const net::ChatPacket& packet) {
    const std::string name = packet.name.empty()
        ? "Player" + std::to_string(packet.slot)
        : packet.name;
    if (packet.kind == net::ChatPacket::KindSystem) {
        Add({}, packet.text, net::ChatPacket::KindSystem);
    } else {
        Add(name, packet.text, packet.kind);
    }
    std::scoped_lock guard(state_lock);
    ++received;
}

void ChatBox::System(std::string text) {
    Add({}, std::move(text), net::ChatPacket::KindSystem);
}

void ChatBox::Add(std::string name, std::string text, std::uint8_t kind) {
    if (null_or_white_space(text)) {
        return;
    }
    std::scoped_lock guard(state_lock);
    lines.push_back(ChatLine{
        std::move(name), std::move(text), kind, tick_count64()
    });
    if (lines.size() > 64) {
        lines.erase(lines.begin(), lines.end() - 64);
    }
}

void ChatBox::Open(bool swallow_opening_char) {
    if (Composing() || !Available()) {
        return;
    }
    std::scoped_lock guard(state_lock);
    compose.clear();
    composing = true;
    swallow_next_char = swallow_opening_char;
}

void ChatBox::Cancel() {
    std::scoped_lock guard(state_lock);
    compose.clear();
    composing = false;
    swallow_next_char = false;
    just_closed = true;
}

void ChatBox::Submit() {
    std::string text;
    {
        std::scoped_lock guard(state_lock);
        text = compose;
        trim_compose(text);
        compose.clear();
        composing = false;
        swallow_next_char = false;
        just_closed = true;
    }
    if (text.empty()) {
        return;
    }
    if (text.size() > static_cast<std::size_t>(MaxLength)) {
        text.resize(MaxLength);
    }
    Send(std::move(text));
}

void ChatBox::Send(std::string text) {
    auto bindings = runtime_snapshot();
    const bool active_for_name = bindings.NetActive != nullptr
        && bindings.NetActive();
    std::string name = "You";
    if (active_for_name) {
        name = bindings.PlayerName == nullptr
            ? std::string("Player") : bindings.PlayerName();
    }
    const bool sendable = !null_or_white_space(text);
    Add(std::move(name), text, net::ChatPacket::KindSay);

    bindings = runtime_snapshot();
    if (bindings.NetActive != nullptr && bindings.NetActive()) {
        if (sendable && bindings.SendChat != nullptr) {
            bindings.SendChat(text);
        }
        std::scoped_lock guard(state_lock);
        ++sent;
    }
}

void ChatBox::HandleText(int code_point) {
    std::scoped_lock guard(state_lock);
    if (!composing) {
        return;
    }
    if (swallow_next_char) {
        swallow_next_char = false;
        return;
    }
    if (code_point < 32 || code_point > 126) {
        return;
    }
    if (compose.size() < static_cast<std::size_t>(MaxLength)) {
        compose.push_back(static_cast<char>(code_point));
    }
}

bool ChatBox::HandleKeyDown(int key, bool control, bool alt, bool can_open,
                            bool swallow_opening_char) {
    if (!Composing()) {
        if (can_open && Available() && key != KeyUnknown
            && key == mods::InputSettings::ChatKey() && !alt && !control) {
            Open(swallow_opening_char);
            return true;
        }
        return false;
    }

    switch (key) {
    case KeyEnter:
    case KeyPadEnter:
        Submit();
        return true;
    case KeyEscape:
        Cancel();
        return true;
    case KeyBackspace: {
        std::scoped_lock guard(state_lock);
        if (!compose.empty()) {
            const std::size_t end = compose.size();
            const std::size_t cut = control ? WordStart(compose, end) : end - 1;
            compose.erase(cut, end - cut);
        }
        return true;
    }
    case KeyV:
        if (control) {
            return true;
        }
        [[fallthrough]];
    default:
        return true;
    }
}

std::size_t ChatBox::WordStart(std::string_view text,
                               std::size_t from) noexcept {
    std::size_t index = from;
    while (index > 0 && text[index - 1] == ' ') {
        --index;
    }
    while (index > 0 && text[index - 1] != ' ') {
        --index;
    }
    return index;
}

namespace detail {

void BindRuntime(RuntimeBindings bindings) noexcept {
    std::scoped_lock guard(runtime_lock);
    runtime = bindings;
}

} // namespace detail
} // namespace fruityprime::chat
