#include "Collada.hpp"
#include "Scripting.hpp"

#include "../Formats/Formats.hpp"
#include "../Formats/Model.hpp"
#include "../Formats/RawFormats.hpp"
#include "../Metadata/Metadata.hpp"
#include "../Program.hpp"
#include "../Read.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
using MphRead::Export::Collada;
using OpenTK::Mathematics::Vector2;
using OpenTK::Mathematics::Vector3;

[[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view value)
{
#if defined(__cpp_char8_t)
    std::u8string converted;
    converted.reserve(value.size());
    for (unsigned char ch : value)
    {
        converted.push_back(static_cast<char8_t>(ch));
    }
    return std::filesystem::path(converted);
#else
    return std::filesystem::u8path(value.begin(), value.end());
#endif
}

void WriteAllText(std::string_view path, std::string_view text)
{
    std::ofstream stream(PathFromUtf8(path), std::ios::binary | std::ios::trunc);
    stream.exceptions(std::ios::failbit | std::ios::badbit);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
}

void AppendTabs(std::string& output, std::int32_t count)
{
    if (count > 0)
    {
        output.append(static_cast<std::size_t>(count), '\t');
    }
}

[[nodiscard]] bool IsNullOrEmpty(const std::string& value) noexcept
{
    return value.empty();
}

[[nodiscard]] std::string PadLeft3(std::int32_t value)
{
    const std::string text = std::to_string(value);
    if (text.size() >= 3)
    {
        return text;
    }
    return std::string(3 - text.size(), '0') + text;
}

[[nodiscard]] std::string ManagedSingleToString(float value)
{
    if (std::isnan(value))
    {
        return "NaN";
    }
    if (std::isinf(value))
    {
        return std::signbit(value) ? "-Infinity" : "Infinity";
    }
    if (value == 0.0F)
    {
        return std::signbit(value) ? "-0" : "0";
    }

    char buffer[64]{};
    const auto result = std::to_chars(
        std::begin(buffer), std::end(buffer), value, std::chars_format::general);
    if (result.ec != std::errc{})
    {
        throw std::runtime_error("Failed to format Single.");
    }
    std::string text(buffer, result.ptr);
    const std::size_t exponent = text.find('e');
    if (exponent != std::string::npos)
    {
        text[exponent] = 'E';
        if (exponent + 1 < text.size() && text[exponent + 1] != '+' && text[exponent + 1] != '-')
        {
            text.insert(exponent + 1, 1, '+');
        }
    }
    return text;
}

[[nodiscard]] float RoundAwayFromZeroSix(float value) noexcept
{
    if (!std::isfinite(value) || value == 0.0F)
    {
        return value;
    }
    constexpr float Scale = 1000000.0F;
    constexpr float RoundLimit = 100000000.0F;
    if (std::fabs(value) >= RoundLimit)
    {
        return value;
    }
    const float scaled = value * Scale;
    const float rounded = scaled >= 0.0F
        ? std::floor(scaled + 0.5F)
        : std::ceil(scaled - 0.5F);
    return rounded / Scale;
}

[[nodiscard]] bool Vector2Equal(Vector2 left, Vector2 right) noexcept
{
    return left.X == right.X && left.Y == right.Y;
}

[[nodiscard]] bool Vector3Equal(Vector3 left, Vector3 right) noexcept
{
    return left.X == right.X && left.Y == right.Y && left.Z == right.Z;
}

[[nodiscard]] bool VertexEqual(const Collada::Vertex& left, const Collada::Vertex& right) noexcept
{
    return Vector3Equal(left.Position, right.Position)
        && Vector3Equal(left.Normal, right.Normal)
        && Vector3Equal(left.Color, right.Color)
        && Vector2Equal(left.Uv, right.Uv)
        && left.MatrixId == right.MatrixId;
}

[[nodiscard]] bool VertexListEqual(
    const std::vector<Collada::Vertex>& left,
    const std::vector<Collada::Vertex>& right) noexcept
{
    if (left.size() != right.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        if (!VertexEqual(left[i], right[i]))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::int32_t SignExtend10(std::uint32_t value) noexcept
{
    std::int32_t result = static_cast<std::int32_t>(value & 0x3FFU);
    if ((result & 0x200) != 0)
    {
        result = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(result) | 0xFFFFFC00U);
    }
    return result;
}

[[nodiscard]] std::int32_t SignExtend16(std::uint32_t value) noexcept
{
    std::int32_t result = static_cast<std::int32_t>(value & 0xFFFFU);
    if ((result & 0x8000) != 0)
    {
        result = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(result) | 0xFFFF0000U);
    }
    return result;
}

[[nodiscard]] float Clamp01(float value) noexcept
{
    return std::min(std::max(value, 0.0F), 1.0F);
}

[[nodiscard]] float RadiansToDegrees(float radians) noexcept
{
    constexpr float RadiansToDegreesFactor = 57.295779513082320876798154814105F;
    return radians * RadiansToDegreesFactor;
}
}

