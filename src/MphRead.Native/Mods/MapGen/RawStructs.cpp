#include "RawStructs.hpp"

#include "../../Formats/Formats.hpp"
#include "../../Program.hpp"
#include "../../Read.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr std::uint32_t ReplacementCharacter = 0xFFFDU;

    [[nodiscard]] std::uint32_t DecodeUtf8(
        std::string_view value,
        std::size_t& index) noexcept
    {
        const auto byte0 = static_cast<std::uint8_t>(value[index]);
        if (byte0 <= 0x7FU)
        {
            ++index;
            return byte0;
        }

        auto continuation = [&value](std::size_t at) noexcept -> std::uint8_t
        {
            if (at >= value.size())
            {
                return 0xFFU;
            }
            const auto byte = static_cast<std::uint8_t>(value[at]);
            return (byte & 0xC0U) == 0x80U ? byte : 0xFFU;
        };

        if (byte0 >= 0xC2U && byte0 <= 0xDFU)
        {
            const std::uint8_t byte1 = continuation(index + 1);
            if (byte1 != 0xFFU)
            {
                index += 2;
                return ((static_cast<std::uint32_t>(byte0) & 0x1FU) << 6)
                    | (static_cast<std::uint32_t>(byte1) & 0x3FU);
            }
        }
        else if (byte0 >= 0xE0U && byte0 <= 0xEFU)
        {
            const std::uint8_t byte1 = continuation(index + 1);
            const std::uint8_t byte2 = continuation(index + 2);
            const bool validSecond = byte1 != 0xFFU
                && (byte0 != 0xE0U || byte1 >= 0xA0U)
                && (byte0 != 0xEDU || byte1 <= 0x9FU);
            if (validSecond && byte2 != 0xFFU)
            {
                index += 3;
                return ((static_cast<std::uint32_t>(byte0) & 0x0FU) << 12)
                    | ((static_cast<std::uint32_t>(byte1) & 0x3FU) << 6)
                    | (static_cast<std::uint32_t>(byte2) & 0x3FU);
            }
        }
        else if (byte0 >= 0xF0U && byte0 <= 0xF4U)
        {
            const std::uint8_t byte1 = continuation(index + 1);
            const std::uint8_t byte2 = continuation(index + 2);
            const std::uint8_t byte3 = continuation(index + 3);
            const bool validSecond = byte1 != 0xFFU
                && (byte0 != 0xF0U || byte1 >= 0x90U)
                && (byte0 != 0xF4U || byte1 <= 0x8FU);
            if (validSecond && byte2 != 0xFFU && byte3 != 0xFFU)
            {
                index += 4;
                return ((static_cast<std::uint32_t>(byte0) & 0x07U) << 18)
                    | ((static_cast<std::uint32_t>(byte1) & 0x3FU) << 12)
                    | ((static_cast<std::uint32_t>(byte2) & 0x3FU) << 6)
                    | (static_cast<std::uint32_t>(byte3) & 0x3FU);
            }
        }

        ++index;
        return ReplacementCharacter;
    }

    void WriteByte(std::vector<std::uint8_t>& bytes, std::uint8_t value)
    {
        bytes.push_back(value);
    }

    void WriteUInt16(std::vector<std::uint8_t>& bytes, std::uint16_t value)
    {
        bytes.push_back(static_cast<std::uint8_t>(value));
        bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    }

    void WriteInt16(std::vector<std::uint8_t>& bytes, std::int32_t value)
    {
        WriteUInt16(bytes, static_cast<std::uint16_t>(value));
    }

    void WriteUInt32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
    {
        bytes.push_back(static_cast<std::uint8_t>(value));
        bytes.push_back(static_cast<std::uint8_t>(value >> 8));
        bytes.push_back(static_cast<std::uint8_t>(value >> 16));
        bytes.push_back(static_cast<std::uint8_t>(value >> 24));
    }

    void WriteInt32(std::vector<std::uint8_t>& bytes, std::int32_t value)
    {
        WriteUInt32(bytes, static_cast<std::uint32_t>(value));
    }

    void WriteFixedString(
        std::vector<std::uint8_t>& writer,
        const std::string& value,
        std::int32_t length)
    {
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length), 0);
        const std::size_t limit = static_cast<std::size_t>(length - 1);
        std::size_t output = 0;
        std::size_t index = 0;

        while (index < value.size() && output < limit)
        {
            const std::uint32_t codePoint = DecodeUtf8(value, index);
            if (codePoint <= 0xFFFFU)
            {
                bytes[output++] = static_cast<std::uint8_t>(codePoint);
            }
            else
            {
                const std::uint32_t scalar = codePoint - 0x10000U;
                const std::uint16_t high = static_cast<std::uint16_t>(
                    0xD800U + (scalar >> 10));
                const std::uint16_t low = static_cast<std::uint16_t>(
                    0xDC00U + (scalar & 0x3FFU));

                bytes[output++] = static_cast<std::uint8_t>(high);
                if (output < limit)
                {
                    bytes[output++] = static_cast<std::uint8_t>(low);
                }
            }
        }

        writer.insert(writer.end(), bytes.begin(), bytes.end());
    }

    void WriteVector3Fx(
        std::vector<std::uint8_t>& writer,
        OpenTK::Mathematics::Vector3 value)
    {
        WriteInt32(writer, MphRead::Fixed::ToInt(value.X));
        WriteInt32(writer, MphRead::Fixed::ToInt(value.Y));
        WriteInt32(writer, MphRead::Fixed::ToInt(value.Z));
    }

    void WriteColorRgb(
        std::vector<std::uint8_t>& writer,
        MphRead::ColorRgb value)
    {
        WriteByte(writer, value.Red);
        WriteByte(writer, value.Green);
        WriteByte(writer, value.Blue);
    }

    template <typename T>
    [[nodiscard]] T Marshal(
        const std::vector<std::uint8_t>& bytes,
        std::string_view typeName)
    {
        const std::size_t size = sizeof(T);
        if (bytes.size() != size)
        {
            throw MphRead::ProgramException(
                "Built " + std::to_string(bytes.size())
                + " bytes for " + std::string(typeName)
                + ", which is " + std::to_string(size) + ".");
        }
        return MphRead::Read::ReadStruct<T>(
            std::span<const std::uint8_t>(bytes.data(), bytes.size()));
    }
}

