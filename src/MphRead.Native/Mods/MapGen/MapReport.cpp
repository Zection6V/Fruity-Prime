#include "MapReport.hpp"

#include "Q3Bsp.hpp"
#include "../../Formats/Enums.hpp"
#include "../../Formats/Model.hpp"
#include "../../Read.hpp"
#include "../../Formats/Types.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "NativeRuntime/System/Globalization.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using ::MphRead::NativeRuntime::ManagedAt;
using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::NativeRuntime::UncheckedAdd;

namespace
{
    struct ShaderCount final
    {
        std::string Name;
        std::int32_t Count;
    };

    [[nodiscard]] std::size_t ManagedStringLength(std::string_view text) noexcept
    {
        std::size_t length = 0;
        for (std::size_t index = 0; index < text.size();)
        {
            const auto first = static_cast<unsigned char>(text[index]);
            std::size_t bytes = 1;
            std::uint32_t scalar = first;
            if (first >= 0xC2U && first <= 0xDFU && index + 1 < text.size()
                && (static_cast<unsigned char>(text[index + 1]) & 0xC0U) == 0x80U)
            {
                bytes = 2;
                scalar = ((first & 0x1FU) << 6)
                    | (static_cast<unsigned char>(text[index + 1]) & 0x3FU);
            }
            else if (first >= 0xE0U && first <= 0xEFU && index + 2 < text.size())
            {
                const auto second = static_cast<unsigned char>(text[index + 1]);
                const auto third = static_cast<unsigned char>(text[index + 2]);
                const bool secondValid = (second & 0xC0U) == 0x80U
                    && (first != 0xE0U || second >= 0xA0U)
                    && (first != 0xEDU || second <= 0x9FU);
                if (secondValid && (third & 0xC0U) == 0x80U)
                {
                    bytes = 3;
                    scalar = ((first & 0x0FU) << 12)
                        | ((second & 0x3FU) << 6)
                        | (third & 0x3FU);
                }
            }
            else if (first >= 0xF0U && first <= 0xF4U && index + 3 < text.size())
            {
                const auto second = static_cast<unsigned char>(text[index + 1]);
                const auto third = static_cast<unsigned char>(text[index + 2]);
                const auto fourth = static_cast<unsigned char>(text[index + 3]);
                const bool secondValid = (second & 0xC0U) == 0x80U
                    && (first != 0xF0U || second >= 0x90U)
                    && (first != 0xF4U || second <= 0x8FU);
                if (secondValid
                    && (third & 0xC0U) == 0x80U
                    && (fourth & 0xC0U) == 0x80U)
                {
                    bytes = 4;
                    scalar = ((first & 0x07U) << 18)
                        | ((second & 0x3FU) << 12)
                        | ((third & 0x3FU) << 6)
                        | (fourth & 0x3FU);
                }
            }
            length += scalar > 0xFFFFU ? 2U : 1U;
            index += bytes;
        }
        return length;
    }

    void AppendRightAligned(
        std::string& output, std::string_view value, std::size_t width)
    {
        const std::size_t length = ManagedStringLength(value);
        if (length < width)
        {
            output.append(width - length, ' ');
        }
        output.append(value);
    }

    void AppendLeftAligned(
        std::string& output, std::string_view value, std::size_t width)
    {
        output.append(value);
        const std::size_t length = ManagedStringLength(value);
        if (length < width)
        {
            output.append(width - length, ' ');
        }
    }

    [[nodiscard]] std::string FormatTextureFormat(MphRead::TextureFormat value)
    {
        switch (value)
        {
        case MphRead::TextureFormat::Palette2Bit: return "Palette2Bit";
        case MphRead::TextureFormat::Palette4Bit: return "Palette4Bit";
        case MphRead::TextureFormat::Palette8Bit: return "Palette8Bit";
        case MphRead::TextureFormat::PaletteA5I3: return "PaletteA5I3";
        case MphRead::TextureFormat::DirectRgb: return "DirectRgb";
        case MphRead::TextureFormat::PaletteA3I5: return "PaletteA3I5";
        default:
            return std::to_string(
                static_cast<unsigned int>(static_cast<std::uint8_t>(value)));
        }
    }

    [[nodiscard]] std::string FormatRenderMode(MphRead::RenderMode value)
    {
        switch (value)
        {
        case MphRead::RenderMode::Normal: return "Normal";
        case MphRead::RenderMode::Decal: return "Decal";
        case MphRead::RenderMode::Translucent: return "Translucent";
        case MphRead::RenderMode::Unknown3: return "Unknown3";
        case MphRead::RenderMode::Unknown4: return "Unknown4";
        default:
            return std::to_string(
                static_cast<unsigned int>(static_cast<std::uint8_t>(value)));
        }
    }

    void WriteLine(std::string_view value)
    {
        std::cout << value << '\n';
    }
}

