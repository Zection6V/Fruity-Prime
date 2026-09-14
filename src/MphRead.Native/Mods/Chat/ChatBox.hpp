#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <utility>
#include <vector>

namespace OpenTK::Windowing::GraphicsLibraryFramework
{
    enum class Keys : std::int32_t;
}

namespace OpenTK::Windowing::Common
{
    struct KeyboardKeyEventArgs
    {
        GraphicsLibraryFramework::Keys Key;
        bool Control = false;
        bool Alt = false;
        bool Command = false;
    };
}

namespace MphRead::Mods::Network
{
    struct ChatPacket;
}

namespace MphRead::Mods::Chat
{
    struct ChatLine
    {
        const std::string Name;
        const std::string Text;
        const std::uint8_t Kind;
        const std::int64_t ArrivedAt;

        ChatLine();
        ChatLine(std::string name, std::string text, std::uint8_t kind,
            std::int64_t arrivedAt);
        ChatLine& operator=(const ChatLine& other);
    };

    class ChatBox final
    {
    public:
        ChatBox() = delete;
        ChatBox(const ChatBox&) = delete;
        ChatBox& operator=(const ChatBox&) = delete;

        inline static constexpr std::int32_t VisibleLines = 3;
        inline static constexpr std::int32_t MaxLength = 80;

        [[nodiscard]] static bool Composing() noexcept;
        [[nodiscard]] static bool ConsumeJustClosed() noexcept;
        [[nodiscard]] static std::int32_t Sent() noexcept;
        [[nodiscard]] static std::int32_t Received() noexcept;

        [[nodiscard]] static std::string ComposeText();
        static void CollectVisible(std::vector<std::pair<ChatLine, float>>& into);

        [[nodiscard]] static bool Available();
        [[nodiscard]] static bool Visible();

        static void Clear();
        static void Receive(const Network::ChatPacket& packet);
        static void System(const std::string& text);
        static void Add(const std::string& name, const std::string& text, std::uint8_t kind);

        static void Open(bool swallowOpeningChar = true);
        static void Cancel();
        static void Submit();
        static void Send(const std::string& text);
        static void HandleText(std::int32_t codePoint);

        static bool HandleKeyDown(
            const OpenTK::Windowing::Common::KeyboardKeyEventArgs& e, bool canOpen);
        static bool HandleKeyDown(
            OpenTK::Windowing::GraphicsLibraryFramework::Keys key,
            bool control, bool alt, bool canOpen, bool swallowOpeningChar = true);

    private:
        inline static constexpr std::int64_t HoldMilliseconds = 10'000;
        inline static constexpr std::int64_t FadeMilliseconds = 1'000;

        struct State;
        [[nodiscard]] static State& GetState();
        [[nodiscard]] static std::int32_t WordStart(const std::string& text, std::int32_t from);
    };
}
