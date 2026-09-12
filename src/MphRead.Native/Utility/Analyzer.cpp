#include "Analyzer.hpp"

#include "../Formats/RawFormats.hpp"
#include "../Read.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <new>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    template <typename T>
    T& AssignReadonly(T& target, const T& source) noexcept
    {
        if (std::addressof(target) != std::addressof(source))
        {
            target.~T();
            ::new (static_cast<void*>(std::addressof(target))) T(source);
        }
        return target;
    }

    [[nodiscard]] std::vector<std::uint8_t> FileReadAllBytes(const std::string& path)
    {
        std::ifstream stream(std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!stream)
        {
            throw std::ios_base::failure("Could not open file: " + path);
        }
        const std::streampos end = stream.tellg();
        if (end < 0)
        {
            throw std::ios_base::failure("Could not determine file length: " + path);
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
        stream.seekg(0, std::ios::beg);
        if (!bytes.empty())
        {
            stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            if (!stream)
            {
                throw std::ios_base::failure("Could not read file: " + path);
            }
        }
        return bytes;
    }

    [[nodiscard]] constexpr std::int32_t WrapNegateInt32(std::int32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(
            0U - std::bit_cast<std::uint32_t>(value));
    }

    [[nodiscard]] float RoundToEven3(float value) noexcept
    {
        if (!std::isfinite(value) || value == 0.0F)
        {
            return value;
        }
        if (std::fabs(value) >= 1.0e8F)
        {
            return value;
        }
        constexpr float scale = 1000.0F;
        const float scaled = value * scale;
        const float lower = std::floor(scaled);
        const float fraction = scaled - lower;
        float rounded = lower;
        if (fraction > 0.5F
            || (fraction == 0.5F && std::fmod(lower, 2.0F) != 0.0F))
        {
            rounded = lower + 1.0F;
        }
        if (rounded == 0.0F)
        {
            rounded = std::copysign(0.0F, scaled);
        }
        return rounded / scale;
    }

    [[nodiscard]] std::string FloatString(float value)
    {
        if (std::isnan(value))
        {
            return "NaN";
        }
        if (std::isinf(value))
        {
            return std::signbit(value) ? "-Infinity" : "Infinity";
        }

        std::array<char, 64> buffer{};
        const auto result = std::to_chars(
            buffer.data(), buffer.data() + buffer.size(),
            value, std::chars_format::general);
        if (result.ec != std::errc{})
        {
            std::ostringstream stream;
            stream << value;
            return stream.str();
        }

        std::string text(buffer.data(), result.ptr);
        const std::size_t exponent = text.find('e');
        if (exponent != std::string::npos)
        {
            text[exponent] = 'E';
        }
        return text;
    }

    [[nodiscard]] std::string HexX2(std::uint32_t value)
    {
        std::ostringstream stream;
        stream << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << value;
        return stream.str();
    }

    template <typename TDictionary, typename TKey, typename TValue>
    void DictionaryAdd(TDictionary& dictionary, TKey&& key, TValue&& value)
    {
        if (!dictionary.emplace(
                std::forward<TKey>(key), std::forward<TValue>(value)).second)
        {
            throw std::invalid_argument("An item with the same key has already been added.");
        }
    }
}

namespace MphRead::Utility
{
    const std::uint32_t Analyzer::_fileOffset = 0x02000000U;
    const std::uint32_t Analyzer::_elemList = 0x001242ACU;
    const std::uint32_t Analyzer::_elapsedGlobal = 0x00123E00U;
    const std::uint32_t Analyzer::_viewMatrix = 0x000DA430U;
    const std::uint32_t Analyzer::_effVec1 = 0x00123EECU;
    const std::uint32_t Analyzer::_effVec2 = 0x00123E8CU;
    const std::uint32_t Analyzer::_effVec3 = 0x00123E98U;

