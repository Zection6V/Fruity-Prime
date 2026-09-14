#pragma once

#include "../../Formats/Types.hpp"

#include <cstdint>
#include <optional>

namespace MphRead
{
    class Material;
    class ModelInstance;
    class Node;
}

#define MPHREAD_PLAYER_DRAW_MEMBERS \
public: \
    void Draw(); \
private: \
    void DrawKandenAlt(); \
    void UpdateSpireAltAttack(); \
    void DrawSpireAltAttack(); \
    void GetDrawItems(::MphRead::ModelInstance& inst, ::MphRead::Node& node, float alpha, \
        std::int32_t polygonId = -1, std::int32_t recolor = -1); \
protected: \
    [[nodiscard]] std::optional<std::int32_t> GetBindingOverride( \
        ::MphRead::ModelInstance& inst, ::MphRead::Material& material, \
        std::int32_t index) override; \
    [[nodiscard]] ::OpenTK::Mathematics::Vector3 GetEmission( \
        ::MphRead::ModelInstance& inst, ::MphRead::Material& material, \
        std::int32_t index) override; \
    [[nodiscard]] ::OpenTK::Mathematics::Matrix4 GetTexcoordMatrix( \
        ::MphRead::ModelInstance& inst, ::MphRead::Material& material, \
        std::int32_t materialId, ::MphRead::Node& node, std::int32_t recolor = -1) override; \
private: \
    void DrawShadow(); \
    void DrawMorphBallTrail(); \
    void DrawDeathParticles(); \
    void DrawVolumes(); \
public: \
    void GetDrawInfo() override;
