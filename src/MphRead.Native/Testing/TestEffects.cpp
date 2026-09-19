#include "TestEffects.hpp"

#include "../Entities/ObjectEntity.hpp"
#include "../Formats/Effects.hpp"
#include "../Formats/Formats.hpp"
#include "../Formats/Model.hpp"
#include "../Metadata/Metadata.hpp"
#include "../Metadata/Rooms.hpp"
#include "../Read.hpp"
#include "../Utility/Rng.hpp"

#include <bit>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <numbers>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    [[nodiscard]] constexpr std::int32_t ManagedInt32(std::uint32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(value);
    }

    [[nodiscard]] constexpr std::uint32_t ManagedUInt32(std::int32_t value) noexcept
    {
        return std::bit_cast<std::uint32_t>(value);
    }

    [[nodiscard]] constexpr std::int32_t AddInt32(
        std::int32_t left, std::int32_t right) noexcept
    {
        return ManagedInt32(ManagedUInt32(left) + ManagedUInt32(right));
    }

    [[nodiscard]] constexpr std::int32_t SubtractInt32(
        std::int32_t left, std::int32_t right) noexcept
    {
        return ManagedInt32(ManagedUInt32(left) - ManagedUInt32(right));
    }

    [[nodiscard]] constexpr std::int32_t MultiplyInt32(
        std::int32_t left, std::int32_t right) noexcept
    {
        return ManagedInt32(ManagedUInt32(left) * ManagedUInt32(right));
    }

    [[nodiscard]] constexpr std::int32_t ShiftRightInt32(
        std::int32_t value, unsigned count) noexcept
    {
        const std::uint32_t bits = ManagedUInt32(value);
        if (value >= 0)
        {
            return ManagedInt32(bits >> count);
        }
        return ManagedInt32((bits >> count) | (~std::uint32_t{0} << (32U - count)));
    }

    [[nodiscard]] constexpr std::int64_t ShiftRightInt64(
        std::int64_t value, unsigned count) noexcept
    {
        const std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
        if (value >= 0)
        {
            return std::bit_cast<std::int64_t>(bits >> count);
        }
        return std::bit_cast<std::int64_t>(
            (bits >> count) | (~std::uint64_t{0} << (64U - count)));
    }

    [[nodiscard]] const std::int32_t& At(
        const std::vector<std::int32_t>& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= values.size())
        {
            throw std::out_of_range(
                "Index was out of range. Must be non-negative and less than the size of the collection. (Parameter 'index')");
        }
        return values[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] constexpr float DegreesToRadians(float degrees) noexcept
    {
        return degrees * (std::numbers::pi_v<float> / 180.0F);
    }

    [[nodiscard]] std::string FormatSingle(float value)
    {
        if (std::isnan(value))
        {
            return "NaN";
        }
        if (std::isinf(value))
        {
            return std::signbit(value) ? "-Infinity" : "Infinity";
        }

        char buffer[64]{};
        const auto converted = std::to_chars(
            buffer, buffer + sizeof(buffer), value, std::chars_format::general);
        if (converted.ec != std::errc{})
        {
            throw std::runtime_error("Failed to format Single value.");
        }
        std::string result(buffer, converted.ptr);
        const std::size_t exponent = result.find('e');
        if (exponent != std::string::npos)
        {
            result[exponent] = 'E';
        }
        return result;
    }

    [[nodiscard]] constexpr bool TestFlag(
        MphRead::Entities::ObjEffFlags value,
        MphRead::Entities::ObjEffFlags flag) noexcept
    {
        return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0U;
    }

    [[nodiscard]] const char* VecsName(std::int32_t id) noexcept
    {
        switch (id)
        {
        case 1:
            return "B0";
        case 2:
            return "BC";
        case 3:
            return "C0";
        case 4:
            return "D4";
        case 5:
            return "D8";
        default:
            return "XX";
        }
    }

    [[nodiscard]] const char* DrawName(std::int32_t id) noexcept
    {
        switch (id)
        {
        case 1:
            return "B4";
        case 2:
            return "B8";
        case 3:
            return "C4";
        case 4:
            return "C8";
        case 5:
            return "CC";
        case 6:
            return "D0";
        case 7:
            return "DC";
        default:
            return "XX";
        }
    }

    void WriteJoined(const std::vector<std::string>& values)
    {
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            if (index != 0)
            {
                std::cout << ", ";
            }
            std::cout << values[index];
        }
        std::cout << '\n';
    }
}

