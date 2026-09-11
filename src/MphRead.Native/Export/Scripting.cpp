#include "Scripting.hpp"

#include "../Formats/Enums.hpp"
#include "../Formats/Formats.hpp"
#include "../Formats/Model.hpp"
#include "../Formats/RawFormats.hpp"
#include "../Program.hpp"
#include "../Read.hpp"

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

void AppendLine(std::string& sb)
{
    sb.append(ManagedNewLine);
}

void AppendLine(std::string& sb, std::string_view text)
{
    sb.append(text);
    sb.append(ManagedNewLine);
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

[[nodiscard]] bool Vector3Equal(Vector3 left, Vector3 right) noexcept
{
    return left.X == right.X && left.Y == right.Y && left.Z == right.Z;
}

struct Utf8CodePoint final
{
    std::uint32_t Value;
    std::size_t Length;
};

[[nodiscard]] std::optional<Utf8CodePoint> DecodeUtf8Forward(
    std::string_view text, std::size_t position) noexcept
{
    if (position >= text.size())
    {
        return std::nullopt;
    }
    const auto first = static_cast<unsigned char>(text[position]);
    if (first <= 0x7F)
    {
        return Utf8CodePoint{first, 1};
    }

    std::uint32_t value = 0;
    std::size_t length = 0;
    std::uint32_t minimum = 0;
    if ((first & 0xE0U) == 0xC0U)
    {
        value = first & 0x1FU;
        length = 2;
        minimum = 0x80;
    }
    else if ((first & 0xF0U) == 0xE0U)
    {
        value = first & 0x0FU;
        length = 3;
        minimum = 0x800;
    }
    else if ((first & 0xF8U) == 0xF0U)
    {
        value = first & 0x07U;
        length = 4;
        minimum = 0x10000;
    }
    else
    {
        return std::nullopt;
    }
    if (position + length > text.size())
    {
        return std::nullopt;
    }
    for (std::size_t i = 1; i < length; ++i)
    {
        const auto next = static_cast<unsigned char>(text[position + i]);
        if ((next & 0xC0U) != 0x80U)
        {
            return std::nullopt;
        }
        value = (value << 6) | (next & 0x3FU);
    }
    if (value < minimum || value > 0x10FFFFU || (value >= 0xD800U && value <= 0xDFFFU))
    {
        return std::nullopt;
    }
    return Utf8CodePoint{value, length};
}

[[nodiscard]] bool IsDotNetTrimWhitespace(std::uint32_t codePoint) noexcept
{
    if (codePoint >= 0x0009U && codePoint <= 0x000DU)
    {
        return true;
    }
    switch (codePoint)
    {
    case 0x0020U:
    case 0x0085U:
    case 0x00A0U:
    case 0x1680U:
    case 0x2000U:
    case 0x2001U:
    case 0x2002U:
    case 0x2003U:
    case 0x2004U:
    case 0x2005U:
    case 0x2006U:
    case 0x2007U:
    case 0x2008U:
    case 0x2009U:
    case 0x200AU:
    case 0x2028U:
    case 0x2029U:
    case 0x202FU:
    case 0x205FU:
    case 0x3000U:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] std::string_view TrimDotNetWhitespace(std::string_view text) noexcept
{
    std::size_t start = 0;
    while (start < text.size())
    {
        const auto decoded = DecodeUtf8Forward(text, start);
        if (!decoded.has_value() || !IsDotNetTrimWhitespace(decoded->Value))
        {
            break;
        }
        start += decoded->Length;
    }

    std::size_t end = text.size();
    while (end > start)
    {
        std::size_t candidate = end - 1;
        while (candidate > start
            && (static_cast<unsigned char>(text[candidate]) & 0xC0U) == 0x80U)
        {
            --candidate;
        }
        const auto decoded = DecodeUtf8Forward(text, candidate);
        if (!decoded.has_value() || candidate + decoded->Length != end
            || !IsDotNetTrimWhitespace(decoded->Value))
        {
            break;
        }
        end = candidate;
    }
    return text.substr(start, end - start);
}

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

[[nodiscard]] std::string PathToUtf8(const std::filesystem::path& value)
{
#if defined(__cpp_char8_t)
    const std::u8string converted = value.u8string();
    std::string result;
    result.reserve(converted.size());
    for (char8_t ch : converted)
    {
        result.push_back(static_cast<char>(ch));
    }
    return result;
#else
    return value.u8string();
#endif
}

[[nodiscard]] std::string GetFullPath(std::string_view value)
{
    return PathToUtf8(std::filesystem::absolute(PathFromUtf8(value)).lexically_normal());
}

template <typename Predicate>
[[nodiscard]] std::int32_t FindVersionComponent(Predicate&& predicate)
{
    std::int32_t low = 0;
    std::int32_t high = std::numeric_limits<std::int32_t>::max();
    while (low < high)
    {
        const std::int32_t middle = static_cast<std::int32_t>(
            static_cast<std::int64_t>(low)
            + (static_cast<std::int64_t>(high) - low + 1) / 2);
        if (predicate(middle))
        {
            low = middle;
        }
        else
        {
            high = middle - 1;
        }
    }
    return low;
}

[[nodiscard]] std::string ManagedVersionToString(const System::Version& version)
{
    const std::int32_t major = FindVersionComponent(
        [&version](std::int32_t value)
        {
            return version >= System::Version(value, 0, 0, 0);
        });
    const std::int32_t minor = FindVersionComponent(
        [&version, major](std::int32_t value)
        {
            return version >= System::Version(major, value, 0, 0);
        });
    const std::int32_t build = FindVersionComponent(
        [&version, major, minor](std::int32_t value)
        {
            return version >= System::Version(major, minor, value, 0);
        });
    const std::int32_t revision = FindVersionComponent(
        [&version, major, minor, build](std::int32_t value)
        {
            return version >= System::Version(major, minor, build, value);
        });
    return std::to_string(major) + "." + std::to_string(minor) + "."
        + std::to_string(build) + "." + std::to_string(revision);
}

[[nodiscard]] std::int32_t FindCombo(
    const std::vector<std::pair<std::int32_t, std::int32_t>>& combos,
    const std::pair<std::int32_t, std::int32_t>& pair) noexcept
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

[[nodiscard]] std::string JoinIntegers(const std::vector<std::int32_t>& values)
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
void StringBuilderExtensions::AppendIndent(std::string& sb)
{
    sb.append(4, ' ');
}

void StringBuilderExtensions::AppendIndent(
    std::string& sb, const std::string& text, std::int32_t indent)
{
    const std::string_view trimmed = TrimDotNetWhitespace(text);
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

void Scripting::PrintAnimations(const Model& model, std::string& sb)
{
    const std::int32_t indent = 1;
    StringBuilderExtensions::AppendIndent(sb, "uv_anims = [", indent);
    for (const TexcoordAnimationGroup& group : model.AnimationGroups().Texcoord())
    {
        if (group.Animations().empty())
        {
            continue;
        }
        StringBuilderExtensions::AppendIndent(sb, "{", indent + 1);
        for (const auto& kvp : group.Animations())
        {
            const TexcoordAnimation& anim = kvp.second;
            StringBuilderExtensions::AppendIndent(sb, "'" + kvp.first + "':", indent + 2);
            StringBuilderExtensions::AppendIndent(sb, "[", indent + 2);
            for (std::int32_t frame = 0; frame < group.FrameCount(); ++frame)
            {
                const float scaleS = model.InterpolateAnimation(group.Scales(), anim.ScaleLutIndexS, frame,
                    anim.ScaleBlendS, anim.ScaleLutLengthS, group.FrameCount());
                const float scaleT = model.InterpolateAnimation(group.Scales(), anim.ScaleLutIndexT, frame,
                    anim.ScaleBlendT, anim.ScaleLutLengthT, group.FrameCount());
                const float rotate = model.InterpolateAnimation(group.Rotations(), anim.RotateLutIndexZ, frame,
                    anim.RotateBlendZ, anim.RotateLutLengthZ, group.FrameCount(), true);
                const float translateS = model.InterpolateAnimation(group.Translations(), anim.TranslateLutIndexS, frame,
                    anim.TranslateBlendS, anim.TranslateLutLengthS, group.FrameCount());
                const float translateT = model.InterpolateAnimation(group.Translations(), anim.TranslateLutIndexT, frame,
                    anim.TranslateBlendT, anim.TranslateLutLengthT, group.FrameCount());
                StringBuilderExtensions::AppendIndent(sb,
                    "[" + ManagedSingleToString(scaleS) + ", " + ManagedSingleToString(scaleT)
                    + ", " + ManagedSingleToString(rotate) + ", " + ManagedSingleToString(translateS)
                    + ", " + ManagedSingleToString(translateT) + "],", indent + 3);
            }
            StringBuilderExtensions::AppendIndent(sb, "],", indent + 2);
        }
        StringBuilderExtensions::AppendIndent(sb, "},", indent + 1);
    }
    StringBuilderExtensions::AppendIndent(sb, "]", indent);

    StringBuilderExtensions::AppendIndent(sb, "mat_anims = [", indent);
    for (const MaterialAnimationGroup& group : model.AnimationGroups().Material())
    {
        if (group.Animations().empty())
        {
            continue;
        }
        StringBuilderExtensions::AppendIndent(sb, "{", indent + 1);
        for (const auto& kvp : group.Animations())
        {
            const MaterialAnimation& anim = kvp.second;
            StringBuilderExtensions::AppendIndent(sb, "'" + kvp.first + "_mat':", indent + 2);
            StringBuilderExtensions::AppendIndent(sb, "[", indent + 2);
            for (std::int32_t frame = 0; frame < group.FrameCount(); ++frame)
            {
                const float red = model.InterpolateAnimation(group.Colors(), anim.DiffuseLutIndexR, frame,
                    anim.DiffuseBlendR, anim.DiffuseLutLengthR, group.FrameCount());
                const float green = model.InterpolateAnimation(group.Colors(), anim.DiffuseLutIndexG, frame,
                    anim.DiffuseBlendG, anim.DiffuseLutLengthG, group.FrameCount());
                const float blue = model.InterpolateAnimation(group.Colors(), anim.DiffuseLutIndexB, frame,
                    anim.DiffuseBlendB, anim.DiffuseLutLengthB, group.FrameCount());
                const float alpha = model.InterpolateAnimation(group.Colors(), anim.AlphaLutIndex, frame,
                    anim.AlphaBlend, anim.AlphaLutLength, group.FrameCount());
                StringBuilderExtensions::AppendIndent(sb,
                    "[" + ManagedSingleToString(red / 31.0F) + ", "
                    + ManagedSingleToString(green / 31.0F) + ", "
                    + ManagedSingleToString(blue / 31.0F) + ", "
                    + ManagedSingleToString(alpha / 31.0F) + "],", indent + 3);
            }
            StringBuilderExtensions::AppendIndent(sb, "],", indent + 2);
        }
        StringBuilderExtensions::AppendIndent(sb, "},", indent + 1);
    }
    StringBuilderExtensions::AppendIndent(sb, "]", indent);

    StringBuilderExtensions::AppendIndent(sb, "tex_anims = [", indent);
    std::vector<std::pair<std::int32_t, std::int32_t>> combos;
    for (const TextureAnimationGroup& group : model.AnimationGroups().Texture())
    {
        for (const auto& kvp : group.Animations())
        {
            for (std::int32_t i = kvp.second.StartIndex; i < kvp.second.StartIndex + kvp.second.Count; ++i)
            {
                const auto pair = std::make_pair(
                    static_cast<std::int32_t>(group.TextureIds().at(static_cast<std::size_t>(i))),
                    static_cast<std::int32_t>(group.PaletteIds().at(static_cast<std::size_t>(i))));
                if (std::find(combos.begin(), combos.end(), pair) == combos.end())
                {
                    combos.push_back(pair);
                }
            }
        }
    }
    for (const TextureAnimationGroup& group : model.AnimationGroups().Texture())
    {
        if (group.Animations().empty())
        {
            continue;
        }
        StringBuilderExtensions::AppendIndent(sb, "{", indent + 1);
        for (const auto& kvp : group.Animations())
        {
            const TextureAnimation& anim = kvp.second;
            StringBuilderExtensions::AppendIndent(sb, "'" + kvp.first + "_mat':", indent + 2);
            StringBuilderExtensions::AppendIndent(sb, "[", indent + 2);
            for (std::int32_t i = anim.StartIndex; i < anim.StartIndex + anim.Count; ++i)
            {
                const std::int32_t frame = static_cast<std::int32_t>(
                    group.FrameIndices().at(static_cast<std::size_t>(i)));
                const auto pair = std::make_pair(
                    static_cast<std::int32_t>(group.TextureIds().at(static_cast<std::size_t>(i))),
                    static_cast<std::int32_t>(group.PaletteIds().at(static_cast<std::size_t>(i))));
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
    for (const NodeAnimationGroup& group : model.AnimationGroups().Node())
    {
        if (group.Animations().empty())
        {
            continue;
        }
        StringBuilderExtensions::AppendIndent(sb, "{", indent + 1);
        for (const auto& kvp : group.Animations())
        {
            const NodeAnimation& anim = kvp.second;
            StringBuilderExtensions::AppendIndent(sb, "'" + kvp.first + "':", indent + 2);
            StringBuilderExtensions::AppendIndent(sb, "[", indent + 2);
            for (std::int32_t frame = 0; frame < group.FrameCount(); ++frame)
            {
                const float scaleX = model.InterpolateAnimation(group.Scales(), anim.ScaleLutIndexX, frame,
                    anim.ScaleBlendX, anim.ScaleLutLengthX, group.FrameCount());
                const float scaleY = model.InterpolateAnimation(group.Scales(), anim.ScaleLutIndexY, frame,
                    anim.ScaleBlendY, anim.ScaleLutLengthY, group.FrameCount());
                const float scaleZ = model.InterpolateAnimation(group.Scales(), anim.ScaleLutIndexZ, frame,
                    anim.ScaleBlendZ, anim.ScaleLutLengthZ, group.FrameCount());
                const float rotateX = model.InterpolateAnimation(group.Rotations(), anim.RotateLutIndexX, frame,
                    anim.RotateBlendX, anim.RotateLutLengthX, group.FrameCount(), true);
                const float rotateY = model.InterpolateAnimation(group.Rotations(), anim.RotateLutIndexY, frame,
                    anim.RotateBlendY, anim.RotateLutLengthY, group.FrameCount(), true);
                const float rotateZ = model.InterpolateAnimation(group.Rotations(), anim.RotateLutIndexZ, frame,
                    anim.RotateBlendZ, anim.RotateLutLengthZ, group.FrameCount(), true);
                const float translateX = model.InterpolateAnimation(group.Translations(), anim.TranslateLutIndexX, frame,
                    anim.TranslateBlendX, anim.TranslateLutLengthX, group.FrameCount());
                const float translateY = model.InterpolateAnimation(group.Translations(), anim.TranslateLutIndexY, frame,
                    anim.TranslateBlendY, anim.TranslateLutLengthY, group.FrameCount());
                const float translateZ = model.InterpolateAnimation(group.Translations(), anim.TranslateLutIndexZ, frame,
                    anim.TranslateBlendZ, anim.TranslateLutLengthZ, group.FrameCount());
                StringBuilderExtensions::AppendIndent(sb,
                    "[" + ManagedSingleToString(scaleX) + ", " + ManagedSingleToString(scaleY)
                    + ", " + ManagedSingleToString(scaleZ) + ", " + ManagedSingleToString(rotateX)
                    + ", " + ManagedSingleToString(rotateY) + ", " + ManagedSingleToString(rotateZ)
                    + ", " + ManagedSingleToString(translateX) + ", " + ManagedSingleToString(translateY)
                    + ", " + ManagedSingleToString(translateZ) + "],", indent + 3);
            }
            StringBuilderExtensions::AppendIndent(sb, "],", indent + 2);
        }
        StringBuilderExtensions::AppendIndent(sb, "},", indent + 1);
    }
    StringBuilderExtensions::AppendIndent(sb, "]", indent);
}

std::string Scripting::GenerateScript(const Model& model,
    const std::vector<std::pair<std::string, std::vector<Collada::Vertex>>>& lists)
{
    std::string sb;
    AppendLine(sb, "import bpy");
    AppendLine(sb, "import math");
    AppendLine(sb, "import mathutils");
    AppendLine(sb, "from mph_common import *");
    AppendLine(sb);
    AppendLine(sb, "export_version = '" + ManagedVersionToString(Program::Version) + "'");

    std::string recolors;
    for (std::size_t i = 0; i < model.Recolors().size(); ++i)
    {
        if (i != 0)
        {
            recolors += ", ";
        }
        recolors += model.Recolors().at(i).Name();
    }
    AppendLine(sb, "# recolors: " + recolors);
    AppendLine(sb, "recolor = '" + model.Recolors().at(0).Name() + "'");

    const auto uvAnimCount = static_cast<std::int32_t>(std::count_if(
        model.AnimationGroups().Texcoord().begin(), model.AnimationGroups().Texcoord().end(),
        [](const TexcoordAnimationGroup& group) { return !group.Animations().empty(); }));
    const auto matAnimCount = static_cast<std::int32_t>(std::count_if(
        model.AnimationGroups().Material().begin(), model.AnimationGroups().Material().end(),
        [](const MaterialAnimationGroup& group) { return !group.Animations().empty(); }));
    const auto nodeAnimCount = static_cast<std::int32_t>(std::count_if(
        model.AnimationGroups().Node().begin(), model.AnimationGroups().Node().end(),
        [](const NodeAnimationGroup& group) { return !group.Animations().empty(); }));
    const auto texAnimCount = static_cast<std::int32_t>(std::count_if(
        model.AnimationGroups().Texture().begin(), model.AnimationGroups().Texture().end(),
        [](const TextureAnimationGroup& group) { return !group.Animations().empty(); }));
    AppendLine(sb, "# uv anims: " + std::to_string(uvAnimCount) + ", mat anims: "
        + std::to_string(matAnimCount) + ", node anims: " + std::to_string(nodeAnimCount)
        + ", tex anims: " + std::to_string(texAnimCount));

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
    const std::string daePath = GetFullPath(Paths::Combine(
        Paths::Combine(Paths::Export(), model.Name()), model.Name() + "_{suffix}.dae"));
    AppendLine(sb, "bpy.ops.wm.collada_import(filepath =");
    StringBuilderExtensions::AppendIndent(sb);
    StringBuilderExtensions::AppendIndent(sb);
    AppendLine(sb, "fr\"" + daePath + "\")");
    StringBuilderExtensions::AppendIndent(sb);
    AppendLine(sb, "set_common()");

    std::unordered_set<std::int32_t> invertMeshIds;
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(model.Materials().size()); ++i)
    {
        const Material& material = model.Materials().at(static_cast<std::size_t>(i));
        if (material.TextureId() != -1)
        {
            StringBuilderExtensions::AppendIndent(sb);
            const auto& pixels = model.Recolors().at(0).GetPixels(material.TextureId(), material.PaletteId());
            const bool alphaPixels = std::any_of(pixels.begin(), pixels.end(),
                [](const auto& pixel) { return pixel.Alpha < 255; });
            AppendLine(sb, "set_texture_alpha('" + material.Name() + "_mat', "
                + std::to_string(material.Alpha()) + ", " + (alphaPixels ? "True" : "False") + ")");
            const bool mirrorX = material.XRepeat() == RepeatMode::Mirror;
            const bool mirrorY = material.YRepeat() == RepeatMode::Mirror;
            if (mirrorX || mirrorY)
            {
                StringBuilderExtensions::AppendIndent(sb);
                AppendLine(sb, "set_mirror('" + material.Name() + "_mat', "
                    + (mirrorX ? "True" : "False") + ", " + (mirrorY ? "True" : "False") + ")");
            }
        }
        else
        {
            StringBuilderExtensions::AppendIndent(sb);
            AppendLine(sb, "set_material_alpha('" + material.Name() + "_mat', "
                + std::to_string(material.Alpha()) + ")");
        }

        if (material.Culling() == CullingMode::Back || material.Culling() == CullingMode::Front)
        {
            StringBuilderExtensions::AppendIndent(sb);
            AppendLine(sb, "set_back_culling('" + material.Name() + "_mat')");
            if (material.Culling() == CullingMode::Front)
            {
                for (std::int32_t j = 0; j < static_cast<std::int32_t>(model.Meshes().size()); ++j)
                {
                    const Mesh& mesh = model.Meshes().at(static_cast<std::size_t>(j));
                    if (mesh.MaterialId() == i)
                    {
                        invertMeshIds.insert(j);
                    }
                }
            }
        }

        std::vector<std::int32_t> withColor;
        std::vector<std::int32_t> noColor;
        for (std::int32_t j = 0; j < static_cast<std::int32_t>(model.Meshes().size()); ++j)
        {
            const Mesh& mesh = model.Meshes().at(static_cast<std::size_t>(j));
            if (mesh.MaterialId() == i)
            {
                const auto& instructions = model.RenderInstructionLists().at(
                    static_cast<std::size_t>(mesh.DlistId()));
                const bool hasColor = std::any_of(instructions.begin(), instructions.end(),
                    [](const RenderInstruction& instruction)
                    {
                        return instruction.Code() == InstructionCode::COLOR;
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
            const auto diffuse = material.Diffuse();
            const std::string color = ManagedSingleToString(diffuse.Red / 31.0F) + ", "
                + ManagedSingleToString(diffuse.Green / 31.0F) + ", "
                + ManagedSingleToString(diffuse.Blue / 31.0F);
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
                    "set_mat_color('" + material.Name() + "_mat', " + color
                    + ", True, ['" + objects + "'])");
            }
            else
            {
                StringBuilderExtensions::AppendIndent(sb,
                    "set_mat_color('" + material.Name() + "_mat', " + color + ", False, [])");
            }
        }
    }

    for (const Node& node : model.Nodes())
    {
        for (std::int32_t meshId : node.GetMeshIds())
        {
            if (invertMeshIds.find(meshId) != invertMeshIds.end())
            {
                StringBuilderExtensions::AppendIndent(sb);
                AppendLine(sb, "invert_normals('geom" + std::to_string(meshId + 1) + "_obj')");
            }
        }
    }
    if (!model.NodeMatrixIds().empty())
    {
        StringBuilderExtensions::AppendIndent(sb, "bone_setup()");
    }
    StringBuilderExtensions::AppendIndent(sb, "anim_setup()");
    for (const Node& node : model.Nodes())
    {
        if (node.BillboardMode() == BillboardMode::None)
        {
            continue;
        }
        for (std::int32_t meshId : node.GetMeshIds())
        {
            StringBuilderExtensions::AppendIndent(sb);
            AppendLine(sb, "set_billboard('geom" + std::to_string(meshId + 1) + "_obj', "
                + std::to_string(static_cast<std::int32_t>(node.BillboardMode())) + ")");
        }
    }

    if (!model.NodeMatrixIds().empty())
    {
        AppendLine(sb);
        AppendLine(sb, "def bone_setup():");
        StringBuilderExtensions::AppendIndent(sb, "bpy.ops.object.mode_set(mode = 'OBJECT')");
        StringBuilderExtensions::AppendIndent(sb,
            "\nbpy.ops.object.armature_add(enter_editmode=True, align='WORLD', location=(0, 0, 0))\n"
            "bpy.ops.armature.select_all(action='SELECT')\n"
            "bpy.ops.armature.delete()");

        for (const Node& node : model.Nodes())
        {
            StringBuilderExtensions::AppendIndent(sb,
                "bpy.ops.armature.bone_primitive_add(name='" + node.Name() + "')");
        }
        StringBuilderExtensions::AppendIndent(sb, "bpy.ops.armature.select_all(action='DESELECT')");
        StringBuilderExtensions::AppendIndent(sb, "bones = bpy.data.armatures[0].edit_bones");

        for (const Node& child : model.Nodes())
        {
            if (child.ParentIndex() == -1)
            {
                continue;
            }
            const Node& parent = model.Nodes().at(static_cast<std::size_t>(child.ParentIndex()));
            StringBuilderExtensions::AppendIndent(sb,
                "bones.get('" + child.Name() + "').parent = bones.get('" + parent.Name() + "')");
        }

        StringBuilderExtensions::AppendIndent(sb,
            "\nbpy.ops.object.editmode_toggle()\n"
            "bpy.ops.object.select_all(action='DESELECT')\n"
            "for obj in bpy.data.objects:\n"
            "    if obj.type == 'MESH':\n"
            "        obj.select_set(True)\n"
            "bpy.data.objects['Armature'].select_set(True)\n"
            "bpy.ops.object.parent_set(type='ARMATURE_NAME')");

        for (const auto& obj : lists)
        {
            StringBuilderExtensions::AppendIndent(sb, "bpy.ops.object.select_all(action='DESELECT')");
            StringBuilderExtensions::AppendIndent(sb, "obj = bpy.data.objects['" + obj.first + "']");
            StringBuilderExtensions::AppendIndent(sb, "obj.select_set(True)");
            std::vector<std::pair<std::string, std::vector<std::int32_t>>> vertices;
            std::int32_t i = 0;
            for (const Collada::Vertex& vertex : obj.second)
            {
                const std::int32_t nodeIndex = model.NodeMatrixIds().at(
                    static_cast<std::size_t>(vertex.MatrixId));
                const Node& node = model.Nodes().at(static_cast<std::size_t>(nodeIndex));
                const auto existing = std::find_if(vertices.begin(), vertices.end(),
                    [&node](const auto& item) { return item.first == node.Name(); });
                if (existing == vertices.end())
                {
                    vertices.emplace_back(node.Name(), std::vector<std::int32_t>{i});
                }
                else
                {
                    existing->second.push_back(i);
                }
                ++i;
            }
            for (const auto& kvp : vertices)
            {
                StringBuilderExtensions::AppendIndent(sb, "group = obj.vertex_groups['" + kvp.first + "']");
                StringBuilderExtensions::AppendIndent(sb,
                    "group.add([" + JoinIntegers(kvp.second) + "], 1.0, 'ADD')");
            }
        }

        for (const Node& node : model.Nodes())
        {
            StringBuilderExtensions::AppendIndent(sb,
                "bone = bpy.data.objects['Armature'].pose.bones['" + node.Name() + "']");
            StringBuilderExtensions::AppendIndent(sb, "bone.rotation_mode = 'XYZ'");
            const Vector3 scale = node.Scale();
            if (!Vector3Equal(scale, Vector3(1.0F, 1.0F, 1.0F)))
            {
                StringBuilderExtensions::AppendIndent(sb,
                    "bone.scale = mathutils.Vector((" + ManagedSingleToString(scale.X) + ", "
                    + ManagedSingleToString(scale.Y) + ", " + ManagedSingleToString(scale.Z) + "))");
            }
            const Vector3 angle = node.Angle();
            if (!Vector3Equal(angle, Vector3::Zero))
            {
                StringBuilderExtensions::AppendIndent(sb,
                    "bone.rotation_euler = mathutils.Vector((" + ManagedSingleToString(angle.X) + ", "
                    + ManagedSingleToString(angle.Y) + ", " + ManagedSingleToString(angle.Z) + "))");
            }
            const Vector3 position = node.Position();
            if (!Vector3Equal(position, Vector3::Zero))
            {
                StringBuilderExtensions::AppendIndent(sb,
                    "bone.location = mathutils.Vector((" + ManagedSingleToString(position.X) + ", "
                    + ManagedSingleToString(position.Y) + ", " + ManagedSingleToString(position.Z) + "))");
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
