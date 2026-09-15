#include "PlayerEntityChatHud.hpp"

#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Features.hpp"
#include "../../Scene.hpp"
#include "../Network/NetProtocol.hpp"
#include "ChatFont.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    template <typename T>
    [[nodiscard]] T& RequireReference(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    [[nodiscard]] std::u16string ToChatChars(std::string_view text)
    {
        std::u16string result;
        result.reserve(text.size());
        for (unsigned char ch : text)
        {
            result.push_back(static_cast<char16_t>(ch));
        }
        return result;
    }

    template <typename T>
    [[nodiscard]] ::MphRead::Hud::ReadOnlyList<T> ToReadOnlyList(std::span<T> values)
    {
        return std::make_shared<const std::vector<T>>(values.begin(), values.end());
    }

    [[nodiscard]] const std::string& RequireString(const std::optional<std::string>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }
}

namespace MphRead::Entities
{
    const ColorRgba PlayerEntity::ChatName(110, 255, 130, 255);
    const ColorRgba PlayerEntity::ChatInk(170, 255, 175, 255);
    const ColorRgba PlayerEntity::ChatSystemInk(110, 205, 125, 255);
    const ColorRgba PlayerEntity::ChatPromptInk(110, 205, 125, 255);

    const Hud::ReadOnlyList<ColorRgba> PlayerEntity::_chatPalette
        = std::make_shared<const std::vector<ColorRgba>>(
            std::initializer_list<ColorRgba>{ColorRgba(), ColorRgba(255, 255, 255, 255)});

    float PlayerEntity::ChatLeft(float aspect)
    {
        const float margin = ChatMargin * aspect;
        if (!Features::ModernHud())
        {
            return margin;
        }
        const float scale = std::clamp(Features::WeaponListScale(), 0.6F, 2.0F);
        return 2.0F * aspect + 26.0F * scale * aspect + margin;
    }

    void PlayerEntity::ModDrawChat()
    {
        using Mods::Chat::ChatBox;
        using Mods::Chat::ChatFont;
        using Mods::Network::ChatPacket;

        if (!ChatBox::Visible())
        {
            return;
        }
        Scene& scene = RequireReference(_scene);
        if (!_chatInst)
        {
            _chatInst = std::make_shared<Hud::HudObjectInstance>(ChatFont::Cell, ChatFont::Cell);
            _chatInst->SetPaletteData(_chatPalette, scene);
            _chatInst->SetCharacterData(ToReadOnlyList(ChatFont::Pixels()), scene);
            _chatInst->Enabled = true;
        }
        ChatBox::CollectVisible(_chatVisible);
        const float aspect = HudAspectFix();
        const float x = ChatLeft(aspect);
        float y = ChatPromptY - static_cast<float>(_chatVisible.size()) * ChatLineHeight;
        for (std::size_t i = 0; i < _chatVisible.size(); ++i)
        {
            const auto& [line, alpha] = _chatVisible[i];
            const bool system = line.Kind == ChatPacket::KindSystem;
            const std::string& lineName = RequireString(line.Name);
            const std::string& lineText = RequireString(line.Text);
            const std::string name = system || lineName.empty()
                ? std::string()
                : lineName + ": ";
            const float at = ChatDraw(x, y, aspect, alpha, name,
                system ? ChatSystemInk : ChatName);
            ChatDraw(at, y, aspect, alpha,
                ChatFit(lineText, aspect, at - x), system ? ChatSystemInk : ChatInk);
            y += ChatLineHeight;
        }
        if (ChatBox::Composing())
        {
            y = ChatPromptY;
            const float at = ChatDraw(x, y, aspect, 1.0F, ChatPrompt, ChatPromptInk);
            ChatDraw(at, y, aspect, 1.0F,
                ChatTail(ChatBox::ComposeText() + "_", aspect, at - x), ChatInk);
        }
    }

    float PlayerEntity::ChatDraw(float x, float y, float aspect, float alpha,
        std::string_view text, ColorRgba color)
    {
        using Mods::Chat::ChatFont;

        Hud::HudObjectInstance& inst = RequireReference(_chatInst);
        Scene& scene = RequireReference(_scene);
        inst.Alpha = alpha;
        for (char ch : text)
        {
            const std::int32_t index = ChatFont::Index(
                static_cast<char16_t>(static_cast<unsigned char>(ch)));
            if (index < 0)
            {
                continue;
            }
            if (ch != ' ')
            {
                inst.PositionX = x / 256.0F;
                inst.PositionY = y / 192.0F;
                inst.SetData(index, color, scene);
                scene.DrawHudObject(inst, 1, ChatScale);
            }
            x += static_cast<float>(ChatFont::Widths()[static_cast<std::size_t>(index)])
                * ChatScale * aspect;
        }
        return x;
    }

    float PlayerEntity::ChatWidth(std::string_view text, float aspect)
    {
        const std::u16string chars = ToChatChars(text);
        return static_cast<float>(Mods::Chat::ChatFont::Measure(chars)) * ChatScale * aspect;
    }

    float PlayerEntity::ChatRoom(float aspect, float used)
    {
        return 256.0F - ChatLeft(aspect) - ChatMargin * aspect - used;
    }

    std::string PlayerEntity::ChatFit(const std::string& text, float aspect, float used)
    {
        const float room = ChatRoom(aspect, used);
        if (ChatWidth(text, aspect) <= room)
        {
            return text;
        }
        std::size_t count = text.size();
        while (count > 0 && ChatWidth(std::string_view(text).substr(0, count), aspect) > room)
        {
            --count;
        }
        return text.substr(0, count);
    }

    std::string PlayerEntity::ChatTail(const std::string& text, float aspect, float used)
    {
        const float room = ChatRoom(aspect, used);
        if (ChatWidth(text, aspect) <= room)
        {
            return text;
        }
        std::size_t start = 0;
        while (start < text.size() && ChatWidth(std::string_view(text).substr(start), aspect) > room)
        {
            ++start;
        }
        return text.substr(start);
    }

    void PlayerEntity::ModForgetInputDeltas()
    {
        _input.MouseState.reset();
        _input.PrevMouseState.reset();
        _input.KeyboardState.reset();
        _input.PrevKeyboardState.reset();
    }
}