namespace MphRead::Export
{
std::string Collada::FloatFormat(Vector3 vector)
{
    return FloatFormat(vector.X) + " " + FloatFormat(vector.Y) + " " + FloatFormat(vector.Z);
}

std::string Collada::FloatFormat(float input)
{
    const float rounded = RoundAwayFromZeroSix(input);
    if (std::isnan(rounded))
    {
        return "NaN";
    }
    if (std::isinf(rounded))
    {
        return std::signbit(rounded) ? "-Infinity" : "Infinity";
    }

    char buffer[128]{};
    const auto result = std::to_chars(
        std::begin(buffer), std::end(buffer), rounded, std::chars_format::fixed, 6);
    if (result.ec != std::errc{})
    {
        throw std::runtime_error("Failed to format Single.");
    }
    return std::string(buffer, result.ptr);
}

void Collada::ExportModel(const Model& model, bool transformRoom)
{
    const std::string exportPath = Paths::Combine(Paths::Export(), model.Name());
    std::filesystem::create_directories(PathFromUtf8(exportPath));

    std::vector<VertexDictionary> lists;
    lists.reserve(model.Recolors().size());
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(model.Recolors().size()); ++i)
    {
        lists.push_back(ExportRecolor(model, transformRoom, i));
    }

    if (lists.empty())
    {
        throw ProgramException("Export failed due to missing recolor.");
    }

    for (std::size_t i = 1; i < lists.size(); ++i)
    {
        const VertexDictionary& current = lists[i];
        const VertexDictionary& previous = lists[i - 1];
        if (current.size() != previous.size())
        {
            throw ProgramException("Export failed due to mismatching objects.");
        }
        for (std::size_t keyIndex = 0; keyIndex < current.size(); ++keyIndex)
        {
            if (current[keyIndex].first != previous[keyIndex].first)
            {
                throw ProgramException("Export failed due to mismatching objects.");
            }
        }

        for (const auto& item : current)
        {
            const auto previousItem = std::find_if(
                previous.begin(), previous.end(),
                [&item](const auto& pair)
                {
                    return pair.first == item.first;
                });
            if (previousItem == previous.end() || !VertexListEqual(item.second, previousItem->second))
            {
                throw ProgramException("Export failed due to mismatching vertices.");
            }
        }
    }

    WriteAllText(
        Paths::Combine(exportPath, "import_" + model.Name() + ".py"),
        Scripting::GenerateScript(model, lists.front()));
}

