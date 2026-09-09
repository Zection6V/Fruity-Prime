#pragma once

// The per-object metadata records that name a model's assets.
// Transliterated from the managed sources so the field names and order stay
// checkable against them.  A C# reference member becomes a pointer.

#include "Formats/Types.hpp"
#include "Formats/raw_formats.hpp"
#include "Formats/enum_tables.hpp"
#include "Metadata/metadata.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::metadata {
using namespace fruityprime::formats;


struct ModelMetadata;
struct RecolorMetadata;
struct ObjectMetadata;
struct PlatformMetadata;
struct DoorMetadata;
struct BotWeaponValues;

// Metadata.cs
struct ModelMetadata {
    std::string_view Name;
    std::string_view ModelPath;
    std::string_view AnimationPath;
    std::string_view AnimationShare;
    std::string_view CollisionPath;
    std::string_view ExtraCollisionPath;
    std::vector<RecolorMetadata> Recolors;
    bool UseLightSources{};
    bool FirstHunt{};
};

// Metadata.cs
struct RecolorMetadata {
    std::string_view Name;
    std::string_view ModelPath;
    std::string_view TexturePath;
    std::string_view PalettePath;
    std::string_view ReplacePath;
};

// Metadata.cs
struct ObjectMetadata {
    bool Lighting{};
    bool IgnoreAnimation{};
    std::string_view Name;
    std::vector<std::int32_t> AnimationIds;
    std::int32_t RecolorId{};
};

// Metadata.cs
struct PlatformMetadata {
    bool Animation{};
    bool Lighting{};
    std::string_view Name;
    std::vector<std::int32_t> AnimationIds;
};

// Metadata.cs
struct DoorMetadata {
    std::string_view Name;
    std::string_view LockName;
    float LockOffset{};
    float Radius{};
};

// Weapons.cs
struct BotWeaponValues {
    std::uint16_t UnchargedDamage{};
    std::uint16_t ChargedDamage{};
    std::uint16_t SplashDamage{};
    std::uint16_t ChargedSplashDamage{};
};

} // namespace fruityprime::metadata
