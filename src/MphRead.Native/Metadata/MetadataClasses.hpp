#pragma once

// Direct native counterparts of the small value types declared in
// Metadata/Metadata.cs.  The large managed tables are kept in their own
// generated/native table units; these classes preserve the path-building and
// default-value rules used by every table entry.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace fruityprime::metadata {

enum class MdlSuffix : std::uint8_t {
    None,
    All,
    Model,
};

enum class MetaDir : std::uint8_t {
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
    WifiGames,
};

class RecolorMetadata {
public:
    std::string Name;
    std::string ModelPath;
    std::string TexturePath;
    std::string PalettePath;
    std::optional<std::string> ReplacePath;
    std::map<int, std::vector<int>> ReplaceIds;

    RecolorMetadata(std::string name, std::string model_path);
    RecolorMetadata(std::string name, std::string model_path,
                    std::string texture_path);
    RecolorMetadata(std::string name, std::string model_path,
                    std::string texture_path, std::string palette_path,
                    std::map<int, std::vector<int>> replace_ids = {},
                    bool separate_replace = false);
};

class ModelMetadata {
public:
    std::string Name;
    std::string ModelPath;
    std::optional<std::string> AnimationPath;
    std::optional<std::string> AnimationShare;
    std::optional<std::string> CollisionPath;
    std::optional<std::string> ExtraCollisionPath;
    std::vector<RecolorMetadata> Recolors;
    bool UseLightSources = false;
    bool FirstHunt = false;

    ModelMetadata(std::string name, std::string model_path,
                  std::optional<std::string> animation_path,
                  std::optional<std::string> collision_path,
                  std::vector<RecolorMetadata> recolors,
                  std::optional<std::string> animation_share = std::nullopt,
                  bool use_light_sources = false);

    ModelMetadata(std::string name, MetaDir dir,
                  std::optional<std::string> anim = std::nullopt);

    ModelMetadata(std::string name, std::string texture_path, MetaDir dir);

    // Corresponds to the (name, animationPath, texturePath) overload.
    ModelMetadata(std::string name,
                  std::optional<std::string> animation_path,
                  std::optional<std::string> texture_path = std::nullopt);

    // Corresponds to the simple models overload with optional archive,
    // suffix, collision, and animation settings.
    ModelMetadata(std::string name, bool animation = true,
                  bool collision = false, bool texture = false,
                  std::optional<std::string> share = std::nullopt,
                  MdlSuffix mdl_suffix = MdlSuffix::None,
                  std::optional<std::string> archive = std::nullopt,
                  std::optional<std::string> add_to_anim = std::nullopt,
                  bool first_hunt = false,
                  std::optional<std::string> animation_path = std::nullopt,
                  std::optional<std::string> extra_collision = std::nullopt);

    // Corresponds to the overload that removes a token from the model name
    // when deriving animation/collision paths.
    [[nodiscard]] static ModelMetadata from_remove(
        std::string name, std::string remove, bool animation = true,
        std::optional<std::string> animation_path = std::nullopt,
        bool collision = false, bool first_hunt = false);

    // Corresponds to the recolor-list overload in Metadata.cs.
    [[nodiscard]] static ModelMetadata from_recolors(
        std::string name, const std::vector<std::string>& recolors,
        std::optional<std::string> remove = std::nullopt,
        bool animation = false,
        std::optional<std::string> animation_path = std::nullopt,
        bool texture = false,
        MdlSuffix mdl_suffix = MdlSuffix::None,
        std::optional<std::string> archive = std::nullopt,
        std::optional<std::string> recolor_name = std::nullopt,
        std::optional<std::string> animation_share = std::nullopt,
        bool use_light_sources = false, bool first_hunt = false,
        bool no_underscore = false);

    // Corresponds to the explicit, case-sensitive path overload.
    [[nodiscard]] static ModelMetadata with_paths(
        std::string name, std::string model_path,
        std::optional<std::string> animation_path,
        std::optional<std::string> collision_path,
        bool first_hunt = false);
};

class ObjectMetadata {
public:
    bool Lighting = false;
    bool IgnoreAnimation = false;
    std::string Name;
    std::vector<int> AnimationIds;
    int RecolorId = 0;

    explicit ObjectMetadata(std::string name, bool lighting = false,
                            int palette_id = 0, bool ignore_anim = false,
                            std::optional<std::vector<int>> animation_ids =
                                std::nullopt);
};

enum class PlatAnimId : std::uint8_t {
    InstantSleep = 0,
    Wake = 1,
    InstantWake = 2,
    Sleep = 3,
};

class PlatformMetadata {
public:
    bool Animation = false;
    bool Lighting = false;
    std::string Name;
    std::vector<int> AnimationIds;

    explicit PlatformMetadata(
        std::string name, bool lighting = false,
        std::optional<std::vector<int>> animation_ids = std::nullopt);
};

class DoorMetadata {
public:
    std::string Name;
    std::string LockName;
    float LockOffset = 0.0F;
    float Radius = 0.0F;

    DoorMetadata(std::string name, std::string lock_name, float lock_offset,
                 float radius);
};

} // namespace fruityprime::metadata
