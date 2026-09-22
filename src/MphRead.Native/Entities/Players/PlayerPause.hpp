#pragma once

#include "../../Formats/Types.hpp"
#include "../../HUD/HudInfo.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace MphRead
{
    class ModelInstance;
    class Node;
}

#define MPHREAD_PLAYER_PAUSE_MEMBERS                                                          \
public:                                                                                       \
    void SetUpMenuPauseHud();                                                                  \
    void SetUpMenuPauseMapNav();                                                               \
    void EndMenuPauseHud();                                                                    \
    void DrawPauseMenuBackground();                                                            \
    void DrawPauseMenuForeground();                                                            \
    void ProcessPauseMenu();                                                                   \
    [[nodiscard]] std::pair<::OpenTK::Mathematics::Matrix4,                                   \
        ::OpenTK::Mathematics::Matrix4> GetPauseMapMatrices();                                 \
    void GetPauseMapRenderItems();                                                             \
private:                                                                                       \
    class MapLegendInfo final                                                                  \
    {                                                                                          \
    public:                                                                                    \
        bool Unlocked = false;                                                                 \
        std::int32_t Group = 0;                                                                \
        std::int32_t MessageId = 0;                                                            \
        std::int32_t OffsetX = 0;                                                              \
        std::int32_t OffsetY = 0;                                                              \
        std::shared_ptr<::MphRead::Hud::HudObjectInstance> HudObject{};                        \
        std::int32_t ObjectIndex = 0;                                                          \
                                                                                               \
        MapLegendInfo(bool unlocked, std::int32_t messageId,                                  \
            std::int32_t offsetX, std::int32_t offsetY,                                       \
            std::shared_ptr<::MphRead::Hud::HudObjectInstance> hudObject,                      \
            std::int32_t objectIndex)                                                          \
            : Unlocked(unlocked), Group(unlocked ? 1 : 0), MessageId(messageId),               \
              OffsetX(offsetX), OffsetY(offsetY), HudObject(std::move(hudObject)),             \
              ObjectIndex(objectIndex)                                                         \
        {                                                                                      \
        }                                                                                      \
    };                                                                                         \
                                                                                               \
    void SetNavMapDrawNode(std::shared_ptr<::MphRead::Node> roomNode,                         \
        std::shared_ptr<::MphRead::Node> centerNode);                                          \
    void DrawPauseQuitInterface();                                                             \
    void ResetPauseQuitDisplay();                                                              \
    void ProcessPauseMenuInput();                                                              \
    [[nodiscard]] std::pair<::OpenTK::Mathematics::Vector3,                                   \
        ::OpenTK::Mathematics::Vector3> GetPauseMapLookVectors();                              \
    void UpdateMapModelTransforms(::MphRead::ModelInstance& inst, std::int32_t area);          \
    [[nodiscard]] bool CheckRoomVisited(const ::MphRead::Node& node) const;                    \
    void GetMapDrawItems(::MphRead::ModelInstance& inst);                                      \
                                                                                               \
    std::int32_t _pauseFrameCount = 0;                                                        \
    static std::int32_t _drawPauseState;                                                       \
    static float _navTextTimer;                                                                \
    static bool _navLoading;                                                                   \
    std::int32_t _pausedPrevBindingId1 = -1;                                                  \
    float _pausedPrevAlpha1 = 1.0F;                                                            \
    std::int32_t _pausedPrevMaskId = -1;                                                      \
    std::int32_t _pausedPrevBindingId2 = -1;                                                  \
    float _pausedPrevAlpha2 = 1.0F;                                                            \
    std::int32_t _pausedPrevBindingId3 = -1;                                                  \
    std::int32_t _pausedPrevBindingId4 = -1;                                                  \
    std::int32_t _pausedPrevBindingId5 = -1;                                                  \
                                                                                               \
    static const std::array<::OpenTK::Mathematics::Vector3, 10> _navDoorColors;               \
    static const std::array<::OpenTK::Mathematics::Vector3, 9> _navMapNodeOffsets;             \
    bool _navMapModelEnabled = false;                                                          \
    std::shared_ptr<::MphRead::Node> _navMapDrawNode{};                                       \
    float _navDrawZoom = 0.0F;                                                                 \
    float _navDrawRotX = 0.0F;                                                                 \
    float _navDrawRotY = 0.0F;                                                                 \
    float _navPanTimer = 0.0F;                                                                 \
    ::OpenTK::Mathematics::Vector3 _navCurRoomNodePos{};                                      \
    ::OpenTK::Mathematics::Vector3 _navCurCenterNodePos{};                                    \
    ::OpenTK::Mathematics::Vector3 _navInitRoomNodePos{};                                     \
    ::OpenTK::Mathematics::Vector3 _navTargetPos{};                                           \
    ::OpenTK::Mathematics::Vector3 _navPanOffset{};                                           \
                                                                                               \
    static const std::array<std::pair<std::int16_t, std::int16_t>, 8> _mapIconPositions;      \
    static const std::array<std::pair<std::int16_t, std::int16_t>, 3> _mapDotOffsets;         \
    std::vector<MapLegendInfo> _mapLegendInfo{};