namespace MphRead::Testing
{
    class TestEffects::RandomState final
    {
    public:
        RandomState()
            : _engine(MakeSeed())
        {
        }

        [[nodiscard]] std::int32_t Next(std::int32_t maxValue)
        {
            if (maxValue < 0)
            {
                throw std::out_of_range("'maxValue' must be greater than zero.");
            }
            if (maxValue == 0)
            {
                return 0;
            }
            std::uniform_int_distribution<std::int32_t> distribution(0, maxValue - 1);
            return distribution(_engine);
        }

    private:
        [[nodiscard]] static std::mt19937::result_type MakeSeed()
        {
            std::random_device randomDevice;
            return randomDevice();
        }

        std::mt19937 _engine;
    };

    const std::shared_ptr<TestEffects::RandomState> TestEffects::_random
        = std::make_shared<TestEffects::RandomState>();

    void TestEffects::TestEffectMath()
    {
        while (true)
        {
            const std::uint32_t v5 = Rng::GetRandomInt1(0x168000);
            const std::int64_t scaled = 0xB60B60B60BLL * static_cast<std::int64_t>(v5);
            const std::int64_t high = (scaled >> 32) + 2048;
            const std::int32_t index1 = static_cast<std::int32_t>(2 * ((16 * high) >> 20));
            const std::int32_t index2 = static_cast<std::int32_t>(2 * ((16 * high) >> 20) + 1);
            const std::int32_t index3 = static_cast<std::int32_t>(
                2 * ((((high >> 12) + 0x4000) >> 4)));
            const std::int32_t index4 = static_cast<std::int32_t>(
                2 * ((((high >> 12) + 0x4000) >> 4)) + 1);
            const float angle1 = static_cast<float>(index1 / 2) * (360.0F / 4096.0F);
            const float angle2 = static_cast<float>(index3 / 2) * (360.0F / 4096.0F);
            assert(index1 % 2 == 0);
            assert(index3 % 2 == 0);
            assert(index1 + 1 == index2);
            assert(index3 + 1 == index4);
            assert(index1 + 2048 == index3);
            assert(angle1 >= 0.0F && angle1 < 360.0F);
            assert(angle2 >= 0.0F && angle2 < 360.0F);
            float test = angle1 + 90.0F;
            if (test >= 360.0F)
            {
                test -= 360.0F;
            }
            assert(test == angle2);
            Nop();
        }
    }

    void TestEffects::TestEffectMathFloat()
    {
        const float angle = static_cast<float>(_random->Next(0x168000)) / 4096.0F;
        const float angle1 = DegreesToRadians(angle);
        const float angle2 = DegreesToRadians(angle + 90.0F);
        [[maybe_unused]] const float cos1 = std::cos(angle1);
        [[maybe_unused]] const float sin1 = std::sin(angle1);
        [[maybe_unused]] const float cos2 = std::cos(angle2);
        [[maybe_unused]] const float sin2 = std::sin(angle2);
    }

    void TestEffects::TestAllEffects()
    {
        for (const auto& [name, archive] : Metadata::Effects)
        {
            if (!name.empty() && name != "sparksFall" && name != "mortarSecondary"
                && name != "powerBeamChargeNoSplatMP")
            {
                const std::shared_ptr<Effect> effect
                    = Read::LoadEffectByName(-1, name, archive, false);
                for (const std::shared_ptr<EffectElement>& element : *effect->Elements)
                {
                    (void)element;
                }
            }
        }
        Nop();
    }

