#pragma once

#include "Metadata.hpp"
#include "../Entities/EnemyInstanceEntity.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace MphRead::Entities::Enemies
{
    class Enemy00Entity;
    class Enemy02Entity;
    class Enemy03Entity;
    class Enemy04Entity;
    class Enemy05Entity;
    class Enemy06Entity;
    class Enemy10Entity;
    class Enemy11Entity;
    class Enemy16Entity;
    class Enemy18Entity;
    class Enemy19Entity;
    class Enemy23Entity;
    class Enemy24Entity;
    class Enemy28Entity;
    class Enemy31Entity;
    class Enemy33Entity;
    class Enemy35Entity;
    class Enemy36Entity;
    class Enemy38Entity;
    class Enemy39Entity;
    class Enemy45Entity;
    class Enemy46Entity;
    class Enemy47Entity;

    struct Enemy10Values;
    struct Enemy18Values;
    struct Enemy19Values;
    struct Enemy23Values;
    struct Enemy36Values;
    struct Enemy39Values;
    struct Enemy41Values;
    struct Enemy44Values;
    struct Enemy45Values;
}

namespace MphRead::Metadata
{
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy00Entity>> Enemy00Subroutines;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy02Entity>> Enemy02Subroutines;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy03Entity>> Enemy03Subroutines;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy04Entity>> Enemy04Subroutines;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy05Entity>> Enemy05Subroutines;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy06Entity>> Enemy06Subroutines;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy10Entity>> Enemy10Subroutines;
    extern std::vector<Entities::Enemies::Enemy10Values> Enemy10Values;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy11Entity>> Enemy11Subroutines;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy16Entity>> Enemy16Subroutines;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy18Entity>> Enemy18Subroutines;
    extern std::vector<Entities::Enemies::Enemy18Values> Enemy18Values;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy45Entity>> Enemy45Subroutines;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy19Entity>> Enemy19Subroutines;
    extern std::vector<Entities::Enemies::Enemy19Values> Enemy19Values;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy23Entity>> Enemy23Subroutines;
    extern std::vector<Entities::Enemies::Enemy23Values> Enemy23Values;

    extern std::vector<std::vector<ColorRgb>> Enemy24Colors;

    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy24Entity>> Enemy24Subroutines;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy28Entity>> Enemy28Subroutines;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy31Entity>> Enemy31Subroutines;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy33Entity>> Enemy33Subroutines;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy35Entity>> Enemy35Subroutines;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy36Entity>> Enemy36Subroutines;
    extern std::vector<Entities::Enemies::Enemy36Values> Enemy36Values;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy38Entity>> Enemy38Subroutines;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy39Entity>> Enemy39Subroutines;
    extern std::vector<Entities::Enemies::Enemy39Values> Enemy39Values;
    extern std::vector<Entities::Enemies::Enemy41Values> Enemy41Values;
    extern std::vector<Entities::Enemies::Enemy44Values> Enemy44Values;
    extern std::vector<Entities::Enemies::Enemy45Values> Enemy45Values;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy46Entity>> Enemy46Subroutines;
    extern std::vector<Entities::EnemySubroutine<Entities::Enemies::Enemy47Entity>> Enemy47Subroutines;

    [[nodiscard]] std::optional<std::string> GetEnemyModelName(EnemyType type);
    extern const std::vector<std::string> EnemyModelNames;

    [[nodiscard]] std::int32_t GetEnemyDeathEffect(EnemyType type);
    extern const std::vector<std::int32_t> EnemyDeathEffects;
    extern const std::vector<std::int32_t> EnemyAudioRangeIndices;
    extern const std::vector<std::int32_t> EnemyScanIds;

    [[nodiscard]] float GetDamageMultiplier(Entities::Effectiveness effectiveness);
    extern const std::vector<float> DamageMultipliers;

    void LoadEffectiveness(EnemyType type, std::span<Entities::Effectiveness> dest);
    void LoadEffectiveness(std::int32_t value, std::span<Entities::Effectiveness> dest);
    void LoadEffectiveness(std::uint32_t value, std::span<Entities::Effectiveness> dest);

    extern const std::vector<std::int32_t> EnemyEffectiveness;
    extern const std::vector<std::int32_t> SlenchEffectiveness;
    extern const std::vector<std::int32_t> SlenchSynapseEffectiveness;
    extern const std::vector<std::int32_t> GoreaEffectiveness;
}
