#pragma once

#include "../Formats/Types.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace MphRead::Utility
{
    class Analyzer final
    {
    public:
        struct ParticleDefinition
        {
            const std::int32_t Model = 0;
            const std::int32_t Node = 0;
            const std::int32_t MaterialId = 0;

            constexpr ParticleDefinition() noexcept = default;
            ParticleDefinition(const ParticleDefinition&) noexcept = default;
            ParticleDefinition& operator=(const ParticleDefinition& other) noexcept;
        };

        struct EffectParticle
        {
            const Fixed CreationTime{};
            const Fixed ExpirationTime{};
            const Fixed Lifespan{};
            const Vector3Fx Position{};
            const Vector3Fx Speed{};
            const Fixed Scale{};
            const Fixed Rotation{};
            const Fixed Red{};
            const Fixed Green{};
            const Fixed Blue{};
            const Fixed Alpha{};
            const std::int32_t ParticleId = 0;
            const Fixed PortionTotal{};
            const std::int32_t RoField1 = 0;
            const std::int32_t RoField2 = 0;
            const std::int32_t RoField3 = 0;
            const std::int32_t RoField4 = 0;
            const std::int32_t RwField1 = 0;
            const std::int32_t RwField2 = 0;
            const std::int32_t RwField3 = 0;
            const std::int32_t RwField4 = 0;
            const std::int32_t Func180 = 0;
            const std::int32_t Func188 = 0;
            const std::int32_t Func18C = 0;
            const std::int32_t Func190 = 0;
            const std::int32_t Func194 = 0;
            const std::int32_t Func198 = 0;
            const std::int32_t Func19C = 0;
            const std::int32_t Func1B0 = 0;
            const std::int32_t Func1B4 = 0;
            const std::int32_t Func1B8 = 0;
            const std::int32_t Func1BC = 0;
            const std::int32_t Prev = 0;
            const std::int32_t Next = 0;

            constexpr EffectParticle() noexcept = default;
            EffectParticle(const EffectParticle&) noexcept = default;
            EffectParticle& operator=(const EffectParticle& other) noexcept;
        };

        struct EffectElementEntry
        {
            const std::int32_t EffectEntry = 0;
            const std::int32_t EffectId = 0;
            const std::int32_t MatrixPointer = 0;
            const Matrix43Fx Transform{};
            const std::int32_t State = 0;
            const std::int32_t Element = 0;
            const std::int32_t CreationTime = 0;
            const std::int32_t ExpirationTime = 0;
            const std::int32_t DrainTime = 0;
            const std::int32_t BufferTime = 0;
            const std::int32_t Func39Called = 0;
            const std::int32_t Field58 = 0;
            const std::int32_t Field5C = 0;
            const std::int32_t Field60 = 0;
            const std::int32_t Field64 = 0;
            const std::int32_t Flags = 0;
            const std::int32_t ChildEffect = 0;
            const std::int32_t ParticleAmount = 0;
            const std::int32_t Lifespan = 0;
            const std::int32_t DrawType = 0;
            const std::int32_t ParticleCount = 0;
            const ParticleDefinition Particle00{};
            const ParticleDefinition Particle01{};
            const ParticleDefinition Particle02{};
            const ParticleDefinition Particle03{};
            const ParticleDefinition Particle04{};
            const ParticleDefinition Particle05{};
            const ParticleDefinition Particle06{};
            const ParticleDefinition Particle07{};
            const ParticleDefinition Particle08{};
            const ParticleDefinition Particle09{};
            const ParticleDefinition Particle10{};
            const ParticleDefinition Particle11{};
            const ParticleDefinition Particle12{};
            const ParticleDefinition Particle13{};
            const ParticleDefinition Particle14{};
            const ParticleDefinition Particle15{};
            const Vector3Fx Position{};
            const Vector3Fx Vector1{};
            const Vector3Fx Vector2{};
            const Vector3Fx Acceleration{};
            const std::int32_t Func170 = 0;
            const std::int32_t Func174 = 0;
            const std::int32_t Func178 = 0;
            const std::int32_t Func17C = 0;
            const std::int32_t Func180 = 0;
            const std::int32_t Func184 = 0;
            const std::int32_t Func188 = 0;
            const std::int32_t Func18C = 0;
            const std::int32_t Func190 = 0;
            const std::int32_t Func194 = 0;
            const std::int32_t Func198 = 0;
            const std::int32_t Func19C = 0;
            const std::int32_t Func1A0 = 0;
            const std::int32_t Func1A4 = 0;
            const std::int32_t Func1A8 = 0;
            const std::int32_t Func1AC = 0;
            const std::int32_t Func1B0 = 0;
            const std::int32_t Func1B4 = 0;
            const std::int32_t Func1B8 = 0;
            const std::int32_t Func1BC = 0;
            const std::int32_t SetVectors = 0;
            const std::int32_t Draw = 0;
            const std::int32_t ParticleFreePointer = 0;
            const EffectParticle ParticleHead{};
            const std::int32_t Prev = 0;
            const std::int32_t Next = 0;

            constexpr EffectElementEntry() noexcept = default;
            EffectElementEntry(const EffectElementEntry&) noexcept = default;
            EffectElementEntry& operator=(const EffectElementEntry& other) noexcept;
        };

    private:
        struct EffectElementEntryHash
        {
            [[nodiscard]] std::size_t operator()(const EffectElementEntry& value) const noexcept;
        };

        struct EffectElementEntryEqual
        {
            [[nodiscard]] bool operator()(
                const EffectElementEntry& left,
                const EffectElementEntry& right) const noexcept;
        };

    public:
        using EffectParticleDictionary = std::unordered_map<
            EffectElementEntry,
            std::shared_ptr<std::vector<EffectParticle>>,
            EffectElementEntryHash,
            EffectElementEntryEqual>;

        [[nodiscard]] static std::shared_ptr<EffectParticleDictionary> ReadThing();

        Analyzer() = delete;
        Analyzer(const Analyzer&) = delete;
        Analyzer& operator=(const Analyzer&) = delete;

    private:
        static const std::uint32_t _fileOffset;
        static const std::uint32_t _elemList;
        static const std::uint32_t _elapsedGlobal;
        static const std::uint32_t _viewMatrix;
        static const std::uint32_t _effVec1;
        static const std::uint32_t _effVec2;
        static const std::uint32_t _effVec3;

        static void Nop() noexcept;
    };

    static_assert(sizeof(Fixed) == 4);
    static_assert(sizeof(Vector3Fx) == 12);
    static_assert(sizeof(Matrix43Fx) == 48);

    static_assert(std::is_standard_layout_v<Analyzer::ParticleDefinition>);
    static_assert(sizeof(Analyzer::ParticleDefinition) == 12);
    static_assert(offsetof(Analyzer::ParticleDefinition, Model) == 0);
    static_assert(offsetof(Analyzer::ParticleDefinition, Node) == 4);
    static_assert(offsetof(Analyzer::ParticleDefinition, MaterialId) == 8);

    static_assert(std::is_standard_layout_v<Analyzer::EffectParticle>);
    static_assert(sizeof(Analyzer::EffectParticle) == 152);
    static_assert(offsetof(Analyzer::EffectParticle, CreationTime) == 0);
    static_assert(offsetof(Analyzer::EffectParticle, Position) == 12);
    static_assert(offsetof(Analyzer::EffectParticle, Speed) == 24);
    static_assert(offsetof(Analyzer::EffectParticle, ParticleId) == 60);
    static_assert(offsetof(Analyzer::EffectParticle, PortionTotal) == 64);
    static_assert(offsetof(Analyzer::EffectParticle, Func180) == 100);
    static_assert(offsetof(Analyzer::EffectParticle, Prev) == 144);
    static_assert(offsetof(Analyzer::EffectParticle, Next) == 148);

    static_assert(std::is_standard_layout_v<Analyzer::EffectElementEntry>);
    static_assert(sizeof(Analyzer::EffectElementEntry) == 620);
    static_assert(offsetof(Analyzer::EffectElementEntry, EffectEntry) == 0);
    static_assert(offsetof(Analyzer::EffectElementEntry, Transform) == 12);
    static_assert(offsetof(Analyzer::EffectElementEntry, State) == 60);
    static_assert(offsetof(Analyzer::EffectElementEntry, Particle00) == 128);
    static_assert(offsetof(Analyzer::EffectElementEntry, Particle15) == 308);
    static_assert(offsetof(Analyzer::EffectElementEntry, Position) == 320);
    static_assert(offsetof(Analyzer::EffectElementEntry, Func170) == 368);
    static_assert(offsetof(Analyzer::EffectElementEntry, SetVectors) == 448);
    static_assert(offsetof(Analyzer::EffectElementEntry, ParticleFreePointer) == 456);
    static_assert(offsetof(Analyzer::EffectElementEntry, ParticleHead) == 0x1CC);
    static_assert(offsetof(Analyzer::EffectElementEntry, Prev) == 612);
    static_assert(offsetof(Analyzer::EffectElementEntry, Next) == 616);
}
