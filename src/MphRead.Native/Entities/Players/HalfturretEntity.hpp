#pragma once

#include "DynamicLightEntity.hpp"

#include "../../Formats/Culling.hpp"
#include "../../Formats/Types.hpp"
#include "../../Mods/Combat/HalfturretFireRate.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

namespace MphRead
{
    class EquipInfo;
    class Material;
    class ModelInstance;
    class Node;
    class Scene;

    namespace Effects
    {
        class EffectEntry;
    }

    namespace Formats
    {
        class NodeData3;
    }
}

namespace MphRead::Entities
{
    class PlayerEntity;

    class HalfturretEntity : public DynamicLightEntityBase
    {
        friend class WeavelAltFormParityTest;
    public:
        HalfturretEntity(std::shared_ptr<PlayerEntity> owner, Scene* scene);

        HalfturretEntity(const HalfturretEntity&) = delete;
        HalfturretEntity& operator=(const HalfturretEntity&) = delete;
        HalfturretEntity(HalfturretEntity&&) = delete;
        HalfturretEntity& operator=(HalfturretEntity&&) = delete;

        [[nodiscard]] std::shared_ptr<PlayerEntity> Owner() const noexcept;
        [[nodiscard]] std::shared_ptr<EntityBase> Target() const noexcept;
        [[nodiscard]] std::shared_ptr<Formats::NodeData3> ClosestNode() const noexcept;
        void SetClosestNode(std::shared_ptr<Formats::NodeData3> value) noexcept;

        [[nodiscard]] std::int32_t Health() const noexcept;
        void SetHealth(std::int32_t value) noexcept;
        [[nodiscard]] std::uint16_t TimeSinceDamage() const noexcept;
        void SetTimeSinceDamage(std::uint16_t value) noexcept;
        [[nodiscard]] std::shared_ptr<MphRead::EquipInfo> EquipInfo() const noexcept;

        void Create();
        void ResetForSpawn();
        void Initialize() override;
        [[nodiscard]] std::int32_t CooldownFactorRaw() const noexcept { return _cooldownFactorRaw; }
        [[nodiscard]] std::uint32_t NativeShotThreshold() const;
        [[nodiscard]] bool Grounded() const noexcept { return _grounded; }
        void GetVectors(::OpenTK::Mathematics::Vector3& position,
            ::OpenTK::Mathematics::Vector3& up,
            ::OpenTK::Mathematics::Vector3& facing) override;
        void Reposition(::OpenTK::Mathematics::Vector3 offset,
            Formats::Culling::NodeRef nodeRef);
        void OnTakeDamage(std::shared_ptr<EntityBase> attacker, std::uint32_t damage);
        void OnFrozen();
        void OnSetOnFire();
        [[nodiscard]] bool Process() override;
        void ResetGroundedState();
        [[nodiscard]] static bool UpdateAim(
            ::OpenTK::Mathematics::Vector3 muzzlePos,
            ::OpenTK::Mathematics::Vector3 targetPos,
            const std::shared_ptr<MphRead::EquipInfo>& equipInfo,
            ::OpenTK::Mathematics::Vector3& aimVector);
        void Die();
        void Destroy() override;
        void GetDrawInfo() override;

    protected:
        [[nodiscard]] std::optional<std::int32_t> GetBindingOverride(
            ModelInstance& inst, Material& material, std::int32_t index) override;
        [[nodiscard]] ::OpenTK::Mathematics::Vector3 GetEmission(
            ModelInstance& inst, Material& material, std::int32_t index) override;
        [[nodiscard]] ::OpenTK::Mathematics::Matrix4 GetTexcoordMatrix(
            ModelInstance& inst, Material& material, std::int32_t materialId,
            Node& node, std::int32_t recolor = -1) override;

    private:
        [[nodiscard]] bool ProcessNativeGameplay();
        std::shared_ptr<PlayerEntity> _owner{};
        std::shared_ptr<EntityBase> _target{};
        std::shared_ptr<Formats::NodeData3> _closestNode{};

        std::int32_t _health = 0;
        std::uint16_t _timeSinceDamage = std::numeric_limits<std::uint16_t>::max();
        std::uint16_t _timeSinceFrozen = 0;
        std::uint16_t _freezeTimer = 0;
        std::uint16_t _burnTimer = 0;
        std::shared_ptr<Effects::EffectEntry> _burnEffect{};

        float _ySpeed = 0.0F;
        bool _grounded = false;
        ::OpenTK::Mathematics::Vector3 _aimVector{};
        std::uint16_t _targetTimer = 0;
        std::uint16_t _cooldownTimer = 0;
        std::int32_t _cooldownFactorRaw = Mods::Combat::HalfturretFireRate::Normal;

        std::shared_ptr<MphRead::EquipInfo> _equipInfo{};

        std::shared_ptr<Node> _baseNode{};
        std::shared_ptr<Node> _baseNodeParent{};
        std::shared_ptr<ModelInstance> _altIceModel{};
    };
}