Collada::VertexDictionary Collada::ExportRecolor(
    const Model& model, bool transformRoom, std::int32_t recolorIndex)
{
    VertexDictionary results;
    const Recolor& recolor = model.Recolors().at(static_cast<std::size_t>(recolorIndex));
    std::string output;

    output += "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
    output += "\n<COLLADA xmlns=\"http://www.collada.org/2005/11/COLLADASchema\" version=\"1.4.1\">";

    output += "\n\t<asset>";
    output += "\n\t\t<up_axis>Y_UP</up_axis>";
    output += "\n\t\t<unit name=\"meter\" meter=\"1\" />";
    output += "\n\t</asset>";

    output += "\n\t<library_images>";
    std::int32_t id = 0;
    std::set<std::pair<std::int32_t, std::int32_t>> imagesInLibrary;
    const auto addLibraryImage =
        [&output, &id, &imagesInLibrary, &recolor](
            std::int32_t textureId, std::int32_t paletteId, const std::string& name)
        {
            const std::pair<std::int32_t, std::int32_t> key(textureId, paletteId);
            if (textureId != -1 && imagesInLibrary.find(key) == imagesInLibrary.end())
            {
                imagesInLibrary.insert(key);
                output += "\n\t\t<image id=\"" + name + "\" name=\"" + name + "\">";
                output += "\n\t\t\t<init_from>";
                if (id <= 0)
                {
                    output += recolor.Name() + "/" + std::to_string(textureId)
                        + "-" + std::to_string(paletteId) + ".png";
                }
                else
                {
                    output += recolor.Name() + "/anim__" + PadLeft3(id) + ".png";
                }
                output += "</init_from>\n\t\t</image>";
            }
        };

    for (const Material& material : model.Materials())
    {
        addLibraryImage(material.TextureId(), material.PaletteId(), material.Name());
    }

    id = 1;
    imagesInLibrary.clear();
    for (const TextureAnimationGroup& group : model.AnimationGroups().Texture())
    {
        for (const auto& animationItem : group.Animations())
        {
            const TextureAnimation& animation = animationItem.second;
            for (std::int32_t i = animation.StartIndex;
                i < animation.StartIndex + animation.Count; ++i)
            {
                const std::int32_t textureId = group.TextureIds().at(static_cast<std::size_t>(i));
                const std::int32_t paletteId = group.PaletteIds().at(static_cast<std::size_t>(i));
                addLibraryImage(
                    textureId, paletteId,
                    "anim__" + std::to_string(textureId) + "-" + std::to_string(paletteId));
                ++id;
            }
        }
    }
    output += "\n\t</library_images>";

    output += "\n\t<library_materials>";
    for (const Material& material : model.Materials())
    {
        const std::string textureName = IsNullOrEmpty(material.Name()) ? "null" : material.Name();
        output += "\n\t\t<material id=\"" + textureName + "-material\" name=\""
            + textureName + "_mat\">";
        output += "\n\t\t\t<instance_effect url=\"#" + textureName + "-effect\" />";
        output += "\n\t\t</material>";
    }
    output += "\n\t</library_materials>";

    output += "\n\t<library_geometries>";
    VertexList meshVerts;
    VertexList tempMeshVerts;

    std::int32_t meshCounter = 0;
    for (const Mesh& mesh : model.Meshes())
    {
        ++meshCounter;
        meshVerts.clear();
        tempMeshVerts.clear();

        const DisplayList& dlist = model.DisplayLists().at(
            static_cast<std::size_t>(mesh.DlistId()));
        (void)dlist;
        const Material& material = model.Materials().at(
            static_cast<std::size_t>(mesh.MaterialId()));

        Texture tex{};
        if (material.TextureId() != -1)
        {
            tex = recolor.Textures().at(static_cast<std::size_t>(material.TextureId()));
        }

        ExportDlist(model, mesh.DlistId(), tempMeshVerts, meshVerts);

        const std::int32_t numVertices = static_cast<std::int32_t>(meshVerts.size());
        const std::string textureName = IsNullOrEmpty(material.Name()) ? "null" : material.Name();
        const std::string geometryID = "geometry" + std::to_string(meshCounter);

        output += "\n\t\t<geometry id=\"geometry" + std::to_string(meshCounter)
            + "\" name=\"geom" + std::to_string(meshCounter) + "\">";
        output += "\n\t\t\t<mesh>";

        output += "\n\t\t\t\t<source id=\"" + geometryID + "-positions\">";
        const std::string positionsArrayID = geometryID + "-positions-array";
        output += "\n\t\t\t\t\t<float_array id=\"" + positionsArrayID + "\" count=\""
            + std::to_string(numVertices * 3) + "\">";
        for (const Vertex& vert : meshVerts)
        {
            output += FloatFormat(vert.Position.X) + " " + FloatFormat(vert.Position.Y)
                + " " + FloatFormat(vert.Position.Z) + " ";
        }
        output += "</float_array>";

        output += "\n\t\t\t\t\t<technique_common>";
        output += "\n\t\t\t\t\t\t<accessor source=\"#" + positionsArrayID + "\" count=\""
            + std::to_string(numVertices) + "\" stride=\"3\">";
        output += "\n\t\t\t\t\t\t\t<param name=\"X\" type=\"float\" />";
        output += "\n\t\t\t\t\t\t\t<param name=\"Y\" type=\"float\" />";
        output += "\n\t\t\t\t\t\t\t<param name=\"Z\" type=\"float\" />";
        output += "\n\t\t\t\t\t\t</accessor>";
        output += "\n\t\t\t\t\t</technique_common>";
        output += "\n\t\t\t\t</source>";

        output += "\n\t\t\t\t<source id=\"" + geometryID + "-normals\">";
        const std::string normalsArrayID = geometryID + "-normals-array";
        output += "\n\t\t\t\t\t<float_array id=\"" + normalsArrayID + "\" count=\""
            + std::to_string(numVertices * 3) + "\">";
        for (std::size_t i = 0; i < meshVerts.size(); i += 3)
        {
            Vector3 normal = Vector3::Cross(
                meshVerts.at(i + 1).Position - meshVerts.at(i).Position,
                meshVerts.at(i + 2).Position - meshVerts.at(i).Position);
            if (!Vector3Equal(normal, Vector3::Zero))
            {
                normal = normal.Normalized();
            }
            const std::string vertNormal =
                FloatFormat(normal.X) + " " + FloatFormat(normal.Y) + " "
                + FloatFormat(normal.Z) + " ";
            output += vertNormal;
            output += vertNormal;
            output += vertNormal;
        }
        output += "</float_array>";

        output += "\n\t\t\t\t\t<technique_common>";
        output += "\n\t\t\t\t\t\t<accessor source=\"#" + normalsArrayID + "\" count=\""
            + std::to_string(numVertices) + "\" stride=\"3\">";
        output += "\n\t\t\t\t\t\t\t<param name=\"X\" type=\"float\" />";
        output += "\n\t\t\t\t\t\t\t<param name=\"Y\" type=\"float\" />";
        output += "\n\t\t\t\t\t\t\t<param name=\"Z\" type=\"float\" />";
        output += "\n\t\t\t\t\t\t</accessor>";
        output += "\n\t\t\t\t\t</technique_common>";
        output += "\n\t\t\t\t</source>";

        output += "\n\t\t\t\t<source id=\"" + geometryID + "-colors\">";
        const std::string vertexcolorsArrayID = geometryID + "-colors-array";
        output += "\n\t\t\t\t\t<float_array id=\"" + vertexcolorsArrayID + "\" count=\""
            + std::to_string(numVertices * 3) + "\">";
        for (const Vertex& vert : meshVerts)
        {
            output += FloatFormat(vert.Color.X) + " " + FloatFormat(vert.Color.Y)
                + " " + FloatFormat(vert.Color.Z) + " ";
        }
        output += "</float_array>";

        output += "\n\t\t\t\t\t<technique_common>";
        output += "\n\t\t\t\t\t\t<accessor source=\"#" + vertexcolorsArrayID + "\" count=\""
            + std::to_string(numVertices) + "\" stride=\"3\">";
        output += "\n\t\t\t\t\t\t\t<param name=\"R\" type=\"float\" />";
        output += "\n\t\t\t\t\t\t\t<param name=\"G\" type=\"float\" />";
        output += "\n\t\t\t\t\t\t\t<param name=\"B\" type=\"float\" />";
        output += "\n\t\t\t\t\t\t</accessor>";
        output += "\n\t\t\t\t\t</technique_common>";
        output += "\n\t\t\t\t</source>";

        output += "\n\t\t\t\t<source id=\"" + geometryID + "-texcoords\">";
        const std::string texcoordsArrayID = geometryID + "-texcoords-array";
        output += "\n\t\t\t\t\t<float_array id=\"" + texcoordsArrayID + "\" count=\""
            + std::to_string(numVertices * 2) + "\">";

        if (material.TextureId() != -1)
        {
            VertexList updated;
            updated.reserve(meshVerts.size());
            for (const Vertex& vert : meshVerts)
            {
                float factorS = 1.0F;
                float factorT = 1.0F;
                if (material.TexgenMode() != TexgenMode::None && model.TextureMatrices().empty())
                {
                    factorS = material.ScaleS();
                    factorT = material.ScaleT();
                }

                Vector2 newUv(
                    vert.Uv.X * factorS * (1.0F / tex.Width),
                    vert.Uv.Y * factorT * (1.0F / tex.Height));
                if (material.XRepeat() == RepeatMode::Clamp)
                {
                    newUv = Vector2(Clamp01(newUv.X), newUv.Y);
                }
                if (material.YRepeat() == RepeatMode::Clamp)
                {
                    newUv = Vector2(newUv.X, Clamp01(newUv.Y));
                }

                updated.emplace_back(
                    vert.Position, vert.Color, vert.Normal, newUv, vert.MatrixId);
                output += FloatFormat(newUv.X) + " " + FloatFormat(1.0F - newUv.Y) + " ";
            }
            meshVerts = std::move(updated);
        }
        output += "</float_array>";

        output += "\n\t\t\t\t\t<technique_common>";
        output += "\n\t\t\t\t\t\t<accessor source=\"#" + texcoordsArrayID + "\" count=\""
            + std::to_string(numVertices) + "\" stride=\"2\">";
        output += "\n\t\t\t\t\t\t\t<param name=\"S\" type=\"float\" />";
        output += "\n\t\t\t\t\t\t\t<param name=\"T\" type=\"float\" />";
        output += "\n\t\t\t\t\t\t</accessor>";
        output += "\n\t\t\t\t\t</technique_common>";
        output += "\n\t\t\t\t</source>";

        output += "\n\t\t\t\t<vertices id=\"" + geometryID + "-vertices\">";
        output += "\n\t\t\t\t\t<input semantic=\"POSITION\" source=\"#" + geometryID
            + "-positions\" />";
        output += "\n\t\t\t\t</vertices>";

        output += "\n\t\t\t\t<triangles material=\"#" + textureName + "-material\" count=\""
            + std::to_string(numVertices / 3) + "\">";
        output += "\n\t\t\t\t\t<input semantic=\"VERTEX\" source=\"#" + geometryID
            + "-vertices\" offset=\"0\" />";
        output += "\n\t\t\t\t\t<input semantic=\"NORMAL\" source=\"#" + geometryID
            + "-normals\" offset=\"1\" />";
        output += "\n\t\t\t\t\t<input semantic=\"TEXCOORD\" source=\"#" + geometryID
            + "-texcoords\" offset=\"2\" set=\"1\" />";
        output += "\n\t\t\t\t\t<input semantic=\"COLOR\" source=\"#" + geometryID
            + "-colors\" offset=\"3\" set=\"0\" />";
        output += "\n\t\t\t\t\t<p>";
        for (std::int32_t i = 0; i < numVertices; ++i)
        {
            const std::string index = std::to_string(i);
            output += index + " " + index + " " + index + " " + index + " ";
        }
        output += "</p>";
        output += "\n\t\t\t\t</triangles>";
        output += "\n\t\t\t</mesh>";
        output += "\n\t\t</geometry>";

        results.emplace_back("geom" + std::to_string(meshCounter) + "_obj", meshVerts);
    }
    output += "\n\t</library_geometries>";

    output += "\n\t<library_effects>";
    std::unordered_map<std::int32_t, std::string> effectsInLibrary;
    for (const Material& material : model.Materials())
    {
        const std::string textureName = IsNullOrEmpty(material.Name()) ? "null" : material.Name();
        output += "\n\t\t<effect id=\"" + textureName + "-effect\">";
        output += "\n\t\t\t<profile_COMMON>";
        output += "\n\t\t\t\t<newparam sid=\"" + textureName + "-surface\">";
        output += "\n\t\t\t\t\t<surface type=\"2D\">";
        output += "\n\t\t\t\t\t\t<init_from>";

        const auto existing = effectsInLibrary.find(material.TextureId());
        if (existing != effectsInLibrary.end())
        {
            output += existing->second;
        }
        else
        {
            effectsInLibrary.emplace(material.TextureId(), textureName);
            output += textureName;
        }
        output += "</init_from>";
        output += "\n\t\t\t\t\t</surface>";
        output += "\n\t\t\t\t</newparam>";

        output += "\n\t\t\t\t<newparam sid=\"" + textureName + "-sampler\">";
        output += "\n\t\t\t\t\t<sampler2D>";
        output += "\n\t\t\t\t\t\t<source>" + textureName + "-surface</source>";
        output += "\n\t\t\t\t\t</sampler2D>";
        output += "\n\t\t\t\t</newparam>";

        output += "\n\t\t\t\t<technique sid=\"common\">";
        output += "\n\t\t\t\t\t<phong>";
        output += "\n\t\t\t\t\t\t<emission>";
        output += "\n\t\t\t\t\t\t\t<color sid=\"emission\">0 0 0 1</color>";
        output += "\n\t\t\t\t\t\t</emission>";
        output += "\n\t\t\t\t\t\t<ambient>";
        output += "\n\t\t\t\t\t\t\t<color sid=\"ambient\">0.5 0.5 0.5 1</color>";
        output += "\n\t\t\t\t\t\t</ambient>";
        output += "\n\t\t\t\t\t\t<diffuse>";
        output += "\n\t\t\t\t\t\t\t<texture texture=\"" + textureName
            + "-sampler\" texcoord=\"UVMap\" />";
        output += "\n\t\t\t\t\t\t</diffuse>";
        output += "\n\t\t\t\t\t\t<specular>";
        output += "\n\t\t\t\t\t\t\t<color sid=\"specular\">0.0 0.0 0.0 1</color>";
        output += "\n\t\t\t\t\t\t</specular>";
        output += "\n\t\t\t\t\t\t<shininess>";
        output += "\n\t\t\t\t\t\t\t<float sid=\"shininess\">5</float>";
        output += "\n\t\t\t\t\t\t</shininess>";
        output += "\n\t\t\t\t\t\t<index_of_refraction>";
        output += "\n\t\t\t\t\t\t\t<float sid=\"index_of_refraction\">1</float>";
        output += "\n\t\t\t\t\t\t</index_of_refraction>";
        output += "\n\t\t\t\t\t</phong>";
        output += "\n\t\t\t\t</technique>";
        output += "\n\t\t\t</profile_COMMON>";
        output += "\n\t\t</effect>";
    }
    output += "\n\t</library_effects>";

    output += "\n\t<library_visual_scenes>";
    output += "\n\t\t<visual_scene id=\"Scene\" name=\"Scene\">\n";
    if (Metadata::RoomMetadata().contains(model.Name()))
    {
        ExportRoomNodes(model, -1, output, 3, transformRoom);
    }
    else
    {
        ExportMeshes(model, output, 3);
    }
    output += "\t\t</visual_scene>";
    output += "\n\t</library_visual_scenes>";
    output += "\n</COLLADA>";

    const std::string exportPath = Paths::Combine(Paths::Export(), model.Name());
    WriteAllText(
        Paths::Combine(exportPath, model.Name() + "_" + recolor.Name() + ".dae"),
        output);
    return results;
}