namespace MphRead::Mods::MapGen
{
    std::int32_t MapReport::ListShaders(
        const std::string& source,
        const std::optional<std::string>& mapName)
    {
        std::shared_ptr<Q3Bsp> bsp;
        try
        {
            bsp = Q3Bsp::Load(source, mapName);
        }
        catch (const std::exception& ex)
        {
            WriteLine(ex.what());
            return 1;
        }

        if (!bsp)
        {
            throw System::NullReferenceException();
        }

        std::vector<ShaderCount> counts;
        for (const std::shared_ptr<Q3Face>& faceRef : bsp->Faces())
        {
            const Q3Face& face = RequireReference(faceRef);
            if (face.Type() != 1 && face.Type() != 3)
            {
                continue;
            }

            const std::shared_ptr<Q3Texture>& textureRef
                = ManagedAt(bsp->Textures(), face.Texture());
            const Q3Texture& texture = RequireReference(textureRef);
            if ((texture.Flags() & (Q3Bsp::SurfaceNoDraw | Q3Bsp::SurfaceSky
                | Q3Bsp::SurfaceHint | Q3Bsp::SurfaceSkip)) != 0)
            {
                continue;
            }

            if (!texture.Name().HasValue())
            {
                throw System::ArgumentNullException("key");
            }
            const std::string& name = texture.Name().Value();

            ShaderCount* found = nullptr;
            for (ShaderCount& pair : counts)
            {
                if (pair.Name == name)
                {
                    found = std::addressof(pair);
                    break;
                }
            }
            if (found == nullptr)
            {
                counts.push_back(ShaderCount{name, 0});
                found = std::addressof(counts.back());
            }
            found->Count = UncheckedAdd(found->Count, face.MeshVertCount() / 3);
        }

        std::string summary = mapName.has_value() ? *mapName : source;
        summary += ": ";
        summary += ::MphRead::NativeRuntime::ToString(static_cast<std::int32_t>(counts.size()));
        summary += " shaders drawn";
        WriteLine(summary);

        std::vector<const ShaderCount*> ordered;
        ordered.reserve(counts.size());
        for (const ShaderCount& pair : counts)
        {
            ordered.push_back(std::addressof(pair));
        }
        std::stable_sort(ordered.begin(), ordered.end(),
            [](const ShaderCount* left, const ShaderCount* right)
            {
                return left->Count > right->Count;
            });

        for (const ShaderCount* pair : ordered)
        {
            std::string line = "  ";
            const std::string count = ::MphRead::NativeRuntime::ToString(pair->Count);
            AppendRightAligned(line, count, 6);
            line += " triangles  ";
            line += pair->Name;
            WriteLine(line);
        }
        return 0;
    }

    std::int32_t MapReport::ListMaterials(const std::string& room)
    {
        std::shared_ptr<Model> model;
        try
        {
            const std::shared_ptr<ModelInstance> instance
                = Read::GetRoomModelInstance(room);
            if (!instance)
            {
                throw System::NullReferenceException();
            }
            model = instance->Model();
        }
        catch (const std::exception& ex)
        {
            std::string message = "Could not load ";
            message += room;
            message += ": ";
            message += ex.what();
            WriteLine(message);
            return 1;
        }

        if (!model)
        {
            throw System::NullReferenceException();
        }
        if (!model->Recolors)
        {
            throw System::NullReferenceException();
        }

        const std::shared_ptr<Recolor>& recolor
            = ManagedAt(*model->Recolors, 0);

        if (!model->Materials)
        {
            throw System::NullReferenceException();
        }
        const std::int32_t materialCount
            = static_cast<std::int32_t>(model->Materials->size());

        if (!recolor)
        {
            throw System::NullReferenceException();
        }
        if (!recolor->Textures)
        {
            throw System::NullReferenceException();
        }
        const std::int32_t textureCount
            = static_cast<std::int32_t>(recolor->Textures->size());

        std::string summary = room;
        summary += ": ";
        summary += ::MphRead::NativeRuntime::ToString(materialCount);
        summary += " materials, ";
        summary += ::MphRead::NativeRuntime::ToString(textureCount);
        summary += " textures";
        WriteLine(summary);

        for (std::int32_t i = 0;
            i < static_cast<std::int32_t>(model->Materials->size()); ++i)
        {
            const std::shared_ptr<Material>& materialRef
                = ManagedAt(*model->Materials, i);
            const Material& material = RequireReference(materialRef);

            std::string size = "no texture";
            std::string format;
            if (material.TextureId >= 0
                && material.TextureId
                    < static_cast<std::int32_t>(recolor->Textures->size()))
            {
                const Texture& texture
                    = ManagedAt(*recolor->Textures, material.TextureId);
                size = ::MphRead::NativeRuntime::ToString(static_cast<std::int32_t>(texture.Width));
                size += 'x';
                size += ::MphRead::NativeRuntime::ToString(static_cast<std::int32_t>(texture.Height));
                format = FormatTextureFormat(texture.Format);
            }

            std::string line = "  ";
            const std::string index = ::MphRead::NativeRuntime::ToString(i);
            AppendRightAligned(line, index, 3);
            line += "  ";
            AppendLeftAligned(line, material.Name, 32);
            line += " tex ";
            const std::string textureId = ::MphRead::NativeRuntime::ToString(material.TextureId);
            AppendRightAligned(line, textureId, 3);
            line += " pal ";
            const std::string paletteId = ::MphRead::NativeRuntime::ToString(material.PaletteId);
            AppendRightAligned(line, paletteId, 3);
            line += "  ";
            AppendLeftAligned(line, size, 9);
            line += ' ';
            line += format;
            line += ' ';
            if (material.RenderMode != RenderMode::Normal)
            {
                line += FormatRenderMode(material.RenderMode);
            }
            WriteLine(line);
        }
        return 0;
    }
}
