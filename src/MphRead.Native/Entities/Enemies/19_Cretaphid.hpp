#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"

#include <array>
#include <cstdint>
#include <memory>

namespace MphRead
{
    class EquipInfo;
    class ModelInstance;
    class Node;
    enum class Movie : std::int32_t;

    namespace Formats::Collision
    {
        class EntityCollision;
    }

    namespace Sound
    {
        class SoundSource;
    }
}

namespace MphRead::Entities
{
    class EnemySpawnEntity;
}

namespace MphRead::Entities::Enemies
{
    class Enemy20Entity;
    class Enemy21Entity;

    struct Enemy19Values
    {
        std::uint16_t CrystalHealth = 0;
        std::uint16_t PhaseFlashTime = 0;
        std::uint16_t Phase0CrystalHealth = 0;
        std::uint16_t Phase1CrystalHealth = 0;
        std::uint16_t Phase2CrystalHealth = 0;
        std::uint16_t Phase0CrystalShotTime = 0;
        std::uint16_t Phase1CrystalShotTime = 0;
        std::uint16_t Phase2CrystalShotTime = 0;
        std::uint16_t Phase0CrystalShotDelay = 0;
        std::uint16_t Phase1CrystalShotDelay = 0;
        std::uint16_t Phase2CrystalShotDelay = 0;
        std::uint16_t Phase0CrystalUpTime = 0;
        std::uint16_t Phase1CrystalUpTime = 0;
        std::uint16_t Phase2CrystalUpTime = 0;
        std::shared_ptr<ManagedArray<std::uint16_t>> CrystalBeamDamage{};
        std::shared_ptr<ManagedArray<std::uint16_t>> EyeBeamDamage{};
        std::shared_ptr<ManagedArray<std::uint16_t>> EyeSplashDamage{};
        std::shared_ptr<ManagedArray<std::uint16_t>> EyeContactDamage{};
        std::int32_t Unused34 = 0;
        std::int32_t Unused38 = 0;
        std::int32_t Unused3C = 0;
        std::int32_t Seg0AngleStep = 0;
        std::int32_t Seg1AngleStep = 0;
        std::int32_t Seg2AngleStep = 0;
        std::int32_t Seg0BeamStartAngle = 0;
        std::int32_t Seg1BeamStartAngle = 0;
        std::int32_t Seg2BeamStartAngle = 0;
        std::int32_t Seg0BeamAngleMin = 0;
        std::int32_t Seg1BeamAngleMin = 0;
        std::int32_t Seg2BeamAngleMin = 0;
        std::int32_t Seg0BeamAngleMax = 0;
        std::int32_t Seg1BeamAngleMax = 0;
        std::int32_t Seg2BeamAngleMax = 0;
        std::int32_t Seg0BeamAngleStep = 0;
        std::int32_t Seg1BeamAngleStep = 0;
        std::int32_t Seg2BeamAngleStep = 0;
        std::uint16_t EyeHealth = 0;
        std::uint8_t ItemChanceHealth = 0;
        std::uint8_t ItemChanceMissile = 0;
        std::uint8_t ItemChanceUa = 0;
        std::uint8_t ItemChanceNone = 0;
        std::shared_ptr<ManagedArray<std::uint8_t>> Phase0EyeState{};
        std::shared_ptr<ManagedArray<std::uint8_t>> Phase0BeamType{};
        std::shared_ptr<ManagedArray<std::uint8_t>> Phase0BeamSpawnMin{};
        std::shared_ptr<ManagedArray<std::uint8_t>> Phase0BeamSpawnMax{};
        std::shared_ptr<ManagedArray<std::uint16_t>> Phase0BeamCooldown{};
        std::shared_ptr<ManagedArray<std::uint16_t>> Phase0EyeStateTimer0{};
        std::shared_ptr<ManagedArray<std::uint16_t>> Phase0EyeStateTimer1{};
        std::shared_ptr<ManagedArray<std::uint16_t>> Phase0EyeStateTimer2{};
        std::shared_ptr<ManagedArray<std::uint16_t>> Phase0EyeStateTimer3{};
        std::shared_ptr<ManagedArray<std::uint8_t>> Phase1EyeState{};
        std::shared_ptr<ManagedArray<std::uint8_t>> Phase1BeamType{};
        std::shared_ptr<ManagedArray<std::uint8_t>> Phase1BeamSpawnMin{};
        std::shared_ptr<ManagedArray<std::uint8_t>> Phase1BeamSpawnMax{};
        std::shared_ptr<ManagedArray<std::uint16_t>> Phase1BeamCooldown{};
        std::shared_ptr<ManagedArray<std::uint16_t>> Phase1EyeStateTimer0{};
        std::shared_ptr<ManagedArray<std::uint16_t>> Phase1EyeStateTimer1{};
        std::shared_ptr<ManagedArray<std::uint16_t>> Phase1EyeStateTimer2{};
        std::shared_ptr<ManagedArray<std::uint16_t>> Phase1EyeStateTimer3{};
        std::shared_ptr<ManagedArray<std::uint8_t>> Phase2EyeState{};
        std::shared_ptr<ManagedArray<std::uint8_t>> Phase2BeamType{};
        std::shared_ptr<ManagedArray<std::uint8_t>> Phase2BeamSpawnMin{};
        std::shared_ptr<ManagedArray<std::uint8_t>> Phase2BeamSpawnMax{};
        std::shared_ptr<ManagedArray<std::uint16_t>> Phase2BeamCooldown{};
        std::shared_ptr<ManagedArray<std::uint16_t>> Phase2EyeStateTimer0{};
        std::shared_ptr<ManagedArray<std::uint16_t>> Phase2EyeStateTimer1{};
        std::shared_ptr<ManagedArray<std::uint16_t>> Phase2EyeStateTimer2{};
        std::shared_ptr<ManagedArray<std::uint16_t>> Phase2EyeStateTimer3{};
        std::uint8_t ItemChanceA = 0;
        std::uint8_t ItemChanceB = 0;
        std::uint8_t ItemChanceC = 0;
        std::uint8_t ItemChanceD = 0;
        std::uint16_t Padding27E = 0;
        std::int32_t CollisionRadius = 0;
        std::uint16_t ScanId = 0;
        std::uint16_t CrystalScanId = 0;
        std::uint32_t CrystalEffectiveness = 0;
        std::int32_t EyeScanId = 0;
        std::uint32_t EyeEffectiveness = 0;
    };

