#include "Assets/model_catalog.hpp"

#include <algorithm>
#include <exception>
#include <utility>

namespace fruityprime::assets {
namespace {

void append_unique(std::vector<std::string>& values, std::string value) {
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(std::move(value));
    }
}

// A number of ModelMetadata entries keep the geometry in the named model but
// keep the texture and palette tables in a shared *_TextureShare model (or in
// the first explicit img_00 recolor).  The managed reader combines those
// resources before a model is rendered.  Keep the small metadata bridge here
// so Entity rendering does not silently try to decode UVs with a zero-sized
// texture table.
[[nodiscard]] std::string_view recolor_resource_path(
    std::string_view name) noexcept {
    if (name == "AlimbicBossDoorLock"
        || name == "AlimbicDoorLock"
        || name == "AlimbicMorphBallDoor"
        || name == "AlimbicMorphBallDoorLock"
        || name == "EnemySpawner"
        || name == "GhostSwitch"
        || name == "Switch"
        || name == "ThinDoorLock") {
        return "models/AlimbicTextureShare_img_Model.bin";
    }
    if (name == "Artifact01" || name == "Artifact02"
        || name == "Artifact03" || name == "Artifact04"
        || name == "Artifact05" || name == "Artifact06"
        || name == "Artifact07" || name == "Artifact08") {
        return "models/ArtifactTextureShare_img_Model.bin";
    }
    if (name == "Teleporter" || name == "TeleporterSmall") {
        return "models/TeleporterTextureShare_img_Model.bin";
    }
    if (name == "Alimbic_Turret") {
        return "models/Alimbic_Turret_img_00_Model.bin";
    }
    if (name == "BarbedWarWasp") {
        return "models/BarbedWarWasp_img_00_Model.bin";
    }
    if (name == "GuardBot1") {
        return "models/GuardBot1_img_00_Model.bin";
    }
    if (name == "LavaDemon") {
        return "models/LavaDemon_img_00_Model.bin";
    }
    if (name == "PsychoBit") {
        return "models/PsychoBit_img_00_Model.bin";
    }
    if (name == "Generic_Console" || name == "Generic_Monitor"
        || name == "Generic_Power" || name == "Generic_Scanner"
        || name == "Generic_Switch") {
        return "models/GenericEquipTextureShare_img_Model.bin";
    }
    if (name == "Alimbic_Console" || name == "Alimbic_Monitor"
        || name == "Alimbic_Power" || name == "Alimbic_Scanner"
        || name == "Alimbic_Switch") {
        return "models/AlimbicEquipTextureShare_img_Model.bin";
    }
    if (name == "Ice_Console" || name == "Ice_Monitor"
        || name == "Ice_Power" || name == "Ice_Scanner"
        || name == "Ice_Switch") {
        return "models/IceEquipTextureShare_img_Model.bin";
    }
    if (name == "Lava_Console" || name == "Lava_Monitor"
        || name == "Lava_Power" || name == "Lava_Scanner"
        || name == "Lava_Switch") {
        return "models/LavaEquipTextureShare_img_Model.bin";
    }
    if (name == "Ruins_Console" || name == "Ruins_Monitor"
        || name == "Ruins_Power" || name == "Ruins_Scanner"
        || name == "Ruins_Switch") {
        return "models/RuinsEquipTextureShare_img_Model.bin";
    }
    return {};
}

[[nodiscard]] bool has_external_texture_resource(
    std::string_view name) noexcept {
    // These are the particle/single-particle models whose ModelMetadata uses
    // the model for the table and a separate models/<name>_Tex.bin resource
    // for image data.  Keep this in the native resolver so effect rendering
    // and headless asset audits use the same ownership rule as Read.GetModel.
    return name == "deathParticle" || name == "geo1"
        || name == "particles" || name == "particles2"
        || name == "TearParticle" || name == "icons";
}

[[nodiscard]] model::File load_named_model_file(
    const Store& assets, std::string_view name,
    const std::string& model_path) {
    auto model_bytes = assets.bytes(model_path);
    if (has_external_texture_resource(name)) {
        try {
            auto texture_bytes = assets.bytes(
                "models/" + std::string(name) + "_Tex.bin");
            return model::File::from_resources(
                std::move(model_bytes), std::move(texture_bytes));
        } catch (const std::exception&) {
            // Keep the standalone path for extracted trees which have not
            // copied the companion texture yet.  The caller can still use a
            // model with embedded data, and the next candidate may be a
            // different case/suffix variant.
        }
    }
    const std::string_view recolor_path = recolor_resource_path(name);
    if (!recolor_path.empty()) {
        // The shared model is used both as the texture-table source and as
        // the image/palette source for the default recolor.  Read it before
        // moving the geometry bytes so the fallback remains exception-safe.
        std::vector<std::uint8_t> shared_bytes;
        try {
            shared_bytes = assets.bytes(recolor_path);
        } catch (const std::exception&) {
            // Extracted development trees may omit optional shared resources;
            // the standalone model still has a useful best-effort path.
        }
        if (!shared_bytes.empty()) {
            return model::File::from_recolor_resources(
                std::move(model_bytes), shared_bytes, shared_bytes);
        }
    }
    return model::File::from_bytes(std::move(model_bytes));
}

} // namespace

