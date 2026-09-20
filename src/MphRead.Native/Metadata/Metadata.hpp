#pragma once

#include "../Formats/Enums.hpp"
#include "../Formats/Formats.hpp"
#include "../Formats/Types.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_map>
#include <vector>

namespace MphRead
{
    enum class MdlSuffix : std::int32_t
    {
        None,
        All,
        Model
    };

    enum class MetaDir : std::int32_t
    {
        Models,
        Hud,
        Stage,
        MainMenu,
        Logo,
        CharSelect,
        CreateJoin,
        GameOption,
        GamersCard,
        Keyboard,
        Keypad,
        MoviePlayer,
        MultiMaster,
        Multiplayer,
        PaxControls,
        Popup,
        Results,
        ScStartGame,
        StartGame,
        ToStart,
        TouchToStart,
        TouchToStart2,
        WifiCreate,
        WifiGames
    };

    class RecolorMetadata
    {
    public:
        const std::string Name;
        const std::string ModelPath;
        const std::string TexturePath;
        const std::string PalettePath;
        const std::optional<std::string> ReplacePath;
        const std::map<int, std::vector<int>> ReplaceIds;

        RecolorMetadata(std::string name, std::string modelPath);
        RecolorMetadata(std::string name, std::string modelPath, std::string texturePath);
        RecolorMetadata(std::string name, std::string modelPath, std::string texturePath,
            std::string palettePath, std::map<int, std::vector<int>> replaceIds = {},
            bool separateReplace = false);
    };

    class ModelMetadata
    {
        struct Values
        {
            std::string Name;
            std::string ModelPath;
            std::optional<std::string> AnimationPath;
            std::optional<std::string> AnimationShare;
            std::optional<std::string> CollisionPath;
            std::optional<std::string> ExtraCollisionPath;
            std::vector<RecolorMetadata> Recolors;
            bool UseLightSources = false;
            bool FirstHunt = false;
        };

        explicit ModelMetadata(Values values);

    public:
        const std::string Name;
        const std::string ModelPath;
        const std::optional<std::string> AnimationPath;
        const std::optional<std::string> AnimationShare;
        const std::optional<std::string> CollisionPath;
        const std::optional<std::string> ExtraCollisionPath;
        const std::vector<RecolorMetadata> Recolors;
        const bool UseLightSources;
        const bool FirstHunt;

        ModelMetadata(std::string name, std::string modelPath,
            std::optional<std::string> animationPath, std::optional<std::string> collisionPath,
            std::vector<RecolorMetadata> recolors,
            std::optional<std::string> animationShare = std::nullopt,
            bool useLightSources = false);
        ModelMetadata(std::string name, MetaDir dir,
            std::optional<std::string> anim = std::nullopt);
        ModelMetadata(std::string name, std::string texturePath, MetaDir dir);
        ModelMetadata(std::string name, std::optional<std::string> animationPath,
            std::optional<std::string> texturePath = std::nullopt);
        ModelMetadata(std::string name, std::string remove, bool animation = true,
            std::optional<std::string> animationPath = std::nullopt,
            bool collision = false, bool firstHunt = false);
        ModelMetadata(std::string name, std::vector<std::string> recolors,
            std::optional<std::string> remove = std::nullopt, bool animation = false,
            std::optional<std::string> animationPath = std::nullopt, bool texture = false,
            MdlSuffix mdlSuffix = MdlSuffix::None,
            std::optional<std::string> archive = std::nullopt,
            std::optional<std::string> recolorName = std::nullopt,
            std::optional<std::string> animationShare = std::nullopt,
            bool useLightSources = false, bool firstHunt = false,
            bool noUnderscore = false);
        ModelMetadata(std::string name, bool animation = true, bool collision = false,
            bool texture = false, std::optional<std::string> share = std::nullopt,
            MdlSuffix mdlSuffix = MdlSuffix::None,
            std::optional<std::string> archive = std::nullopt,
            std::optional<std::string> addToAnim = std::nullopt,
            bool firstHunt = false,
            std::optional<std::string> animationPath = std::nullopt,
            std::optional<std::string> extraCollision = std::nullopt);
        ModelMetadata(std::string name, std::string modelPath,
            std::optional<std::string> animationPath,
            std::optional<std::string> collisionPath, bool firstHunt = false);
    };

    class ObjectMetadata
    {
    public:
        const bool Lighting;
        const bool IgnoreAnimation;
        const std::string Name;
        const std::vector<int> AnimationIds;
        const int RecolorId;