    class Enemy19Entity : public EnemyInstanceEntity
    {
    public:
        struct SegmentInfo
        {
            float Angle = 0.0F;
            float AngleStep = 0.0F;
            float BeamAngle = 0.0F;
            float BeamAngleMax = 0.0F;
            float BeamAngleMin = 0.0F;
            float BeamAngleStep = 0.0F;
            std::shared_ptr<Node> JointNode{};
            std::uint16_t Unused1C = 0;
            std::int8_t SpinDirection = 0;
            bool InvertBeamRotation = false;
        };

        Enemy19Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy19Entity(const Enemy19Entity&) = delete;
        Enemy19Entity& operator=(const Enemy19Entity&) = delete;
        Enemy19Entity(Enemy19Entity&&) = delete;
        Enemy19Entity& operator=(Enemy19Entity&&) = delete;

        [[nodiscard]] Enemy19Values Values() const;
        [[nodiscard]] std::int32_t PhaseIndex() const noexcept;
        [[nodiscard]] ModelInstance* BeamModel() const noexcept;
        [[nodiscard]] ModelInstance* BeamColModel() const noexcept;
        [[nodiscard]] Sound::SoundSource& SoundSource() noexcept;

        void Sub2135F54();
        void Sub213619C(Enemy20Entity* eye);
        void UpdateTransforms(bool rootPosition);
        void ResetTransforms();