std::vector<std::string> model_path_candidates(std::string_view name) {
    std::vector<std::string> result;
    if (name.empty()) {
        return result;
    }
    const std::string base(name);
    if (name == "deathParticle" || name == "geo1"
        || name == "particles" || name == "particles2") {
        append_unique(result, "_archives/effectsBase/" + base
                      + "_Model.bin");
    } else if (name == "icons") {
        append_unique(result, "hud/icons_Model.bin");
    }
    append_unique(result, "models/" + base + "_Model.bin");
    append_unique(result, "models/" + base + "_mdl_Model.bin");
    append_unique(result, "models/" + base + "_model.bin");
    append_unique(result, "models/" + base + "_mdl_model.bin");
    return result;
}

std::vector<std::string> animation_path_candidates(std::string_view name) {
    std::vector<std::string> result;
    if (name.empty()) {
        return result;
    }
    const std::string base(name);
    if (name == "Artifact01" || name == "Artifact02"
        || name == "Artifact03" || name == "Artifact04"
        || name == "Artifact05" || name == "Artifact06"
        || name == "Artifact07" || name == "Artifact08") {
        append_unique(result, "models/Artifact_Anim.bin");
    }
    const auto append_name_variants = [&result](std::string_view stem) {
        const std::string value(stem);
        append_unique(result, "models/" + value + "_Anim.bin");
        append_unique(result, "models/" + value + "_mdl_Anim.bin");
        append_unique(result, "models/" + value + "_anim.bin");
        append_unique(result, "models/" + value + "_mdl_anim.bin");
    };
    append_name_variants(base);
    // ModelMetadata can remove a LOD suffix for the companion animation
    // resource while retaining it in the model resource name.  Gorea1A/B/2
    // are the most visible cases, but this is the same rule for every *_lod0
    // and *_lod1 model in the managed catalog.
    if (base.ends_with("_lod0") || base.ends_with("_lod1")) {
        append_name_variants(std::string_view(base).substr(0, base.size() - 5));
    }
    return result;
}

std::optional<NamedModel> try_load_named_model(const Store& assets,
                                               std::string_view name,
                                               bool load_animation) {
    for (const auto& model_path : model_path_candidates(name)) {
        try {
            NamedModel result{load_named_model_file(assets, name, model_path),
                              model_path, {}};
            if (load_animation) {
                for (const auto& animation_path :
                     animation_path_candidates(name)) {
                    try {
                        auto animation = assets.bytes(animation_path);
                        result.model.load_animations(std::move(animation),
                                                     std::string(name));
                        result.animation_path = animation_path;
                        break;
                    } catch (const std::exception&) {
                        // The animation is optional for static geometry. Keep
                        // trying the case/suffix variants before giving up.
                    }
                }
            }
            return result;
        } catch (const std::exception&) {
            // Keep trying the path variants. A real ROM and an extracted
            // directory can differ in case even though the managed lookup is
            // expressed with one metadata name.
        }
    }

    // A small set of shared managed models, notably items_base, lives in the
    // common ARC rather than under models/.  Keep this fallback here because
    // Entity code should not need to know whether a ModelMetadata entry was
    // archived.
    try {
        const auto resource = assets.archive("archives/common.arc");
        for (const auto& entry_name : model_path_candidates(name)) {
            const std::string path_prefix = "models/";
            if (!entry_name.starts_with(path_prefix)) {
                continue;
            }
            const std::string candidate = entry_name.substr(path_prefix.size());
            const auto found = std::find_if(
                resource.entries().begin(), resource.entries().end(),
                [&candidate](const auto& entry) {
                    return entry.filename == candidate;
                });
            if (found == resource.entries().end()) {
                continue;
            }
            NamedModel result{
                model::File::from_bytes(resource.file(static_cast<std::size_t>(
                    found - resource.entries().begin()))),
                "archives/common.arc/" + candidate,
                {}};
            return result;
        }
    } catch (const std::exception&) {
        // Optional shared archives are allowed to be absent in extracted
        // development directories.
    }
    return std::nullopt;
}

} // namespace fruityprime::assets
