#pragma once

#include "../../Formats/Enums.hpp"
#include "../../Formats/Types.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <memory>

namespace MphRead::Entities
{
    class EntityBase;
}

namespace MphRead::Hud
{
    class HudObject;
    class HudObjectInstance;
}

#define MPHREAD_PLAYER_SCAN_MEMBERS                                                            \
public:                                                                                       \
    inline static std::array<std::array<std::int32_t, 4>, 8> ScanIds = {{                     \
        {{0, 0, 0, 0}},                                                                        \
        {{232, 233, 532, 233}},                                                                \
        {{236, 237, 534, 237}},                                                                \
        {{230, 231, 531, 231}},                                                                \
        {{228, 229, 530, 229}},                                                                \
        {{234, 235, 533, 235}},                                                                \
        {{238, 239, 535, 239}},                                                                \
        {{225, 225, 224, 225}}                                                                 \
    }};                                                                                        \
    void SetCombatVisor();                                                                     \
    void ResetCombatVisor();                                                                   \
private:                                                                                       \
    class ScanTarget final                                                                     \
    {                                                                                          \
    public:                                                                                    \
        std::shared_ptr<::MphRead::Entities::EntityBase> Entity{};                             \
        float Distance = 0.0F;                                                                 \
        float CenterDist = std::numeric_limits<float>::max();                                  \
        std::int32_t Category = 0;                                                             \
        ::OpenTK::Mathematics::Vector3 Position{};                                             \
        float ScreenX = 0.0F;                                                                  \
        float ScreenY = 0.0F;                                                                  \
        float Scale = 0.0F;                                                                    \
        bool Dim = false;                                                                      \
    };                                                                                         \
    void SwitchVisors(bool reset);                                                             \
    void ResetScanValues();                                                                    \
    void UpdateScanHud();                                                                      \
    void UpdateScanning(bool scanning);                                                        \
    void UpdateScanState();                                                                    \
    void AfterScan();                                                                          \
    void DrawScanModels();                                                                     \
    void DrawScanObjects();                                                                    \
    void DrawScanProgress();                                                                   \
    void UpdateVisorMessage();                                                                 \
    void DrawVisorMessage();                                                                   \
                                                                                               \
    bool _silentVisorSwitch = false;                                                           \
    float _visorMessageTime = 30.0F / 30.0F;                                                  \
    float _visorMessageTimer = 30.0F / 30.0F;                                                 \
    std::int32_t _visorMessageId = 0;                                                          \
    bool _visorMessageScrollOut = false;                                                       \
                                                                                               \
    bool _scanning = false;                                                                    \
    bool _scanComplete = false;                                                                \
    std::shared_ptr<::MphRead::Entities::EntityBase> _scanningEntity{};                        \
    float _scanningTime = 0.0F;                                                                \
    float _scanningTimer = 0.0F;                                                               \
    std::int32_t _scanCategoryIndex = 0;                                                       \
    inline static const std::array<std::int32_t, 6> _scanCategoryLayers{{0, 1, 3, 2, 4, 4}}; \
                                                                                               \
    float _boxCornerFac = 0.125F;                                                              \
    float _boxSizeFac = 4.0F;                                                                  \
    float _boxCornerX = 0.5F;                                                                  \
    float _boxCornerY = 0.5F;                                                                  \
                                                                                               \
    inline static const std::array<::MphRead::SingleType, 10> _scanParticles{{                \
        ::MphRead::SingleType::Lore,                                                           \
        ::MphRead::SingleType::LoreDim,                                                        \
        ::MphRead::SingleType::Enemy,                                                          \
        ::MphRead::SingleType::EnemyDim,                                                       \
        ::MphRead::SingleType::Object,                                                         \
        ::MphRead::SingleType::ObjectDim,                                                      \
        ::MphRead::SingleType::Equipment,                                                      \
        ::MphRead::SingleType::EquipmentDim,                                                   \
        ::MphRead::SingleType::Red,                                                            \
        ::MphRead::SingleType::RedDim                                                          \
    }};                                                                                        \
    std::array<std::shared_ptr<::MphRead::Hud::HudObjectInstance>, 10> _scanIconInsts{};      \
    std::shared_ptr<::MphRead::Hud::HudObject> _scanCornerObj{};                               \
    std::shared_ptr<::MphRead::Hud::HudObject> _scanCornerSmallObj{};                          \
    std::shared_ptr<::MphRead::Hud::HudObjectInstance> _scanCornerInst{};                      \
    std::shared_ptr<::MphRead::Hud::HudObjectInstance> _scanLineHorizInst{};                   \
    std::shared_ptr<::MphRead::Hud::HudObjectInstance> _scanLineVertInst{};                    \
                                                                                               \
    std::int32_t _scanTargetCount = 0;                                                         \
    const std::shared_ptr<ScanTarget> _curScanTarget = std::make_shared<ScanTarget>();         \
    const std::array<std::shared_ptr<ScanTarget>, 32> _scanTargets = []                        \
    {                                                                                          \
        std::array<std::shared_ptr<ScanTarget>, 32> values{};                                  \
        for (auto& value : values)                                                             \
        {                                                                                      \
            value = std::make_shared<ScanTarget>();                                            \
        }                                                                                      \
        return values;                                                                         \
    }();
