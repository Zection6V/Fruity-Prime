#include "Scripting.hpp"

#include "../Formats/Enums.hpp"
#include "../Formats/Formats.hpp"
#include "../Formats/Model.hpp"
#include "../Formats/RawFormats.hpp"
#include "../Program.hpp"
#include "../Read.hpp"
#include "../NativeRuntime/System/Globalization.hpp"
#include "../NativeRuntime/System/IO.hpp"
#include "../NativeRuntime/System/Managed.hpp"
#include "NativeRuntime/System/Globalization.hpp"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

using ::MphRead::NativeRuntime::PathFromUtf8;
using ::MphRead::NativeRuntime::PathGetFullPath;
using ::MphRead::NativeRuntime::PathToUtf8;
using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::NativeRuntime::StringTrimView;

namespace
{
    using MphRead::BillboardMode;
    using MphRead::CullingMode;
    using MphRead::InstructionCode;
    using MphRead::MaterialAnimation;
    using MphRead::MaterialAnimationGroup;
    using MphRead::NodeAnimation;
    using MphRead::NodeAnimationGroup;
    using MphRead::RepeatMode;
    using MphRead::TexcoordAnimation;
    using MphRead::TexcoordAnimationGroup;
    using MphRead::TextureAnimation;
    using MphRead::TextureAnimationGroup;
    using OpenTK::Mathematics::Vector3;

#if defined(_WIN32)
    constexpr std::string_view ManagedNewLine = "\r\n";
#else
    constexpr std::string_view ManagedNewLine = "\n";
#endif

    void AppendLine(std::string &sb)
    {
        sb.append(ManagedNewLine);
    }

    void AppendLine(std::string &sb, std::string_view text)
    {
        sb.append(text);
        sb.append(ManagedNewLine);
    }

    [[nodiscard]] bool Vector3Equal(Vector3 left, Vector3 right) noexcept
    {
        return left.X == right.X && left.Y == right.Y && left.Z == right.Z;
    }

    struct Utf8CodePoint final
    {
        std::uint32_t Value;
        std::size_t Length;
    };

    [[nodiscard]] std::int32_t FindCombo(
        const std::vector<std::pair<std::int32_t, std::int32_t>> &combos,
        const std::pair<std::int32_t, std::int32_t> &pair) noexcept
    {
        for (std::size_t i = 0; i < combos.size(); ++i)
        {
            if (combos[i] == pair)
            {
                return static_cast<std::int32_t>(i);
            }
        }
        return -1;
    }

    [[nodiscard]] std::string JoinIntegers(const std::vector<std::int32_t> &values)
    {
        std::string result;
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            if (i != 0)
            {
                result += ", ";
            }
            result += std::to_string(values[i]);
        }
        return result;
    }
}

namespace MphRead::Export
{
    void StringBuilderExtensions::AppendIndent(std::string &sb)
    {
        sb.append(4, ' ');
    }

    void StringBuilderExtensions::AppendIndent(
        std::string &sb, const std::string &text, std::int32_t indent)
    {
        const std::string_view trimmed = StringTrimView(text);
        std::string normalized;
        normalized.reserve(trimmed.size());
        for (std::size_t i = 0; i < trimmed.size(); ++i)
        {
            if (trimmed[i] == '\r')
            {
                normalized.push_back('\n');
                if (i + 1 < trimmed.size() && trimmed[i + 1] == '\n')
                {
                    ++i;
                }
            }
            else
            {
                normalized.push_back(trimmed[i]);
            }
        }

        std::size_t start = 0;
        while (true)
        {
            const std::size_t newline = normalized.find('\n', start);
            if (indent > 0)
            {
                sb.append(static_cast<std::size_t>(indent) * 4, ' ');
            }
            if (newline == std::string::npos)
            {
                AppendLine(sb, std::string_view(normalized).substr(start));
                break;
            }
            AppendLine(sb, std::string_view(normalized).substr(start, newline - start));
            start = newline + 1;
        }
    }

