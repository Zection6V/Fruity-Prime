#pragma once

// Model.AnimationGroups: the per-index animation group lookup a model instance selects with one animation index.
// Transliterated from the managed sources so the field names and order stay
// checkable against them.  A C# reference member becomes a pointer.

#include "Formats/Types.hpp"
#include "Formats/raw_formats.hpp"
#include "Formats/enum_tables.hpp"
#include "Formats/model_format.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::model {
using namespace fruityprime::formats;


struct AnimationOffsets;
struct AnimationGroups;

// Model.cs
struct AnimationOffsets {
    std::vector<std::uint32_t> Node;
    std::vector<std::uint32_t> Material;
    std::vector<std::uint32_t> Texcoord;
    std::vector<std::uint32_t> Texture;
};

// Model.cs
struct AnimationGroups {
    bool Any{};
    std::vector<NodeAnimationGroup> Node;
    std::vector<MaterialAnimationGroup> Material;
    std::vector<TexcoordAnimationGroup> Texcoord;
    std::vector<TextureAnimationGroup> Texture;
    AnimationOffsets Offsets{};
};

} // namespace fruityprime::model
