#pragma once

#include "Assets/game_assets.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::assets {

// ModelMetadata in the managed tree has a few historical spelling variants:
// some resources use `_Model`, some use `_model`, and models which were
// authored with MdlSuffix use `_mdl_Model`.  Keep that compatibility in one
// native resolver instead of spreading path guesses across every Entity.
struct NamedModel {
    model::File model;
    std::string model_path;
    std::string animation_path;
};

[[nodiscard]] std::vector<std::string> model_path_candidates(
    std::string_view name);
[[nodiscard]] std::vector<std::string> animation_path_candidates(
    std::string_view name);

// Missing optional animation is not a missing model.  This mirrors the
// managed ModelMetadata(animation: false) cases while still attaching an
// animation whenever a matching resource exists.
[[nodiscard]] std::optional<NamedModel> try_load_named_model(
    const Store& assets, std::string_view name,
    bool load_animation = true);

} // namespace fruityprime::assets
