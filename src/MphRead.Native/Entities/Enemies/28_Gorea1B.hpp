#pragma once

#include "24_Gorea1A.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>

namespace MphRead
{
    class ModelInstance;
    class Node;

    namespace Effects
    {
        class EffectEntry;
    }
}

namespace MphRead::Entities::Enemies
{
    class Enemy29Entity;
    class Enemy30Entity;

    enum class Gorea1BFlags : std::uint8_t
    {
        None = 0x0,
        Bit0 = 0x1,
        Bit1 = 0x2,
        Bit2 = 0x4,
        Bit3 = 0x8,
        Bit4 = 0x10,
        Bit5 = 0x20,
        Bit6 = 0x40,
        Bit7 = 0x80
    };

    [[nodiscard]] constexpr Gorea1BFlags operator|(
        Gorea1BFlags left, Gorea1BFlags right) noexcept
    {
        return static_cast<Gorea1BFlags>(
            static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr Gorea1BFlags operator&(
        Gorea1BFlags left, Gorea1BFlags right) noexcept
    {
        return static_cast<Gorea1BFlags>(
            static_cast<std::uint8_t>(left) & static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr Gorea1BFlags operator^(
        Gorea1BFlags left, Gorea1BFlags right) noexcept
    {
        return static_cast<Gorea1BFlags>(
            static_cast<std::uint8_t>(left) ^ static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr Gorea1BFlags operator~(Gorea1BFlags value) noexcept
    {
        return static_cast<Gorea1BFlags>(
            static_cast<std::uint8_t>(~static_cast<std::uint8_t>(value)));
    }

    constexpr Gorea1BFlags& operator|=(
        Gorea1BFlags& left, Gorea1BFlags right) noexcept
    {
        left = left | right;
        return left;
    }

    constexpr Gorea1BFlags& operator&=(
        Gorea1BFlags& left, Gorea1BFlags right) noexcept
    {
        left = left & right;
        return left;
    }

    constexpr Gorea1BFlags& operator^=(
        Gorea1BFlags& left, Gorea1BFlags right) noexcept
    {
        left = left ^ right;
        return left;
    }

    class Enemy28Entity : public GoreaEnemyEntityBase
    {
    private:
        Enemy24Entity* _gorea1A = nullptr;
        std::shared_ptr<Enemy29Entity> _sealSphere{};

    public:
        const std::shared_ptr<Enemy29Entity>& SealSphere = _sealSphere;

    private:
        std::shared_ptr<Node> _spineNode{};
        std::array<std::shared_ptr<Enemy30Entity>, 30> _trocra{};
        CollisionVolume _volume{};
        Gorea1BFlags _goreaFlags = Gorea1BFlags::None;

        std::int32_t _phasesLeft = 0;

    public:
        const std::int32_t& PhasesLeft = _phasesLeft;

    private:
        OpenTK::Mathematics::Vector3 _targetFacing{};
        std::int32_t _damageTimer = 0;
        std::int32_t _swingTimer = 0;
        std::int32_t _holdTimer = 0;

        std::array<OpenTK::Mathematics::Vector3, 24> _grappleVecs{};
        OpenTK::Mathematics::Matrix4 _grappleMtx{
            OpenTK::Mathematics::Vector4(1.0F, 0.0F, 0.0F, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, 1.0F, 0.0F, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, 0.0F, 1.0F, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, 0.0F, 0.0F, 1.0F)
        };
        float _grappleInt = 0.0F;
        OpenTK::Mathematics::Vector3 _field10{};
        float _field24 = 0.0F;
        float _field28 = 0.0F;
        float _field30 = 0.0F;
        float _field34 = 0.0F;
        float _field38 = 0.0F;

    public:
        bool _grappling = false;
        float _field21C = 0.0F;
        float _field21E = 0.0F;

    private:
        float _field224 = 0.0F;
        std::int32_t _field234 = 0;

        ModelInstance* _trickModel = nullptr;
        ModelInstance* _grappleModel = nullptr;
        std::shared_ptr<Effects::EffectEntry> _grappleEffect{};

        const std::array<std::string_view, 6> _bodyMatNames1{
            "ChestMembrane", "Eye", "Head1", "Legs", "Torso", "Shoulder"
        };
        const std::array<std::string_view, 2> _bodyMatNames2{
            "ChestCore", "HeadFullLit"
        };

    public:
        Enemy28Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy28Entity(const Enemy28Entity&) = delete;
        Enemy28Entity& operator=(const Enemy28Entity&) = delete;
        Enemy28Entity(Enemy28Entity&&) = delete;
        Enemy28Entity& operator=(Enemy28Entity&&) = delete;

        void Activate();
        void DrawSelf();

        [[nodiscard]] bool BehaviorXX();
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
        [[nodiscard]] bool Behavior13();
        [[nodiscard]] bool Behavior14();
        [[nodiscard]] bool Behavior15();

        [[nodiscard]] static bool BehaviorXX(Enemy28Entity* enemy);
        [[nodiscard]] static bool Behavior00(Enemy28Entity* enemy);
        [[nodiscard]] static bool Behavior01(Enemy28Entity* enemy);
        [[nodiscard]] static bool Behavior02(Enemy28Entity* enemy);
        [[nodiscard]] static bool Behavior03(Enemy28Entity* enemy);
        [[nodiscard]] static bool Behavior04(Enemy28Entity* enemy);
        [[nodiscard]] static bool Behavior05(Enemy28Entity* enemy);
        [[nodiscard]] static bool Behavior06(Enemy28Entity* enemy);
        [[nodiscard]] static bool Behavior07(Enemy28Entity* enemy);
        [[nodiscard]] static bool Behavior08(Enemy28Entity* enemy);
        [[nodiscard]] static bool Behavior09(Enemy28Entity* enemy);
        [[nodiscard]] static bool Behavior10(Enemy28Entity* enemy);
        [[nodiscard]] static bool Behavior11(Enemy28Entity* enemy);
        [[nodiscard]] static bool Behavior12(Enemy28Entity* enemy);
        [[nodiscard]] static bool Behavior13(Enemy28Entity* enemy);
        [[nodiscard]] static bool Behavior14(Enemy28Entity* enemy);
        [[nodiscard]] static bool Behavior15(Enemy28Entity* enemy);

        void HandleMessage(MessageInfo info) override;

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;
        [[nodiscard]] bool EnemyGetDrawInfo() override;

    private:
        void ActivateTrocraSpawns();
        void CheckPlayerCollision();
        void UpdateTargetFacing();
        void Func2139F54();
        void Func2139FE8();
        void Func213C8C4(OpenTK::Mathematics::Vector3 last);
        void Func213BCB8();
        void UpdateGrappleDrawValues();
        void UpdateGrappleDrawMatrix();
        void UpdateGrappleDrawInt();
        void Func213B90C();
        [[nodiscard]] OpenTK::Mathematics::Vector3 Func213BF7C();
        [[nodiscard]] OpenTK::Mathematics::Vector3 Func213C458(float index);
        void Func213BC3C();
        void Func213B678();
        [[nodiscard]] float Func213B784();
        void Func213C624(float factor);
        void Func213B7E0();
        void Func213C4DC();
        [[nodiscard]] OpenTK::Mathematics::Vector3 Func204D57C(
            OpenTK::Mathematics::Vector3 vec,
            OpenTK::Mathematics::Vector3 axis);
        void TickGrappleDamage();
        [[nodiscard]] bool CheckMovementOutsideVolume();
        void StopGrappling();
        void Func213B2B4();
        void Func213B348();
        void Func213AF2C();
        [[nodiscard]] bool Func213B188(Enemy30Entity* trocra);
        void Deactivate();
        void DeactivateAllTrocraSpawns();
        void DestroyAllTrocras();
        void ResetMaterialColors();
        void UpdateMaterials();
        void TransformGrappleEffect();
        void DrawMindTricks();
        void DrawGrappleBeam();
        void Func213C238(float index, OpenTK::Mathematics::Vector3& pos);
        void DrawGrappleSegment(
            OpenTK::Mathematics::Vector3& pos, float index,
            OpenTK::Mathematics::Vector3 a5,
            OpenTK::Mathematics::Vector3 a6);

        void State00();
        void State01();
        void State02();
        void State03();
        void State04();
        void State05();
        void State06();
        void State07();
        void State08();
        void State09();
        void State10();
        void State11();
        void State12();
        void State13();
    };
}
