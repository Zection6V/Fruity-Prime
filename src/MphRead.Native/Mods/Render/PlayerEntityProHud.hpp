#pragma once

#include "../../Formats/Types.hpp"
#include "../../HUD/HudInfo.hpp"

#include <cstdint>
#include <optional>
#include <string>

#define MPHREAD_PLAYER_ENTITY_PRO_HUD_MEMBERS                                                  \
private:                                                                                       \
    inline static constexpr float ProHudWarn = 60.0F / 99.0F;                                 \
    inline static constexpr float ProHudDanger = 33.0F / 99.0F;                               \
    static const ::OpenTK::Mathematics::Vector4 ProGood;                                      \
    static const ::OpenTK::Mathematics::Vector4 ProWarn;                                      \
    static const ::OpenTK::Mathematics::Vector4 ProDanger;                                    \
    static const ::OpenTK::Mathematics::Vector4 ProHudPanel;                                  \
    static const ::OpenTK::Mathematics::Vector4 ProHudShade;                                  \
    static const ::OpenTK::Mathematics::Vector4 ProHudTrack;                                  \
    static const ::MphRead::ColorRgba ProHudInk;                                               \
    static const ::MphRead::ColorRgba ProHudDim;                                               \
    static const ::MphRead::ColorRgba ProHudShadow;                                            \
    inline static constexpr float ProAmmoPanelWidth = 58.0F;                                  \
    inline static constexpr float ProAmmoNumberScale = 1.5F;                                  \
    void DrawProHud();                                                                          \
    void DrawProAmmo();                                                                         \
    void DrawProAmmoIcon(float x, float y);                                                     \
    [[nodiscard]] float ProHealthFraction();                                                    \
    [[nodiscard]] std::int32_t ProHealthSpan();                                                 \
    [[nodiscard]] ::OpenTK::Mathematics::Vector4 ProHealthColor();                             \
    [[nodiscard]] static ::MphRead::ColorRgba ProInk(::OpenTK::Mathematics::Vector4 color);   \
    [[nodiscard]] std::optional<std::string> ProAmmoText();                                    \
    [[nodiscard]] static std::int32_t ProAmmoFull();                                            \
    [[nodiscard]] float ProAmmoFraction();                                                      \
    [[nodiscard]] ::OpenTK::Mathematics::Vector4 ProAmmoColor();                              \
    void ProBar(float x, float y, float width, float height, float fill,                       \
        ::OpenTK::Mathematics::Vector4 color);                                                 \
    void ProNumber(float x, float y, ::MphRead::Hud::Align align, const std::string& text,     \
        ::MphRead::ColorRgba color, float scale);                                               \
    void ProScore(float x, float y, ::MphRead::Hud::Align align, float scale);                 \
    [[nodiscard]] static std::int32_t ProScoreMessageId();