    void TestEffects::TestEffectBases()
    {
        const std::vector<std::string> names{"deathParticle", "geo1", "particles", "particles2"};
        for (const std::string& name : names)
        {
            const std::shared_ptr<Model> model = Read::GetModelInstance(name)->Model();
            for (const std::shared_ptr<Material>& material : *model->Materials)
            {
                if (material->XRepeat == RepeatMode::Mirror || material->YRepeat == RepeatMode::Mirror)
                {
                    std::cout << name << " - " << material->Name << " ("
                              << material->TextureId << ", " << material->PaletteId << ")\n";
                }
                if (material->XRepeat == RepeatMode::Mirror)
                {
                    std::cout << "S: " << FormatSingle(material->ScaleS) << '\n';
                }
                if (material->YRepeat == RepeatMode::Mirror)
                {
                    std::cout << "T: " << FormatSingle(material->ScaleT) << '\n';
                }
                if (material->XRepeat == RepeatMode::Mirror || material->YRepeat == RepeatMode::Mirror)
                {
                    std::cout << '\n';
                }
            }
        }
        Nop();
    }

    std::int32_t TestEffects::FxDiv(std::int32_t a, std::int32_t b)
    {
        if (b == 0)
        {
            throw std::domain_error("Attempted to divide by zero.");
        }
        const std::int64_t numerator = static_cast<std::int64_t>(a) * 4096;
        const std::int64_t quotient = numerator / static_cast<std::int64_t>(b);
        return ManagedInt32(static_cast<std::uint32_t>(quotient));
    }

    std::int32_t TestEffects::TestFx41(
        const std::vector<std::int32_t>& parameters, std::int32_t percent)
    {
        std::int32_t result;
        std::int32_t next;
        std::int32_t index1 = -1;
        std::int32_t index2 = 0;
        if (percent < At(parameters, AddInt32(index2, 0)))
        {
            return At(parameters, AddInt32(index2, 1));
        }
        if (At(parameters, AddInt32(index2, 0)) != std::numeric_limits<std::int32_t>::min())
        {
            do
            {
                if (At(parameters, AddInt32(index2, 0)) > percent)
                {
                    break;
                }
                index1 = index2;
                next = At(parameters, AddInt32(index2, 2));
                index2 = AddInt32(index2, 2);
            }
            while (next != std::numeric_limits<std::int32_t>::min());
        }
        if (index1 == -1)
        {
            return 0;
        }
        const std::int32_t v7 = At(parameters, AddInt32(index1, 2));
        if (v7 == std::numeric_limits<std::int32_t>::min())
        {
            result = At(parameters, AddInt32(index1, 1));
        }
        else
        {
            const std::int32_t valueDelta = SubtractInt32(
                At(parameters, AddInt32(index1, 3)),
                At(parameters, AddInt32(index1, 1)));
            const std::int32_t percentDelta = SubtractInt32(
                percent, At(parameters, AddInt32(index1, 0)));
            const std::int32_t pointDelta = SubtractInt32(
                v7, At(parameters, AddInt32(index1, 0)));
            const std::int32_t fraction = FxDiv(percentDelta, pointDelta);
            const std::int64_t wide = static_cast<std::int64_t>(valueDelta)
                * static_cast<std::int64_t>(fraction) + 2048;
            const std::int32_t interpolated = ManagedInt32(
                static_cast<std::uint32_t>(ShiftRightInt64(wide, 12)));
            result = AddInt32(At(parameters, AddInt32(index1, 1)), interpolated);

            const std::int32_t left = SubtractInt32(
                At(parameters, AddInt32(index1, 3)),
                At(parameters, AddInt32(index1, 1)));
            const std::int32_t right = FxDiv(
                SubtractInt32(percent, At(parameters, AddInt32(index1, 0))),
                SubtractInt32(v7, At(parameters, AddInt32(index1, 0))));
            const std::int32_t prod = ShiftRightInt32(
                AddInt32(MultiplyInt32(left, right), 2048), 12);
            const std::int32_t parm = At(parameters, AddInt32(index1, 1));
            [[maybe_unused]] const std::int32_t final = AddInt32(parm, prod);
            Nop();
        }
        return result;
    }