    void Scripting::PrintAnimations(const Model &model, std::string &sb)
    {
        const std::int32_t indent = 1;
        StringBuilderExtensions::AppendIndent(sb, "uv_anims = [", indent);
        for (const auto &groupRef : RequireReference(RequireReference(model.AnimationGroups).Texcoord))
        {
            const TexcoordAnimationGroup &group = RequireReference(groupRef);
            if (RequireReference(group.Animations).empty())
            {
                continue;
            }
            StringBuilderExtensions::AppendIndent(sb, "{", indent + 1);
            for (const auto &kvp : RequireReference(group.Animations))
            {
                const TexcoordAnimation &anim = kvp.second;
                StringBuilderExtensions::AppendIndent(sb, "'" + kvp.first + "':", indent + 2);
                StringBuilderExtensions::AppendIndent(sb, "[", indent + 2);
                for (std::int32_t frame = 0; frame < group.FrameCount; ++frame)
                {
                    const float scaleS = model.InterpolateAnimation(group.Scales, anim.ScaleLutIndexS, frame,
                                                                    anim.ScaleBlendS, anim.ScaleLutLengthS, group.FrameCount);
                    const float scaleT = model.InterpolateAnimation(group.Scales, anim.ScaleLutIndexT, frame,
                                                                    anim.ScaleBlendT, anim.ScaleLutLengthT, group.FrameCount);
                    const float rotate = model.InterpolateAnimation(group.Rotations, anim.RotateLutIndexZ, frame,
                                                                    anim.RotateBlendZ, anim.RotateLutLengthZ, group.FrameCount, true);
                    const float translateS = model.InterpolateAnimation(group.Translations, anim.TranslateLutIndexS, frame,
                                                                        anim.TranslateBlendS, anim.TranslateLutLengthS, group.FrameCount);
                    const float translateT = model.InterpolateAnimation(group.Translations, anim.TranslateLutIndexT, frame,
                                                                        anim.TranslateBlendT, anim.TranslateLutLengthT, group.FrameCount);
                    StringBuilderExtensions::AppendIndent(sb,
                                                          "[" + ::MphRead::NativeRuntime::ToString(scaleS) + ", " + ::MphRead::NativeRuntime::ToString(scaleT) + ", " + ::MphRead::NativeRuntime::ToString(rotate) + ", " + ::MphRead::NativeRuntime::ToString(translateS) + ", " + ::MphRead::NativeRuntime::ToString(translateT) + "],", indent + 3);
                }
                StringBuilderExtensions::AppendIndent(sb, "],", indent + 2);
            }
            StringBuilderExtensions::AppendIndent(sb, "},", indent + 1);
        }
        StringBuilderExtensions::AppendIndent(sb, "]", indent);

        StringBuilderExtensions::AppendIndent(sb, "mat_anims = [", indent);
        for (const auto &groupRef : RequireReference(RequireReference(model.AnimationGroups).Material))
        {
            const MaterialAnimationGroup &group = RequireReference(groupRef);
            if (RequireReference(group.Animations).empty())
            {
                continue;
            }
            StringBuilderExtensions::AppendIndent(sb, "{", indent + 1);
            for (const auto &kvp : RequireReference(group.Animations))
            {
                const MaterialAnimation &anim = kvp.second;
                StringBuilderExtensions::AppendIndent(sb, "'" + kvp.first + "_mat':", indent + 2);
                StringBuilderExtensions::AppendIndent(sb, "[", indent + 2);
                for (std::int32_t frame = 0; frame < group.FrameCount; ++frame)
                {
                    const float red = model.InterpolateAnimation(group.Colors, anim.DiffuseLutIndexR, frame,
                                                                 anim.DiffuseBlendR, anim.DiffuseLutLengthR, group.FrameCount);
                    const float green = model.InterpolateAnimation(group.Colors, anim.DiffuseLutIndexG, frame,
                                                                   anim.DiffuseBlendG, anim.DiffuseLutLengthG, group.FrameCount);
                    const float blue = model.InterpolateAnimation(group.Colors, anim.DiffuseLutIndexB, frame,
                                                                  anim.DiffuseBlendB, anim.DiffuseLutLengthB, group.FrameCount);
                    const float alpha = model.InterpolateAnimation(group.Colors, anim.AlphaLutIndex, frame,
                                                                   anim.AlphaBlend, anim.AlphaLutLength, group.FrameCount);
                    StringBuilderExtensions::AppendIndent(sb,
                                                          "[" + ::MphRead::NativeRuntime::ToString(red / 31.0F) + ", " + ::MphRead::NativeRuntime::ToString(green / 31.0F) + ", " + ::MphRead::NativeRuntime::ToString(blue / 31.0F) + ", " + ::MphRead::NativeRuntime::ToString(alpha / 31.0F) + "],", indent + 3);
                }
                StringBuilderExtensions::AppendIndent(sb, "],", indent + 2);
            }
            StringBuilderExtensions::AppendIndent(sb, "},", indent + 1);
        }
        StringBuilderExtensions::AppendIndent(sb, "]", indent);

        StringBuilderExtensions::AppendIndent(sb, "tex_anims = [", indent);
        std::vector<std::pair<std::int32_t, std::int32_t>> combos;
        for (const auto &groupRef : RequireReference(RequireReference(model.AnimationGroups).Texture))
        {
            const TextureAnimationGroup &group = RequireReference(groupRef);
            for (const auto &kvp : RequireReference(group.Animations))
            {
                for (std::int32_t i = kvp.second.StartIndex; i < kvp.second.StartIndex + kvp.second.Count; ++i)
                {
                    const auto pair = std::make_pair(
                        static_cast<std::int32_t>(RequireReference(group.TextureIds).at(static_cast<std::size_t>(i))),
                        static_cast<std::int32_t>(RequireReference(group.PaletteIds).at(static_cast<std::size_t>(i))));
                    if (std::find(combos.begin(), combos.end(), pair) == combos.end())
                    {
                        combos.push_back(pair);
                    }
                }
            }
        }
        for (const auto &groupRef : RequireReference(RequireReference(model.AnimationGroups).Texture))
        {
            const TextureAnimationGroup &group = RequireReference(groupRef);
            if (RequireReference(group.Animations).empty())
            {
                continue;
            }
            StringBuilderExtensions::AppendIndent(sb, "{", indent + 1);
            for (const auto &kvp : RequireReference(group.Animations))
            {
                const TextureAnimation &anim = kvp.second;
                StringBuilderExtensions::AppendIndent(sb, "'" + kvp.first + "_mat':", indent + 2);
                StringBuilderExtensions::AppendIndent(sb, "[", indent + 2);
                for (std::int32_t i = anim.StartIndex; i < anim.StartIndex + anim.Count; ++i)
                {
                    const std::int32_t frame = static_cast<std::int32_t>(
                        RequireReference(group.FrameIndices).at(static_cast<std::size_t>(i)));
                    const auto pair = std::make_pair(
                        static_cast<std::int32_t>(RequireReference(group.TextureIds).at(static_cast<std::size_t>(i))),
                        static_cast<std::int32_t>(RequireReference(group.PaletteIds).at(static_cast<std::size_t>(i))));
                    const std::int32_t index = FindCombo(combos, pair);
                    assert(index != -1);
                    StringBuilderExtensions::AppendIndent(sb,
                                                          "[" + std::to_string(frame) + ", " + std::to_string(index) + "],", indent + 3);
                }
                StringBuilderExtensions::AppendIndent(sb, "],", indent + 2);
            }
            StringBuilderExtensions::AppendIndent(sb, "},", indent + 1);
        }
        StringBuilderExtensions::AppendIndent(sb, "]", indent);

        StringBuilderExtensions::AppendIndent(sb, "node_anims = [", indent);
        for (const auto &groupRef : RequireReference(RequireReference(model.AnimationGroups).Node))
        {
            const NodeAnimationGroup &group = RequireReference(groupRef);
            if (RequireReference(group.Animations).empty())
            {
                continue;
            }
            StringBuilderExtensions::AppendIndent(sb, "{", indent + 1);
            for (const auto &kvp : RequireReference(group.Animations))
            {
                const NodeAnimation &anim = kvp.second;
                StringBuilderExtensions::AppendIndent(sb, "'" + kvp.first + "':", indent + 2);
                StringBuilderExtensions::AppendIndent(sb, "[", indent + 2);
                for (std::int32_t frame = 0; frame < group.FrameCount; ++frame)
                {
                    const float scaleX = model.InterpolateAnimation(group.Scales, anim.ScaleLutIndexX, frame,
                                                                    anim.ScaleBlendX, anim.ScaleLutLengthX, group.FrameCount);
                    const float scaleY = model.InterpolateAnimation(group.Scales, anim.ScaleLutIndexY, frame,
                                                                    anim.ScaleBlendY, anim.ScaleLutLengthY, group.FrameCount);
                    const float scaleZ = model.InterpolateAnimation(group.Scales, anim.ScaleLutIndexZ, frame,
                                                                    anim.ScaleBlendZ, anim.ScaleLutLengthZ, group.FrameCount);
                    const float rotateX = model.InterpolateAnimation(group.Rotations, anim.RotateLutIndexX, frame,
                                                                     anim.RotateBlendX, anim.RotateLutLengthX, group.FrameCount, true);
                    const float rotateY = model.InterpolateAnimation(group.Rotations, anim.RotateLutIndexY, frame,
                                                                     anim.RotateBlendY, anim.RotateLutLengthY, group.FrameCount, true);
                    const float rotateZ = model.InterpolateAnimation(group.Rotations, anim.RotateLutIndexZ, frame,
                                                                     anim.RotateBlendZ, anim.RotateLutLengthZ, group.FrameCount, true);
                    const float translateX = model.InterpolateAnimation(group.Translations, anim.TranslateLutIndexX, frame,
                                                                        anim.TranslateBlendX, anim.TranslateLutLengthX, group.FrameCount);
                    const float translateY = model.InterpolateAnimation(group.Translations, anim.TranslateLutIndexY, frame,
                                                                        anim.TranslateBlendY, anim.TranslateLutLengthY, group.FrameCount);
                    const float translateZ = model.InterpolateAnimation(group.Translations, anim.TranslateLutIndexZ, frame,
                                                                        anim.TranslateBlendZ, anim.TranslateLutLengthZ, group.FrameCount);
                    StringBuilderExtensions::AppendIndent(sb,
                                                          "[" + ::MphRead::NativeRuntime::ToString(scaleX) + ", " + ::MphRead::NativeRuntime::ToString(scaleY) + ", " + ::MphRead::NativeRuntime::ToString(scaleZ) + ", " + ::MphRead::NativeRuntime::ToString(rotateX) + ", " + ::MphRead::NativeRuntime::ToString(rotateY) + ", " + ::MphRead::NativeRuntime::ToString(rotateZ) + ", " + ::MphRead::NativeRuntime::ToString(translateX) + ", " + ::MphRead::NativeRuntime::ToString(translateY) + ", " + ::MphRead::NativeRuntime::ToString(translateZ) + "],", indent + 3);
                }
                StringBuilderExtensions::AppendIndent(sb, "],", indent + 2);
            }
            StringBuilderExtensions::AppendIndent(sb, "},", indent + 1);
        }
        StringBuilderExtensions::AppendIndent(sb, "]", indent);
    }