namespace MphRead::Mods::MapGen
{
    std::shared_ptr<Node> RawStructs::MakeNode(
        const std::string& name,
        std::int32_t meshCount,
        std::int32_t firstMeshId,
        std::int32_t parent,
        std::int32_t child,
        std::int32_t next)
    {
        std::vector<std::uint8_t> writer;
        writer.reserve(sizeof(RawNode));

        WriteFixedString(writer, name, 64);
        WriteInt16(writer, parent);
        WriteInt16(writer, child);
        WriteInt16(writer, next);
        WriteUInt16(writer, 0);
        WriteUInt32(writer, 1U);
        WriteUInt16(writer, static_cast<std::uint16_t>(meshCount));
        const std::uint32_t firstMeshOffset
            = static_cast<std::uint32_t>(firstMeshId) * 2U;
        WriteUInt16(writer, static_cast<std::uint16_t>(firstMeshOffset));
        WriteVector3Fx(
            writer,
            OpenTK::Mathematics::Vector3(1.0F, 1.0F, 1.0F));
        WriteInt16(writer, 0);
        WriteInt16(writer, 0);
        WriteInt16(writer, 0);
        WriteUInt16(writer, 0);
        WriteVector3Fx(writer, OpenTK::Mathematics::Vector3::Zero);
        WriteInt32(writer, 0);
        WriteVector3Fx(writer, OpenTK::Mathematics::Vector3::Zero);
        WriteVector3Fx(writer, OpenTK::Mathematics::Vector3::Zero);
        WriteByte(writer, static_cast<std::uint8_t>(BillboardMode::None));
        WriteByte(writer, 0);
        WriteUInt16(writer, 0);
        for (std::int32_t i = 0; i < 12; ++i)
        {
            WriteInt32(writer, 0);
        }
        for (std::int32_t i = 0; i < 12; ++i)
        {
            WriteUInt32(writer, 0U);
        }

        return std::make_shared<Node>(
            Marshal<RawNode>(writer, "RawNode"));
    }

    std::shared_ptr<Material> RawStructs::MakeMaterial(
        const std::string& name,
        std::int32_t textureId,
        std::int32_t paletteId,
        RepeatMode xRepeat,
        RepeatMode yRepeat,
        bool lighting,
        ColorRgb diffuse,
        ColorRgb ambient)
    {
        std::vector<std::uint8_t> writer;
        writer.reserve(sizeof(RawMaterial));

        WriteFixedString(writer, name, 64);
        WriteByte(writer, lighting ? 1U : 0U);
        WriteByte(writer, static_cast<std::uint8_t>(CullingMode::Back));
        WriteByte(writer, 31);
        WriteByte(writer, 0);
        WriteInt16(writer, paletteId);
        WriteInt16(writer, textureId);
        WriteByte(writer, static_cast<std::uint8_t>(xRepeat));
        WriteByte(writer, static_cast<std::uint8_t>(yRepeat));
        WriteColorRgb(writer, diffuse);
        WriteColorRgb(writer, ambient);
        WriteColorRgb(writer, ColorRgb(0, 0, 0));
        WriteByte(writer, 0);
        WriteUInt32(writer, static_cast<std::uint32_t>(PolygonMode::Modulate));
        WriteByte(writer, static_cast<std::uint8_t>(RenderMode::Normal));
        WriteByte(writer, 0);
        WriteUInt16(writer, 0);
        WriteUInt32(writer, static_cast<std::uint32_t>(TexgenMode::None));
        WriteUInt16(writer, 0);
        WriteUInt16(writer, 0);
        WriteUInt32(writer, 0U);
        WriteInt32(writer, 4096);
        WriteInt32(writer, 4096);
        WriteUInt16(writer, 0);
        WriteUInt16(writer, 0);
        WriteInt32(writer, 0);
        WriteInt32(writer, 0);
        WriteUInt16(writer, 0);
        WriteUInt16(writer, 0);
        WriteByte(writer, 0);
        WriteByte(writer, 0);
        WriteUInt16(writer, 0);

        return std::make_shared<Material>(
            Marshal<RawMaterial>(writer, "RawMaterial"));
    }

    std::shared_ptr<Mesh> RawStructs::MakeMesh(
        std::int32_t materialId,
        std::int32_t dlistId)
    {
        std::vector<std::uint8_t> writer;
        writer.reserve(sizeof(RawMesh));

        WriteUInt16(writer, static_cast<std::uint16_t>(materialId));
        WriteUInt16(writer, static_cast<std::uint16_t>(dlistId));

        return std::make_shared<Mesh>(
            Marshal<RawMesh>(writer, "RawMesh"));
    }
}
