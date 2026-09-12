#pragma once

#include "Entity.hpp"
#include "EntityClass.hpp"
#include "Enums.hpp"
#include "Formats.hpp"
#include "RawFormats.hpp"
#include "Types.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace MphRead
{
    struct EnemySpawnFields00
    {
        const RawCollisionVolume Volume0{};
        const RawCollisionVolume Volume1{};
        const RawCollisionVolume Volume2{};
        const RawCollisionVolume Volume3{};

        EnemySpawnFields00() noexcept = default;
        EnemySpawnFields00(const EnemySpawnFields00&) noexcept = default;
        EnemySpawnFields00& operator=(const EnemySpawnFields00& other) noexcept;
    };

    // C++ requires EnemySpawnFieldsWW to be complete before EnemySpawnFields01 can contain it by value.
    struct EnemySpawnFieldsWW
    {
        const RawCollisionVolume Volume0{};
        const RawCollisionVolume Volume1{};
        const RawCollisionVolume Volume2{};
        const Vector3FxArray16 MovementVectors{};
        const std::uint8_t PositionCount = 0;
        const std::uint8_t Padding1A9 = 0;
        const std::uint16_t Padding1AA = 0;
        const std::uint32_t MovementType = 0;

        EnemySpawnFieldsWW() noexcept = default;
        EnemySpawnFieldsWW(const EnemySpawnFieldsWW&) noexcept = default;
        EnemySpawnFieldsWW& operator=(const EnemySpawnFieldsWW& other) noexcept;
    };

    struct EnemySpawnFields01
    {
        const EnemySpawnFieldsWW WarWasp{};
        const std::uint32_t Padding1B0 = 0;
        const std::uint32_t Padding1B4 = 0;

        EnemySpawnFields01() noexcept = default;
        EnemySpawnFields01(const EnemySpawnFields01&) noexcept = default;
        EnemySpawnFields01& operator=(const EnemySpawnFields01& other) noexcept;
    };

    struct EnemySpawnFields02
    {
        const RawCollisionVolume Volume0{};
        const Vector3Fx PathVector{};
        const RawCollisionVolume Volume1{};
        const RawCollisionVolume Volume2{};

        EnemySpawnFields02() noexcept = default;
        EnemySpawnFields02(const EnemySpawnFields02&) noexcept = default;
        EnemySpawnFields02& operator=(const EnemySpawnFields02& other) noexcept;
    };

    struct EnemySpawnFields03
    {
        const RawCollisionVolume Volume0{};
        const std::uint32_t Unused68 = 0;
        const std::uint32_t Unused6C = 0;
        const std::uint32_t Unused70 = 0;
        const std::uint32_t Unused74 = 0;
        const std::uint32_t Unused78 = 0;
        const std::uint32_t Unused7C = 0;
        const std::uint32_t Unused80 = 0;
        const Vector3Fx Facing{};
        const Vector3Fx Position{};
        const Vector3Fx IdleRange{};

        EnemySpawnFields03() noexcept = default;
        EnemySpawnFields03(const EnemySpawnFields03&) noexcept = default;
        EnemySpawnFields03& operator=(const EnemySpawnFields03& other) noexcept;
    };

    struct EnemySpawnFields04
    {
        const RawCollisionVolume Volume0{};
        const std::uint32_t Unused68 = 0;
        const std::uint32_t Unused6C = 0;
        const std::uint32_t Unused70 = 0;
        const std::uint32_t Unused74 = 0;
        const Vector3Fx Position{};
        const std::int32_t WeaveOffset = 0;
        const std::int32_t Field88 = 0;

        EnemySpawnFields04() noexcept = default;
        EnemySpawnFields04(const EnemySpawnFields04&) noexcept = default;
        EnemySpawnFields04& operator=(const EnemySpawnFields04& other) noexcept;
    };

    struct EnemySpawnFields05
    {
        const std::uint32_t EnemySubtype = 0;
        const RawCollisionVolume Volume0{};
        const RawCollisionVolume Volume1{};
        const RawCollisionVolume Volume2{};
        const RawCollisionVolume Volume3{};

        EnemySpawnFields05() noexcept = default;
        EnemySpawnFields05(const EnemySpawnFields05&) noexcept = default;
        EnemySpawnFields05& operator=(const EnemySpawnFields05& other) noexcept;
    };

    struct EnemySpawnFields06
    {
        const std::uint32_t EnemySubtype = 0;
        const std::uint32_t EnemyVersion = 0;
        const RawCollisionVolume Volume0{};
        const RawCollisionVolume Volume1{};
        const RawCollisionVolume Volume2{};
        const RawCollisionVolume Volume3{};

        EnemySpawnFields06() noexcept = default;
        EnemySpawnFields06(const EnemySpawnFields06&) noexcept = default;
        EnemySpawnFields06& operator=(const EnemySpawnFields06& other) noexcept;
    };

    struct EnemySpawnFields07
    {
        const std::uint16_t EnemyHealth = 0;
        const std::uint16_t EnemyDamage = 0;
        const std::uint32_t EnemySubtype = 0;
        const RawCollisionVolume Volume0{};

        EnemySpawnFields07() noexcept = default;
        EnemySpawnFields07(const EnemySpawnFields07&) noexcept = default;
        EnemySpawnFields07& operator=(const EnemySpawnFields07& other) noexcept;
    };

    struct EnemySpawnFields08
    {
        const std::uint32_t EnemySubtype = 0;
        const std::uint32_t EnemyVersion = 0;
        const EnemySpawnFieldsWW WarWasp{};

        EnemySpawnFields08() noexcept = default;
        EnemySpawnFields08(const EnemySpawnFields08&) noexcept = default;
        EnemySpawnFields08& operator=(const EnemySpawnFields08& other) noexcept;
    };

    struct EnemySpawnFields09
    {
        const std::uint32_t HunterId = 0;
        const std::uint32_t EncounterType = 0;
        const std::uint32_t HunterWeapon = 0;
        const std::uint16_t HunterHealth = 0;
        const std::uint16_t HunterHealthMax = 0;
        const std::uint16_t HunterHealthThreshold = 0;
        const std::uint8_t HunterColor = 0;
        const std::uint8_t HunterChance = 0;

        EnemySpawnFields09() noexcept = default;
        EnemySpawnFields09(const EnemySpawnFields09&) noexcept = default;
        EnemySpawnFields09& operator=(const EnemySpawnFields09& other) noexcept;
    };

    struct EnemySpawnFields10
    {
        const std::uint32_t EnemySubtype = 0;
        const std::uint32_t EnemyVersion = 0;
        const RawCollisionVolume Volume0{};
        const RawCollisionVolume Volume1{};
        const std::int32_t Index = 0;

        EnemySpawnFields10() noexcept = default;
        EnemySpawnFields10(const EnemySpawnFields10&) noexcept = default;
        EnemySpawnFields10& operator=(const EnemySpawnFields10& other) noexcept;
    };

    struct EnemySpawnFields11
    {
        const Vector3Fx Sphere1Position{};
        const Fixed Sphere1Radius{};
        const Vector3Fx Sphere2Position{};
        const Fixed Sphere2Radius{};

        EnemySpawnFields11() noexcept = default;
        EnemySpawnFields11(const EnemySpawnFields11&) noexcept = default;
        EnemySpawnFields11& operator=(const EnemySpawnFields11& other) noexcept;
    };

    struct EnemySpawnFields12
    {
        const Vector3Fx Field28{};
        const Fixed Field34{};
        const Fixed Field38{};

        EnemySpawnFields12() noexcept = default;
        EnemySpawnFields12(const EnemySpawnFields12&) noexcept = default;
        EnemySpawnFields12& operator=(const EnemySpawnFields12& other) noexcept;
    };

    union EnumSpawnUnion
    {
        const EnemySpawnFields00 S00;
        const EnemySpawnFields01 S01;
        const EnemySpawnFields02 S02;
        const EnemySpawnFields03 S03;
        const EnemySpawnFields04 S04;
        const EnemySpawnFields05 S05;
        const EnemySpawnFields06 S06;
        const EnemySpawnFields07 S07;
        const EnemySpawnFields08 S08;
        const EnemySpawnFields09 S09;
        const EnemySpawnFields10 S10;
        const EnemySpawnFields11 S11;
        const EnemySpawnFields12 S12;

        EnumSpawnUnion() noexcept;
        EnumSpawnUnion(const EnumSpawnUnion&) noexcept = default;
        EnumSpawnUnion& operator=(const EnumSpawnUnion& other) noexcept;
        ~EnumSpawnUnion() noexcept = default;
    };

    struct EnemySpawnEntityData
    {
        const EntityDataHeader Header{};
        const MphRead::EnemyType EnemyType{};
        const std::uint8_t Padding25 = 0;
        const std::uint16_t Padding26 = 0;
        const EnumSpawnUnion Fields{};
        const std::int16_t LinkedEntityId = 0;
        const std::uint8_t SpawnTotal = 0;
        const std::uint8_t SpawnLimit = 0;
        const std::uint8_t SpawnCount = 0;
        const std::uint8_t Active = 0;
        const std::uint8_t AlwaysActive = 0;
        const std::uint8_t ItemChance = 0;
        const std::uint16_t SpawnerHealth = 0;
        const std::uint16_t CooldownTime = 0;
        const std::uint16_t InitialCooldown = 0;
        const std::uint16_t Padding1C6 = 0;
        const Fixed ActiveDistance{};
        const Fixed EnemyActiveDistance{};
        char NodeName[16]{};
        const std::int16_t EntityId1 = 0;
        const std::uint16_t Padding1E2 = 0;
        const Message Message1{};
        const std::int16_t EntityId2 = 0;
        const std::uint16_t Padding1EA = 0;
        const Message Message2{};
        const std::int16_t EntityId3 = 0;
        const std::uint16_t Padding1F2 = 0;
        const Message Message3{};
        const MphRead::ItemType ItemType{};

        EnemySpawnEntityData() noexcept = default;
        EnemySpawnEntityData(const EnemySpawnEntityData&) noexcept = default;
        EnemySpawnEntityData& operator=(const EnemySpawnEntityData& other) noexcept;
    };

    struct FhEnemySpawnEntityData
    {
        const EntityDataHeader Header{};
        const FhRawCollisionVolume Box{};
        const FhRawCollisionVolume Cylinder{};
        const FhRawCollisionVolume Sphere{};
        const MphRead::FhEnemyType EnemyType{};
        const std::uint8_t SpawnTotal = 0;
        const std::uint8_t SpawnLimit = 0;
        const std::uint8_t SpawnCount = 0;
        const std::uint8_t PaddingEB = 0;
        const std::uint16_t Cooldown = 0;
        const std::uint16_t StartFrame = 0;
        char NodeName[16]{};
        const std::int16_t ParentId = 0;
        const std::uint16_t Padding102 = 0;
        const FhMessage EmptyMessage{};

        FhEnemySpawnEntityData() noexcept = default;
        FhEnemySpawnEntityData(const FhEnemySpawnEntityData&) noexcept = default;
        FhEnemySpawnEntityData& operator=(const FhEnemySpawnEntityData& other) noexcept;
    };

    static_assert(sizeof(EnumSpawnUnion) == 400);
    static_assert(sizeof(EnemySpawnEntityData) == 512);
    static_assert(sizeof(FhEnemySpawnEntityData) == 268);
}

