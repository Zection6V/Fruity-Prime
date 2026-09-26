#pragma once

#include "../EndScreen.hpp"
#include "../../Formats/Types.hpp"

#include <cstdint>
#include <vector>

#define MPHREAD_PLAYER_MAP_PICK_MEMBERS \
private: \
    static const ::OpenTK::Mathematics::Vector4 _pickPanel; \
    static const ::OpenTK::Mathematics::Vector4 _pickEdge; \
    static const ::OpenTK::Mathematics::Vector4 _pickRing; \
    static const ::OpenTK::Mathematics::Vector4 _pickHover; \
    static const ::OpenTK::Mathematics::Vector4 _pickCursor; \
    static const ::OpenTK::Mathematics::Vector4 _pickVoted; \
    static const ::OpenTK::Mathematics::Vector4 _pickCarriedBand; \
    static const ::OpenTK::Mathematics::Vector4 _pickWell; \
    static const ::OpenTK::Mathematics::Vector4 _pickBar; \
    static const ::MphRead::ColorRgba _pickInk; \
    static const ::MphRead::ColorRgba _pickDim; \
    static const ::MphRead::ColorRgba _pickTally; \
    static const ::MphRead::ColorRgba _pickCarried; \
    static constexpr float PickTitle = 8; \
    static constexpr float PickThumb = 13; \
    static constexpr float PickGap = 2.5F; \
    static constexpr float PickRow = PickThumb + PickGap; \
    static constexpr float PickThumbWidth = PickThumb * 16 / 9.0F; \
    static constexpr float PickFloor = 190; \
    static constexpr std::int32_t PickRowsMax = 4; \
    std::vector<::MphRead::Mods::EndScreen::Hit> _pickHits = std::vector<::MphRead::Mods::EndScreen::Hit>(PickRowsMax); \
public: \
    void ModDrawMapPick(float panelBottom); \
private: \
    void DrawPickScrollBar(float right, float top, float height, std::int32_t scroll, std::int32_t rows, \
        std::int32_t total, float aspect); \
    void DrawPickRow(std::int32_t slot, std::int32_t index, float left, float top, float width, float scale, \
        float aspect, bool picked, bool hovered, bool cursor); \
    void DrawPickRing(float left, float top, float right, float bottom, float scale, float aspect);

#ifndef MPHREAD_PLAYER_ENTITY_CANONICAL_HEADER
#include "../../Entities/Players/PlayerEntity.hpp"
#endif
