#include "MetadataClasses.hpp"

#include <stdexcept>
#include <string_view>
#include <utility>

namespace fruityprime::metadata {
namespace {

std::string replace_all(std::string value, std::string_view from,
                        std::string_view to) {
    if (from.empty()) {
        return value;
    }
    std::size_t position = 0;
    while ((position = value.find(from, position)) != std::string::npos) {
        value.replace(position, from.size(), to);
        position += to.size();
    }
    return value;
}

std::string directory_for(MetaDir dir) {
    switch (dir) {
    case MetaDir::CharSelect: return "characterselect";
    case MetaDir::CreateJoin: return "createjoin";
    case MetaDir::GameOption: return "gameoptions";
    case MetaDir::GamersCard: return "gamerscard";
    case MetaDir::Hud: return "hud";
    case MetaDir::Keyboard: return "keyboard";
    case MetaDir::Keypad: return "keypad";
    case MetaDir::Logo: return R"(logo_screen\MAYA)";
    case MetaDir::MainMenu: return "main menu";
    case MetaDir::Models: return "models";
    case MetaDir::MoviePlayer: return "movieplayer";
    case MetaDir::MultiMaster: return "multimaster";
    case MetaDir::Multiplayer: return "multiplayer";
    case MetaDir::PaxControls: return "pax_controls";
    case MetaDir::Popup: return "popup";
    case MetaDir::Results: return "results";
    case MetaDir::ScStartGame: return "sc_startgame";
    case MetaDir::Stage: return "stage";
    case MetaDir::StartGame: return "startgame";
    case MetaDir::ToStart: return "tostart";
    case MetaDir::TouchToStart: return "touchtostart";
    case MetaDir::TouchToStart2: return "touchtostart_2";
    case MetaDir::WifiCreate: return "wifi_createjoin";
    case MetaDir::WifiGames: return "wifi_games";
    }
    throw std::invalid_argument("unknown MetaDir");
}

std::string optional_string(const std::optional<std::string>& value) {
    return value.has_value() ? *value : std::string{};
}

} // namespace

RecolorMetadata::RecolorMetadata(std::string name, std::string model_path)
    : Name(std::move(name)), ModelPath(std::move(model_path)),
      TexturePath(ModelPath), PalettePath(ModelPath) {}

RecolorMetadata::RecolorMetadata(std::string name, std::string model_path,
                                 std::string texture_path)
    : Name(std::move(name)), ModelPath(std::move(model_path)),
      TexturePath(std::move(texture_path)), PalettePath(TexturePath) {}

RecolorMetadata::RecolorMetadata(
    std::string name, std::string model_path, std::string texture_path,
    std::string palette_path, std::map<int, std::vector<int>> replace_ids,
    bool separate_replace)
    : Name(std::move(name)), ModelPath(std::move(model_path)),
      TexturePath(std::move(texture_path)),
      PalettePath(separate_replace ? TexturePath : std::move(palette_path)),
      ReplaceIds(std::move(replace_ids)) {
    if (separate_replace) {
        ReplacePath = std::move(palette_path);
    }
}

ModelMetadata::ModelMetadata(
    std::string name, std::string model_path,
    std::optional<std::string> animation_path,
    std::optional<std::string> collision_path,
    std::vector<RecolorMetadata> recolors,
    std::optional<std::string> animation_share, bool use_light_sources)
    : Name(std::move(name)), ModelPath(std::move(model_path)),
      AnimationPath(std::move(animation_path)),
      AnimationShare(std::move(animation_share)),
      CollisionPath(std::move(collision_path)), Recolors(std::move(recolors)),
      UseLightSources(use_light_sources) {}

ModelMetadata::ModelMetadata(std::string name, MetaDir dir,
                             std::optional<std::string> anim)
    : Name(std::move(name)) {
    const std::string directory = directory_for(dir);
    ModelPath = directory + "\\" + Name + "_Model.bin";
    if (anim.has_value()) {
        AnimationPath = directory + "\\" + *anim + "_Anim.bin";
    }
    Recolors.emplace_back("default", ModelPath, ModelPath);
}

ModelMetadata::ModelMetadata(std::string name, std::string texture_path,
                             MetaDir dir)
    : Name(std::move(name)) {
    const std::string directory = directory_for(dir);
    ModelPath = directory + "\\" + Name + "_Model.bin";
    Recolors.emplace_back("default", ModelPath, std::move(texture_path));
}

ModelMetadata::ModelMetadata(std::string name,
                             std::optional<std::string> animation_path,
                             std::optional<std::string> texture_path)
    : Name(std::move(name)),
      ModelPath("models\\" + Name + "_Model.bin"),
      AnimationPath(std::move(animation_path)) {
    Recolors.emplace_back("default", ModelPath,
                          texture_path.has_value() ? *texture_path : ModelPath);
}

ModelMetadata::ModelMetadata(
    std::string name, bool animation, bool collision, bool texture,
    std::optional<std::string> share, MdlSuffix mdl_suffix,
    std::optional<std::string> archive, std::optional<std::string> add_to_anim,
    bool first_hunt, std::optional<std::string> animation_path,
    std::optional<std::string> extra_collision)
    : Name(std::move(name)), FirstHunt(first_hunt) {
    const std::string path = archive.has_value()
        ? "_archives\\" + *archive : "models";
    std::string suffix;
    if (mdl_suffix != MdlSuffix::None) {
        suffix = "_mdl";
    }
    ModelPath = path + "\\" + Name + suffix + "_Model.bin";
    if (mdl_suffix != MdlSuffix::All) {
        suffix.clear();
    }
    if (animation) {
        AnimationPath = animation_path.has_value()
            ? std::move(animation_path)
            : std::optional<std::string>(
                path + "\\" + Name + optional_string(add_to_anim)
                + suffix + "_Anim.bin");
    }
    if (collision) {
        CollisionPath = path + "\\" + Name + suffix + "_Collision.bin";
    }
    if (extra_collision.has_value()) {
        ExtraCollisionPath = path + "\\" + *extra_collision
            + "_Collision.bin";
    }

    bool use_texture = texture;
    std::string recolor_model = ModelPath;
    if (share.has_value()) {
        use_texture = false;
        recolor_model = *share;
    }
    const std::string texture_path = use_texture
        ? "models\\" + Name + suffix + "_Tex.bin" : recolor_model;
    Recolors.emplace_back("default", std::move(recolor_model), texture_path);
}

ModelMetadata ModelMetadata::from_remove(
    std::string name, std::string remove, bool animation,
    std::optional<std::string> animation_path, bool collision, bool first_hunt) {
    const std::string model_path = "models\\" + name + "_Model.bin";
    const std::string removed = replace_all(name, remove, "");
    std::optional<std::string> derived_animation;
    std::optional<std::string> derived_collision;
    if (animation) {
        derived_animation = animation_path.has_value()
            ? std::move(animation_path)
            : std::optional<std::string>("models\\" + removed + "_Anim.bin");
    }
    if (collision) {
        derived_collision = "models\\" + removed + "_Collision.bin";
    }
    ModelMetadata result(name, model_path, std::move(derived_animation),
                         std::move(derived_collision),
                         {RecolorMetadata("default", model_path)});
    result.FirstHunt = first_hunt;
    return result;
}

ModelMetadata ModelMetadata::from_recolors(
    std::string name, const std::vector<std::string>& recolors,
    std::optional<std::string> remove, bool animation,
    std::optional<std::string> animation_path, bool texture,
    MdlSuffix mdl_suffix, std::optional<std::string> archive,
    std::optional<std::string> recolor_name,
    std::optional<std::string> animation_share, bool use_light_sources,
    bool first_hunt, bool no_underscore) {
    std::string suffix;
    if (mdl_suffix != MdlSuffix::None) {
        suffix = "_mdl";
    }
    const std::string model_path = archive.has_value()
        ? "_archives\\" + *archive + "\\" + name + "_Model.bin"
        : "models\\" + name + suffix + "_Model.bin";
    std::string derived_name = name;
    if (remove.has_value()) {
        derived_name = replace_all(derived_name, *remove, "");
    }
    if (mdl_suffix != MdlSuffix::All) {
        suffix.clear();
    }

    std::optional<std::string> derived_animation;
    if (animation_path.has_value()) {
        derived_animation = std::move(animation_path);
    } else if (animation) {
        if (archive.has_value()) {
            derived_animation = "_archives\\" + *archive + "\\"
                + derived_name + "_Anim.bin";
        } else {
            derived_animation = "models\\" + derived_name + suffix
                + "_Anim.bin";
        }
    }

    std::vector<RecolorMetadata> recolor_list;
    recolor_list.reserve(recolors.size());
    for (const std::string& recolor : recolors) {
        std::string recolor_string = recolor_name.value_or(derived_name);
        if (!no_underscore) {
            recolor_string += "_";
        }
        recolor_string += recolor;
        if (!recolor.empty() && recolor.front() == '*') {
            recolor_string = replace_all(recolor, "*", "");
        }
        const std::string recolor_model = "models\\" + recolor_string
            + "_Model.bin";
        const std::string texture_path = texture
            ? "models\\" + recolor_string + "_Tex.bin" : recolor_model;
        recolor_list.emplace_back(recolor, recolor_model, texture_path);
    }
    ModelMetadata result(name, model_path, std::move(derived_animation),
                         std::nullopt, std::move(recolor_list),
                         std::move(animation_share), use_light_sources);
    result.FirstHunt = first_hunt;
    return result;
}

ModelMetadata ModelMetadata::with_paths(
    std::string name, std::string model_path,
    std::optional<std::string> animation_path,
    std::optional<std::string> collision_path, bool first_hunt) {
    ModelMetadata result(name, model_path, std::move(animation_path),
                         std::move(collision_path),
                         {RecolorMetadata("default", model_path, model_path)});
    result.FirstHunt = first_hunt;
    return result;
}

ObjectMetadata::ObjectMetadata(
    std::string name, bool lighting, int palette_id, bool ignore_anim,
    std::optional<std::vector<int>> animation_ids)
    : Lighting(lighting), IgnoreAnimation(ignore_anim), Name(std::move(name)),
      RecolorId(palette_id) {
    if (!animation_ids.has_value()) {
        AnimationIds = {0, 0, 0, 0};
    } else if (animation_ids->size() != 4) {
        throw std::invalid_argument("animationIds must contain four values");
    } else {
        AnimationIds = std::move(*animation_ids);
    }
}

PlatformMetadata::PlatformMetadata(
    std::string name, bool lighting,
    std::optional<std::vector<int>> animation_ids)
    : Animation(animation_ids.has_value()), Lighting(lighting),
      Name(std::move(name)) {
    if (!animation_ids.has_value()) {
        AnimationIds = {-1, -1, -1, -1};
    } else if (animation_ids->size() != 4) {
        throw std::invalid_argument("animationIds must contain four values");
    } else {
        AnimationIds = std::move(*animation_ids);
    }
}

DoorMetadata::DoorMetadata(std::string name, std::string lock_name,
                           float lock_offset, float radius)
    : Name(std::move(name)), LockName(std::move(lock_name)),
      LockOffset(lock_offset), Radius(radius) {}

} // namespace fruityprime::metadata
