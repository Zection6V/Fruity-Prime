#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"

#include <cstdint>
#include <memory>

namespace MphRead
{
    class Node;
}

namespace MphRead::Entities::Enemies
{
    class Enemy19Entity;

    class Enemy20Entity : public EnemyInstanceEntity
    {
    public:
        Enemy20Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy20Entity(const Enemy20Entity&) = delete;
        Enemy20Entity& operator=(const Enemy20Entity&) = delete;
        Enemy20Entity(Enemy20Entity&&) = delete;
        Enemy20Entity& operator=(Enemy20Entity&&) = delete;

        std::uint16_t BeamType = 2;
        bool EyeActive = true;
        std::uint16_t BeamSpawnCount = 0;
        std::int32_t BeamSpawnCooldown = 0;
        std::int32_t BeamSpawnTimer = 0;
        bool SpawnBurn = false;
        std::int32_t EyeIndex = 0;
        bool BeamColliding = false;
        std::int32_t SegmentIndex = 0;

        void SetUp(std::shared_ptr<Node> attachNode, std::int32_t scanId,
            std::uint32_t effectiveness, std::uint16_t health,
            OpenTK::Mathematics::Vector3 position, float radius);
        void UpdateState(std::uint8_t newState);

    protected:
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;
        [[nodiscard]] bool EnemyGetDrawInfo() override;

    private:
        Enemy19Entity* const _cretaphid;
        std::int32_t _stateTimer = 0;
        OpenTK::Mathematics::Vector3 _beamCollisionPos{};
        std::shared_ptr<Node> _attachNode{};
        OpenTK::Mathematics::Matrix4 _beamTransform{
            OpenTK::Mathematics::Vector4(1.0F, 0.0F, 0.0F, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, 1.0F, 0.0F, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, 0.0F, 1.0F, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, 0.0F, 0.0F, 1.0F)};
        void SpawnBeam();
        void CheckBeamCollision();
        void UpdateTransforms();
        [[nodiscard]] static OpenTK::Mathematics::Vector3 GetCrossVector(
            OpenTK::Mathematics::Vector3 up);
    };
}
