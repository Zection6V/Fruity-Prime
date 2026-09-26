#include "MapCheck.hpp"

#include "BuiltMap.hpp"
#include "CollisionObj.hpp"
#include "CustomRooms.hpp"
#include "MapBuilder.hpp"
#include "MapDefinition.hpp"
#include "MapPacker.hpp"
#include "Q3Import.hpp"
#include "../../Formats/Types.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/Number.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

namespace MphRead::Mods::MapGen
{
    namespace Runtime = ::MphRead::NativeRuntime;
    using OpenTK::Mathematics::Vector2;
    using OpenTK::Mathematics::Vector3;

    namespace
    {
        constexpr float EdgeMargin = -0.03125F;
        constexpr float CellSize = 4;
        constexpr float CoverStep = 0.5F;
        constexpr float CoverBehind = 1;
        constexpr float CoverFront = 0.25F;

        using CellKey = std::tuple<std::int32_t, std::int32_t, std::int32_t>;

        // A face as MapCheck reads it: its points, normal and attributes.
        struct Face
        {
            const BuiltFace* Source = nullptr;
            std::vector<Vector3> Points;
            Vector3 Normal;
        };

        [[nodiscard]] Face Read(const BuiltFace* face)
        {
            const auto* points = reinterpret_cast<const ManagedArray<Vector3>*>(face->Points());
            Face result{face, {}, face->Normal()};
            for (std::size_t i = 0; i < points->Length(); i++)
            {
                result.Points.push_back((*points)[i]);
            }
            return result;
        }

        [[nodiscard]] std::vector<Face> ReadAll(const std::vector<BuiltFace*>& faces)
        {
            std::vector<Face> result;
            for (const BuiltFace* face : faces)
            {
                result.push_back(Read(face));
            }
            return result;
        }

        [[nodiscard]] std::string N(float value, std::string_view format)
        {
            return Runtime::ToString(value, format);
        }

        [[nodiscard]] std::string Pad(const std::string& text, std::size_t width, bool left = false)
        {
            if (text.size() >= width)
            {
                return text;
            }
            return left ? text + std::string(width - text.size(), ' ') : std::string(width - text.size(), ' ') + text;
        }

        [[nodiscard]] std::string I(std::int64_t value, std::size_t width)
        {
            return Pad(std::to_string(value), width);
        }

        void Bounds(const std::vector<Face>& faces, Vector3& low, Vector3& high)
        {
            constexpr float max = std::numeric_limits<float>::max();
            constexpr float min = std::numeric_limits<float>::lowest();
            low = Vector3(max, max, max);
            high = Vector3(min, min, min);
            for (const Face& face : faces)
            {
                for (const Vector3& point : face.Points)
                {
                    low = Vector3(std::min(low.X, point.X), std::min(low.Y, point.Y), std::min(low.Z, point.Z));
                    high = Vector3(std::max(high.X, point.X), std::max(high.Y, point.Y), std::max(high.Z, point.Z));
                }
            }
        }

        [[nodiscard]] Vector3 Centre(const Face& face)
        {
            Vector3 total{};
            for (const Vector3& point : face.Points)
            {
                total = OpenTK::Mathematics::Add(total, point);
            }
            return OpenTK::Mathematics::Divide(total, static_cast<float>(face.Points.size()));
        }

        [[nodiscard]] float Area(const Face& face)
        {
            const Vector3 twice = CollisionObj::TwiceArea(face.Points);
            return std::sqrt(OpenTK::Mathematics::LengthSquared(twice)) / 2;
        }

        [[nodiscard]] std::string Where(Vector3 v)
        {
            return "(" + N(v.X, "0.###") + ", " + N(v.Y, "0.###") + ", " + N(v.Z, "0.###") + ")";
        }

        [[nodiscard]] std::string Percent(std::int32_t value, std::int32_t of)
        {
            return N(static_cast<float>(value) * 100.0F / static_cast<float>(of), "0.#") + "%";
        }