        [[nodiscard]] static bool Behavior00(Enemy19Entity* enemy);
        [[nodiscard]] static bool Behavior01(Enemy19Entity* enemy);
        [[nodiscard]] static bool Behavior02(Enemy19Entity* enemy);
        [[nodiscard]] static bool Behavior03(Enemy19Entity* enemy);
        [[nodiscard]] static bool Behavior04(Enemy19Entity* enemy);
        [[nodiscard]] static bool Behavior05(Enemy19Entity* enemy);
        [[nodiscard]] static bool Behavior06(Enemy19Entity* enemy);
        [[nodiscard]] static bool Behavior07(Enemy19Entity* enemy);
        [[nodiscard]] static bool Behavior08(Enemy19Entity* enemy);
        [[nodiscard]] static bool Behavior09(Enemy19Entity* enemy);
        [[nodiscard]] static bool Behavior10(Enemy19Entity* enemy);
        [[nodiscard]] static bool Behavior11(Enemy19Entity* enemy);
        [[nodiscard]] static bool Behavior12(Enemy19Entity* enemy);

        void HandleMessage(MessageInfo info) override;

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyGetDrawInfo() override;

    private:
        enum class PhaseValue : std::int32_t
        {
            CrystalShotDelay = 0,
            CrystalShotTime = 1,
            CrystalUpTime = 2,
            CrystalHealth = 3
        };

        static constexpr std::uint16_t _flashPeriod = 10 * 2;
        static constexpr std::uint16_t _flashLength = 5 * 2;
        static constexpr std::uint16_t _eyeCount = 12;
        static const std::array<const char*, _eyeCount> _eyeNodes;
        static const std::array<Movie, 4> _deathMovieIds;

        EnemySpawnEntity* _spawner = nullptr;
        std::int32_t _subtype = 0;
        std::int32_t _crystalDownTimer = 0;
        std::uint16_t _flashTimer = 0;
        std::array<std::array<std::int32_t, 4>, 3> _phaseValues{};
        std::int32_t _eyeStartIndex = 0;
        std::int32_t _eyeEndIndex = 0;
        std::int32_t _eyeBurnIndex = 0;
        float _eyeBurnUpdateTimer = 0.0F;
        std::int32_t _phaseIndex = 0;
        std::int32_t _crystalShotDelay = 0;
        std::int32_t _crystalShotTimer = 0;
        std::int32_t _crystalUpTimer = 0;

    public:
        std::array<std::shared_ptr<EquipInfo>, 2> EquipInfo{};

    private:
        std::int32_t _ammo0 = 1000;
        std::int32_t _ammo1 = 1000;
        std::shared_ptr<Formats::Collision::EntityCollision> _parentEntCol{};
        OpenTK::Mathematics::Matrix4 _invTransform{
            OpenTK::Mathematics::Vector4(1.0F, 0.0F, 0.0F, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, 1.0F, 0.0F, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, 0.0F, 1.0F, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, 0.0F, 0.0F, 1.0F)};

    public:
        std::array<std::shared_ptr<SegmentInfo>, 3> Segments{};

    private:
        std::array<std::shared_ptr<Enemy20Entity>, _eyeCount> _eyes{};
        std::shared_ptr<Enemy21Entity> _crystal{};
        ModelInstance* _model = nullptr;
        ModelInstance* _beamModel = nullptr;
        ModelInstance* _beamColModel = nullptr;

        [[nodiscard]] std::int32_t GetPhaseValue(PhaseValue value) const;
        void SpawnEyes();
        void RespawnEyes();
        void SpawnCrystal();
        void SetPhase0() noexcept;
        void SetPhase1() noexcept;
        void SetPhase2() noexcept;
        void State0();
        void KillEyes();

        [[nodiscard]] bool Behavior00();
        [[nodiscard]] bool Behavior01();
        [[nodiscard]] bool Behavior02();
        [[nodiscard]] bool Behavior03();
        [[nodiscard]] bool Behavior04();
        [[nodiscard]] bool Behavior05();
        [[nodiscard]] bool Behavior06();
        [[nodiscard]] bool Behavior07();
        [[nodiscard]] bool Behavior08();
        [[nodiscard]] bool Behavior09();
        [[nodiscard]] bool Behavior10();
        [[nodiscard]] bool Behavior11();
        [[nodiscard]] bool Behavior12();
    };
}