    std::string Scripting::GenerateScript(const Model &model,
                                          const std::vector<std::pair<std::string, std::vector<Collada::Vertex>>> &lists)
    {
        std::string sb;
        AppendLine(sb, "import bpy");
        AppendLine(sb, "import math");
        AppendLine(sb, "import mathutils");
        AppendLine(sb, "from mph_common import *");
        AppendLine(sb);
        AppendLine(sb, "export_version = '" + Program::Version.ToString() + "'");

        std::string recolors;
        for (std::size_t i = 0; i < RequireReference(model.Recolors).size(); ++i)
        {
            if (i != 0)
            {
                recolors += ", ";
            }
            recolors += RequireReference(RequireReference(model.Recolors).at(i)).Name;
        }
        AppendLine(sb, "# recolors: " + recolors);
        AppendLine(sb, "recolor = '" + RequireReference(RequireReference(model.Recolors).at(0)).Name + "'");

        const auto uvAnimCount = static_cast<std::int32_t>(std::count_if(
            RequireReference(RequireReference(model.AnimationGroups).Texcoord).begin(), RequireReference(RequireReference(model.AnimationGroups).Texcoord).end(),
            [](const std::shared_ptr<TexcoordAnimationGroup> &group)
            { return !RequireReference(RequireReference(group).Animations).empty(); }));
        const auto matAnimCount = static_cast<std::int32_t>(std::count_if(
            RequireReference(RequireReference(model.AnimationGroups).Material).begin(), RequireReference(RequireReference(model.AnimationGroups).Material).end(),
            [](const std::shared_ptr<MaterialAnimationGroup> &group)
            { return !RequireReference(RequireReference(group).Animations).empty(); }));
        const auto nodeAnimCount = static_cast<std::int32_t>(std::count_if(
            RequireReference(RequireReference(model.AnimationGroups).Node).begin(), RequireReference(RequireReference(model.AnimationGroups).Node).end(),
            [](const std::shared_ptr<NodeAnimationGroup> &group)
            { return !RequireReference(RequireReference(group).Animations).empty(); }));
        const auto texAnimCount = static_cast<std::int32_t>(std::count_if(
            RequireReference(RequireReference(model.AnimationGroups).Texture).begin(), RequireReference(RequireReference(model.AnimationGroups).Texture).end(),
            [](const std::shared_ptr<TextureAnimationGroup> &group)
            { return !RequireReference(RequireReference(group).Animations).empty(); }));
        AppendLine(sb, "# uv anims: " + std::to_string(uvAnimCount) + ", mat anims: " + std::to_string(matAnimCount) + ", node anims: " + std::to_string(nodeAnimCount) + ", tex anims: " + std::to_string(texAnimCount));

        const std::int32_t texcoordId = uvAnimCount > 0 ? 0 : -1;
        const std::int32_t materialId = matAnimCount > 0 ? 0 : -1;
        const std::int32_t nodeId = nodeAnimCount > 0 ? 0 : -1;
        const std::int32_t textureId = texAnimCount > 0 ? 0 : -1;
        AppendLine(sb, "uv_index = " + std::to_string(texcoordId));
        AppendLine(sb, "mat_index = " + std::to_string(materialId));
        AppendLine(sb, "node_index = " + std::to_string(nodeId));
        AppendLine(sb, "tex_index = " + std::to_string(textureId));
        AppendLine(sb);

        AppendLine(sb, "def import_dae(suffix):");
        StringBuilderExtensions::AppendIndent(sb);
        AppendLine(sb, "cleanup()");
        StringBuilderExtensions::AppendIndent(sb);
        const std::string daePath = PathGetFullPath(Paths::Combine(
            Paths::Combine(Paths::Export(), model.Name), model.Name + "_{suffix}.dae"));
        AppendLine(sb, "bpy.ops.wm.collada_import(filepath =");
        StringBuilderExtensions::AppendIndent(sb);
        StringBuilderExtensions::AppendIndent(sb);
        AppendLine(sb, "fr\"" + daePath + "\")");
        StringBuilderExtensions::AppendIndent(sb);
        AppendLine(sb, "set_common()");

        std::unordered_set<std::int32_t> invertMeshIds;
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(RequireReference(model.Materials).size()); ++i)
        {
            const Material &material = RequireReference(RequireReference(model.Materials).at(static_cast<std::size_t>(i)));
            if (material.TextureId != -1)
            {
                StringBuilderExtensions::AppendIndent(sb);
                const auto &pixels = RequireReference(RequireReference(model.Recolors).at(0)).GetPixels(material.TextureId, material.PaletteId);
                const bool alphaPixels = std::any_of(pixels.begin(), pixels.end(),
                                                     [](const auto &pixel)
                                                     { return pixel.Alpha < 255; });
                AppendLine(sb, "set_texture_alpha('" + material.Name + "_mat', " + std::to_string(material.Alpha) + ", " + (alphaPixels ? "True" : "False") + ")");
                const bool mirrorX = material.XRepeat == RepeatMode::Mirror;
                const bool mirrorY = material.YRepeat == RepeatMode::Mirror;
                if (mirrorX || mirrorY)
                {
                    StringBuilderExtensions::AppendIndent(sb);
                    AppendLine(sb, "set_mirror('" + material.Name + "_mat', " + (mirrorX ? "True" : "False") + ", " + (mirrorY ? "True" : "False") + ")");
                }
            }
            else
            {
                StringBuilderExtensions::AppendIndent(sb);
                AppendLine(sb, "set_material_alpha('" + material.Name + "_mat', " + std::to_string(material.Alpha) + ")");
            }

            if (material.Culling == CullingMode::Back || material.Culling == CullingMode::Front)
            {
                StringBuilderExtensions::AppendIndent(sb);
                AppendLine(sb, "set_back_culling('" + material.Name + "_mat')");
                if (material.Culling == CullingMode::Front)
                {
                    for (std::int32_t j = 0; j < static_cast<std::int32_t>(RequireReference(model.Meshes).size()); ++j)
                    {
                        const Mesh &mesh = RequireReference(RequireReference(model.Meshes).at(static_cast<std::size_t>(j)));
                        if (mesh.MaterialId == i)
                        {
                            invertMeshIds.insert(j);
                        }
                    }
                }
            }

            std::vector<std::int32_t> withColor;
            std::vector<std::int32_t> noColor;
            for (std::int32_t j = 0; j < static_cast<std::int32_t>(RequireReference(model.Meshes).size()); ++j)
            {
                const Mesh &mesh = RequireReference(RequireReference(model.Meshes).at(static_cast<std::size_t>(j)));
                if (mesh.MaterialId == i)
                {
                    const auto &instructions = RequireReference(model.RenderInstructionLists).at(
                        static_cast<std::size_t>(mesh.DlistId));
                    const bool hasColor = std::any_of(RequireReference(instructions).begin(), RequireReference(instructions).end(),
                                                      [](const std::shared_ptr<RenderInstruction> &instruction)
                                                      {
                                                          return RequireReference(instruction).Code == InstructionCode::COLOR;
                                                      });
                    if (hasColor)
                    {
                        withColor.push_back(j);
                    }
                    else
                    {
                        noColor.push_back(j);
                    }
                }
            }
            if (!noColor.empty())
            {
                const auto diffuse = material.Diffuse;
                const std::string color = ::MphRead::NativeRuntime::ToString(diffuse.Red / 31.0F) + ", " + ::MphRead::NativeRuntime::ToString(diffuse.Green / 31.0F) + ", " + ::MphRead::NativeRuntime::ToString(diffuse.Blue / 31.0F);
                if (!withColor.empty())
                {
                    std::string objects;
                    for (std::size_t j = 0; j < noColor.size(); ++j)
                    {
                        if (j != 0)
                        {
                            objects += "', '";
                        }
                        objects += "geom" + std::to_string(noColor[j]) + "_obj";
                    }
                    StringBuilderExtensions::AppendIndent(sb,
                                                          "set_mat_color('" + material.Name + "_mat', " + color + ", True, ['" + objects + "'])");
                }
                else
                {
                    StringBuilderExtensions::AppendIndent(sb,
                                                          "set_mat_color('" + material.Name + "_mat', " + color + ", False, [])");
                }
            }
        }