    Analyzer::ParticleDefinition& Analyzer::ParticleDefinition::operator=(
        const ParticleDefinition& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    Analyzer::EffectParticle& Analyzer::EffectParticle::operator=(
        const EffectParticle& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    Analyzer::EffectElementEntry& Analyzer::EffectElementEntry::operator=(
        const EffectElementEntry& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    std::size_t Analyzer::EffectElementEntryHash::operator()(
        const EffectElementEntry& value) const noexcept
    {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(std::addressof(value));
        std::size_t hash;
        std::size_t prime;
        if constexpr (sizeof(std::size_t) == 8)
        {
            hash = static_cast<std::size_t>(14695981039346656037ULL);
            prime = static_cast<std::size_t>(1099511628211ULL);
        }
        else
        {
            hash = static_cast<std::size_t>(2166136261U);
            prime = static_cast<std::size_t>(16777619U);
        }
        for (std::size_t index = 0; index < sizeof(value); ++index)
        {
            hash ^= static_cast<std::size_t>(bytes[index]);
            hash *= prime;
        }
        return hash;
    }

    bool Analyzer::EffectElementEntryEqual::operator()(
        const EffectElementEntry& left,
        const EffectElementEntry& right) const noexcept
    {
        return std::memcmp(
            static_cast<const void*>(std::addressof(left)),
            static_cast<const void*>(std::addressof(right)),
            sizeof(EffectElementEntry)) == 0;
    }

    std::shared_ptr<Analyzer::EffectParticleDictionary> Analyzer::ReadThing()
    {
        std::vector<EffectElementEntry> entries;
        std::unordered_map<
            EffectElementEntry,
            RawEffectElement,
            EffectElementEntryHash,
            EffectElementEntryEqual> effElems;
        auto effParts = std::make_shared<EffectParticleDictionary>();

        const std::vector<std::uint8_t> matrixStorage
            = FileReadAllBytes(R"(D:\Cdrv\MPH\Disassembly\mtx.bin)");
        const std::span<const std::uint8_t> matrixBytes(matrixStorage);
        const Matrix43Fx viewMatrix = Read::DoOffset<Matrix43Fx>(matrixBytes, _viewMatrix);
        const Vector3Fx effVec1 = Read::DoOffset<Vector3Fx>(matrixBytes, _effVec1);
        const Vector3Fx effVec2 = Read::DoOffset<Vector3Fx>(matrixBytes, _effVec2);
        [[maybe_unused]] const Vector3Fx effVec3
            = Read::DoOffset<Vector3Fx>(matrixBytes, _effVec3);

        assert(effVec1.X.Value == viewMatrix.One.X.Value);
        assert(effVec1.Y.Value == viewMatrix.Two.X.Value);
        assert(effVec1.Z.Value == viewMatrix.Three.X.Value);
        assert(effVec2.X.Value == WrapNegateInt32(viewMatrix.One.Y.Value));
        assert(effVec2.Y.Value == WrapNegateInt32(viewMatrix.Two.Y.Value));
        assert(effVec2.Z.Value == WrapNegateInt32(viewMatrix.Three.Y.Value));

        const std::vector<std::uint8_t> storage
            = FileReadAllBytes(R"(D:\Cdrv\MPH\Disassembly\dump.bin)");
        const std::span<const std::uint8_t> bytes(storage);
        const std::uint32_t elapsed = Read::SpanReadUint(bytes, _elapsedGlobal);
        const float time = static_cast<float>(elapsed) / 4096.0F;
        std::cout << FloatString(RoundToEven3(time)) << '\n';

        const EffectElementEntry head = Read::DoOffset<EffectElementEntry>(bytes, _elemList);
        std::uint32_t offset = static_cast<std::uint32_t>(head.Prev) - _fileOffset;
        while (offset != _elemList)
        {
            const EffectElementEntry entry = Read::DoOffset<EffectElementEntry>(bytes, offset);
            const std::uint32_t elemOffset = static_cast<std::uint32_t>(entry.Element) - _fileOffset;
            const RawEffectElement elem = Read::DoOffset<RawEffectElement>(bytes, elemOffset);
            std::uint32_t partOffset
                = static_cast<std::uint32_t>(entry.ParticleHead.Prev) - _fileOffset;

            entries.push_back(entry);
            DictionaryAdd(effElems, entry, elem);
            DictionaryAdd(
                *effParts, entry,
                std::make_shared<std::vector<EffectParticle>>());

            std::vector<EffectParticle> particles;
            while (partOffset != offset + 0x1CCU)
            {
                const EffectParticle part = Read::DoOffset<EffectParticle>(bytes, partOffset);
                particles.push_back(part);
                effParts->at(entry)->push_back(part);
                partOffset = static_cast<std::uint32_t>(part.Prev) - _fileOffset;
                Nop();
            }

            const float creation = RoundToEven3(
                static_cast<float>(entry.CreationTime / 4096));
            const float expiration = RoundToEven3(
                static_cast<float>(entry.ExpirationTime / 4096));
            std::cout << "0x" << HexX2(offset) << ' '
                << FloatString(creation) << " - " << FloatString(expiration)
                << " x" << particles.size() << " (" << elem.NameString() << ")\n";

            std::cout << " -- ";
            for (std::size_t index = 0; index < particles.size(); ++index)
            {
                if (index != 0)
                {
                    std::cout << ", ";
                }
                std::cout << FloatString(RoundToEven3(particles[index].Rotation.FloatValue()));
            }
            std::cout << '\n';

            offset = static_cast<std::uint32_t>(entry.Prev) - _fileOffset;
        }

        std::cout << '\n';
        for (const EffectElementEntry& entry : entries)
        {
            std::cout << FloatString(entry.Position.X.FloatValue()) << ", "
                << FloatString(entry.Position.Y.FloatValue()) << ", "
                << FloatString(entry.Position.Z.FloatValue()) << '\n';
            std::cout << '\n';

            const std::shared_ptr<std::vector<EffectParticle>>& particles = effParts->at(entry);
            for (const EffectParticle& particle : *particles)
            {
                const float creation = particle.CreationTime.FloatValue();
                const float expiration = particle.ExpirationTime.FloatValue();
                const float age = time - creation;
                const float lifespan = expiration - creation;
                const float percent = age / lifespan;
                std::cout << FloatString(RoundToEven3(percent * 100.0F)) << "%\n";
                std::cout << FloatString(particle.Position.X.FloatValue()) << ", "
                    << FloatString(particle.Position.Y.FloatValue()) << ", "
                    << FloatString(particle.Position.Z.FloatValue()) << '\n';
                std::cout << FloatString(particle.Scale.FloatValue()) << '\n';
                std::cout << FloatString(particle.Rotation.FloatValue()) << '\n';
                std::cout << '\n';
            }
            std::cout << "-----------------------------------\n";
            std::cout << '\n';
        }

        Nop();
        return effParts;
    }

    void Analyzer::Nop() noexcept
    {
    }
}