void Collada::ExportRoomNodes(
    const Model& model, std::int32_t parentId, std::string& output,
    std::int32_t indent, bool transformRoom)
{
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(model.Nodes().size()); ++i)
    {
        const Node& node = model.Nodes().at(static_cast<std::size_t>(i));
        if (node.ParentIndex() == parentId)
        {
            Vector3 angle = Vector3::Zero;
            Vector3 scale(1.0F, 1.0F, 1.0F);
            Vector3 position = Vector3::Zero;
            if (transformRoom)
            {
                angle = Vector3(
                    RadiansToDegrees(node.Angle().X),
                    RadiansToDegrees(node.Angle().Y),
                    RadiansToDegrees(node.Angle().Z));
                scale = node.Scale();
                position = node.Position();
            }
            if (i == 0)
            {
                scale = Vector3(
                    scale.X * model.Scale().X,
                    scale.Y * model.Scale().Y,
                    scale.Z * model.Scale().Z);
            }

            AppendTabs(output, indent);
            output += "<node id=\"" + node.Name() + "\" type=\"NODE\">\n";
            AppendTabs(output, indent + 1);
            output += "<rotate>1.0 0.0 0.0 " + ManagedSingleToString(angle.X) + "</rotate>\n";
            AppendTabs(output, indent + 1);
            output += "<rotate>0.0 1.0 0.0 " + ManagedSingleToString(angle.Y) + "</rotate>\n";
            AppendTabs(output, indent + 1);
            output += "<rotate>0.0 0.0 1.0 " + ManagedSingleToString(angle.Z) + "</rotate>\n";
            AppendTabs(output, indent + 1);
            output += "<scale>" + FloatFormat(scale) + "</scale>\n";
            AppendTabs(output, indent + 1);
            output += "<translate>" + FloatFormat(position) + "</translate>\n";
            ExportNodeMeshes(model, i, output, indent + 1);
            if (node.ChildIndex() != -1)
            {
                ExportRoomNodes(model, i, output, indent + 1, transformRoom);
            }
            AppendTabs(output, indent);
            output += "</node>\n";
        }
    }
}