        explicit ObjectMetadata(std::string name, bool lighting = false,
            int paletteId = 0, bool ignoreAnim = false,
            std::optional<std::vector<int>> animationIds = std::nullopt);
    };

    enum class PlatAnimId : std::int32_t
    {
        InstantSleep = 0,
        Wake = 1,
        InstantWake = 2,
        Sleep = 3
    };

    class PlatformMetadata
    {
    public:
        const bool Animation;
        const bool Lighting;
        const std::string Name;
        const std::vector<int> AnimationIds;

        explicit PlatformMetadata(std::string name, bool lighting = false,
            std::optional<std::vector<int>> animationIds = std::nullopt);
    };

    class DoorMetadata
    {
    public:
        const std::string Name;
        const std::string LockName;
        const float LockOffset;
        const float Radius;

        DoorMetadata(std::string name, std::string lockName, float lockOffset, float radius);
    };
}

#include <functional>
#include <limits>

namespace MphRead
{
    class BeamProjectileEntity;
    class WeaponInfo;

    enum class EquipFlags : std::uint8_t
    {
        None = 0x0,
        Zoomed = 0x1
    };

    [[nodiscard]] constexpr EquipFlags operator|(EquipFlags lhs, EquipFlags rhs) noexcept
    {
        return static_cast<EquipFlags>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
    }
    [[nodiscard]] constexpr EquipFlags operator&(EquipFlags lhs, EquipFlags rhs) noexcept
    {
        return static_cast<EquipFlags>(static_cast<std::uint8_t>(lhs) & static_cast<std::uint8_t>(rhs));
    }
    [[nodiscard]] constexpr EquipFlags operator^(EquipFlags lhs, EquipFlags rhs) noexcept
    {
        return static_cast<EquipFlags>(static_cast<std::uint8_t>(lhs) ^ static_cast<std::uint8_t>(rhs));
    }
    [[nodiscard]] constexpr EquipFlags operator~(EquipFlags value) noexcept
    {
        return static_cast<EquipFlags>(~static_cast<std::uint8_t>(value));
    }
    constexpr EquipFlags& operator|=(EquipFlags& lhs, EquipFlags rhs) noexcept { return lhs = lhs | rhs; }
    constexpr EquipFlags& operator&=(EquipFlags& lhs, EquipFlags rhs) noexcept { return lhs = lhs & rhs; }
    constexpr EquipFlags& operator^=(EquipFlags& lhs, EquipFlags rhs) noexcept { return lhs = lhs ^ rhs; }

    enum class WeaponFlags : std::uint32_t
    {
        None = 0x0,
        PartialCharge = 0x100,
        CanCharge = 0x200,
        RepeatFire = 0x400,
        CanZoom = 0x800,
        RicochetUncharged = 0x1000,
        RicochetCharged = 0x2000,
        AutoRelease = 0x4000,
        SelfDamageUncharged = 0x8000,
        SelfDamageCharged = 0x10000,
        ForceEffectUncharged = 0x20000,
        ForceEffectCharged = 0x40000,
        AoeUncharged = 0x80000,
        AoeCharged = 0x100000,
        Continuous = 0x200000,
        DestroyableUncharged = 0x400000,
        DestroyableCharged = 0x800000,
        RadIdx1Uncharged = 0x1000000,
        RadIdx2Uncharged = 0x2000000,
        RadIdx1Charged = 0x4000000,
        RadIdx2Charged = 0x8000000,
        LifeDrainUncharged = 0x10000000,
        LifeDrainCharged = 0x20000000,
        SurfaceCollision = 0x40000000
    };

