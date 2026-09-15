#pragma once

#include "../../Formats/Types.hpp"
#include "../../HUD/HudInfo.hpp"
#include "ChatBox.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#define MPHREAD_PLAYER_ENTITY_CHAT_HUD_MEMBERS                                                \
public:                                                                                       \
    void ModDrawChat();                                                                        \
    void ModForgetInputDeltas();                                                               \
private:                                                                                       \
    inline static constexpr float ChatScale = 0.45F;                                          \
    inline static constexpr float ChatLineHeight = 7.0F * ChatScale + 1.2F;                  \
    inline static constexpr float ChatBottom = 168.0F;                                        \
    inline static constexpr float ChatPromptY = ChatBottom - ChatLineHeight;                  \
    inline static constexpr float ChatMargin = 3.0F;                                          \
    [[nodiscard]] static float ChatLeft(float aspect);                                        \
    static const ::MphRead::ColorRgba ChatName;                                               \
    static const ::MphRead::ColorRgba ChatInk;                                                \
    static const ::MphRead::ColorRgba ChatSystemInk;                                          \
    static const ::MphRead::ColorRgba ChatPromptInk;                                          \
    static const ::MphRead::Hud::ReadOnlyList<::MphRead::ColorRgba> _chatPalette;             \
    std::shared_ptr<::MphRead::Hud::HudObjectInstance> _chatInst{};                            \
    std::vector<std::pair<::MphRead::Mods::Chat::ChatLine, float>> _chatVisible{};            \
    inline static constexpr std::string_view ChatPrompt = "Says: ";                           \
    float ChatDraw(float x, float y, float aspect, float alpha,                               \
        std::string_view text, ::MphRead::ColorRgba color);                                   \
    [[nodiscard]] static float ChatWidth(std::string_view text, float aspect);                \
    [[nodiscard]] static float ChatRoom(float aspect, float used);                            \
    [[nodiscard]] static std::string ChatFit(                                                 \
        const std::string& text, float aspect, float used);                                   \
    [[nodiscard]] static std::string ChatTail(                                                \
        const std::string& text, float aspect, float used);
