#include "CollisionObj.hpp"

#include "BuiltMap.hpp"
#include "../../Formats/Types.hpp"
#include "../../Program.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/IO.hpp"
#include "../../NativeRuntime/System/Number.hpp"

#include <algorithm>
#include <cmath>

namespace MphRead::Mods::MapGen
{
    namespace Runtime = ::MphRead::NativeRuntime;
    using OpenTK::Mathematics::Vector2;
    using OpenTK::Mathematics::Vector3;

    namespace
    {
        // StreamReader.ReadLine over bytes read as UTF-8: a leading BOM
        // dropped, and \n, \r and \r\n all end a line.
        [[nodiscard]] std::vector<std::string> ReadLines(std::span<const std::uint8_t> bytes)
        {
            std::string text(bytes.begin(), bytes.end());
            if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF
                && static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF)
            {
                text.erase(0, 3);
            }
            std::vector<std::string> lines;
            std::string current;
            bool pending = false;
            for (std::size_t i = 0; i < text.size(); i++)
            {
                const char c = text[i];
                if (c == '\r' || c == '\n')
                {
                    lines.push_back(current);
                    current.clear();
                    pending = false;
                    if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
                    {
                        i++;
                    }
                    continue;
                }
                current += c;
                pending = true;
            }
            if (pending)
            {
                lines.push_back(current);
            }
            return lines;
        }

        // String.Split(null, RemoveEmptyEntries): any white space separates.
        [[nodiscard]] std::vector<std::string> Words(const std::string& text)
        {
            std::vector<std::string> words;
            std::string current;
            for (const char c : text)
            {
                if (c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r' || c == '\n')
                {
                    if (!current.empty())
                    {
                        words.push_back(current);
                        current.clear();
                    }
                    continue;
                }
                current += c;
            }
            if (!current.empty())
            {
                words.push_back(current);
            }
            return words;
        }

        [[nodiscard]] std::string TerrainNames()
        {
            std::string names;
            for (std::size_t i = 0; i < CollisionObj::Terrains.size(); i++)
            {
                names += (i == 0 ? "" : ", ") + Runtime::ToLowerInvariant(ToString(CollisionObj::Terrains[i]));
            }
            return names;
        }
    }

    const std::array<Terrain, 11> CollisionObj::Terrains{
        Terrain::Metal, Terrain::OrangeHolo, Terrain::GreenHolo, Terrain::BlueHolo,
        Terrain::Ice, Terrain::Snow, Terrain::Sand, Terrain::Rock,
        Terrain::Lava, Terrain::Acid, Terrain::Gorea};

    const std::array<const char*, 9> CollisionObj::Attributes{
        "slip0", "slip1", "slip2", "slip3", "damaging", "reflect", "noplayers", "nobeams", "noscan"};

    CollisionObj::Result CollisionObj::Read(const std::string& path, bool zUp)
    {
        const std::vector<std::uint8_t> bytes = Runtime::FileReadAllBytes(path);
        return Read(bytes, Runtime::PathGetFileName(path), zUp);
    }

    CollisionObj::Result CollisionObj::Read(std::span<const std::uint8_t> bytes, const std::string& name, bool zUp)
    {
        std::vector<Vector3> points;
        Result result{};
        Surface surface{};
        std::string materialName = "(none)";
        std::int32_t line = 0;
        for (const std::string& text : ReadLines(bytes))
        {
            line++;
            const std::vector<std::string> parts = Words(text);
            if (parts.empty() || parts[0].starts_with('#'))
            {
                continue;
            }
            if (parts[0] == "v")
            {
                points.push_back(Snap(Place(Vertex(parts, name, line), zUp)));
            }
            else if (parts[0] == "usemtl")
            {
                materialName = parts.size() > 1 ? parts[1] : "(none)";
                surface = Surface::Parse(materialName, name, line);
            }
            else if (parts[0] == "f")
            {
                AddFace(result, points, parts, surface, materialName, name, line);
            }
        }
        result.Vertices = static_cast<std::int32_t>(points.size());
        if (result.Faces.empty())
        {
            throw ProgramException(name + " has no usable collision faces in it. A face needs three points that "
                "are not all in one line; check that the mesh was exported as faces rather "
                "than as loose vertices or edges.");
        }
        return result;
    }

    Vector3 CollisionObj::Place(Vector3 point, bool zUp) noexcept
    {
        return zUp ? Vector3(point.X, point.Z, -point.Y) : point;
    }

    Vector3 CollisionObj::Snap(Vector3 point) noexcept
    {
        return Vector3(std::nearbyint(point.X * FixedOne) / FixedOne, std::nearbyint(point.Y * FixedOne) / FixedOne,
            std::nearbyint(point.Z * FixedOne) / FixedOne);
    }