        for (const auto &nodeRef : RequireReference(model.Nodes))
        {
            const Node &node = RequireReference(nodeRef);
            for (std::int32_t meshId : node.GetMeshIds())
            {
                if (invertMeshIds.find(meshId) != invertMeshIds.end())
                {
                    StringBuilderExtensions::AppendIndent(sb);
                    AppendLine(sb, "invert_normals('geom" + std::to_string(meshId + 1) + "_obj')");
                }
            }
        }
        if (!RequireReference(model.NodeMatrixIds).empty())
        {
            StringBuilderExtensions::AppendIndent(sb, "bone_setup()");
        }
        StringBuilderExtensions::AppendIndent(sb, "anim_setup()");
        for (const auto &nodeRef : RequireReference(model.Nodes))
        {
            const Node &node = RequireReference(nodeRef);
            if (node.BillboardMode == BillboardMode::None)
            {
                continue;
            }
            for (std::int32_t meshId : node.GetMeshIds())
            {
                StringBuilderExtensions::AppendIndent(sb);
                AppendLine(sb, "set_billboard('geom" + std::to_string(meshId + 1) + "_obj', " + std::to_string(static_cast<std::int32_t>(node.BillboardMode)) + ")");
            }
        }

        if (!RequireReference(model.NodeMatrixIds).empty())
        {
            AppendLine(sb);
            AppendLine(sb, "def bone_setup():");
            StringBuilderExtensions::AppendIndent(sb, "bpy.ops.object.mode_set(mode = 'OBJECT')");
            StringBuilderExtensions::AppendIndent(sb,
                                                  "\nbpy.ops.object.armature_add(enter_editmode=True, align='WORLD', location=(0, 0, 0))\n"
                                                  "bpy.ops.armature.select_all(action='SELECT')\n"
                                                  "bpy.ops.armature.delete()");

            for (const auto &nodeRef : RequireReference(model.Nodes))
            {
                const Node &node = RequireReference(nodeRef);
                StringBuilderExtensions::AppendIndent(sb,
                                                      "bpy.ops.armature.bone_primitive_add(name='" + node.Name + "')");
            }
            StringBuilderExtensions::AppendIndent(sb, "bpy.ops.armature.select_all(action='DESELECT')");
            StringBuilderExtensions::AppendIndent(sb, "bones = bpy.data.armatures[0].edit_bones");

            for (const auto &childRef : RequireReference(model.Nodes))
            {
                const Node &child = RequireReference(childRef);
                if (child.ParentIndex == -1)
                {
                    continue;
                }
                const Node &parent = RequireReference(RequireReference(model.Nodes).at(static_cast<std::size_t>(child.ParentIndex)));
                StringBuilderExtensions::AppendIndent(sb,
                                                      "bones.get('" + child.Name + "').parent = bones.get('" + parent.Name + "')");
            }

            StringBuilderExtensions::AppendIndent(sb,
                                                  "\nbpy.ops.object.editmode_toggle()\n"
                                                  "bpy.ops.object.select_all(action='DESELECT')\n"
                                                  "for obj in bpy.data.objects:\n"
                                                  "    if obj.type == 'MESH':\n"
                                                  "        obj.select_set(True)\n"
                                                  "bpy.data.objects['Armature'].select_set(True)\n"
                                                  "bpy.ops.object.parent_set(type='ARMATURE_NAME')");

            for (const auto &obj : lists)
            {
                StringBuilderExtensions::AppendIndent(sb, "bpy.ops.object.select_all(action='DESELECT')");
                StringBuilderExtensions::AppendIndent(sb, "obj = bpy.data.objects['" + obj.first + "']");
                StringBuilderExtensions::AppendIndent(sb, "obj.select_set(True)");
                std::vector<std::pair<std::string, std::vector<std::int32_t>>> vertices;
                std::int32_t i = 0;
                for (const Collada::Vertex &vertex : obj.second)
                {
                    const std::int32_t nodeIndex = RequireReference(model.NodeMatrixIds).at(
                        static_cast<std::size_t>(vertex.MatrixId));
                    const Node &node = RequireReference(RequireReference(model.Nodes).at(static_cast<std::size_t>(nodeIndex)));
                    const auto existing = std::find_if(vertices.begin(), vertices.end(),
                                                       [&node](const auto &item)
                                                       { return item.first == node.Name; });
                    if (existing == vertices.end())
                    {
                        vertices.emplace_back(node.Name, std::vector<std::int32_t>{i});
                    }
                    else
                    {
                        existing->second.push_back(i);
                    }
                    ++i;
                }
                for (const auto &kvp : vertices)
                {
                    StringBuilderExtensions::AppendIndent(sb, "group = obj.vertex_groups['" + kvp.first + "']");
                    StringBuilderExtensions::AppendIndent(sb,
                                                          "group.add([" + JoinIntegers(kvp.second) + "], 1.0, 'ADD')");
                }
            }

            for (const auto &nodeRef : RequireReference(model.Nodes))
            {
                const Node &node = RequireReference(nodeRef);
                StringBuilderExtensions::AppendIndent(sb,
                                                      "bone = bpy.data.objects['Armature'].pose.bones['" + node.Name + "']");
                StringBuilderExtensions::AppendIndent(sb, "bone.rotation_mode = 'XYZ'");
                const Vector3 scale = node.Scale;
                if (!Vector3Equal(scale, Vector3(1.0F, 1.0F, 1.0F)))
                {
                    StringBuilderExtensions::AppendIndent(sb,
                                                          "bone.scale = mathutils.Vector((" + ::MphRead::NativeRuntime::ToString(scale.X) + ", " + ::MphRead::NativeRuntime::ToString(scale.Y) + ", " + ::MphRead::NativeRuntime::ToString(scale.Z) + "))");
                }
                const Vector3 angle = node.Angle;
                if (!Vector3Equal(angle, Vector3::Zero))
                {
                    StringBuilderExtensions::AppendIndent(sb,
                                                          "bone.rotation_euler = mathutils.Vector((" + ::MphRead::NativeRuntime::ToString(angle.X) + ", " + ::MphRead::NativeRuntime::ToString(angle.Y) + ", " + ::MphRead::NativeRuntime::ToString(angle.Z) + "))");
                }
                const Vector3 position = node.Position;
                if (!Vector3Equal(position, Vector3::Zero))
                {
                    StringBuilderExtensions::AppendIndent(sb,
                                                          "bone.location = mathutils.Vector((" + ::MphRead::NativeRuntime::ToString(position.X) + ", " + ::MphRead::NativeRuntime::ToString(position.Y) + ", " + ::MphRead::NativeRuntime::ToString(position.Z) + "))");
                }
            }
        }

