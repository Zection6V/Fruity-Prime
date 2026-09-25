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

    bool TryParse(std::string_view text, bool ignoreCase, Hunter& value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumTryParse(
            text, ignoreCase, HunterNames, std::size(HunterNames), value);
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

namespace MphRead
{
    namespace
    {
        // Formats\Enums.cs Message : uint
        constexpr ::MphRead::NativeRuntime::EnumNameEntry MessageNames[] = {
            {0x0ULL, "None"},
            {0x5ULL, "SetActive"},
            {0x6ULL, "Destroyed"},
            {0x7ULL, "Damage"},
            {0x9ULL, "Trigger"},
            {0xCULL, "UpdateMusic"},
            {0xFULL, "Gravity"},
            {0x10ULL, "Unlock"},
            {0x11ULL, "Lock"},
            {0x12ULL, "Activate"},
            {0x13ULL, "Complete"},
            {0x14ULL, "Impact"},
            {0x15ULL, "Death"},
            {0x16ULL, "Unused22"},
            {0x17ULL, "ShipHatch"},
            {0x18ULL, "Unused24"},
            {0x19ULL, "Unused25"},
            {0x1AULL, "ShowPrompt"},
            {0x1BULL, "ShowWarning"},
            {0x1CULL, "ShowOverlay"},
            {0x1DULL, "MoveItemSpawner"},
            {0x1EULL, "SetCamSeqAi"},
            {0x1FULL, "PlayerCollideWith"},
            {0x20ULL, "BeamCollideWith"},
            {0x21ULL, "UnlockConnectors"},
            {0x22ULL, "LockConnectors"},
            {0x23ULL, "PreventFormSwitch"},
            {0x24ULL, "Gorea2Trigger"},
            {0x2AULL, "SetTriggerState"},
            {0x2BULL, "ClearTriggerState"},
            {0x2CULL, "PlatformWakeup"},
            {0x2DULL, "PlatformSleep"},
            {0x2EULL, "DripMoatPlatform"},
            {0x30ULL, "ActivateTurret"},
            {0x31ULL, "DecreaseTurretLights"},
            {0x32ULL, "IncreaseTurretLights"},
            {0x33ULL, "DeactivateTurret"},
            {0x34ULL, "SetBeamReflection"},
            {0x35ULL, "SetPlatformIndex"},
            {0x36ULL, "PlaySfxScript"},
            {0x38ULL, "UnlockOubliette"},
            {0x39ULL, "Checkpoint"},
            {0x3AULL, "EscapeUpdate1"},
            {0x3BULL, "SetSeekPlayerY"},
            {0x3CULL, "LoadOubliette"},
            {0x3DULL, "EscapeUpdate2"},
        };

        // Formats\Enums.cs DoorType : uint
        constexpr ::MphRead::NativeRuntime::EnumNameEntry DoorTypeNames[] = {
            {0x0ULL, "Standard"},
            {0x1ULL, "MorphBall"},
            {0x2ULL, "Boss"},
            {0x3ULL, "Thin"},
        };

        // Formats\Enums.cs FhItemType : int
        constexpr ::MphRead::NativeRuntime::EnumNameEntry FhItemTypeNames[] = {
            {0xFFFFFFFFULL, "None"},
            {0x0ULL, "AmmoSmall"},
            {0x1ULL, "AmmoBig"},
            {0x2ULL, "HealthSmall"},
            {0x3ULL, "HealthBig"},
            {0x4ULL, "DoubleDamage"},
            {0x5ULL, "PowerBeam"},
            {0x6ULL, "ElectroLob"},
            {0x7ULL, "Missile"},
        };

        // Formats\Enums.cs FhMessage : uint
        constexpr ::MphRead::NativeRuntime::EnumNameEntry FhMessageNames[] = {
            {0x0ULL, "None"},
            {0x5ULL, "Activate"},
            {0x6ULL, "Destroyed"},
            {0x7ULL, "Damage"},
            {0x9ULL, "Trigger"},
            {0xFULL, "Gravity"},
            {0x10ULL, "Unlock"},
            {0x11ULL, "SetActive"},
            {0x12ULL, "Complete"},
            {0x13ULL, "Impact"},
            {0x14ULL, "Death"},
            {0x15ULL, "Unknown21"},
        };

        // Formats\Enums.cs FhTriggerType : uint
        constexpr ::MphRead::NativeRuntime::EnumNameEntry FhTriggerTypeNames[] = {
            {0x0ULL, "Sphere"},
            {0x1ULL, "Box"},
            {0x2ULL, "Cylinder"},
            {0x3ULL, "Threshold"},
        };

        // Formats\Enums.cs TriggerType : uint
        constexpr ::MphRead::NativeRuntime::EnumNameEntry TriggerTypeNames[] = {
            {0x0ULL, "Volume"},
            {0x1ULL, "Threshold"},
            {0x2ULL, "Relay"},
            {0x3ULL, "Automatic"},
            {0x4ULL, "StateBits"},
        };
    }

    std::string ToString(Message value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(
            value, MessageNames, std::size(MessageNames), false);
    }

    std::string ToString(DoorType value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(
            value, DoorTypeNames, std::size(DoorTypeNames), false);
    }

    std::string ToString(FhItemType value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(
            value, FhItemTypeNames, std::size(FhItemTypeNames), false);
    }

    std::string ToString(FhMessage value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(
            value, FhMessageNames, std::size(FhMessageNames), false);
    }

    std::string ToString(FhTriggerType value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(
            value, FhTriggerTypeNames, std::size(FhTriggerTypeNames), false);
    }

    std::string ToString(TriggerType value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(
            value, TriggerTypeNames, std::size(TriggerTypeNames), false);
    }

    namespace
    {
        // Enums.cs EntityType : ushort
        constexpr ::MphRead::NativeRuntime::EnumNameEntry EntityTypeNames[] = {
            {0ULL, "Platform"},
            {1ULL, "Object"},
            {2ULL, "PlayerSpawn"},
            {3ULL, "Door"},
            {4ULL, "ItemSpawn"},
            {5ULL, "ItemInstance"},
            {6ULL, "EnemySpawn"},
            {7ULL, "TriggerVolume"},
            {8ULL, "AreaVolume"},
            {9ULL, "JumpPad"},
            {10ULL, "PointModule"},
            {11ULL, "MorphCamera"},
            {12ULL, "OctolithFlag"},
            {13ULL, "FlagBase"},
            {14ULL, "Teleporter"},
            {15ULL, "NodeDefense"},
            {16ULL, "LightSource"},
            {17ULL, "Artifact"},
            {18ULL, "CameraSequence"},
            {19ULL, "ForceField"},
            {21ULL, "BeamEffect"},
            {22ULL, "Bomb"},
            {23ULL, "EnemyInstance"},
            {24ULL, "Halfturret"},
            {25ULL, "Player"},
            {26ULL, "BeamProjectile"},
            {27ULL, "ListHead"},
            {100ULL, "FhUnknown0"},
            {101ULL, "FhPlayerSpawn"},
            {102ULL, "FhUnknown2"},
            {103ULL, "FhDoor"},
            {104ULL, "FhItemSpawn"},
            {105ULL, "FhItemInstance"},
            {106ULL, "FhEnemySpawn"},
            {107ULL, "FhEffectInstance"},
            {108ULL, "FhBomb"},
            {109ULL, "FhTriggerVolume"},
            {110ULL, "FhAreaVolume"},
            {111ULL, "FhPlatform"},
            {112ULL, "FhJumpPad"},
            {113ULL, "FhPointModule"},
            {114ULL, "FhMorphCamera"},
            {115ULL, "FhEnemyInstance"},
            {116ULL, "FhPlayer"},
            {117ULL, "FhBeamProjectile"},
            {200ULL, "Room"},
            {201ULL, "Model"},
            {255ULL, "All"},
        };
    }

    std::string ToString(EntityType value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(
            value, EntityTypeNames, std::size(EntityTypeNames), false);
    }

    namespace
    {
        // Enums.cs BeamType : sbyte
        constexpr ::MphRead::NativeRuntime::EnumNameEntry BeamTypeNames[] = {
            {0xFFFFFFFFFFFFFFFFULL, "None"},
            {0ULL, "PowerBeam"},
            {1ULL, "VoltDriver"},
            {2ULL, "Missile"},
            {3ULL, "Battlehammer"},
            {4ULL, "Imperialist"},
            {5ULL, "Judicator"},
            {6ULL, "Magmaul"},
            {7ULL, "ShockCoil"},
            {8ULL, "OmegaCannon"},
            {9ULL, "Platform"},
            {10ULL, "Enemy"},
        };
    }

    std::string ToString(BeamType value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(
            value, BeamTypeNames, std::size(BeamTypeNames), false);
    }

    bool TryParse(std::string_view text, bool ignoreCase, BeamType& value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumTryParse(
            text, ignoreCase, BeamTypeNames, std::size(BeamTypeNames), value);
    }

    namespace
    {
        // Enums.cs Terrain : byte
        constexpr ::MphRead::NativeRuntime::EnumNameEntry TerrainNames[] = {
            {0ULL, "Metal"},
            {1ULL, "OrangeHolo"},
            {2ULL, "GreenHolo"},
            {3ULL, "BlueHolo"},
            {4ULL, "Ice"},
            {5ULL, "Snow"},
            {6ULL, "Sand"},
            {7ULL, "Rock"},
            {8ULL, "Lava"},
            {9ULL, "Acid"},
            {10ULL, "Gorea"},
            {11ULL, "Unknown11"},
            {12ULL, "All"},
        };
    }

    std::string ToString(Terrain value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(
            value, TerrainNames, std::size(TerrainNames), false);
    }

    bool TryParse(std::string_view text, bool ignoreCase, Terrain& value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumTryParse(
            text, ignoreCase, TerrainNames, std::size(TerrainNames), value);
    }

    bool TryParse(std::string_view text, bool ignoreCase, ItemType& value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumTryParse(
            text, ignoreCase, ItemTypeNames, std::size(ItemTypeNames), value);
    }
}