    Vector3 CollisionObj::Vertex(const std::vector<std::string>& parts, const std::string& name, std::int32_t line)
    {
        if (parts.size() < 4)
        {
            throw ProgramException(name + " line " + std::to_string(line) + ": a vertex needs three numbers.");
        }
        return Vector3(Number(parts[1], name, line), Number(parts[2], name, line), Number(parts[3], name, line));
    }

    float CollisionObj::Number(const std::string& text, const std::string& name, std::int32_t line)
    {
        float value = 0;
        if (!Runtime::SingleTryParseInvariant(text, value))
        {
            throw ProgramException(name + " line " + std::to_string(line) + ": " + text + " is not a number.");
        }
        return value;
    }

    void CollisionObj::AddFace(Result& result, const std::vector<Vector3>& points, const std::vector<std::string>& parts,
        const Surface& surface, const std::string& materialName, const std::string& name, std::int32_t line)
    {
        std::vector<Vector3> corners;
        for (std::size_t i = 1; i < parts.size(); i++)
        {
            std::string field = parts[i];
            const std::size_t slash = field.find('/');
            if (slash != std::string::npos)
            {
                field = field.substr(0, slash);
            }
            std::int32_t index = 0;
            if (!Runtime::Int32TryParseInvariant(field, index) || index == 0)
            {
                throw ProgramException(name + " line " + std::to_string(line) + ": " + parts[i] + " is not a vertex reference.");
            }
            const std::int32_t resolved = index > 0 ? index - 1 : static_cast<std::int32_t>(points.size()) + index;
            if (resolved < 0 || resolved >= static_cast<std::int32_t>(points.size()))
            {
                throw ProgramException(name + " line " + std::to_string(line) + ": vertex " + std::to_string(index)
                    + " is not one of the " + std::to_string(points.size()) + " declared before it.");
            }
            const Vector3 point = points[static_cast<std::size_t>(resolved)];
            if (corners.empty() || !OpenTK::Mathematics::Equal(corners.back(), point))
            {
                corners.push_back(point);
            }
        }
        if (corners.size() > 2 && OpenTK::Mathematics::Equal(corners.front(), corners.back()))
        {
            corners.pop_back();
        }
        if (corners.size() < 3)
        {
            result.Degenerate++;
            return;
        }
        const Vector3 normal = Newell(corners);
        if (OpenTK::Mathematics::Equal(normal, Vector3()))
        {
            result.Degenerate++;
            return;
        }
        auto* world = new ManagedArray<Vector3>(corners.size());
        auto* texcoords = new ManagedArray<Vector2>(corners.size());
        for (std::size_t i = 0; i < corners.size(); i++)
        {
            (*world)[i] = corners[i];
        }
        auto* face = new BuiltFace(reinterpret_cast<Interop::ManagedArray<Vector3>*>(world),
            reinterpret_cast<Interop::ManagedArray<Vector2>*>(texcoords), normal, 0, 1.0F);
        face->Terrain(surface.Terrain);
        face->Damaging(surface.Damaging);
        face->Slipperiness = surface.Slipperiness;
        face->ReflectBeams = surface.ReflectBeams;
        face->IgnorePlayers = surface.IgnorePlayers;
        face->IgnoreBeams = surface.IgnoreBeams;
        face->IgnoreScan = surface.IgnoreScan;
        result.Faces.push_back(face);
        const auto found = std::find_if(result.Materials.begin(), result.Materials.end(),
            [&materialName](const auto& entry) { return entry.first == materialName; });
        if (found == result.Materials.end())
        {
            result.Materials.emplace_back(materialName, 1);
        }
        else
        {
            found->second++;
        }
    }

    Vector3 CollisionObj::Newell(std::span<const Vector3> points)
    {
        auto [nx, ny, nz] = Sum(points);
        double length = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (length < 1e-6)
        {
            return Vector3();
        }
        nx /= length;
        ny /= length;
        nz /= length;
        const double largest = std::max(std::abs(nx), std::max(std::abs(ny), std::abs(nz)));
        const double floor = largest * 1e-6;
        nx = std::abs(nx) < floor ? 0 : nx;
        ny = std::abs(ny) < floor ? 0 : ny;
        nz = std::abs(nz) < floor ? 0 : nz;
        length = std::sqrt(nx * nx + ny * ny + nz * nz);
        return Vector3(static_cast<float>(nx / length), static_cast<float>(ny / length), static_cast<float>(nz / length));
    }

    Vector3 CollisionObj::TwiceArea(std::span<const Vector3> points)
    {
        const auto [nx, ny, nz] = Sum(points);
        return Vector3(static_cast<float>(nx), static_cast<float>(ny), static_cast<float>(nz));
    }