namespace MphRead::Editor
{
    class EnemySpawnEntityEditor : public EntityEditorBase
    {
    public:
        MphRead::EnemyType EnemyType{};
        std::int16_t LinkedEntityId = 0;
        std::uint8_t SpawnTotal = 0;
        std::uint8_t SpawnLimit = 0;
        std::uint8_t SpawnCount = 0;
        bool Active = false;
        bool AlwaysActive = false;
        std::uint8_t ItemChance = 0;
        std::uint16_t SpawnerHealth = 0;
        std::uint16_t CooldownTime = 0;
        std::uint16_t InitialCooldown = 0;
        float ActiveDistance = 0.0F;
        float EnemyActiveDistance = 0.0F;
        std::shared_ptr<std::string> SpawnNodeName{};
        std::int16_t EntityId1 = 0;
        Message Message1{};
        std::int16_t EntityId2 = 0;
        Message Message2{};
        std::int16_t EntityId3 = 0;
        Message Message3{};
        MphRead::ItemType ItemType{};

        std::uint32_t EnemySubtype = 0;
        std::uint32_t EnemyVersion = 0;
        CollisionVolume Volume0;
        CollisionVolume Volume1;
        CollisionVolume Volume2;
        CollisionVolume Volume3;

        OpenTK::Mathematics::Vector3 PathVector{};

