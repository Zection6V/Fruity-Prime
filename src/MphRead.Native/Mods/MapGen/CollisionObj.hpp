#pragma once

#include "../../Formats/Enums.hpp"
#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace MphRead::Mods::MapGen
{
    class BuiltFace;

    // A room's collision read from a Wavefront OBJ. A material named
    // <terrain>[_attribute...] carries everything the format holds per face,
    // and winding is what says which side blocks.
    class CollisionObj final
    {
    public:
        CollisionObj() = delete;

        struct Result
        {
            std::vector<BuiltFace*> Faces{};
            std::int32_t Degenerate = 0;
            std::int32_t Vertices = 0;
            // Dictionary<string, int>, in the order the materials were met.
            std::vector<std::pair<std::string, std::int32_t>> Materials{};
        };

        [[nodiscard]] static Result Read(const std::string& path, bool zUp);
        [[nodiscard]] static Result Read(std::span<const std::uint8_t> bytes, const std::string& name, bool zUp);
        [[nodiscard]] static OpenTK::Mathematics::Vector3 Newell(std::span<const OpenTK::Mathematics::Vector3> points);
        [[nodiscard]] static OpenTK::Mathematics::Vector3 TwiceArea(std::span<const OpenTK::Mathematics::Vector3> points);

        static const std::array<Terrain, 11> Terrains;
        static const std::array<const char*, 9> Attributes;

        [[nodiscard]] static std::string MaterialName(const BuiltFace& face);

    private:
        struct Surface
        {
            MphRead::Terrain Terrain = MphRead::Terrain::Metal;
            std::int32_t Slipperiness = 0;
            bool Damaging = false;
            bool ReflectBeams = false;
            bool IgnorePlayers = false;
            bool IgnoreBeams = false;
            bool IgnoreScan = false;

            [[nodiscard]] static Surface Parse(const std::string& material, const std::string& file, std::int32_t line);

        private:
            [[nodiscard]] static Surface Apply(Surface surface, const std::string& word, const std::string& material,
                const std::string& file, std::int32_t line);
        };

        static constexpr float FixedOne = 4096;

        [[nodiscard]] static OpenTK::Mathematics::Vector3 Place(OpenTK::Mathematics::Vector3 point, bool zUp) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector3 Snap(OpenTK::Mathematics::Vector3 point) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector3 Vertex(const std::vector<std::string>& parts,
            const std::string& name, std::int32_t line);
        [[nodiscard]] static float Number(const std::string& text, const std::string& name, std::int32_t line);
        static void AddFace(Result& result, const std::vector<OpenTK::Mathematics::Vector3>& points,
            const std::vector<std::string>& parts, const Surface& surface, const std::string& materialName,
            const std::string& name, std::int32_t line);
        [[nodiscard]] static std::array<double, 3> Sum(std::span<const OpenTK::Mathematics::Vector3> points);
    };
}