    std::array<double, 3> CollisionObj::Sum(std::span<const Vector3> points)
    {
        double cx = 0;
        double cy = 0;
        double cz = 0;
        for (const Vector3& point : points)
        {
            cx += point.X;
            cy += point.Y;
            cz += point.Z;
        }
        cx /= static_cast<double>(points.size());
        cy /= static_cast<double>(points.size());
        cz /= static_cast<double>(points.size());
        double nx = 0;
        double ny = 0;
        double nz = 0;
        for (std::size_t i = 0; i < points.size(); i++)
        {
            const Vector3 first = points[i];
            const Vector3 second = points[(i + 1) % points.size()];
            const double ax = first.X - cx;
            const double ay = first.Y - cy;
            const double az = first.Z - cz;
            const double bx = second.X - cx;
            const double by = second.Y - cy;
            const double bz = second.Z - cz;
            nx += (ay - by) * (az + bz);
            ny += (az - bz) * (ax + bx);
            nz += (ax - bx) * (ay + by);
        }
        return {nx, ny, nz};
    }

    std::string CollisionObj::MaterialName(const BuiltFace& face)
    {
        std::string name = Runtime::ToLowerInvariant(ToString(face.Terrain()));
        if (face.Slipperiness != 0)
        {
            name += "_slip" + std::to_string(face.Slipperiness);
        }
        if (face.Damaging())
        {
            name += "_damaging";
        }
        if (face.ReflectBeams)
        {
            name += "_reflect";
        }
        if (face.IgnorePlayers)
        {
            name += "_noplayers";
        }
        if (face.IgnoreBeams)
        {
            name += "_nobeams";
        }
        if (face.IgnoreScan)
        {
            name += "_noscan";
        }
        return name;
    }

    CollisionObj::Surface CollisionObj::Surface::Parse(const std::string& material, const std::string& file, std::int32_t line)
    {
        std::string name = material;
        const std::size_t dot = name.rfind('.');
        if (dot != std::string::npos && dot > 0 && name.size() - dot == 4
            && std::all_of(name.begin() + static_cast<std::ptrdiff_t>(dot + 1), name.end(),
                [](char c) { return c >= '0' && c <= '9'; }))
        {
            name = name.substr(0, dot);
        }
        std::vector<std::string> words;
        for (const std::string& word : Runtime::StringSplit(name, '_'))
        {
            if (!word.empty())
            {
                words.push_back(word);
            }
        }
        if (words.empty() || name == "(none)")
        {
            return {};
        }
        for (const MphRead::Terrain candidate : Terrains)
        {
            if (Runtime::StringEqualsOrdinalIgnoreCase(ToString(candidate), words[0]))
            {
                Surface surface{};
                surface.Terrain = candidate;
                for (std::size_t i = 1; i < words.size(); i++)
                {
                    surface = Apply(surface, words[i], material, file, line);
                }
                return surface;
            }
        }
        throw ProgramException(file + " line " + std::to_string(line) + ": \"" + material
            + "\" does not begin with a terrain. A material is <terrain>[_attribute...]; the terrains are "
            + TerrainNames() + ". A face with no material at all is plain metal, so naming none is also an "
            "answer -- but a name that is not one of these is a typo, and a typo on a "
            "lava face is a floor that kills.");
    }

    CollisionObj::Surface CollisionObj::Surface::Apply(Surface surface, const std::string& word, const std::string& material,
        const std::string& file, std::int32_t line)
    {
        const std::string lower = Runtime::ToLowerInvariant(word);
        if (lower == "slip0") { surface.Slipperiness = 0; }
        else if (lower == "slip1") { surface.Slipperiness = 1; }
        else if (lower == "slip2") { surface.Slipperiness = 2; }
        else if (lower == "slip3") { surface.Slipperiness = 3; }
        else if (lower == "damaging") { surface.Damaging = true; }
        else if (lower == "reflect") { surface.ReflectBeams = true; }
        else if (lower == "noplayers") { surface.IgnorePlayers = true; }
        else if (lower == "nobeams") { surface.IgnoreBeams = true; }
        else if (lower == "noscan") { surface.IgnoreScan = true; }
        else
        {
            std::string attributes;
            for (std::size_t i = 0; i < Attributes.size(); i++)
            {
                attributes += std::string(i == 0 ? "" : ", ") + Attributes[i];
            }
            throw ProgramException(file + " line " + std::to_string(line) + ": \"" + material + "\" has no attribute \""
                + word + "\". The attributes are " + attributes + ". This is refused rather "
                "than ignored on purpose: a misspelt \"damaging\" would silently be an "
                "ordinary floor, and a misspelt anything on a lava face is a floor that "
                "kills without saying so.");
        }
        return surface;
    }
}
