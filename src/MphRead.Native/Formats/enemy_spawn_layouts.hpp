#pragma once

// Native counterpart of Formats/EntityEnemy.cs.
// The thirteen MPH enemy-spawn union layouts, exactly as the managed editor groups them.
// Transliterated from the managed source so the field order and names stay
// checkable against it.

#include "Formats/Types.hpp"
#include "Formats/raw_formats.hpp"
#include "Formats/enum_tables.hpp"

#include "Formats/enemy_spawn.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::formats {

using EntityLengthArray = std::array<std::uint16_t, 16>;

using Vector3FxArray10 = std::array<Vector3Fx, 10>;

using Vector3FxArray16 = std::array<Vector3Fx, 16>;

using Vector3FxArray8 = std::array<Vector3Fx, 8>;

using Vector4FxArray10 = std::array<Vector4Fx, 10>;

// The managed EnemySpawnFieldsWW is enemy_spawn::WarWaspFields.
using EnemySpawnFieldsWW = enemy_spawn::WarWaspFields;

struct EnemySpawnFields00 {
    raw::RawCollisionVolume Volume0{};
    raw::RawCollisionVolume Volume1{};
    raw::RawCollisionVolume Volume2{};
    raw::RawCollisionVolume Volume3{};
};

struct EnemySpawnFields01 {
    EnemySpawnFieldsWW WarWasp{};
    std::uint32_t Padding1B0{};
    std::uint32_t Padding1B4{};
};

struct EnemySpawnFields02 {
    raw::RawCollisionVolume Volume0{};
    formats::Vector3Fx PathVector{};
    raw::RawCollisionVolume Volume1{};
    raw::RawCollisionVolume Volume2{};
};

struct EnemySpawnFields03 {
    raw::RawCollisionVolume Volume0{};
    std::uint32_t Unused68{};
    std::uint32_t Unused6C{};
    std::uint32_t Unused70{};
    std::uint32_t Unused74{};
    std::uint32_t Unused78{};
    std::uint32_t Unused7C{};
    std::uint32_t Unused80{};
    formats::Vector3Fx Facing{};
    formats::Vector3Fx Position{};
    formats::Vector3Fx IdleRange{};
};

struct EnemySpawnFields04 {
    raw::RawCollisionVolume Volume0{};
    std::uint32_t Unused68{};
    std::uint32_t Unused6C{};
    std::uint32_t Unused70{};
    std::uint32_t Unused74{};
    formats::Vector3Fx Position{};
    std::int32_t WeaveOffset{};
    std::int32_t Field88{};
};

struct EnemySpawnFields05 {
    std::uint32_t EnemySubtype{};
    raw::RawCollisionVolume Volume0{};
    raw::RawCollisionVolume Volume1{};
    raw::RawCollisionVolume Volume2{};
    raw::RawCollisionVolume Volume3{};
};

struct EnemySpawnFields06 {
    std::uint32_t EnemySubtype{};
    std::uint32_t EnemyVersion{};
    raw::RawCollisionVolume Volume0{};
    raw::RawCollisionVolume Volume1{};
    raw::RawCollisionVolume Volume2{};
    raw::RawCollisionVolume Volume3{};
};

struct EnemySpawnFields07 {
    std::uint16_t EnemyHealth{};
    std::uint16_t EnemyDamage{};
    std::uint32_t EnemySubtype{};
    raw::RawCollisionVolume Volume0{};
};

struct EnemySpawnFields08 {
    std::uint32_t EnemySubtype{};
    std::uint32_t EnemyVersion{};
    EnemySpawnFieldsWW WarWasp{};
};

struct EnemySpawnFields09 {
    std::uint32_t HunterId{};
    std::uint32_t EncounterType{};
    std::uint32_t HunterWeapon{};
    std::uint16_t HunterHealth{};
    std::uint16_t HunterHealthMax{};
    std::uint16_t HunterHealthThreshold{};
    std::uint8_t HunterColor{};
    std::uint8_t HunterChance{};
};

struct EnemySpawnFields10 {
    std::uint32_t EnemySubtype{};
    std::uint32_t EnemyVersion{};
    raw::RawCollisionVolume Volume0{};
    raw::RawCollisionVolume Volume1{};
    std::int32_t Index{};
};

struct EnemySpawnFields11 {
    formats::Vector3Fx Sphere1Position{};
    formats::Fixed Sphere1Radius{};
    formats::Vector3Fx Sphere2Position{};
    formats::Fixed Sphere2Radius{};
};

struct EnemySpawnFields12 {
    formats::Vector3Fx Field28{};
    formats::Fixed Field34{};
    formats::Fixed Field38{};
};

struct EnumSpawnUnion {
    EnemySpawnFields00 S00{};
    EnemySpawnFields01 S01{};
    EnemySpawnFields02 S02{};
    EnemySpawnFields03 S03{};
    EnemySpawnFields04 S04{};
    EnemySpawnFields05 S05{};
    EnemySpawnFields06 S06{};
    EnemySpawnFields07 S07{};
    EnemySpawnFields08 S08{};
    EnemySpawnFields09 S09{};
    EnemySpawnFields10 S10{};
    EnemySpawnFields11 S11{};
    EnemySpawnFields12 S12{};
};

} // namespace fruityprime::formats