        OpenTK::Mathematics::Vector3 EnemyFacing{};
        OpenTK::Mathematics::Vector3 EnemyPosition{};
        OpenTK::Mathematics::Vector3 IdleRange{};
        std::uint32_t Unused68 = 0;
        std::uint32_t Unused6C = 0;
        std::uint32_t Unused70 = 0;
        std::uint32_t Unused74 = 0;
        std::uint32_t Unused78 = 0;
        std::uint32_t Unused7C = 0;
        std::uint32_t Unused80 = 0;
        std::int32_t WeaveOffset = 0;
        std::int32_t Unknown01 = 0;

        std::uint16_t EnemyHealth = 0;
        std::uint16_t EnemyDamage = 0;

        std::shared_ptr<std::vector<OpenTK::Mathematics::Vector3>> MovementVectors
            = std::make_shared<std::vector<OpenTK::Mathematics::Vector3>>();
        std::uint8_t PositionCount = 0;
        std::uint32_t MovementType = 0;

        MphRead::Hunter Hunter{};
        std::uint32_t EncounterType = 0;
        std::uint32_t HunterWeapon = 0;
        std::uint16_t HunterHealth = 0;
        std::uint16_t HunterHealthMax = 0;
        std::uint16_t HunterHealthThreshold = 0;
        std::uint8_t HunterColor = 0;
        std::uint8_t HunterChance = 0;

