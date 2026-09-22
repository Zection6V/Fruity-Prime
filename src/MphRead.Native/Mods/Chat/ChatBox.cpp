#include "ChatBox.hpp"

#include "../../GameState.hpp"
#include "../InputSettings.hpp"
#include "../Network/NetProtocol.hpp"
#include "../Network/NetSession.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    using Keys = OpenTK::Windowing::GraphicsLibraryFramework::Keys;

    constexpr Keys KeyUnknown = static_cast<Keys>(-1);
    constexpr Keys KeyV = static_cast<Keys>(86);
    constexpr Keys KeyEscape = static_cast<Keys>(256);
    constexpr Keys KeyEnter = static_cast<Keys>(257);
    constexpr Keys KeyBackspace = static_cast<Keys>(259);
    constexpr Keys KeyPadEnter = static_cast<Keys>(335);

    [[nodiscard]] std::int64_t TickCount64() noexcept
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }

    [[nodiscard]] std::int32_t IncrementInt32(std::int32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value) + 1U);
    }

    [[nodiscard]] std::int64_t SubtractInt64(std::int64_t left, std::int64_t right) noexcept
    {
        const std::uint64_t result = static_cast<std::uint64_t>(left)
            - static_cast<std::uint64_t>(right);
        return std::bit_cast<std::int64_t>(result);
    }

    [[nodiscard]] bool IsWhiteSpaceCodePoint(std::uint32_t value) noexcept
    {
        return (value >= 0x0009U && value <= 0x000DU)
            || value == 0x0020U
            || value == 0x0085U
            || value == 0x00A0U
            || value == 0x1680U
            || (value >= 0x2000U && value <= 0x200AU)
            || value == 0x2028U
            || value == 0x2029U
            || value == 0x202FU
            || value == 0x205FU
            || value == 0x3000U;
    }

    [[nodiscard]] bool NextUtf8CodePoint(
        std::string_view text, std::size_t& offset, std::uint32_t& value) noexcept
    {
        const auto first = static_cast<std::uint8_t>(text[offset]);
        if (first < 0x80U)
        {
            value = first;
            offset++;
            return true;
        }

        std::size_t count = 0;
        std::uint32_t codePoint = 0;
        std::uint32_t minimum = 0;
        if ((first & 0xE0U) == 0xC0U)
        {
            count = 2;
            codePoint = first & 0x1FU;
            minimum = 0x80U;
        }
        else if ((first & 0xF0U) == 0xE0U)
        {
            count = 3;
            codePoint = first & 0x0FU;
            minimum = 0x800U;
        }
        else if ((first & 0xF8U) == 0xF0U)
        {
            count = 4;
            codePoint = first & 0x07U;
            minimum = 0x10000U;
        }
        else
        {
            return false;
        }
        if (offset + count > text.size())
        {
            return false;
        }
        for (std::size_t i = 1; i < count; i++)
        {
            const auto next = static_cast<std::uint8_t>(text[offset + i]);
            if ((next & 0xC0U) != 0x80U)
            {
                return false;
            }
            codePoint = (codePoint << 6U) | (next & 0x3FU);
        }
        if (codePoint < minimum || codePoint > 0x10FFFFU
            || (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
        {
            return false;
        }
        value = codePoint;
        offset += count;
        return true;
    }

    [[nodiscard]] bool IsNullOrWhiteSpace(std::string_view text) noexcept
    {
        if (text.empty())
        {
            return true;
        }
        std::size_t offset = 0;
        while (offset < text.size())
        {
            std::uint32_t value = 0;
            if (!NextUtf8CodePoint(text, offset, value) || !IsWhiteSpaceCodePoint(value))
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] std::string TrimCompose(std::string_view text)
    {
        std::size_t first = 0;
        while (first < text.size() && text[first] == ' ')
        {
            first++;
        }
        std::size_t last = text.size();
        while (last > first && text[last - 1] == ' ')
        {
            last--;
        }
        return std::string(text.substr(first, last - first));
    }
}

namespace MphRead::Mods::Chat
{
    struct ChatBox::State
    {
        std::vector<ChatLine> Lines{};
        std::string Compose{};
        std::mutex Lock{};
        bool Composing = false;
        bool SwallowNextChar = false;
        bool JustClosed = false;
        std::int32_t Sent = 0;
        std::int32_t Received = 0;
    };

    ChatLine::ChatLine()
        : Name(), Text(), Kind(0), ArrivedAt(0)
    {
    }

    ChatLine::ChatLine(std::optional<std::string> name, std::optional<std::string> text,
        std::uint8_t kind, std::int64_t arrivedAt)
        : Name(std::move(name)), Text(std::move(text)), Kind(kind), ArrivedAt(arrivedAt)
    {
    }

    ChatLine& ChatLine::operator=(const ChatLine& other)
    {
        if (this != std::addressof(other))
        {
            std::destroy_at(this);
            std::construct_at(this, other);
        }
        return *this;
    }

    ChatBox::State& ChatBox::GetState()
    {
        static State state;
        return state;
    }

    bool ChatBox::Composing() noexcept
    {
        return GetState().Composing;
    }

    bool ChatBox::ConsumeJustClosed() noexcept
    {
        State& state = GetState();
        if (!state.JustClosed)
        {
            return false;
        }
        state.JustClosed = false;
        return true;
    }

    std::int32_t ChatBox::Sent() noexcept
    {
        return GetState().Sent;
    }

    std::int32_t ChatBox::Received() noexcept
    {
        return GetState().Received;
    }

    std::string ChatBox::ComposeText()
    {
        State& state = GetState();
        std::lock_guard<std::mutex> guard(state.Lock);
        return state.Compose;
    }

    void ChatBox::CollectVisible(std::vector<std::pair<ChatLine, float>>& into)
    {
        into.clear();
        const std::int64_t now = TickCount64();
        State& state = GetState();
        {
            std::lock_guard<std::mutex> guard(state.Lock);
            for (std::int32_t i = static_cast<std::int32_t>(state.Lines.size()) - 1;
                i >= 0 && static_cast<std::int32_t>(into.size()) < VisibleLines; i--)
            {
                const ChatLine& line = state.Lines[static_cast<std::size_t>(i)];
                const std::int64_t age = SubtractInt64(now, line.ArrivedAt);
                if (age >= HoldMilliseconds)
                {
                    break;
                }
                const float alpha = age > HoldMilliseconds - FadeMilliseconds
                    ? static_cast<float>(SubtractInt64(HoldMilliseconds, age))
                        / static_cast<float>(FadeMilliseconds)
                    : 1.0F;
                into.emplace_back(line, alpha);
            }
        }
        std::reverse(into.begin(), into.end());
    }

    bool ChatBox::Available()
    {
        return !MphRead::GameState::SinglePlayer();
    }

    bool ChatBox::Visible()
    {
        if (!Available())
        {
            return false;
        }
        State& state = GetState();
        if (state.Composing)
        {
            return true;
        }
        const std::int64_t now = TickCount64();
        std::lock_guard<std::mutex> guard(state.Lock);
        return !state.Lines.empty()
            && SubtractInt64(now, state.Lines.back().ArrivedAt) < HoldMilliseconds;
    }

    void ChatBox::Clear()
    {
        State& state = GetState();
        {
            std::lock_guard<std::mutex> guard(state.Lock);
            state.Lines.clear();
            state.Compose.clear();
        }
        state.Composing = false;
        state.SwallowNextChar = false;
    }

    void ChatBox::Receive(const Network::ChatPacket& packet)
    {
        if (!packet.Name.has_value())
        {
            throw System::NullReferenceException();
        }
        const std::string name = packet.Name->empty()
            ? "Player" + std::to_string(static_cast<std::uint32_t>(packet.Slot))
            : *packet.Name;
        if (packet.Kind == Network::ChatPacket::KindSystem)
        {
            Add(std::string{}, packet.Text, Network::ChatPacket::KindSystem);
        }
        else
        {
            Add(name, packet.Text, packet.Kind);
        }
        State& state = GetState();
        state.Received = IncrementInt32(state.Received);
    }

    void ChatBox::System(const std::optional<std::string>& text)
    {
        Add(std::string{}, text, Network::ChatPacket::KindSystem);
    }

    void ChatBox::Add(const std::optional<std::string>& name,
        const std::optional<std::string>& text, std::uint8_t kind)
    {
        if (!text.has_value() || IsNullOrWhiteSpace(*text))
        {
            return;
        }
        State& state = GetState();
        std::lock_guard<std::mutex> guard(state.Lock);
        state.Lines.emplace_back(name, text, kind, TickCount64());
        if (state.Lines.size() > 64U)
        {
            state.Lines.erase(state.Lines.begin(),
                state.Lines.begin() + static_cast<std::ptrdiff_t>(state.Lines.size() - 64U));
        }
    }

    void ChatBox::Open(bool swallowOpeningChar)
    {
        State& state = GetState();
        if (state.Composing || !Available())
        {
            return;
        }
        {
            std::lock_guard<std::mutex> guard(state.Lock);
            state.Compose.clear();
        }
        state.Composing = true;
        state.SwallowNextChar = swallowOpeningChar;
    }

    void ChatBox::Cancel()
    {
        State& state = GetState();
        {
            std::lock_guard<std::mutex> guard(state.Lock);
            state.Compose.clear();
        }
        state.Composing = false;
        state.SwallowNextChar = false;
        state.JustClosed = true;
    }

    void ChatBox::Submit()
    {
        State& state = GetState();
        std::string text;
        {
            std::lock_guard<std::mutex> guard(state.Lock);
            text = TrimCompose(state.Compose);
            state.Compose.clear();
        }
        state.Composing = false;
        state.SwallowNextChar = false;
        state.JustClosed = true;
        if (text.empty())
        {
            return;
        }
        if (text.size() > static_cast<std::size_t>(MaxLength))
        {
            text.resize(static_cast<std::size_t>(MaxLength));
        }
        Send(text);
    }

    void ChatBox::Send(const std::optional<std::string>& text)
    {
        Add(Network::NetSession::Active() ? Network::NetSession::PlayerName() : "You",
            text, Network::ChatPacket::KindSay);
        if (Network::NetSession::Active())
        {
            if (text.has_value())
            {
                Network::NetSession::SendChat(*text);
            }
            State& state = GetState();
            state.Sent = IncrementInt32(state.Sent);
        }
    }

    void ChatBox::HandleText(std::int32_t codePoint)
    {
        State& state = GetState();
        if (!state.Composing)
        {
            return;
        }
        if (state.SwallowNextChar)
        {
            state.SwallowNextChar = false;
            return;
        }
        if (codePoint < 32 || codePoint > 126)
        {
            return;
        }
        std::lock_guard<std::mutex> guard(state.Lock);
        if (state.Compose.size() < static_cast<std::size_t>(MaxLength))
        {
            state.Compose.push_back(static_cast<char>(codePoint));
        }
    }

    bool ChatBox::HandleKeyDown(
        const OpenTK::Windowing::Common::KeyboardKeyEventArgs& e, bool canOpen)
    {
        return HandleKeyDown(e.Key, e.Control, e.Alt || e.Command, canOpen);
    }

    bool ChatBox::HandleKeyDown(
        OpenTK::Windowing::GraphicsLibraryFramework::Keys key,
        bool control, bool alt, bool canOpen, bool swallowOpeningChar)
    {
        State& state = GetState();
        if (!state.Composing)
        {
            if (canOpen && Available() && key != KeyUnknown
                && key == MphRead::Mods::InputSettings::ChatKey() && !alt && !control)
            {
                Open(swallowOpeningChar);
                return true;
            }
            return false;
        }

        if (key == KeyEnter || key == KeyPadEnter)
        {
            Submit();
            return true;
        }
        if (key == KeyEscape)
        {
            Cancel();
            return true;
        }
        if (key == KeyBackspace)
        {
            std::lock_guard<std::mutex> guard(state.Lock);
            if (!state.Compose.empty())
            {
                const std::int32_t end = static_cast<std::int32_t>(state.Compose.size());
                const std::int32_t cut = control ? WordStart(state.Compose, end) : end - 1;
                state.Compose.erase(static_cast<std::size_t>(cut),
                    static_cast<std::size_t>(end - cut));
            }
            return true;
        }
        if (key == KeyV && control)
        {
            return true;
        }
        return true;
    }

    std::int32_t ChatBox::WordStart(const std::string& text, std::int32_t from)
    {
        std::int32_t i = from;
        while (i > 0 && text[static_cast<std::size_t>(i - 1)] == ' ')
        {
            i--;
        }
        while (i > 0 && text[static_cast<std::size_t>(i - 1)] != ' ')
        {
            i--;
        }
        return i;
    }
}