void Collada::ExportNodeMeshes(
    const Model& model, std::int32_t nodeId, std::string& output, std::int32_t indent)
{
    for (std::int32_t meshId :
        model.Nodes().at(static_cast<std::size_t>(nodeId)).GetMeshIds())
    {
        AppendTabs(output, indent);
        output += "<node id=\"geom" + std::to_string(meshId + 1) + "_obj\" type=\"NODE\">\n";
        ExportMesh(model, meshId, output, indent + 1);
        AppendTabs(output, indent);
        output += "</node>\n";
    }
}

void Collada::ExportMeshes(
    const Model& model, std::string& output, std::int32_t indent)
{
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(model.Meshes().size()); ++i)
    {
        AppendTabs(output, indent);
        output += "<node id=\"geom" + std::to_string(i + 1) + "_obj\" type=\"NODE\">\n";
        ExportMesh(model, i, output, indent + 1);
        AppendTabs(output, indent);
        output += "</node>\n";
    }
}

void Collada::ExportMesh(
    const Model& model, std::int32_t meshId, std::string& output, std::int32_t indent)
{
    const Mesh& mesh = model.Meshes().at(static_cast<std::size_t>(meshId));
    const Material& material = model.Materials().at(static_cast<std::size_t>(mesh.MaterialId()));
    const std::string textureName = IsNullOrEmpty(material.Name()) ? "null" : material.Name();

    AppendTabs(output, indent);
    output += "<instance_geometry url=\"#geometry" + std::to_string(meshId + 1) + "\">\n";
    AppendTabs(output, indent + 1);
    output += "<bind_material>\n";
    AppendTabs(output, indent + 2);
    output += "<technique_common>\n";
    AppendTabs(output, indent + 3);
    output += "<instance_material symbol=\"" + textureName
        + "-material\" target=\"#" + textureName + "-material\">\n";
    AppendTabs(output, indent + 4);
    output += "<bind_vertex_input semantic=\"UVMap\" input_semantic=\"TEXCOORD\" input_set=\"0\" />\n";
    AppendTabs(output, indent + 3);
    output += "</instance_material>\n";
    AppendTabs(output, indent + 2);
    output += "</technique_common>\n";
    AppendTabs(output, indent + 1);
    output += "</bind_material>\n";
    AppendTabs(output, indent);
    output += "</instance_geometry>\n";
}