        std::int32_t Index = 0;

        OpenTK::Mathematics::Vector3 Unknown05{};
        float Unknown06 = 0.0F;
        float Unknown07 = 0.0F;

        [[nodiscard]] std::int32_t SpawnerType() const;
        [[nodiscard]] static std::int32_t GetSpawnerType(MphRead::EnemyType type);

        EnemySpawnEntityEditor();
        EnemySpawnEntityEditor(const std::shared_ptr<Entity>& header, EnemySpawnEntityData raw);
        void CompareTo(const std::shared_ptr<EnemySpawnEntityEditor>& other) const;
    };

    class FhEnemySpawnEntityEditor : public EntityEditorBase
    {
    public:
        CollisionVolume Box;
        CollisionVolume Cylinder;
        CollisionVolume Sphere;
        MphRead::FhEnemyType EnemyType{};
        std::uint8_t SpawnTotal = 0;
        std::uint8_t SpawnLimit = 0;
        std::uint8_t SpawnCount = 0;
        std::uint16_t Cooldown = 0;
        std::uint16_t StartFrame = 0;
        std::shared_ptr<std::string> SpawnNodeName{};
        std::int16_t ParentId = 0;
        FhMessage EmptyMessage{};

        FhEnemySpawnEntityEditor();
        FhEnemySpawnEntityEditor(const std::shared_ptr<Entity>& header, FhEnemySpawnEntityData raw);
        void CompareTo(const std::shared_ptr<FhEnemySpawnEntityEditor>& other) const;
    };
}
