#include "EntityEnemy.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <string>

namespace
{
    template <typename T>
    T& AssignReadonly(T& self, const T& other) noexcept
    {
        if (std::addressof(self) != std::addressof(other))
        {
            self.~T();
            ::new (static_cast<void*>(std::addressof(self))) T(other);
        }
        return self;
    }

    std::shared_ptr<std::string> EmptyString()
    {
        static const auto value = std::make_shared<std::string>();
        return value;
    }

    template <std::size_t N>
    std::shared_ptr<std::string> MarshalString(const char (&value)[N])
    {
        std::size_t length = 0;
        while (length < N && value[length] != '\0')
        {
            ++length;
        }
        return length == 0 ? EmptyString() : std::make_shared<std::string>(value, length);
    }

    void RequireReference(bool present)
    {
        if (!present)
        {
            throw System::NullReferenceException();
        }
    }
}

namespace MphRead
{
    EnemySpawnFields00& EnemySpawnFields00::operator=(const EnemySpawnFields00& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    EnemySpawnFieldsWW& EnemySpawnFieldsWW::operator=(const EnemySpawnFieldsWW& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    EnemySpawnFields01& EnemySpawnFields01::operator=(const EnemySpawnFields01& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    EnemySpawnFields02& EnemySpawnFields02::operator=(const EnemySpawnFields02& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    EnemySpawnFields03& EnemySpawnFields03::operator=(const EnemySpawnFields03& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    EnemySpawnFields04& EnemySpawnFields04::operator=(const EnemySpawnFields04& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    EnemySpawnFields05& EnemySpawnFields05::operator=(const EnemySpawnFields05& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    EnemySpawnFields06& EnemySpawnFields06::operator=(const EnemySpawnFields06& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    EnemySpawnFields07& EnemySpawnFields07::operator=(const EnemySpawnFields07& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    EnemySpawnFields08& EnemySpawnFields08::operator=(const EnemySpawnFields08& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    EnemySpawnFields09& EnemySpawnFields09::operator=(const EnemySpawnFields09& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    EnemySpawnFields10& EnemySpawnFields10::operator=(const EnemySpawnFields10& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    EnemySpawnFields11& EnemySpawnFields11::operator=(const EnemySpawnFields11& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    EnemySpawnFields12& EnemySpawnFields12::operator=(const EnemySpawnFields12& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    EnumSpawnUnion::EnumSpawnUnion() noexcept
        : S00{}
    {
    }

    EnumSpawnUnion& EnumSpawnUnion::operator=(const EnumSpawnUnion& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    EnemySpawnEntityData& EnemySpawnEntityData::operator=(const EnemySpawnEntityData& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    FhEnemySpawnEntityData& FhEnemySpawnEntityData::operator=(const FhEnemySpawnEntityData& other) noexcept
    {
        return AssignReadonly(*this, other);
    }
}

namespace MphRead::Editor
{
    std::int32_t EnemySpawnEntityEditor::SpawnerType() const
    {
        return GetSpawnerType(EnemyType);
    }

    std::int32_t EnemySpawnEntityEditor::GetSpawnerType(MphRead::EnemyType type)
    {
        if (type == MphRead::EnemyType::Zoomer || type == MphRead::EnemyType::Geemer
            || type == MphRead::EnemyType::Blastcap || type == MphRead::EnemyType::Voldrum2
            || type == MphRead::EnemyType::Quadtroid || type == MphRead::EnemyType::CrashPillar
            || type == MphRead::EnemyType::Slench || type == MphRead::EnemyType::LesserIthrak
            || type == MphRead::EnemyType::Trocra)
        {
            return 0;
        }
        if (type == MphRead::EnemyType::WarWasp)
        {
            return 1;
        }
        if (type == MphRead::EnemyType::Shriekbat)
        {
            return 2;
        }
        if (type == MphRead::EnemyType::Temroid || type == MphRead::EnemyType::Petrasyl1)
        {
            return 3;
        }
        if (type == MphRead::EnemyType::Petrasyl2 || type == MphRead::EnemyType::Petrasyl3
            || type == MphRead::EnemyType::Petrasyl4)
        {
            return 4;
        }
        if (type == MphRead::EnemyType::Cretaphid || type == MphRead::EnemyType::GreaterIthrak)
        {
            return 5;
        }
        if (type == MphRead::EnemyType::AlimbicTurret || type == MphRead::EnemyType::PsychoBit1
            || type == MphRead::EnemyType::PsychoBit1 || type == MphRead::EnemyType::Voldrum1
            || type == MphRead::EnemyType::FireSpawn)
        {
            return 6;
        }
        if (type == MphRead::EnemyType::CarnivorousPlant)
        {
            return 7;
        }
        if (type == MphRead::EnemyType::BarbedWarWasp)
        {
            return 8;
        }
        if (type == MphRead::EnemyType::Hunter)
        {
            return 9;
        }
        if (type == MphRead::EnemyType::SlenchTurret)
        {
            return 10;
        }
        if (type == MphRead::EnemyType::Gorea1A)
        {
            return 11;
        }
        if (type == MphRead::EnemyType::Gorea2)
        {
            return 12;
        }
#ifndef NDEBUG
#error "EntityEnemy DEBUG parity requires the missing one-to-one Native owner for System.Diagnostics.Debug.Assert."
#endif
        return 0;
    }

    EnemySpawnEntityEditor::EnemySpawnEntityEditor()
        : EntityEditorBase(EntityType::EnemySpawn),
          SpawnNodeName(EmptyString())
    {
    }

    EnemySpawnEntityEditor::EnemySpawnEntityEditor(
        const std::shared_ptr<Entity>& header, EnemySpawnEntityData raw)
        : EntityEditorBase(header),
          SpawnNodeName(EmptyString())
    {
        EnemyType = raw.EnemyType;
        LinkedEntityId = raw.LinkedEntityId;
        SpawnTotal = raw.SpawnTotal;
        SpawnLimit = raw.SpawnLimit;
        SpawnCount = raw.SpawnCount;
        Active = raw.Active != 0;
        AlwaysActive = raw.AlwaysActive != 0;
        ItemChance = raw.ItemChance;
        SpawnerHealth = raw.SpawnerHealth;
        CooldownTime = raw.CooldownTime;
        InitialCooldown = raw.InitialCooldown;
        ActiveDistance = raw.ActiveDistance.FloatValue();
        EnemyActiveDistance = raw.ActiveDistance.FloatValue();
        SpawnNodeName = MarshalString(raw.NodeName);
        EntityId1 = raw.EntityId1;
        Message1 = raw.Message1;
        EntityId2 = raw.EntityId2;
        Message2 = raw.Message2;
        EntityId3 = raw.EntityId3;
        Message3 = raw.Message3;
        ItemType = raw.ItemType;

        const std::int32_t spawnerType = GetSpawnerType(EnemyType);
        if (spawnerType == 0)
        {
            Volume0 = CollisionVolume(raw.Fields.S00.Volume0);
            Volume1 = CollisionVolume(raw.Fields.S00.Volume1);
            Volume2 = CollisionVolume(raw.Fields.S00.Volume2);
            Volume3 = CollisionVolume(raw.Fields.S00.Volume3);
        }
        else if (spawnerType == 2)
        {
            Volume0 = CollisionVolume(raw.Fields.S02.Volume0);
            Volume1 = CollisionVolume(raw.Fields.S02.Volume1);
            Volume2 = CollisionVolume(raw.Fields.S02.Volume2);
            PathVector = raw.Fields.S02.PathVector.ToFloatVector();
        }
        else if (spawnerType == 3)
        {
            Volume0 = CollisionVolume(raw.Fields.S03.Volume0);
            EnemyPosition = raw.Fields.S03.Position.ToFloatVector();
            EnemyFacing = raw.Fields.S03.Facing.ToFloatVector();
            IdleRange = raw.Fields.S03.IdleRange.ToFloatVector();
            Unused68 = raw.Fields.S03.Unused68;
            Unused6C = raw.Fields.S03.Unused6C;
            Unused70 = raw.Fields.S03.Unused70;
            Unused74 = raw.Fields.S03.Unused74;
            Unused78 = raw.Fields.S03.Unused78;
            Unused7C = raw.Fields.S03.Unused7C;
            Unused80 = raw.Fields.S03.Unused80;
        }
        else if (spawnerType == 4)
        {
            Volume0 = CollisionVolume(raw.Fields.S04.Volume0);
            EnemyPosition = raw.Fields.S04.Position.ToFloatVector();
            WeaveOffset = raw.Fields.S04.WeaveOffset;
            Unknown01 = raw.Fields.S04.Field88;
            Unused68 = raw.Fields.S04.Unused68;
            Unused6C = raw.Fields.S04.Unused6C;
            Unused70 = raw.Fields.S04.Unused70;
            Unused74 = raw.Fields.S04.Unused74;
        }
        else if (spawnerType == 1 || spawnerType == 8)
        {
            const auto setWarWaspFields = [this](EnemySpawnFieldsWW fields)
            {
                Volume0 = CollisionVolume(fields.Volume0);
                Volume1 = CollisionVolume(fields.Volume1);
                Volume2 = CollisionVolume(fields.Volume2);
                for (std::int32_t i = 0; i < 16; ++i)
                {
                    MovementVectors->push_back(fields.MovementVectors[i].ToFloatVector());
                }
                PositionCount = fields.PositionCount;
                MovementType = fields.MovementType;
            };

            if (EnemyType == MphRead::EnemyType::WarWasp)
            {
                setWarWaspFields(raw.Fields.S01.WarWasp);
            }
            else
            {
                EnemySubtype = raw.Fields.S08.EnemySubtype;
                EnemyVersion = raw.Fields.S08.EnemyVersion;
                setWarWaspFields(raw.Fields.S08.WarWasp);
            }
        }
        else if (spawnerType == 5)
        {
            EnemySubtype = raw.Fields.S05.EnemySubtype;
            Volume0 = CollisionVolume(raw.Fields.S05.Volume0);
            Volume1 = CollisionVolume(raw.Fields.S05.Volume1);
            Volume2 = CollisionVolume(raw.Fields.S05.Volume2);
            Volume3 = CollisionVolume(raw.Fields.S05.Volume3);
        }
        else if (spawnerType == 6)
        {
            EnemySubtype = raw.Fields.S06.EnemySubtype;
            EnemyVersion = raw.Fields.S06.EnemyVersion;
            Volume0 = CollisionVolume(raw.Fields.S06.Volume0);
            Volume1 = CollisionVolume(raw.Fields.S06.Volume1);
            Volume2 = CollisionVolume(raw.Fields.S06.Volume2);
            Volume3 = CollisionVolume(raw.Fields.S06.Volume3);
        }
        else if (spawnerType == 7)
        {
            EnemySubtype = raw.Fields.S07.EnemySubtype;
            EnemyHealth = raw.Fields.S07.EnemyHealth;
            EnemyDamage = raw.Fields.S07.EnemyDamage;
            Volume0 = CollisionVolume(raw.Fields.S07.Volume0);
        }
        else if (spawnerType == 9)
        {
            Hunter = static_cast<MphRead::Hunter>(raw.Fields.S09.HunterId);
            EncounterType = raw.Fields.S09.EncounterType;
            HunterWeapon = raw.Fields.S09.HunterWeapon;
            HunterHealth = raw.Fields.S09.HunterHealth;
            HunterHealthMax = raw.Fields.S09.HunterHealthMax;
            HunterHealthThreshold = raw.Fields.S09.HunterHealthThreshold;
            HunterColor = raw.Fields.S09.HunterColor;
            HunterChance = raw.Fields.S09.HunterChance;
        }
        else if (spawnerType == 10)
        {
            EnemySubtype = raw.Fields.S10.EnemySubtype;
            EnemyVersion = raw.Fields.S10.EnemyVersion;
            Volume0 = CollisionVolume(raw.Fields.S10.Volume0);
            Volume1 = CollisionVolume(raw.Fields.S10.Volume1);
            Index = raw.Fields.S10.Index;
        }
        else if (spawnerType == 11)
        {
            Volume0 = CollisionVolume(
                raw.Fields.S11.Sphere1Position.ToFloatVector(), raw.Fields.S11.Sphere1Radius.FloatValue());
            Volume1 = CollisionVolume(
                raw.Fields.S11.Sphere2Position.ToFloatVector(), raw.Fields.S11.Sphere2Radius.FloatValue());
        }
        else if (spawnerType == 12)
        {
            Unknown05 = raw.Fields.S12.Field28.ToFloatVector();
            Unknown06 = raw.Fields.S12.Field34.FloatValue();
            Unknown07 = raw.Fields.S12.Field38.FloatValue();
        }
    }

    void EnemySpawnEntityEditor::CompareTo(const std::shared_ptr<EnemySpawnEntityEditor>& other) const
    {
        RequireReference(static_cast<bool>(other));
        PrintValue(EnemyType, other->EnemyType, "EnemyType");
        PrintValue(LinkedEntityId, other->LinkedEntityId, "LinkedEntityId");
        PrintValue(SpawnTotal, other->SpawnTotal, "SpawnTotal");
        PrintValue(SpawnLimit, other->SpawnLimit, "SpawnLimit");
        PrintValue(SpawnCount, other->SpawnCount, "SpawnCount");
        PrintValue(Active, other->Active, "Active");
        PrintValue(AlwaysActive, other->AlwaysActive, "AlwaysActive");
        PrintValue(ItemChance, other->ItemChance, "ItemChance");
        PrintValue(SpawnerHealth, other->SpawnerHealth, "SpawnerHealth");
        PrintValue(CooldownTime, other->CooldownTime, "CooldownTime");
        PrintValue(InitialCooldown, other->InitialCooldown, "InitialCooldown");
        PrintValue(ActiveDistance, other->ActiveDistance, "ActiveDistance");
        PrintValue(EnemyActiveDistance, other->EnemyActiveDistance, "EnemyActiveDistance");
        PrintValue(SpawnNodeName, other->SpawnNodeName, "SpawnNodeName");
        PrintValue(EntityId1, other->EntityId1, "EntityId1");
        PrintValue(Message1, other->Message1, "Message1");
        PrintValue(EntityId2, other->EntityId2, "EntityId2");
        PrintValue(Message2, other->Message2, "Message2");
        PrintValue(EntityId3, other->EntityId3, "EntityId3");
        PrintValue(Message3, other->Message3, "Message3");
        PrintValue(ItemType, other->ItemType, "ItemType");
        PrintValue(EnemySubtype, other->EnemySubtype, "EnemySubtype");
        PrintValue(EnemyVersion, other->EnemyVersion, "EnemyVersion");
        PrintValue(Volume0, other->Volume0, "Volume0");
        PrintValue(Volume1, other->Volume1, "Volume1");
        PrintValue(Volume2, other->Volume2, "Volume2");
        PrintValue(Volume3, other->Volume3, "Volume3");
        PrintValue(PathVector, other->PathVector, "PathVector");
        PrintValue(EnemyFacing, other->EnemyFacing, "EnemyFacing");
        PrintValue(EnemyPosition, other->EnemyPosition, "EnemyPosition");
        PrintValue(IdleRange, other->IdleRange, "IdleRange");
        PrintValue(Unused68, other->Unused68, "Unused68");
        PrintValue(Unused6C, other->Unused6C, "Unused6C");
        PrintValue(Unused70, other->Unused70, "Unused70");
        PrintValue(Unused74, other->Unused74, "Unused74");
        PrintValue(Unused78, other->Unused78, "Unused78");
        PrintValue(Unused7C, other->Unused7C, "Unused7C");
        PrintValue(Unused80, other->Unused80, "Unused80");
        PrintValue(EnemyHealth, other->EnemyHealth, "EnemyHealth");
        PrintValue(EnemyDamage, other->EnemyDamage, "EnemyDamage");
        PrintValues(MovementVectors, other->MovementVectors, "MovementVectors");
        PrintValue(PositionCount, other->PositionCount, "PositionCount");
        PrintValue(MovementType, other->MovementType, "MovementType");
        PrintValue(Hunter, other->Hunter, "Hunter");
        PrintValue(EncounterType, other->EncounterType, "EncounterType");
        PrintValue(HunterWeapon, other->HunterWeapon, "HunterWeapon");
        PrintValue(HunterHealth, other->HunterHealth, "HunterHealth");
        PrintValue(HunterHealthMax, other->HunterHealthMax, "HunterHealthMax");
        PrintValue(HunterHealthThreshold, other->HunterHealthThreshold, "HunterHealthThreshold");
        PrintValue(HunterColor, other->HunterColor, "HunterColor");
        PrintValue(HunterChance, other->HunterChance, "HunterChance");
        PrintValue(Index, other->Index, "Index");
        PrintValue(Unknown05, other->Unknown05, "Unknown05");
        PrintValue(Unknown06, other->Unknown06, "Unknown06");
        PrintValue(Unknown07, other->Unknown07, "Unknown07");
        PrintValue(SpawnerType(), other->SpawnerType(), "SpawnerType");
    }

    FhEnemySpawnEntityEditor::FhEnemySpawnEntityEditor()
        : EntityEditorBase(EntityType::FhEnemySpawn),
          SpawnNodeName(EmptyString())
    {
    }

    FhEnemySpawnEntityEditor::FhEnemySpawnEntityEditor(
        const std::shared_ptr<Entity>& header, FhEnemySpawnEntityData raw)
        : EntityEditorBase(header),
          SpawnNodeName(EmptyString())
    {
        Box = CollisionVolume(raw.Box);
        Cylinder = CollisionVolume(raw.Cylinder);
        Sphere = CollisionVolume(raw.Sphere);
        EnemyType = raw.EnemyType;
        SpawnTotal = raw.SpawnTotal;
        SpawnLimit = raw.SpawnLimit;
        SpawnCount = raw.SpawnCount;
        Cooldown = raw.Cooldown;
        StartFrame = raw.StartFrame;
        SpawnNodeName = MarshalString(raw.NodeName);
        ParentId = raw.ParentId;
        EmptyMessage = raw.EmptyMessage;
    }

    void FhEnemySpawnEntityEditor::CompareTo(const std::shared_ptr<FhEnemySpawnEntityEditor>& other) const
    {
        RequireReference(static_cast<bool>(other));
        PrintValue(Box, other->Box, "Box");
        PrintValue(Cylinder, other->Cylinder, "Cylinder");
        PrintValue(Sphere, other->Sphere, "Sphere");
        PrintValue(EnemyType, other->EnemyType, "EnemyType");
        PrintValue(SpawnTotal, other->SpawnTotal, "SpawnTotal");
        PrintValue(SpawnLimit, other->SpawnLimit, "SpawnLimit");
        PrintValue(SpawnCount, other->SpawnCount, "SpawnCount");
        PrintValue(Cooldown, other->Cooldown, "Cooldown");
        PrintValue(StartFrame, other->StartFrame, "StartFrame");
        PrintValue(SpawnNodeName, other->SpawnNodeName, "SpawnNodeName");
        PrintValue(ParentId, other->ParentId, "ParentId");
        PrintValue(EmptyMessage, other->EmptyMessage, "EmptyMessage");
    }
}