    [[nodiscard]] constexpr WeaponFlags operator|(WeaponFlags lhs, WeaponFlags rhs) noexcept
    {
        return static_cast<WeaponFlags>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
    }
    [[nodiscard]] constexpr WeaponFlags operator&(WeaponFlags lhs, WeaponFlags rhs) noexcept
    {
        return static_cast<WeaponFlags>(static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
    }
    [[nodiscard]] constexpr WeaponFlags operator^(WeaponFlags lhs, WeaponFlags rhs) noexcept
    {
        return static_cast<WeaponFlags>(static_cast<std::uint32_t>(lhs) ^ static_cast<std::uint32_t>(rhs));
    }
    [[nodiscard]] constexpr WeaponFlags operator~(WeaponFlags value) noexcept
    {
        return static_cast<WeaponFlags>(~static_cast<std::uint32_t>(value));
    }
    constexpr WeaponFlags& operator|=(WeaponFlags& lhs, WeaponFlags rhs) noexcept { return lhs = lhs | rhs; }
    constexpr WeaponFlags& operator&=(WeaponFlags& lhs, WeaponFlags rhs) noexcept { return lhs = lhs & rhs; }
    constexpr WeaponFlags& operator^=(WeaponFlags& lhs, WeaponFlags rhs) noexcept { return lhs = lhs ^ rhs; }

    using BeamProjectileArray = std::vector<std::shared_ptr<BeamProjectileEntity>>;

    class EquipInfo
    {
        std::uint16_t _unchargedDamage = std::numeric_limits<std::uint16_t>::max();
        std::uint16_t _minChargeDamage = std::numeric_limits<std::uint16_t>::max();
        std::uint16_t _chargedDamage = std::numeric_limits<std::uint16_t>::max();
        std::uint16_t _headshotDamage = std::numeric_limits<std::uint16_t>::max();
        std::uint16_t _minChargeHeadshotDamage = std::numeric_limits<std::uint16_t>::max();
        std::uint16_t _chargedHeadshotDamage = std::numeric_limits<std::uint16_t>::max();
        std::uint16_t _splashDamage = std::numeric_limits<std::uint16_t>::max();
        std::uint16_t _minChargeSplashDamage = std::numeric_limits<std::uint16_t>::max();
        std::uint16_t _chargedSplashDamage = std::numeric_limits<std::uint16_t>::max();
        std::int32_t _homingTolerance = std::numeric_limits<std::int32_t>::max();

        [[nodiscard]] const WeaponInfo& RequireWeapon() const;

    public:
        bool Zoomed = false;
        std::shared_ptr<WeaponInfo> Weapon;
        std::shared_ptr<BeamProjectileArray> Beams;
        std::uint16_t ChargeLevel = 0;
        std::uint16_t SmokeLevel = 0;
        std::function<int()> GetAmmo;
        std::function<void(int)> SetAmmo;
        bool InfiniteAmmo = false;
        std::array<std::uint8_t, 2> DrawFuncIds{{255, 255}};
        std::array<std::uint8_t, 2> DmgDirTypes{{255, 255}};

        EquipInfo() = default;
        EquipInfo(std::shared_ptr<WeaponInfo> weapon, std::shared_ptr<BeamProjectileArray> beams);

        [[nodiscard]] int Ammo() const;
        void Ammo(int value);
        [[nodiscard]] std::uint16_t UnchargedDamage() const;
        void UnchargedDamage(std::uint16_t value) noexcept;
        [[nodiscard]] std::uint16_t MinChargeDamage() const;
        void MinChargeDamage(std::uint16_t value) noexcept;
        [[nodiscard]] std::uint16_t ChargedDamage() const;
        void ChargedDamage(std::uint16_t value) noexcept;
        [[nodiscard]] std::uint16_t HeadshotDamage() const;
        void HeadshotDamage(std::uint16_t value) noexcept;
        [[nodiscard]] std::uint16_t MinChargeHeadshotDamage() const;
        void MinChargeHeadshotDamage(std::uint16_t value) noexcept;
        [[nodiscard]] std::uint16_t ChargedHeadshotDamage() const;
        void ChargedHeadshotDamage(std::uint16_t value) noexcept;
        [[nodiscard]] std::uint16_t SplashDamage() const;
        void SplashDamage(std::uint16_t value) noexcept;
        [[nodiscard]] std::uint16_t MinChargeSplashDamage() const;
        void MinChargeSplashDamage(std::uint16_t value) noexcept;
        [[nodiscard]] std::uint16_t ChargedSplashDamage() const;
        void ChargedSplashDamage(std::uint16_t value) noexcept;
        [[nodiscard]] std::int32_t HomingTolerance() const;
        void HomingTolerance(std::int32_t value) noexcept;
    };

    class WeaponInfo
    {
        const std::int32_t _unchargedRicoWeaponIdx;
        const std::int32_t _chargedRicoWeaponIdx;

    public:
        const BeamType Beam;
        const BeamType BeamKind;
        const std::shared_ptr<std::vector<std::uint8_t>> DrawFuncIds;
        const std::shared_ptr<std::vector<std::uint16_t>> Colors;
        const std::uint8_t Priority;
        const WeaponFlags Flags;
        const std::uint16_t SplashDamage;
        const std::uint16_t MinChargeSplashDamage;
        const std::uint16_t ChargedSplashDamage;
        const std::shared_ptr<std::vector<std::uint8_t>> SplashDamageTypes;
        const std::uint8_t ShotCooldown;
        const std::uint8_t AutofireCooldown;
        const std::uint8_t AmmoType;
        const std::shared_ptr<std::vector<std::uint8_t>> CollisionEffects;
        const std::shared_ptr<std::vector<std::uint8_t>> MuzzleEffects;
        const std::shared_ptr<std::vector<std::uint8_t>> DmgDirTypes;
        const std::shared_ptr<std::vector<std::uint8_t>> DamageInterpolations;
        const std::shared_ptr<std::vector<Affliction>> Afflictions;
        const std::uint8_t Padding21;
        const std::uint16_t MinCharge;
        const std::uint16_t FullCharge;
        const std::uint16_t AmmoCost;
        const std::uint16_t MinChargeCost;
        const std::uint16_t ChargeCost;
        const std::uint16_t UnchargedDamage;
        const std::uint16_t MinChargeDamage;
        const std::uint16_t ChargedDamage;
        const std::uint16_t HeadshotDamage;
        const std::uint16_t MinChargeHeadshotDamage;
        const std::uint16_t ChargedHeadshotDamage;
        const std::uint16_t UnchargedLifespan;
        const std::uint16_t MinChargeLifespan;
        const std::uint16_t ChargedLifespan;
        const std::shared_ptr<std::vector<std::uint16_t>> SpeedDecayTimes;
        const std::uint16_t Padding42;
        const std::shared_ptr<std::vector<std::uint16_t>> SpeedInterpolations;
        const std::int32_t UnchargedDmgDirMag;
        const std::int32_t MinChargeDmgDirMag;
        const std::int32_t ChargedDmgDirMag;
        const std::int32_t ZoomFov;
        const std::int32_t UnchargedCylRadius;
        const std::int32_t MinChargeCylRadius;
        const std::int32_t ChargedCylRadius;
        const std::int32_t UnchargedSpeed;
        const std::int32_t MinChargeSpeed;
        const std::int32_t ChargedSpeed;
        const std::int32_t UnchargedFinalSpeed;
        const std::int32_t MinChargeFinalSpeed;
        const std::int32_t ChargedFinalSpeed;
        const std::int32_t UnchargedGravity;
        const std::int32_t MinChargeGravity;
        const std::int32_t ChargedGravity;
        const std::int32_t UnchargedHoming;
        const std::int32_t MinChargeHoming;
        const std::int32_t ChargedHoming;
        const std::int32_t HomingRange;
        const std::int32_t HomingTolerance;
        const std::int32_t UnchargedSplashRadius;
        const std::int32_t MinChargeSplashRadius;
        const std::int32_t ChargedSplashRadius;
        const std::int32_t UnchargedDistance;
        const std::int32_t MinChargeDistance;
        const std::int32_t ChargedDistance;
        const std::int32_t UnchargedSpread;
        const std::int32_t MinChargeSpread;
        const std::int32_t ChargedSpread;
        const std::int32_t UnchargedRicochetLossH;
        const std::int32_t MinChargeRicochetLossH;
        const std::int32_t ChargedRicochetLossH;
        const std::int32_t UnchargedRicochetLossV;
        const std::int32_t MinChargeRicochetLossV;
        const std::int32_t ChargedRicochetLossV;
        const std::uint16_t Projectiles;
        const std::uint16_t MinChargeProjectiles;
        const std::uint16_t ChargedProjectiles;
        const std::uint16_t SmokeStart;
        const std::uint16_t SmokeMinimum;
        const std::uint16_t SmokeDrain;
        const std::uint16_t SmokeShotAmount;
        const std::uint16_t SmokeChargeAmount;
        const std::string Description;

        WeaponInfo(BeamType beam, BeamType beamKind,
            std::shared_ptr<std::vector<std::uint8_t>> drawFuncIds,
            std::shared_ptr<std::vector<std::uint16_t>> colors, std::uint8_t priority, WeaponFlags flags,
            std::uint16_t splashDamage, std::uint16_t minChargeSplashDamage, std::uint16_t chargedSplashDamage,
            std::shared_ptr<std::vector<std::uint8_t>> splashDmgTypes, std::uint8_t shotCooldown,
            std::uint8_t autofireCooldown, std::uint8_t ammoType,
            std::shared_ptr<std::vector<std::uint8_t>> colEffects,
            std::shared_ptr<std::vector<std::uint8_t>> muzzleEffects,
            std::shared_ptr<std::vector<std::uint8_t>> dmgDirTypes,
            std::shared_ptr<std::vector<std::uint8_t>> dmgInterp,
            std::shared_ptr<std::vector<Affliction>> afflictions, std::uint8_t padding21,
            std::uint16_t minCharge, std::uint16_t fullCharge, std::uint16_t ammoCost,
            std::uint16_t minChargeCost, std::uint16_t chargeCost, std::uint16_t unchargedDamage,
            std::uint16_t minChargeDamage, std::uint16_t chargedDamage, std::uint16_t headshotDamage,
            std::uint16_t minChargeHeadshotDamage, std::uint16_t chargedHeadshotDamage,
            std::uint16_t unchargedLifespan, std::uint16_t minChargeLifespan,
            std::uint16_t chargedLifespan, std::shared_ptr<std::vector<std::uint16_t>> speedDecay,
            std::uint16_t padding42, std::shared_ptr<std::vector<std::uint16_t>> speedInterp,
            std::int32_t unchargedDmgDirMag, std::int32_t minChargeDmgDirMag, std::int32_t chargedDmgDirMag,
            std::int32_t zoomFov, std::int32_t unchargedCylRadius, std::int32_t minChargeCylRadius,
            std::int32_t chargedCylRadius, std::int32_t unchargedSpeed, std::int32_t minChargeSpeed,
            std::int32_t chargedSpeed, std::int32_t unchargedFinalSpeed, std::int32_t minChargeFinalSpeed,
            std::int32_t chargedFinalSpeed, std::int32_t unchargedGravity, std::int32_t minChargeGravity,
            std::int32_t chargedGravity, std::int32_t unchargedHoming, std::int32_t minChargeHoming,
            std::int32_t chargedHoming, std::int32_t homingRange, std::int32_t homingTolerance,
            std::int32_t unchargedSplashRadius, std::int32_t minChargeSplashRadius,
            std::int32_t chargedSplashRadius, std::int32_t unchargedDistance, std::int32_t minChargeDistance,
            std::int32_t chargedDistance, std::int32_t unchargedSpread, std::int32_t minChargeSpread,
            std::int32_t chargedSpread, std::int32_t unRicoLossH, std::int32_t minRicoLossH,
            std::int32_t chRicoLossH, std::int32_t unRicoLossV, std::int32_t minRicoLossV,
            std::int32_t chRicoLossV, std::uint16_t projectileCount,
            std::uint16_t minChargedProjectileCount, std::uint16_t chargeProjectileCount,
            std::uint16_t smokeStart, std::uint16_t smokeMinimum, std::uint16_t smokeDrain,
            std::uint16_t smokeShotAmount, std::uint16_t smokeChargeAmount, std::string description,
            std::int32_t unchargedRicoWeaponIdx = -1, std::int32_t chargedRicoWeaponIdx = -1);

        [[nodiscard]] const std::string& Name() const;
        [[nodiscard]] std::shared_ptr<WeaponInfo> UnchargedRicochetWeapon() const;
        [[nodiscard]] std::shared_ptr<WeaponInfo> ChargedRicochetWeapon() const;
    };

    namespace Weapons
    {
        using WeaponList = std::vector<std::shared_ptr<WeaponInfo>>;

        struct BotWeaponValues
        {
            std::uint16_t UnchargedDamage = 0;
            std::uint16_t ChargedDamage = 0;
            std::uint16_t SplashDamage = 0;
            std::uint16_t ChargedSplashDamage = 0;
        };

        using BotWeaponList = std::vector<std::shared_ptr<BotWeaponValues>>;
        using BotWeaponTable = std::vector<std::shared_ptr<const BotWeaponList>>;

        [[nodiscard]] BeamType GetAffinityBeam(Hunter hunter);
        extern const std::vector<BeamType> AffinityWeapons;
        extern std::shared_ptr<const WeaponList> Current;
        extern const std::shared_ptr<const WeaponList> Weapons1P;
        extern const std::shared_ptr<const WeaponList> WeaponsMP;
        extern const std::shared_ptr<const WeaponList> EnemyWeapons;
        extern const std::shared_ptr<const WeaponList> BossWeapons;
        extern const std::shared_ptr<const WeaponList> GoreaWeapons;
        extern const std::shared_ptr<const WeaponList> PlatformWeapons;
        extern const std::shared_ptr<const WeaponList> Ricochets;
        extern const std::shared_ptr<const BotWeaponTable> BotWeapons;
    }
}

#include "FrontendMeta.hpp"
#include "Rooms.hpp"

namespace MphRead::Metadata
{
    [[nodiscard]] int GetMultiplayerEntityLayer(GameMode mode, int playerCount);
    [[nodiscard]] std::string GetLayerName(int layerId, bool multiplayer);
    [[nodiscard]] std::string GetLayerNames(int layerMask, bool multiplayer);

