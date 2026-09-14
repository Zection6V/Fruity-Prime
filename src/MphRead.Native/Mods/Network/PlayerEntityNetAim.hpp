#pragma once

#include "../../Formats/Culling.hpp"
#include "../../Formats/Enums.hpp"
#include "../../Formats/Types.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace MphRead
{
    class Scene;
}

namespace MphRead::Entities
{
    // PlayerEntityNetAim.cs partial surface. PlayerEntity's canonical aggregate
    // declaration remains Entities/Players/PlayerEntity.hpp; this contributor
    // owns only the members and state declared by PlayerEntityNetAim.cs.
    class PlayerEntity
    {
    public:
        void ModRecordNetworkPosition(std::uint32_t frame);
        [[nodiscard]] bool ModGetNetworkPosition(
            std::uint32_t frame, OpenTK::Mathematics::Vector3& position);

        [[nodiscard]] OpenTK::Mathematics::Vector3 ModGunVector() const;
        void ModRefreshNetworkAim();
        void ModSetAim(OpenTK::Mathematics::Vector3 aim);

        [[nodiscard]] static Formats::Culling::NodeRef ModWalkNodeRef(
            MphRead::Scene& scene,
            Formats::Culling::NodeRef current,
            OpenTK::Mathematics::Vector3 previous,
            OpenTK::Mathematics::Vector3 position);

        void ModPlaceAt(OpenTK::Mathematics::Vector3 position);
        [[nodiscard]] bool ModPlacementBelongsHere(OpenTK::Mathematics::Vector3 position);
        void ModRefreshNodeRef(OpenTK::Mathematics::Vector3 previousPosition);
        [[nodiscard]] bool ModNodeUnresolved() const noexcept;

        void ModLogCollisionRange();
        void ModSetFacing(OpenTK::Mathematics::Vector3 facing);
        void ModSetSpectating(bool value);
        [[nodiscard]] bool ModInPlay() const;
        [[nodiscard]] bool ModIsInPlay() const;

        void ModNetSpawn(
            OpenTK::Mathematics::Vector3 position,
            OpenTK::Mathematics::Vector3 facing);
        [[nodiscard]] static Formats::Culling::NodeRef ModSpawnNodeRef(
            MphRead::Scene& scene, OpenTK::Mathematics::Vector3 position);

        [[nodiscard]] std::pair<float, float> ModAimDeltaTowards(
            OpenTK::Mathematics::Vector3 target);
        [[nodiscard]] OpenTK::Mathematics::Vector3 ModAimTarget() const;

        void ModSetHunter(MphRead::Hunter hunter);
        [[nodiscard]] bool ModDamageIndicatorActive() const;

        void ModStartFormSwitch();
        void ModForceForm(bool altForm);

        void ModSetWeapon(MphRead::BeamType weapon);
        [[nodiscard]] std::pair<std::int32_t, std::int32_t> ModAmmo() const;
        void ModSetAmmo(std::int32_t ua, std::int32_t missiles);
        void ModSetZoom(bool zoomed);

        void ModArmAffinityWeapon();
        void ModArmWeapon(MphRead::BeamType beam);
        void ModArmZoomWeapon();

        void ModApplyScriptAim(float deltaX, float deltaY);
        [[nodiscard]] std::string ModWeaponState() const;
        [[nodiscard]] std::int32_t ModChargeLevel() const;
        [[nodiscard]] bool ModChargeReady() const;

        [[nodiscard]] bool ModFrozen() const;
        void ModSetFrozen(bool frozen);
        void ModRefreshVolume();
        [[nodiscard]] bool ModBurning() const;
        [[nodiscard]] bool ModDisrupted() const;
        void ModSetDisrupted(bool disrupted);
        void ModSetBurning(bool burning);
        [[nodiscard]] bool ModCanZoom() const;

        [[nodiscard]] std::string ModFormState() const;
        void ModRepairVectors();

        void ModNetDie();
        [[nodiscard]] std::pair<std::int32_t, float> ModScoreboardSize() const;
        [[nodiscard]] bool ModCanBeHurt() const;

        void ModNoteInput();

    private:
        static constexpr std::int32_t NetworkHistoryLength = 120;
        static constexpr float _nodeWalkStepMax = 4.0F;

        std::array<OpenTK::Mathematics::Vector3, NetworkHistoryLength>
            _networkPositionHistory{};
        std::array<std::uint32_t, NetworkHistoryLength> _networkPositionFrames{};
        std::int32_t _networkPositionHistoryCount = 0;

        bool _modNodeUnresolved = false;

        OpenTK::Mathematics::Vector3 _modLastGoodFacing{0.0F, 0.0F, -1.0F};
        OpenTK::Mathematics::Vector3 _modLastGoodGunVec{0.0F, 0.0F, -1.0F};
        OpenTK::Mathematics::Vector3 _modLastGoodPosition{};

        [[nodiscard]] OpenTK::Mathematics::Vector3 ModAimVectorTowards(
            OpenTK::Mathematics::Vector3 target) const;
        [[nodiscard]] static bool Finite(OpenTK::Mathematics::Vector3 value) noexcept;

        void ApplyModAim();
        void ApplyGamepadAim();
    };
}