        AppendLine(sb);
        AppendLine(sb, "def anim_setup():");
        PrintAnimations(model, sb);
        StringBuilderExtensions::AppendIndent(sb, "bpy.context.scene.render.fps = 30");
        StringBuilderExtensions::AppendIndent(sb, "if uv_index >= 0:");
        StringBuilderExtensions::AppendIndent(sb);
        StringBuilderExtensions::AppendIndent(sb, "set_uv_anims(uv_anims[uv_index])");
        StringBuilderExtensions::AppendIndent(sb, "if tex_index >= 0:");
        StringBuilderExtensions::AppendIndent(sb);
        StringBuilderExtensions::AppendIndent(sb, "set_tex_anims(tex_anims[tex_index])");
        StringBuilderExtensions::AppendIndent(sb, "if node_index >= 0:");
        StringBuilderExtensions::AppendIndent(sb);
        StringBuilderExtensions::AppendIndent(sb, "set_node_anims(node_anims[node_index])");
        StringBuilderExtensions::AppendIndent(sb, "if mat_index >= 0:");
        StringBuilderExtensions::AppendIndent(sb);
        StringBuilderExtensions::AppendIndent(sb, "set_mat_anims(mat_anims[mat_index])");
        AppendLine(sb);
        AppendLine(sb, "if __name__ == '__main__':");
        StringBuilderExtensions::AppendIndent(sb, "validate_version(export_version)");
        StringBuilderExtensions::AppendIndent(sb, "import_dae(recolor)");
        return sb;
    }
}
