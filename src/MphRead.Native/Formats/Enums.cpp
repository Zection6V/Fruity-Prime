#include "Enums.hpp"

#include "../NativeRuntime/System/Enum.hpp"

namespace MphRead
{
    namespace
    {
        // Enums.cs Hunter : byte
        constexpr ::MphRead::NativeRuntime::EnumNameEntry HunterNames[] = {
            {0x0ULL, "Samus"},
            {0x1ULL, "Kanden"},
            {0x2ULL, "Trace"},
            {0x3ULL, "Sylux"},
            {0x4ULL, "Noxus"},
            {0x5ULL, "Spire"},
            {0x6ULL, "Weavel"},
            {0x7ULL, "Guardian"},
            {0x8ULL, "Random"},
        };
    }

    std::string ToString(Hunter value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(
            value, HunterNames, std::size(HunterNames), false);
    }
}

namespace MphRead
{
    namespace
    {
        // Enums.cs EnemyType
        constexpr ::MphRead::NativeRuntime::EnumNameEntry EnemyTypeNames[] = {
            {0x0ULL, "WarWasp"},
            {0x1ULL, "Zoomer"},
            {0x2ULL, "Temroid"},
            {0x3ULL, "Petrasyl1"},
            {0x4ULL, "Petrasyl2"},
            {0x5ULL, "Petrasyl3"},
            {0x6ULL, "Petrasyl4"},
            {0x7ULL, "Unknown7"},
            {0x8ULL, "Unknown8"},
            {0x9ULL, "Unknown9"},
            {0xAULL, "BarbedWarWasp"},
            {0xBULL, "Shriekbat"},
            {0xCULL, "Geemer"},
            {0xDULL, "Unknown13"},
            {0xEULL, "Unknown14"},
            {0xFULL, "Unknown15"},
            {0x10ULL, "Blastcap"},
            {0x11ULL, "Unknown17"},
            {0x12ULL, "AlimbicTurret"},
            {0x13ULL, "Cretaphid"},
            {0x14ULL, "CretaphidEye"},
            {0x15ULL, "CretaphidCrystal"},
            {0x16ULL, "Unknown22"},
            {0x17ULL, "PsychoBit1"},
            {0x18ULL, "Gorea1A"},
            {0x19ULL, "GoreaHead"},
            {0x1AULL, "GoreaArm"},
            {0x1BULL, "GoreaLeg"},
            {0x1CULL, "Gorea1B"},
            {0x1DULL, "GoreaSealSphere1"},
            {0x1EULL, "Trocra"},
            {0x1FULL, "Gorea2"},
            {0x20ULL, "GoreaSealSphere2"},
            {0x21ULL, "GoreaMeteor"},
            {0x22ULL, "PsychoBit2"},
            {0x23ULL, "Voldrum2"},
            {0x24ULL, "Voldrum1"},
            {0x25ULL, "Quadtroid"},
            {0x26ULL, "CrashPillar"},
            {0x27ULL, "FireSpawn"},
            {0x28ULL, "Spawner"},
            {0x29ULL, "Slench"},
            {0x2AULL, "SlenchShield"},
            {0x2BULL, "SlenchNest"},
            {0x2CULL, "SlenchSynapse"},
            {0x2DULL, "SlenchTurret"},
            {0x2EULL, "LesserIthrak"},
            {0x2FULL, "GreaterIthrak"},
            {0x30ULL, "Hunter"},
            {0x31ULL, "ForceFieldLock"},
            {0x32ULL, "HitZone"},
            {0x33ULL, "CarnivorousPlant"},
        };

        // Enums.cs FhEnemyType
        constexpr ::MphRead::NativeRuntime::EnumNameEntry FhEnemyTypeNames[] = {
            {0x0ULL, "WarWasp"},
            {0x1ULL, "Zoomer"},
            {0x2ULL, "Metroid"},
            {0x3ULL, "Mochtroid1"},
            {0x4ULL, "Mochtroid2"},
            {0x5ULL, "Mochtroid3"},
            {0x6ULL, "Mochtroid4"},
        };
    }

    std::string ToString(EnemyType value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(
            value, EnemyTypeNames, std::size(EnemyTypeNames), false);
    }

    std::string ToString(FhEnemyType value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(
            value, FhEnemyTypeNames, std::size(FhEnemyTypeNames), false);
    }
}

namespace MphRead
{
    namespace
    {
        // Enums.cs ItemType : int. None is -1, which the name table holds as
        // the underlying type's unsigned bits.
        constexpr ::MphRead::NativeRuntime::EnumNameEntry ItemTypeNames[] = {
            {0xFFFFFFFFULL, "None"},
            {0x0ULL, "HealthMedium"},
            {0x1ULL, "HealthSmall"},
            {0x2ULL, "HealthBig"},
            {0x3ULL, "DoubleDamage"},
            {0x4ULL, "EnergyTank"},
            {0x5ULL, "VoltDriver"},
            {0x6ULL, "MissileExpansion"},
            {0x7ULL, "Battlehammer"},
            {0x8ULL, "Imperialist"},
            {0x9ULL, "Judicator"},
            {0xAULL, "Magmaul"},
            {0xBULL, "ShockCoil"},
            {0xCULL, "OmegaCannon"},
            {0xDULL, "UASmall"},
            {0xEULL, "UABig"},
            {0xFULL, "MissileSmall"},
            {0x10ULL, "MissileBig"},
            {0x11ULL, "Cloak"},
            {0x12ULL, "UAExpansion"},
            {0x13ULL, "ArtifactKey"},
            {0x14ULL, "Deathalt"},
            {0x15ULL, "AffinityWeapon"},
            {0x16ULL, "PickWpnMissile"},
        };
    }

    std::string ToString(ItemType value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(
            value, ItemTypeNames, std::size(ItemTypeNames), false);
    }
}
