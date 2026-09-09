#pragma once

#include "Formats/model_format.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace fruityprime::utility::repack_model {

enum class TextureStorage {
    Inline,
    Separate
};

enum class BoundsMode {
    None,
    Capped,
    Uncapped
};

struct ModelPackOptions {
    TextureStorage texture = TextureStorage::Inline;
    bool is_room = false;
    BoundsMode bounds = BoundsMode::None;
};

struct ModelPackResult {
    std::vector<std::uint8_t> model;
    // Empty for inline textures. For separate textures this contains the
    // encoded image and palette streams in the same order as the managed
    // RepackTexture.Separate writer.
    std::vector<std::uint8_t> texture;
};

// Native equivalent of Repack.PackAnim.  AnimationResults is the decoded
// model-side representation already used by the renderer; the offset arrays
// identify empty groups in the same way the managed nullable group lists do.
[[nodiscard]] std::vector<std::uint8_t> pack_animation(
    const model::AnimationResults& animations, bool first_hunt_padding = false);

// Native equivalent of Repack.PackModel for an already decoded model. The
// writer follows the managed ordering: optional node tables, encoded texture
// data, texture/palette tables, display lists, materials, nodes, meshes, then
// the header. It deliberately consumes the renderer-neutral model::File API.
[[nodiscard]] ModelPackResult pack_model(
    const model::File& source, ModelPackOptions options = {});

[[nodiscard]] ModelPackResult repack_model(
    std::span<const std::uint8_t> bytes, ModelPackOptions options = {});

} // namespace fruityprime::utility::repack_model
