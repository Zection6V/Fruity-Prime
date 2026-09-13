#include "Enemies.hpp"

#include <cassert>
#include <cstddef>
#include <stdexcept>

namespace MphRead::Metadata
{
    namespace
    {
        [[noreturn]] void ThrowIndexOutOfRange()
        {
            throw std::out_of_range("Index was outside the bounds of the array.");
        }

        template <typename T>
        const T& GetAt(const std::vector<T>& values, std::size_t index)
        {
            if (index >= values.size())
            {
                ThrowIndexOutOfRange();
            }
            return values[index];
        }
    }

    std::vector<std::vector<ColorRgb>> Enemy24Colors = {
        {
            ColorRgb(31, 31, 31),
            ColorRgb(0, 31, 0),
            ColorRgb(9, 7, 0),
            ColorRgb(31, 31, 31)
        },
        {
            ColorRgb(31, 31, 31),
            ColorRgb(31, 24, 0),
            ColorRgb(0, 5, 0),
            ColorRgb(31, 31, 31)
        },
        {
            ColorRgb(31, 31, 31),
            ColorRgb(31, 15, 0),
            ColorRgb(3, 2, 4),
            ColorRgb(31, 31, 31)
        },
        {
            ColorRgb(31, 31, 31),
            ColorRgb(20, 10, 31),
            ColorRgb(9, 3, 0),
            ColorRgb(31, 31, 31)
        },
        {
            ColorRgb(31, 31, 31),
            ColorRgb(31, 0, 0),
            ColorRgb(0, 2, 4),
            ColorRgb(31, 31, 31)
        },
        {
            ColorRgb(31, 31, 31),
            ColorRgb(0, 13, 31),
            ColorRgb(9, 0, 0),
            ColorRgb(31, 31, 31)
        }
    };

    std::optional<std::string> GetEnemyModelName(EnemyType type)
    {
        const std::size_t index = static_cast<std::size_t>(type);
        const std::string& name = GetAt(EnemyModelNames, index);
        if (name.empty())
        {
            return std::nullopt;
        }
        return name;
    }

    const std::vector<std::string> EnemyModelNames = {
        "warwasp_lod0",
        "zoomer",
        "Temroid_lod0",
        "Chomtroid",
        "Chomtroid",
        "Chomtroid",
        "Chomtroid",
        "",
        "",
        "",
        "BarbedWarWasp",
        "shriekbat",
        "geemer",
        "",
        "",
        "",
        "blastcap",
        "",
        "Alimbic_Turret",
        "CylinderBoss",
        "CylinderBossEye",
        "",
        "",
        "PsychoBit",
        "Gorea1A_lod0",
        "",
        "",
        "",
        "Gorea1B_lod0",
        "",
        "PowerBomb",
        "Gorea2_lod0",
        "",
        "goreaMeteor",
        "PsychoBit",
        "GuardBot2_lod0",
        "GuardBot1",
        "DripStank_lod0",
        "AlimbicStatue_lod0",
        "LavaDemon",
        "",
        "BigEyeBall",
        "",
        "BigEyeNest",
        "",
        "BigEyeTurret",
        "SphinkTick_lod0",
        "SphinkTick_lod0",
        "",
        "",
        "",
        ""
    };

    std::int32_t GetEnemyDeathEffect(EnemyType type)
    {
        const std::size_t index = static_cast<std::size_t>(type);
        return GetAt(EnemyDeathEffects, index);
    }

    const std::vector<std::int32_t> EnemyDeathEffects = {
        193, 221, 219, 219, 219, 219, 219, 76, 76, 76,
        193, 108, 221, 76, 76, 76, 76, 6, 6, 76,
        77, 76, 76, 77, 76, 76, 76, 76, 76, 76,
        77, 76, 76, 76, 77, 77, 77, 220, 222, 0,
        6, 76, 76, 76, 76, 76, 223, 223, 76, 77,
        0, 220
    };

    const std::vector<std::int32_t> EnemyAudioRangeIndices = {
        8, 9, 10, 11, 11, 11, 11, 4, 4, 8,
        8, 12, 9, 4, 4, 4, 13, 18, 18, 15,
        15, 19, 15, 16, 22, 22, 22, 22, 22, 22,
        22, 22, 22, 34, 16, 20, 20, 9, 4, 4,
        24, 25, 25, 25, 25, 25, 30, 30, 4, 4,
        4, 4
    };

    const std::vector<std::int32_t> EnemyScanIds = {
        214, 210, 0, 224, 224, 224, 224, 0, 0, 0,
        215, 213, 211, 0, 0, 0, 212, 219, 219, 0,
        0, 226, 0, 216, 243, 0, 241, 0, 244, 242,
        467, 0, 466, 0, 0, 217, 217, 218, 221, 222,
        0, 227, 227, 227, 227, 227, 220, 245, 0, 0,
        0, 0
    };

    float GetDamageMultiplier(Entities::Effectiveness effectiveness)
    {
        const std::size_t index = static_cast<std::size_t>(effectiveness);
        return GetAt(DamageMultipliers, index);
    }

    const std::vector<float> DamageMultipliers = { 0.0F, 0.5F, 1.0F, 2.0F };

    void LoadEffectiveness(EnemyType type, std::span<Entities::Effectiveness> dest)
    {
        const std::size_t index = static_cast<std::size_t>(type);
        LoadEffectiveness(GetAt(EnemyEffectiveness, index), dest);
    }

    void LoadEffectiveness(std::int32_t value, std::span<Entities::Effectiveness> dest)
    {
        assert(value >= 0);
        LoadEffectiveness(static_cast<std::uint32_t>(value), dest);
    }

    void LoadEffectiveness(std::uint32_t value, std::span<Entities::Effectiveness> dest)
    {
        assert(dest.size() == 9);
        if (dest.size() < 9)
        {
            ThrowIndexOutOfRange();
        }
        dest[0] = static_cast<Entities::Effectiveness>(value & 3U);
        dest[1] = static_cast<Entities::Effectiveness>((value >> 2U) & 3U);
        dest[2] = static_cast<Entities::Effectiveness>((value >> 4U) & 3U);
        dest[3] = static_cast<Entities::Effectiveness>((value >> 6U) & 3U);
        dest[4] = static_cast<Entities::Effectiveness>((value >> 8U) & 3U);
        dest[5] = static_cast<Entities::Effectiveness>((value >> 10U) & 3U);
        dest[6] = static_cast<Entities::Effectiveness>((value >> 12U) & 3U);
        dest[7] = static_cast<Entities::Effectiveness>((value >> 14U) & 3U);
        dest[8] = static_cast<Entities::Effectiveness>((value >> 16U) & 3U);
    }

    const std::vector<std::int32_t> EnemyEffectiveness = {
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAA8,
        0x00000,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x20000,
        0x2AAAA,
        0x2AAAA,
        0x2AABA,
        0x2AABA,
        0x2AAAA,
        0x2EAFA,
        0x24D55,
        0x2AAAA,
        0x2AA99,
        0x2AA99,
        0x2AA99,
        0x2AA99,
        0x2AA99,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA,
        0x2AAAA
    };

    const std::vector<std::int32_t> SlenchEffectiveness = {
        0x2AAAA, 0x2AAAA, 0x155F5, 0x16566
    };

    const std::vector<std::int32_t> SlenchSynapseEffectiveness = {
        0x2AAAA, 0x800, 0x80, 0x2000
    };

    const std::vector<std::int32_t> GoreaEffectiveness = {
        5, 0x81, 0x401, 0x2001, 0x4001, 0x301
    };
}