    extern const OpenTK::Mathematics::Vector3 EmissionOrange;
    extern const OpenTK::Mathematics::Vector3 EmissionGreen;
    extern const OpenTK::Mathematics::Vector3 EmissionGray;
    extern const std::array<ColorRgb, 2> TeamColors;
    extern const OpenTK::Mathematics::Vector3 OctolithLight1Vector;
    extern const OpenTK::Mathematics::Vector3 OctolithLight2Vector;
    extern const OpenTK::Mathematics::Vector3 OctolithLightColor;
    extern const std::array<OpenTK::Mathematics::Vector3, 32> ToonTable;
    extern const std::unordered_map<std::string, std::vector<PaletteData>> PowerPalettes;
    extern const std::unordered_map<Hunter, float> HunterScales;
    extern const std::unordered_map<Hunter, std::array<std::string, 4>> HunterModels;
    extern const std::array<int, 89> AdpcmTable;
    extern const std::array<int, 16> ImaIndexTable;
    extern const std::array<std::string, 60> MusicSeqs;
    extern const std::array<float, 3> DamageLevels;
    extern const std::array<DoorMetadata, 4> Doors;
    extern const std::array<std::string, 3> FhDoors;
    extern const std::array<int, 10> DoorPalettes;
    extern const std::array<std::string, 6> JumpPads;
    extern const std::array<std::string, 23> Items;
    extern const std::array<std::string, 8> FhItems;
    extern std::array<OpenTK::Mathematics::Vector3, 54> ObjectVisPosOffsets;
    extern const PlatformMetadata InvisiblePlat;
    extern const std::array<std::string, 11> WeaponNames;
    extern const std::array<std::string, 11> WeaponNamesUpper;
    extern const std::array<int, 11> WeaponMessageIds;
    extern const std::array<std::pair<std::string, std::optional<std::string>>, 247> Effects;
    extern const std::array<float, 4> BeamRadiusValues;
    extern const std::array<int, 23> BeamDrawEffects;
    extern const std::array<int, 6> SyluxBombEffects;
    extern const std::unordered_map<SingleType, std::pair<std::string, std::string>> SingleParticles;
    extern const std::unordered_map<std::string, bool> PreloadResources;
    extern const OpenTK::Mathematics::Vector4 RedPalette;
    extern const OpenTK::Mathematics::Vector4 WhitePalette;
    extern const ::MphRead::ModelMetadata DoubleDamageImg;
    extern const std::unordered_map<std::string, ::MphRead::ModelMetadata> ModelMetadata;
    extern const std::unordered_map<std::string, ::MphRead::ModelMetadata> FirstHuntModels;

    [[nodiscard]] const ::MphRead::ModelMetadata* GetModelByName(
        std::string_view name, MetaDir dir = MetaDir::Models) noexcept;
    [[nodiscard]] const ::MphRead::ModelMetadata* GetFirstHuntModelByName(
        std::string_view name) noexcept;
    [[nodiscard]] const ::MphRead::ModelMetadata* GetEntityByPath(
        std::string_view path) noexcept;
    [[nodiscard]] const ObjectMetadata& GetObjectById(int id);
    [[nodiscard]] const ObjectMetadata& GetObjectById(std::uint32_t id);
    [[nodiscard]] const PlatformMetadata* GetPlatformById(int id);
    [[nodiscard]] OpenTK::Mathematics::Vector3 GetEventColor(Message eventId) noexcept;
    [[nodiscard]] OpenTK::Mathematics::Vector3 GetEventColor(FhMessage eventId) noexcept;
    [[nodiscard]] std::pair<const ::MphRead::RoomMetadata*, int> GetRoomByName(std::string_view name);
    [[nodiscard]] const ::MphRead::RoomMetadata* GetRoomById(int id, bool noThrow = false);
    [[nodiscard]] int GetAreaInfo(int roomId) noexcept;
}