void Collada::ExportDlist(
    const Model& model, std::int32_t dlistId,
    VertexList& meshVerts, VertexList& tempMeshVerts)
{
    std::array<float, 3> vtxState{0.0F, 0.0F, 0.0F};
    std::array<float, 3> nrmState{0.0F, 0.0F, 0.0F};
    std::array<float, 2> uvState{0.0F, 0.0F};
    std::array<float, 3> colState{1.0F, 1.0F, 1.0F};
    std::int32_t mtxState = 0;
    std::int32_t curMeshType = 0;
    bool curMeshActive = false;

    const auto& list = model.RenderInstructionLists().at(static_cast<std::size_t>(dlistId));
    for (const RenderInstruction& instruction : list)
    {
        switch (instruction.Code)
        {
        case InstructionCode::MTX_RESTORE:
            mtxState = static_cast<std::int32_t>(instruction.Arguments.at(0));
            break;

        case InstructionCode::BEGIN_VTXS:
            if (instruction.Arguments.at(0) > 3U)
            {
                throw ProgramException("Invalid geo type");
            }
            curMeshType = static_cast<std::int32_t>(instruction.Arguments.at(0)) + 1;
            curMeshActive = true;
            meshVerts.clear();
            break;

        case InstructionCode::COLOR:
        {
            const std::uint32_t rgb = instruction.Arguments.at(0);
            const std::uint32_t r = (rgb >> 0) & 0x1FU;
            const std::uint32_t g = (rgb >> 5) & 0x1FU;
            const std::uint32_t b = (rgb >> 10) & 0x1FU;
            colState[0] = r / 31.0F;
            colState[1] = g / 31.0F;
            colState[2] = b / 31.0F;
            break;
        }

        case InstructionCode::NORMAL:
        {
            const std::uint32_t xyz = instruction.Arguments.at(0);
            const std::int32_t x = SignExtend10(xyz >> 0);
            const std::int32_t y = SignExtend10(xyz >> 10);
            const std::int32_t z = SignExtend10(xyz >> 20);
            nrmState[0] = x / 512.0F;
            nrmState[1] = y / 512.0F;
            nrmState[2] = z / 512.0F;
            break;
        }

        case InstructionCode::TEXCOORD:
        {
            const std::uint32_t st = instruction.Arguments.at(0);
            const std::int32_t s = SignExtend16(st >> 0);
            const std::int32_t t = SignExtend16(st >> 16);
            uvState[0] = s / 16.0F;
            uvState[1] = t / 16.0F;
            break;
        }

        case InstructionCode::VTX_16:
        {
            const std::uint32_t xy = instruction.Arguments.at(0);
            const std::int32_t x = SignExtend16(xy >> 0);
            const std::int32_t y = SignExtend16(xy >> 16);
            const std::int32_t z = SignExtend16(instruction.Arguments.at(1));
            vtxState[0] = Fixed::ToFloat(x);
            vtxState[1] = Fixed::ToFloat(y);
            vtxState[2] = Fixed::ToFloat(z);
            if (curMeshActive)
            {
                meshVerts.push_back(GetCurrentExportTri(
                    vtxState, nrmState, uvState, colState, mtxState));
            }
            break;
        }

        case InstructionCode::VTX_10:
        {
            const std::uint32_t xyz = instruction.Arguments.at(0);
            const std::int32_t x = SignExtend10(xyz >> 0);
            const std::int32_t y = SignExtend10(xyz >> 10);
            const std::int32_t z = SignExtend10(xyz >> 20);
            vtxState[0] = x / 64.0F;
            vtxState[1] = y / 64.0F;
            vtxState[2] = z / 64.0F;
            if (curMeshActive)
            {
                meshVerts.push_back(GetCurrentExportTri(
                    vtxState, nrmState, uvState, colState, mtxState));
            }
            break;
        }

        case InstructionCode::VTX_XY:
        {
            const std::uint32_t xy = instruction.Arguments.at(0);
            const std::int32_t x = SignExtend16(xy >> 0);
            const std::int32_t y = SignExtend16(xy >> 16);
            vtxState[0] = Fixed::ToFloat(x);
            vtxState[1] = Fixed::ToFloat(y);
            if (curMeshActive)
            {
                meshVerts.push_back(GetCurrentExportTri(
                    vtxState, nrmState, uvState, colState, mtxState));
            }
            break;
        }

        case InstructionCode::VTX_XZ:
        {
            const std::uint32_t xz = instruction.Arguments.at(0);
            const std::int32_t x = SignExtend16(xz >> 0);
            const std::int32_t z = SignExtend16(xz >> 16);
            vtxState[0] = Fixed::ToFloat(x);
            vtxState[2] = Fixed::ToFloat(z);
            if (curMeshActive)
            {
                meshVerts.push_back(GetCurrentExportTri(
                    vtxState, nrmState, uvState, colState, mtxState));
            }
            break;
        }

        case InstructionCode::VTX_YZ:
        {
            const std::uint32_t yz = instruction.Arguments.at(0);
            const std::int32_t y = SignExtend16(yz >> 0);
            const std::int32_t z = SignExtend16(yz >> 16);
            vtxState[1] = Fixed::ToFloat(y);
            vtxState[2] = Fixed::ToFloat(z);
            if (curMeshActive)
            {
                meshVerts.push_back(GetCurrentExportTri(
                    vtxState, nrmState, uvState, colState, mtxState));
            }
            break;
        }

        case InstructionCode::VTX_DIFF:
        {
            const std::uint32_t xyz = instruction.Arguments.at(0);
            const std::int32_t x = SignExtend10(xyz >> 0);
            const std::int32_t y = SignExtend10(xyz >> 10);
            const std::int32_t z = SignExtend10(xyz >> 20);
            vtxState[0] += Fixed::ToFloat(x);
            vtxState[1] += Fixed::ToFloat(y);
            vtxState[2] += Fixed::ToFloat(z);
            if (curMeshActive)
            {
                meshVerts.push_back(GetCurrentExportTri(
                    vtxState, nrmState, uvState, colState, mtxState));
            }
            break;
        }

        case InstructionCode::END_VTXS:
        {
            curMeshActive = false;
            VertexList triangulatedMesh;

            switch (curMeshType)
            {
            case 1:
                triangulatedMesh.insert(
                    triangulatedMesh.end(), meshVerts.begin(), meshVerts.end());
                break;

            case 2:
                if (meshVerts.size() > 3)
                {
                    for (std::size_t i = 0; i < meshVerts.size(); i += 4)
                    {
                        const Vertex& a = meshVerts.at(i);
                        const Vertex& b = meshVerts.at(i + 1);
                        const Vertex& c = meshVerts.at(i + 2);
                        const Vertex& d = meshVerts.at(i + 3);
                        triangulatedMesh.push_back(a);
                        triangulatedMesh.push_back(b);
                        triangulatedMesh.push_back(c);
                        triangulatedMesh.push_back(c);
                        triangulatedMesh.push_back(d);
                        triangulatedMesh.push_back(a);
                    }
                }
                break;

            case 3:
                if (meshVerts.size() > 2)
                {
                    for (std::size_t i = 0; i < meshVerts.size() - 2; ++i)
                    {
                        const Vertex& a = meshVerts.at(i);
                        const Vertex& b = meshVerts.at(i + 1);
                        const Vertex& c = meshVerts.at(i + 2);
                        if (i % 2 > 0)
                        {
                            triangulatedMesh.push_back(c);
                            triangulatedMesh.push_back(b);
                            triangulatedMesh.push_back(a);
                        }
                        else
                        {
                            triangulatedMesh.push_back(a);
                            triangulatedMesh.push_back(b);
                            triangulatedMesh.push_back(c);
                        }
                    }
                }
                break;

            case 4:
                if (meshVerts.size() > 3)
                {
                    for (std::size_t i = 0; i < meshVerts.size() - 2; i += 2)
                    {
                        const Vertex& a = meshVerts.at(i);
                        const Vertex& b = meshVerts.at(i + 1);
                        const Vertex& c = meshVerts.at(i + 2);
                        const Vertex& d = meshVerts.at(i + 3);
                        triangulatedMesh.push_back(a);
                        triangulatedMesh.push_back(b);
                        triangulatedMesh.push_back(c);
                        triangulatedMesh.push_back(d);
                        triangulatedMesh.push_back(c);
                        triangulatedMesh.push_back(b);
                    }
                }
                break;

            default:
                break;
            }

            curMeshType = -1;
            tempMeshVerts.insert(
                tempMeshVerts.end(), triangulatedMesh.begin(), triangulatedMesh.end());
            meshVerts.clear();
            break;
        }

        case InstructionCode::DIF_AMB:
        case InstructionCode::NOP:
            break;

        default:
            throw ProgramException("Unknown opcode");
        }
    }
}

Collada::Vertex Collada::GetCurrentExportTri(
    const std::array<float, 3>& vtxState,
    const std::array<float, 3>& nrmState,
    const std::array<float, 2>& uvState,
    const std::array<float, 3>& colState,
    std::int32_t mtxState)
{
    return Vertex(
        Vector3(vtxState[0], vtxState[1], vtxState[2]),
        Vector3(nrmState[0], nrmState[1], nrmState[2]),
        Vector3(colState[0], colState[1], colState[2]),
        Vector2(uvState[0], uvState[1]),
        mtxState);
}
}