    void TestEffects::TestEntityEffects()
    {
        std::unordered_map<std::int32_t, std::shared_ptr<Effect>> effects;
        for (std::int32_t index = 0;
            index < static_cast<std::int32_t>(Metadata::Effects.size());
            index = AddInt32(index, 1))
        {
            const auto& [name, archive] = Metadata::Effects[static_cast<std::size_t>(index)];
            if (!name.empty() && name != "sparksFall" && name != "mortarSecondary"
                && name != "powerBeamChargeNoSplatMP")
            {
                effects.emplace(index, Read::LoadEffectByName(-1, name, archive, false));
            }
        }
        for (const auto& meta : Metadata::RoomMetadata)
        {
            bool printed = false;
            const std::shared_ptr<RoomMetadata>& room = meta.second;
            if (room->EntityPath.has_value())
            {
                const std::shared_ptr<const std::vector<std::shared_ptr<Entity>>> entities
                    = Read::GetEntities(*room->EntityPath, -1, room->FirstHunt);
                for (const std::shared_ptr<Entity>& entity : *entities)
                {
                    if (entity->Type == EntityType::Object)
                    {
                        const auto& objectEntity
                            = dynamic_cast<const EntityOf<ObjectEntityData>&>(*entity);
                        const ObjectEntityData data = objectEntity.Data;
                        if (data.EffectId > 0)
                        {
                            if (!printed)
                            {
                                std::cout << "--------------------------------------------------------------------------------------\n";
                                std::cout << '\n';
                                std::cout << meta.first << " (" << room->InGameName.value_or(std::string{}) << ")\n";
                                std::cout << '\n';
                                printed = true;
                            }
                            const std::shared_ptr<Effect>& effect = effects.at(data.EffectId);
                            std::cout << "[ ] Entity " << entity->EntityId << ", Effect "
                                      << data.EffectId << " (" << effect->Name << ")\n";
                            std::vector<std::string> elems;
                            for (const std::shared_ptr<EffectElement>& element : *effect->Elements)
                            {
                                const auto [setVecsId, drawId]
                                    = Effects::EffectFuncBase::GetFuncIds(element->Flags, element->DrawType);
                                std::string value = element->Name;
                                value.append(" v:");
                                value.append(VecsName(setVecsId));
                                value.append(" d:");
                                value.append(DrawName(drawId));
                                elems.push_back(std::move(value));
                            }
                            WriteJoined(elems);
                            std::cout << "Spawns: ";
                            if (TestFlag(data.EffectFlags, Entities::ObjEffFlags::AlwaysUpdateEffect))
                            {
                                std::cout << "Always\n";
                            }
                            else if (TestFlag(data.EffectFlags, Entities::ObjEffFlags::UseEffectVolume))
                            {
                                std::cout << "Volume\n";
                            }
                            else
                            {
                                std::cout << "Anim ID\n";
                            }
                            std::cout << "Attach: "
                                      << (TestFlag(data.EffectFlags, Entities::ObjEffFlags::AttachEffect)
                                          ? "Yes" : "No")
                                      << '\n';
                            std::cout << "Linked: ";
                            if (data.LinkedEntity != -1)
                            {
                                std::cout << data.LinkedEntity << '\n';
                            }
                            else
                            {
                                std::cout << "No\n";
                            }
                            std::cout << '\n';
                        }
                    }
                }
            }
        }
        Nop();
    }

    void TestEffects::Nop() noexcept
    {
    }
}
