#pragma once

#include "../../Formats/Types.hpp"
#include "../EndScreen.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Members contributed by the C# PlayerEntity partial in PlayerEntityEndScreen.cs.
#define MPHREAD_PLAYER_ENTITY_END_SCREEN_MEMBERS                                                \
public:                                                                                         \
    void ModDrawEndScreen();                                                                    \
private:                                                                                        \
    static const ::OpenTK::Mathematics::Vector4 _endPanel;                                     \
    static const ::OpenTK::Mathematics::Vector4 _endPanelEdge;                                 \
    static const ::OpenTK::Mathematics::Vector4 _endSwatchEdge;                                \
    static const ::OpenTK::Mathematics::Vector4 _endSwatchWell;                                \
    static const ::MphRead::ColorRgba _endInk;                                                  \
    static const ::MphRead::ColorRgba _endDim;                                                  \
    static const ::MphRead::ColorRgba _endArrow;                                                \
    static const ::OpenTK::Mathematics::Vector4 _endSwatchHover;                               \
    [[nodiscard]] static float EndScale() noexcept;                                             \
    [[nodiscard]] static float EndPanelWidth() noexcept;                                        \
    inline static constexpr float EndPanelTop = 4.0F;                                           \
    [[nodiscard]] static float EndPanelHeight() noexcept;                                       \
    inline static constexpr float EndRowTitle = 3.0F;                                           \
    inline static constexpr float EndRowPreview = 13.0F;                                        \
    inline static constexpr float EndRowName = 53.5F;                                           \
    inline static constexpr float EndRowSuits = 67.5F;                                          \
    inline static constexpr float EndRowSuitName = 79.5F;                                       \
    inline static constexpr float EndRowReady = 90.0F;                                          \
    inline static constexpr float EndRowNext = 107.5F;                                          \
    inline static constexpr float EndStackHeight = 118.0F;                                      \
    [[nodiscard]] static float EndRow(float offset) noexcept;                                   \
    [[nodiscard]] static float EndPortrait() noexcept;                                          \
    [[nodiscard]] static float EndPreview() noexcept;                                           \
    void DrawEndFrame(float left, float top, float right, float bottom, float aspect);          \
    [[nodiscard]] static float EndArrowBox() noexcept;                                          \
    [[nodiscard]] static float EndReadyWidth() noexcept;                                        \
    [[nodiscard]] static float EndReadyHeight() noexcept;                                       \
    [[nodiscard]] ::MphRead::Mods::EndScreen::Hit DrawEndReady(                                \
        float centre, float top, float aspect);                                                  \
    [[nodiscard]] ::MphRead::Mods::EndScreen::Hit DrawEndArrow(                                \
        float x, float y, const std::string& glyph, bool hovered, float aspect);                \
    [[nodiscard]] static ::MphRead::Mods::EndScreen::Hit ModHudHit(                             \
        float left, float top, float right, float bottom) noexcept;                             \
    static const ::OpenTK::Mathematics::Vector4 _endReadyOn;                                   \
    static const ::MphRead::ColorRgba _endReadyInk;                                             \
    static const ::OpenTK::Mathematics::Vector4 _endArrowWell;                                 \
    static const ::OpenTK::Mathematics::Vector4 _endArrowHover;                                \
    [[nodiscard]] static std::size_t EndSuitHitCount() noexcept;                                \
    std::shared_ptr<std::vector<::MphRead::Mods::EndScreen::Hit>> _endSuitHits                 \
        = std::make_shared<std::vector<::MphRead::Mods::EndScreen::Hit>>(EndSuitHitCount());    \
    void DrawEndSuits(::MphRead::Hunter hunter, std::int32_t chosen,                            \
        float left, float top, float aspect);
