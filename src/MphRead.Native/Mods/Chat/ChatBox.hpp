#pragma once

#include "Mods/Network/net_protocol.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::chat {

struct ChatLine {
    std::string Name;
    std::string Text;
    std::uint8_t Kind = net::ChatPacket::KindSay;
    std::int64_t ArrivedAt = 0;
};

struct VisibleChatLine {
    ChatLine Line;
    float Alpha = 1.0F;
};

// Native counterpart of the static MphRead.Mods.Chat.ChatBox type.
class ChatBox final {
public:
    static constexpr int VisibleLines = 3;
    static constexpr int MaxLength = 80;

    [[nodiscard]] static bool Composing() noexcept;
    [[nodiscard]] static bool ConsumeJustClosed() noexcept;
    [[nodiscard]] static int Sent() noexcept;
    [[nodiscard]] static int Received() noexcept;

    [[nodiscard]] static std::string ComposeText();
    static void CollectVisible(std::vector<VisibleChatLine>& into);
    [[nodiscard]] static bool Available();
    [[nodiscard]] static bool Visible();

    static void Clear();
    static void Receive(const net::ChatPacket& packet);
    static void System(std::string text);
    static void Add(std::string name, std::string text, std::uint8_t kind);
    static void Open(bool swallow_opening_char = true);
    static void Cancel();
    static void Submit();
    static void Send(std::string text);
    static void HandleText(int code_point);
    [[nodiscard]] static bool HandleKeyDown(
        int key, bool control, bool alt, bool can_open,
        bool swallow_opening_char = true);

    ChatBox() = delete;

private:
    [[nodiscard]] static std::size_t WordStart(
        std::string_view text, std::size_t from) noexcept;
};

// ChatBox.cs reads process-wide GameState and NetSession statics directly.
// Native frontends bind those same observations here; this adapter owns no
// chat behavior or state of its own.
namespace detail {

struct RuntimeBindings {
    bool (*SinglePlayer)() noexcept = nullptr;
    bool (*NetActive)() noexcept = nullptr;
    std::string (*PlayerName)() = nullptr;
    void (*SendChat)(std::string_view) = nullptr;
    std::int64_t (*TickCount64)() noexcept = nullptr;
};

void BindRuntime(RuntimeBindings bindings) noexcept;

} // namespace detail

} // namespace fruityprime::chat
