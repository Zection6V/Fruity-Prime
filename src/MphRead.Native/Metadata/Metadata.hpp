#pragma once

#include "../Formats/Enums.hpp"
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
    enum class GameMode : std::int32_t;

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

    struct PaletteData
    {
        std::uint16_t Data = 0;

        constexpr PaletteData() noexcept = default;
        constexpr explicit PaletteData(std::uint16_t data) noexcept : Data(data) {}
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