        [[nodiscard]] std::int32_t Span(float low, float high, float origin, std::int32_t parts)
        {
            const std::int32_t first = std::clamp(static_cast<std::int32_t>((low - origin) / CellSize), 0, parts - 1);
            const std::int32_t last = std::clamp(static_cast<std::int32_t>((high - origin) / CellSize), 0, parts - 1);
            return last - first + 1;
        }

        [[nodiscard]] bool Accepts(const Face& face, Vector3 point)
        {
            const std::vector<Vector3>& points = face.Points;
            for (std::size_t i = 0; i < points.size(); i++)
            {
                const Vector3 first = points[i];
                const Vector3 second = points[(i + 1) % points.size()];
                const Vector3 edge = OpenTK::Mathematics::Subtract(first, second);
                if (OpenTK::Mathematics::LengthSquared(edge) < 1e-12F)
                {
                    continue;
                }
                const Vector3 cross = Vector3::Cross(edge.Normalized(), face.Normal);
                if (Vector3::Dot(point, cross) - Vector3::Dot(cross, second) < EdgeMargin)
                {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] Vector3 Blend(Vector3 a, Vector3 b, Vector3 c, float u, float v)
        {
            return OpenTK::Mathematics::Add(OpenTK::Mathematics::Add(OpenTK::Mathematics::Multiply(a, 1 - u - v),
                OpenTK::Mathematics::Multiply(b, u)), OpenTK::Mathematics::Multiply(c, v));
        }

        [[nodiscard]] float Usable(const Face& face)
        {
            std::int32_t inside = 0;
            std::int32_t total = 0;
            const std::vector<Vector3>& points = face.Points;
            for (std::size_t i = 1; i + 1 < points.size(); i++)
            {
                for (std::int32_t a = 1; a <= 6; a++)
                {
                    for (std::int32_t b = 1; a + b <= 7; b++)
                    {
                        const Vector3 sample = Blend(points[0], points[i], points[i + 1], a / 8.0F, b / 8.0F);
                        total++;
                        if (Accepts(face, sample))
                        {
                            inside++;
                        }
                    }
                }
            }
            return total == 0 ? 1.0F : static_cast<float>(inside) / static_cast<float>(total);
        }

        [[nodiscard]] CellKey Cell(Vector3 point, Vector3 origin)
        {
            return {static_cast<std::int32_t>(std::floor((point.X - origin.X) / CellSize)),
                static_cast<std::int32_t>(std::floor((point.Y - origin.Y) / CellSize)),
                static_cast<std::int32_t>(std::floor((point.Z - origin.Z) / CellSize))};
        }

        [[nodiscard]] std::int32_t Size(const std::vector<Face>& solid)
        {
            std::set<std::tuple<float, float, float>> points;
            std::int32_t faces = 0;
            std::int32_t fanned = 0;
            for (const Face& face : solid)
            {
                for (const Vector3& point : face.Points)
                {
                    points.insert({point.X, point.Y, point.Z});
                }
                if (face.Points.size() > 10)
                {
                    faces += static_cast<std::int32_t>(face.Points.size()) - 2;
                    fanned++;
                }
                else
                {
                    faces++;
                }
            }
            Vector3 low;
            Vector3 high;
            Bounds(solid, low, high);
            const std::int32_t partsX = std::max(1, static_cast<std::int32_t>(std::floor((high.X - low.X) / CellSize)) + 1);
            const std::int32_t partsY = std::max(1, static_cast<std::int32_t>(std::floor((high.Y - low.Y) / CellSize)) + 1);
            const std::int32_t partsZ = std::max(1, static_cast<std::int32_t>(std::floor((high.Z - low.Z) / CellSize)) + 1);
            std::int32_t references = 0;
            for (const Face& face : solid)
            {
                Vector3 faceLow;
                Vector3 faceHigh;
                Bounds({face}, faceLow, faceHigh);
                references += Span(faceLow.X, faceHigh.X, low.X, partsX) * Span(faceLow.Y, faceHigh.Y, low.Y, partsY)
                    * Span(faceLow.Z, faceHigh.Z, low.Z, partsZ);
            }
            const auto distinct = static_cast<std::int32_t>(points.size());
            std::cout << "\n  against the format's limits\n";
            std::cout << "    faces written      " << I(faces, 8)
                << (fanned > 0 ? "   (" + std::to_string(fanned) + " with more than ten points, split into triangles)" : std::string())
                << '\n';
            std::cout << "    distinct points    " << I(distinct, 8) << "   of 65535  (" << Percent(distinct, 65535) << ")\n";
            std::cout << "    grid references    " << I(references, 8) << "   of 65535  (" << Percent(references, 65535) << ")"
                << "   in " << partsX << "x" << partsY << "x" << partsZ << " cells\n";
            if (distinct > 65535 || references > 65535)
            {
                std::cout << "    This will not pack. Take collision out, or convert at a larger scale.\n";
                return 1;
            }
            return 0;
        }

        [[nodiscard]] std::int32_t Shape(const std::vector<Face>& solid)
        {
            std::int32_t degenerate = 0;
            struct Partial
            {
                const Face* Face;
                float Usable;
                float Area;
            };
            std::vector<Partial> partial;
            for (const Face& face : solid)
            {
                if (OpenTK::Mathematics::Equal(CollisionObj::Newell(face.Points), Vector3()))
                {
                    degenerate++;
                    continue;
                }
                const float usable = Usable(face);
                if (usable < 0.999F)
                {
                    partial.push_back({&face, usable, Area(face)});
                }
            }
            std::cout << "\n  shape\n";
            std::cout << "    faces enclosing no area            " << I(degenerate, 6)
                << (degenerate > 0 ? "   (nothing can touch one; they are dropped)" : "") << '\n';
            std::cout << "    faces that reject part of themselves " << I(static_cast<std::int64_t>(partial.size()), 4) << '\n';
            if (!partial.empty())
            {
                std::cout << "      A face is tested edge by edge at run time, so a polygon that bends"
                    " back on itself blocks only part of its own area. Make them convex, or triangulate.\n";
                std::stable_sort(partial.begin(), partial.end(), [](const Partial& a, const Partial& b)
                {
                    return a.Area * (1 - a.Usable) > b.Area * (1 - b.Usable);
                });
                for (std::size_t i = 0; i < partial.size() && i < 8; i++)
                {
                    const Partial& p = partial[i];
                    std::cout << "      " << Pad(N(p.Area, "0.00"), 8) << " u2  " << Pad(N(p.Usable * 100, "0.0"), 5)
                        << "% usable  at " << Where(Centre(*p.Face)) << "  " << p.Face->Points.size() << " points\n";
                }
            }
            return (partial.empty() ? 0 : 1) + (degenerate > 0 ? 1 : 0);
        }

        void Surfaces(const std::vector<Face>& solid)
        {
            std::vector<std::pair<Terrain, std::int32_t>> terrain;
            std::map<std::int32_t, std::int32_t> slip;
            std::int32_t damaging = 0;
            std::int32_t reflect = 0;
            std::int32_t noPlayers = 0;
            std::int32_t noBeams = 0;
            std::int32_t noScan = 0;
            float damagingArea = 0;
            for (const Face& face : solid)
            {
                const BuiltFace& source = *face.Source;
                auto found = std::find_if(terrain.begin(), terrain.end(),
                    [&source](const auto& entry) { return entry.first == source.Terrain(); });
                if (found == terrain.end())
                {
                    terrain.emplace_back(source.Terrain(), 1);
                }
                else
                {
                    found->second++;
                }
                slip[source.Slipperiness]++;
                if (source.Damaging())
                {
                    damaging++;
                    damagingArea += Area(face);
                }
                reflect += source.ReflectBeams ? 1 : 0;
                noPlayers += source.IgnorePlayers ? 1 : 0;
                noBeams += source.IgnoreBeams ? 1 : 0;
                noScan += source.IgnoreScan ? 1 : 0;
            }
            std::cout << "\n  what the surfaces are\n";
            std::vector<std::pair<Terrain, std::int32_t>> ordered = terrain;
            std::stable_sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
            for (const auto& [type, count] : ordered)
            {
                std::cout << "    " << Pad(Runtime::ToLowerInvariant(ToString(type)), 12, true) << " " << I(count, 6) << '\n';
            }
            if (slip.size() > 1 || !slip.contains(0))
            {
                std::string line;
                for (const auto& [key, value] : slip)
                {
                    line += (line.empty() ? "" : ", ") + std::to_string(key) + ": " + std::to_string(value);
                }
                std::cout << "    slipperiness  " << line << '\n';
            }
            std::int32_t lava = 0;
            std::int32_t acid = 0;
            for (const auto& [type, count] : terrain)
            {
                lava += type == Terrain::Lava ? count : 0;
                acid += type == Terrain::Acid ? count : 0;
            }
            std::cout << "    faces that hurt: " << damaging << " damaging (" << N(damagingArea, "0.#") << " u2), "
                << lava << " lava, " << acid << " acid"
                << (damaging + lava + acid == 0 ? "" : "  <- check this is deliberate") << '\n';
            if (reflect + noPlayers + noBeams + noScan > 0)
            {
                std::cout << "    reflect " << reflect << ", ignore players " << noPlayers << ", ignore beams " << noBeams
                    << ", ignore scan " << noScan << '\n';
            }
        }

        [[nodiscard]] std::vector<Face> Split(const Face& face)
        {
            if (face.Points.size() <= 10)
            {
                return {face};
            }
            std::vector<Face> parts;
            for (std::size_t i = 1; i + 1 < face.Points.size(); i++)
            {
                parts.push_back(Face{face.Source, {face.Points[0], face.Points[i], face.Points[i + 1]}, face.Normal});
            }
            return parts;
        }

        [[nodiscard]] std::string ShapeKey(const Face& face)
        {
            std::vector<std::string> keys;
            for (const Vector3& p : face.Points)
            {
                keys.push_back(std::to_string(Fixed::ToInt(p.X)) + "," + std::to_string(Fixed::ToInt(p.Y)) + ","
                    + std::to_string(Fixed::ToInt(p.Z)));
            }
            std::sort(keys.begin(), keys.end());
            std::string joined;
            for (std::size_t i = 0; i < keys.size(); i++)
            {
                joined += (i == 0 ? "" : ";") + keys[i];
            }
            return joined;
        }

        void Difference(const std::vector<Face>& geometry, const std::vector<Face>& edited)
        {
            // Dictionary<string, BuiltFace>: first insertion fixes the order,
            // a repeat replaces the value in place.
            std::vector<std::pair<std::string, Face>> before;
            std::map<std::string, std::size_t> index;
            for (const Face& whole : geometry)
            {
                for (const Face& face : Split(whole))
                {
                    const std::string key = ShapeKey(face);
                    const auto found = index.find(key);
                    if (found == index.end())
                    {
                        index[key] = before.size();
                        before.emplace_back(key, face);
                    }
                    else
                    {
                        before[found->second].second = face;
                    }
                }
            }
            std::set<std::string> after;
            std::int32_t added = 0;
            for (const Face& whole : edited)
            {
                for (const Face& face : Split(whole))
                {
                    const std::string key = ShapeKey(face);
                    after.insert(key);
                    if (!index.contains(key))
                    {
                        added++;
                    }
                }
            }
            std::vector<Face> removed;
            float total = 0;
            for (const auto& [key, face] : before)
            {
                total += Area(face);
                if (!after.contains(key))
                {
                    removed.push_back(face);
                }
            }
            float removedArea = 0;
            for (const Face& face : removed)
            {
                removedArea += Area(face);
            }
            const auto kept = static_cast<std::int64_t>(before.size() - removed.size());
            std::cout << "\n  what the .obj changed, against the collision the geometry makes\n";
            std::cout << "    kept    " << I(kept, 6) << " faces\n";
            std::cout << "    removed " << I(static_cast<std::int64_t>(removed.size()), 6) << " faces  (" << N(removedArea, "0.#")
                << " u2, " << N(total > 0 ? removedArea / total * 100 : 0, "0.#") << "% of the room's collision area)\n";
            std::cout << "    added   " << I(added, 6) << " faces\n";
            std::stable_sort(removed.begin(), removed.end(), [](const Face& a, const Face& b) { return Area(a) > Area(b); });
            for (std::size_t i = 0; i < removed.size() && i < 6; i++)
            {
                std::cout << "      " << Pad(N(Area(removed[i]), "0.00"), 9) << " u2 gone from " << Where(Centre(removed[i])) << '\n';
            }
        }

        [[nodiscard]] std::vector<Vector3> Samples(const Face& face)
        {
            std::vector<Vector3> samples;
            const std::vector<Vector3>& points = face.Points;
            for (std::size_t i = 1; i + 1 < points.size(); i++)
            {
                const Vector3 a = points[0];
                const Vector3 b = points[i];
                const Vector3 c = points[i + 1];
                const auto length = [](Vector3 v) { return std::sqrt(OpenTK::Mathematics::LengthSquared(v)); };
                const float longest = std::max(length(OpenTK::Mathematics::Subtract(b, a)),
                    std::max(length(OpenTK::Mathematics::Subtract(c, b)), length(OpenTK::Mathematics::Subtract(a, c))));
                const std::int32_t steps = std::clamp(static_cast<std::int32_t>(std::ceil(longest / CoverStep)), 1, 64);
                for (std::int32_t u = 0; u <= steps; u++)
                {
                    for (std::int32_t v = 0; u + v <= steps; v++)
                    {
                        samples.push_back(Blend(a, b, c, u / static_cast<float>(steps), v / static_cast<float>(steps)));
                    }
                }
            }
            return samples;
        }

        [[nodiscard]] bool Covered(const std::map<CellKey, std::vector<const Face*>>& grid, Vector3 low, Vector3 point,
            Vector3 normal)
        {
            const auto found = grid.find(Cell(point, low));
            if (found == grid.end())
            {
                return false;
            }
            for (const Face* solid : found->second)
            {
                if (Vector3::Dot(solid->Normal, normal) < 0.5F)
                {
                    continue;
                }
                const float distance = Vector3::Dot(solid->Normal, point) - Vector3::Dot(solid->Normal, solid->Points[0]);
                if (distance > CoverBehind || distance < -CoverFront)
                {
                    continue;
                }
                if (Accepts(*solid, OpenTK::Mathematics::Subtract(point, OpenTK::Mathematics::Multiply(solid->Normal, distance))))
                {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] std::int32_t Cover(const std::vector<Face>& solid, const std::vector<Face>& drawn)
        {
            Vector3 low;
            Vector3 high;
            Bounds(solid, low, high);
            std::map<CellKey, std::vector<const Face*>> grid;
            for (const Face& face : solid)
            {
                Vector3 faceLow;
                Vector3 faceHigh;
                Bounds({face}, faceLow, faceHigh);
                const auto [x0, y0, z0] = Cell(faceLow, low);
                const auto [x1, y1, z1] = Cell(faceHigh, low);
                for (std::int32_t x = x0; x <= x1; x++)
                {
                    for (std::int32_t y = y0; y <= y1; y++)
                    {
                        for (std::int32_t z = z0; z <= z1; z++)
                        {
                            grid[{x, y, z}].push_back(&face);
                        }
                    }
                }
            }
            std::int32_t samples = 0;
            std::int32_t uncovered = 0;
            std::vector<std::pair<CellKey, std::pair<std::int32_t, Vector3>>> worst;
            std::map<CellKey, std::size_t> worstIndex;
            for (const Face& face : drawn)
            {
                if (face.Source->Sky)
                {
                    continue;
                }
                for (const Vector3& point : Samples(face))
                {
                    samples++;
                    if (Covered(grid, low, point, face.Normal))
                    {
                        continue;
                    }
                    uncovered++;
                    const CellKey cell = Cell(point, low);
                    const auto found = worstIndex.find(cell);
                    if (found == worstIndex.end())
                    {
                        worstIndex[cell] = worst.size();
                        worst.push_back({cell, {1, point}});
                    }
                    else
                    {
                        worst[found->second].second.first++;
                    }
                }
            }
            std::cout << "\n  drawn surfaces with nothing solid behind them\n";
            std::cout << "    Read this as a list of places to look at, not as a list of faults: a level draws plenty\n";
            std::cout << "    that was never meant to stop anybody -- trim, decoration, and every brush its own author made non-solid.\n";
            std::cout << "    " << uncovered << " of " << samples << " samples uncovered  (about "
                << N(static_cast<float>(uncovered) * CoverStep * CoverStep, "0.#") << " u2, in " << worst.size()
                << " places, sampled " << N(CoverStep, "") << " units apart)\n";
            std::stable_sort(worst.begin(), worst.end(), [](const auto& a, const auto& b) { return a.second.first > b.second.first; });
            for (std::size_t i = 0; i < worst.size() && i < 8; i++)
            {
                std::cout << "      " << I(worst[i].second.first, 5) << " samples around " << Where(worst[i].second.second) << '\n';
            }
            return 0;
        }
    }

    std::int32_t MapCheck::Run(const std::string& room)
    {
        MapDefinition* def = nullptr;
        for (const std::shared_ptr<MapDefinition>& candidate : CustomRooms::Definitions())
        {
            if (candidate != nullptr && Runtime::StringEqualsOrdinalIgnoreCase(candidate->Name(), room))
            {
                def = candidate.get();
                break;
            }
        }
        if (def == nullptr)
        {
            std::string names;
            for (const std::shared_ptr<MapDefinition>& candidate : CustomRooms::Definitions())
            {
                names += (names.empty() ? "" : ", ") + candidate->Name();
            }
            std::cout << "No custom map called " << room << ". " << CustomRooms::MapDirectory() << " holds " << names << ".\n";
            return 1;
        }
        std::shared_ptr<BuiltMap> map;
        std::vector<Face> geometry;
        try
        {
            map = def->Import() == nullptr ? MapBuilder::Build(def) : Q3Import::Build(def, false);
            geometry = ReadAll(map->Solid());
            MapPacker::ApplyCollision(map.get(), def, false);
        }
        catch (const std::exception& ex)
        {
            std::cout << room << " cannot be built at all: " << ex.what() << '\n';
            return 1;
        }
        const std::vector<Face> solid = ReadAll(map->Solid());
        const std::vector<Face> drawn = ReadAll(map->Faces());
        std::cout << def->Name() << ": " << solid.size() << " collision faces, " << drawn.size() << " drawn polygons"
            << (def->Collision() == nullptr ? std::string() : ", collision from " + def->Collision()->Source) << '\n';
        std::int32_t problems = 0;
        problems += Size(solid);
        problems += Shape(solid);
        Surfaces(solid);
        if (def->Collision() != nullptr)
        {
            Difference(geometry, solid);
        }
        problems += Cover(solid, drawn);
        std::cout << '\n';
        std::cout << (problems == 0 ? std::string("  Nothing to report.")
            : "  " + std::to_string(problems) + " thing" + (problems == 1 ? "" : "s") + " to look at.") << '\n';
        return 0;
    }
}
