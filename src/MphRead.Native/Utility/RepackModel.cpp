#include "RepackModel.hpp"

#include "../Metadata/Metadata.hpp"
#include "../Metadata/Rooms.hpp"
#include "../Program.hpp"
#include "../Read.hpp"
#include "../SceneSetup.hpp"
#include "../Formats/Model.hpp"
#include "../Formats/Types.hpp"
#include "../NativeRuntime/System/Encoding.hpp"
#include "../NativeRuntime/System/Globalization.hpp"
#include "../NativeRuntime/System/IO.hpp"
#include "../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <numbers>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(DEBUG)
#define REPACK_MODEL_DEBUG_ASSERT(condition) do { if (!(condition)) { std::abort(); } } while (false)
#else
#define REPACK_MODEL_DEBUG_ASSERT(condition) do { } while (false)
#endif

using ::MphRead::NativeRuntime::FileReadAllBytes;
using ::MphRead::NativeRuntime::FileWriteAllBytes;
using ::MphRead::NativeRuntime::ManagedListAt;
using ::MphRead::NativeRuntime::MathClamp;
using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::NativeRuntime::RoundToEven;
using ::MphRead::NativeRuntime::StringReplace;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::UncheckedMultiply;
using ::MphRead::NativeRuntime::Utf8ToUtf16;

namespace
{
    using MphRead::Utility::Repack;
    using MphRead::Utility::BinaryWriter;
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Vector3i;

    [[nodiscard]] std::int32_t Sign16(std::uint32_t value) noexcept
    {
        const std::uint32_t bits = value & 0xFFFFU;
        return (bits & 0x8000U) != 0
            ? std::bit_cast<std::int32_t>(bits | 0xFFFF0000U)
            : static_cast<std::int32_t>(bits);
    }

    [[nodiscard]] std::int32_t Sign10(std::uint32_t value) noexcept
    {
        const std::uint32_t bits = value & 0x3FFU;
        return (bits & 0x200U) != 0
            ? std::bit_cast<std::int32_t>(bits | 0xFFFFFC00U)
            : static_cast<std::int32_t>(bits);
    }

    template <typename T>
    [[nodiscard]] std::int32_t ManagedInt32FromFloating(T value) noexcept
    {
        const double wide = static_cast<double>(value);
        if (std::isnan(wide))
        {
            return 0;
        }
        if (wide <= static_cast<double>(std::numeric_limits<std::int32_t>::min()))
        {
            return std::numeric_limits<std::int32_t>::min();
        }
        if (wide >= static_cast<double>(std::numeric_limits<std::int32_t>::max()))
        {
            return std::numeric_limits<std::int32_t>::max();
        }
        return static_cast<std::int32_t>(wide);
    }

    [[nodiscard]] std::uint16_t ManagedUShortFromRounded(double value) noexcept
    {
        const double rounded = RoundToEven(value);
        if (!std::isfinite(rounded))
        {
            return 0;
        }
        const auto wide = static_cast<std::int64_t>(rounded);
        return static_cast<std::uint16_t>(static_cast<std::uint64_t>(wide) & 0xFFFFULL);
    }

    [[nodiscard]] std::uint8_t ManagedByteFromRounded(double value) noexcept
    {
        return static_cast<std::uint8_t>(ManagedUShortFromRounded(value) & 0xFFU);
    }

    [[nodiscard]] std::uint16_t AngleValue(float angle) noexcept
    {
        const float scaled = angle / std::numbers::pi_v<float> / 2.0F * 65536.0F;
        return ManagedUShortFromRounded(static_cast<double>(scaled));
    }

    [[nodiscard]] std::uint16_t Color5(std::uint8_t component) noexcept
    {
        const float scaled = static_cast<float>(component) * 31.0F / 255.0F;
        return ManagedUShortFromRounded(static_cast<double>(scaled));
    }

    [[nodiscard]] std::uint8_t AlphaBits(
        std::uint8_t alpha, float maximum) noexcept
    {
        const float scaled = static_cast<float>(alpha) * maximum / 255.0F;
        return ManagedByteFromRounded(static_cast<double>(scaled));
    }

    [[nodiscard]] Vector3i FixedVector(Vector3 vector) noexcept
    {
        return Vector3i(
            MphRead::Fixed::ToInt(vector.X),
            MphRead::Fixed::ToInt(vector.Y),
            MphRead::Fixed::ToInt(vector.Z));
    }

    [[nodiscard]] bool EndsWith(const std::string& value, std::string_view suffix) noexcept
    {
        return value.size() >= suffix.size()
            && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    [[nodiscard]] std::string ToLowerAscii(std::string value)
    {
        for (char& c : value)
        {
            if (c >= 'A' && c <= 'Z')
            {
                c = static_cast<char>(c - 'A' + 'a');
            }
        }
        return value;
    }

    [[nodiscard]] std::shared_ptr<const std::vector<std::uint16_t>>
        PaletteWords(const std::shared_ptr<const std::vector<MphRead::PaletteData>>& data)
    {
        const auto& source = RequireReference(data);
        auto result = std::make_shared<std::vector<std::uint16_t>>();
        result->reserve(source.size());
        for (const auto& entry : source)
        {
            result->push_back(entry.Data);
        }
        return result;
    }

    [[nodiscard]] std::int32_t ToIntCount(std::size_t value) noexcept
    {
        return static_cast<std::int32_t>(static_cast<std::uint32_t>(value));
    }

    template <typename T>
    void WriteScalar(BinaryWriter& writer, T value)
    {
        using Value = std::remove_cv_t<T>;
        if constexpr (std::is_enum_v<Value>)
        {
            WriteScalar(writer, static_cast<std::underlying_type_t<Value>>(value));
        }
        else if constexpr (std::is_same_v<Value, bool>)
        {
            writer.Write(static_cast<std::uint8_t>(value ? 1 : 0));
        }
        else if constexpr (sizeof(Value) == 1 && std::is_signed_v<Value>)
        {
            writer.Write(static_cast<std::int8_t>(value));
        }
        else if constexpr (sizeof(Value) == 1)
        {
            writer.Write(static_cast<std::uint8_t>(value));
        }
        else if constexpr (sizeof(Value) == 2 && std::is_signed_v<Value>)
        {
            writer.Write(static_cast<std::int16_t>(value));
        }
        else if constexpr (sizeof(Value) == 2)
        {
            writer.Write(static_cast<std::uint16_t>(value));
        }
        else if constexpr (sizeof(Value) == 4 && std::is_signed_v<Value>)
        {
            writer.Write(static_cast<std::int32_t>(value));
        }
        else if constexpr (sizeof(Value) == 4)
        {
            writer.Write(static_cast<std::uint32_t>(value));
        }
        else
        {
            static_assert(sizeof(Value) <= 4, "Unsupported BinaryWriter scalar.");
        }
    }
}

namespace MphRead::Utility
{
    Repack::TextureInfo::TextureInfo(
        TextureFormat format, bool opaque, std::uint16_t height,
        std::uint16_t width, std::shared_ptr<const std::vector<std::uint8_t>> data)
        : Format(format), Opaque(opaque), Height(height), Width(width), Data(std::move(data))
    {
    }

    Repack::PaletteInfo::PaletteInfo(
        std::shared_ptr<const std::vector<std::uint16_t>> data)
        : Data(std::move(data))
    {
    }

    Repack::ImageInfo::ImageInfo(
        TextureFormat format, std::uint16_t height, std::uint16_t width,
        std::shared_ptr<const std::vector<ColorRgba>> pixels, bool paletteOpaque)
        : Format(format), Height(height), Width(width), Pixels(std::move(pixels)),
          PaletteOpaque(
              format == TextureFormat::Palette2Bit
              || format == TextureFormat::Palette4Bit
              || format == TextureFormat::Palette8Bit
                  ? paletteOpaque
                  : false)
    {
    }

    class Repack::TexMtxMap
    {
    public:
        using Key = std::tuple<
            std::uint16_t, std::uint16_t, std::int32_t, std::int32_t,
            std::uint16_t, std::int32_t, std::int32_t>;

        [[nodiscard]] std::int32_t Count() const noexcept
        {
            return static_cast<std::int32_t>(_entries.size());
        }

        [[nodiscard]] bool TryGetValue(
            std::uint16_t width, std::uint16_t height,
            std::int32_t scaleS, std::int32_t scaleT,
            std::uint16_t rotZ, std::int32_t transS, std::int32_t transT,
            std::int32_t& index) const
        {
            const Key key(width, height, scaleS, scaleT, rotZ, transS, transT);
            for (const auto& entry : _entries)
            {
                if (entry.first == key)
                {
                    index = entry.second;
                    return true;
                }
            }
            index = 0;
            return false;
        }

        void Add(
            std::uint16_t width, std::uint16_t height,
            std::int32_t scaleS, std::int32_t scaleT,
            std::uint16_t rotZ, std::int32_t transS, std::int32_t transT,
            std::int32_t index)
        {
            std::int32_t existing = 0;
            if (TryGetValue(width, height, scaleS, scaleT, rotZ, transS, transT, existing))
            {
                throw std::invalid_argument("An item with the same key has already been added.");
            }
            _entries.emplace_back(
                Key(width, height, scaleS, scaleT, rotZ, transS, transT), index);
        }

    private:
        std::vector<std::pair<Key, std::int32_t>> _entries{};
    };

    void Repack::WriteString(BinaryWriter& writer, const std::string& value, std::int32_t length)
    {
        const std::u16string units = Utf8ToUtf16(value);
        REPACK_MODEL_DEBUG_ASSERT(
            length >= 0 && units.size() <= static_cast<std::size_t>(length));
        std::int32_t i = 0;
        for (const char16_t unit : units)
        {
            writer.Write(static_cast<std::uint8_t>(unit));
            i = UncheckedAdd(i, 1);
        }
        for (; i < length; i = UncheckedAdd(i, 1))
        {
            writer.Write(static_cast<std::uint8_t>(0));
        }
    }

    void Repack::WriteFloat(BinaryWriter& writer, float value)
    {
        writer.WriteFloat(value);
    }

    void Repack::WriteVector3(BinaryWriter& writer, Vector3 vector)
    {
        writer.WriteVector3(vector);
    }

    void Repack::WriteVector4(BinaryWriter& writer, OpenTK::Mathematics::Vector4 vector)
    {
        writer.WriteVector4(vector);
    }

    void Repack::WriteColorRgb(BinaryWriter& writer, ColorRgb color)
    {
        writer.WriteColorRgb(color);
    }

    void Repack::WriteAngle(BinaryWriter& writer, float angle)
    {
        writer.Write(AngleValue(angle));
    }

    void Repack::WriteAngles(BinaryWriter& writer, Vector3 angles)
    {
        WriteAngle(writer, angles.X);
        WriteAngle(writer, angles.Y);
        WriteAngle(writer, angles.Z);
    }

    void Repack::WriteByte(BinaryWriter& writer, bool value)
    {
        writer.WriteByte(value);
    }

    void Repack::WriteInt(BinaryWriter& writer, bool value)
    {
        writer.WriteInt(value);
    }

    std::vector<std::uint8_t> Repack::PackAnim(
        const std::shared_ptr<const std::vector<std::shared_ptr<NodeAnimationGroup>>>& nodeGroups,
        const std::shared_ptr<const std::vector<std::shared_ptr<MaterialAnimationGroup>>>& matGroups,
        const std::shared_ptr<const std::vector<std::shared_ptr<TexcoordAnimationGroup>>>& uvGroups,
        const std::shared_ptr<const std::vector<std::shared_ptr<TextureAnimationGroup>>>& texGroups,
        bool fhPad)
    {
        const auto& nodeValues = RequireReference(nodeGroups);
        const std::uint16_t padShort = fhPad ? static_cast<std::uint16_t>(0xCCCC) : 0;

        REPACK_MODEL_DEBUG_ASSERT(
            nodeValues.size() == RequireReference(matGroups).size()
            && RequireReference(matGroups).size() == RequireReference(uvGroups).size()
            && RequireReference(uvGroups).size() == RequireReference(texGroups).size());

        const std::int32_t count = ToIntCount(nodeValues.size());
        std::vector<std::int32_t> nodeGroupOffsets;
        std::vector<std::int32_t> matGroupOffsets;
        std::vector<std::int32_t> uvGroupOffsets;
        std::vector<std::int32_t> unusedGroupOffsets;
        std::vector<std::int32_t> texGroupOffsets;
        nodeGroupOffsets.reserve(nodeValues.size());
        matGroupOffsets.reserve(nodeValues.size());
        uvGroupOffsets.reserve(nodeValues.size());
        unusedGroupOffsets.reserve(nodeValues.size());
        texGroupOffsets.reserve(nodeValues.size());

        BinaryWriter writer;
        writer.Position(static_cast<std::size_t>(Sizes::AnimationHeader));

        for (std::int32_t i = 0; i < count; ++i)
        {
            const auto& nodeGroup = ManagedListAt(nodeValues, i);
            nodeGroupOffsets.push_back(nodeGroup ? WriteNodeGroup(nodeGroup, fhPad, writer) : 0);
            const auto& matGroup = ManagedListAt(RequireReference(matGroups), i);
            matGroupOffsets.push_back(matGroup ? WriteMatGroup(matGroup, fhPad, writer) : 0);
            const auto& texGroup = ManagedListAt(RequireReference(texGroups), i);
            texGroupOffsets.push_back(texGroup ? WriteTexGroup(texGroup, fhPad, writer) : 0);
            const auto& uvGroup = ManagedListAt(RequireReference(uvGroups), i);
            uvGroupOffsets.push_back(uvGroup ? WriteUvGroup(uvGroup, fhPad, writer) : 0);
            unusedGroupOffsets.push_back(0);
        }

        const std::int32_t nodeGroupList = ToIntCount(writer.Position());
        for (std::int32_t offset : nodeGroupOffsets)
        {
            writer.Write(offset);
        }
        const std::int32_t unusedGroupList = ToIntCount(writer.Position());
        for (std::int32_t offset : unusedGroupOffsets)
        {
            writer.Write(offset);
        }
        const std::int32_t matGroupList = ToIntCount(writer.Position());
        for (std::int32_t offset : matGroupOffsets)
        {
            writer.Write(offset);
        }
        const std::int32_t uvGroupList = ToIntCount(writer.Position());
        for (std::int32_t offset : uvGroupOffsets)
        {
            writer.Write(offset);
        }
        const std::int32_t texGroupList = ToIntCount(writer.Position());
        for (std::int32_t offset : texGroupOffsets)
        {
            writer.Write(offset);
        }

        writer.Position(0);
        writer.Write(nodeGroupList);
        writer.Write(unusedGroupList);
        writer.Write(matGroupList);
        writer.Write(uvGroupList);
        writer.Write(texGroupList);
        writer.Write(static_cast<std::uint16_t>(count));
        writer.Write(padShort);
        REPACK_MODEL_DEBUG_ASSERT(
            writer.Position() == static_cast<std::size_t>(Sizes::AnimationHeader));
        return writer.ToArray();
    }

    std::int32_t Repack::WriteNodeGroup(
        const std::shared_ptr<NodeAnimationGroup>& group, bool fhPad, BinaryWriter& writer)
    {
        NodeAnimationGroup& value = RequireReference(group);
        const std::uint16_t padShort = fhPad ? static_cast<std::uint16_t>(0xCCCC) : 0;

        const std::int32_t scaleOffset = ToIntCount(writer.Position());
        for (float entry : RequireReference(value.Scales))
        {
            WriteFloat(writer, entry);
        }

        const std::int32_t rotOffset = ToIntCount(writer.Position());
        for (float entry : RequireReference(value.Rotations))
        {
            WriteAngle(writer, entry);
        }
        while (writer.Position() % 4 != 0)
        {
            writer.Write(padShort);
        }

        const std::int32_t transOffset = ToIntCount(writer.Position());
        for (float entry : RequireReference(value.Translations))
        {
            WriteFloat(writer, entry);
        }

        const std::int32_t animOffset = ToIntCount(writer.Position());
        for (const auto& pair : RequireReference(value.Animations))
        {
            const NodeAnimation& anim = pair.second;
            WriteScalar(writer, anim.ScaleBlendX);
            WriteScalar(writer, anim.ScaleBlendY);
            WriteScalar(writer, anim.ScaleBlendZ);
            WriteScalar(writer, anim.Flags);
            WriteScalar(writer, anim.ScaleLutLengthX);
            WriteScalar(writer, anim.ScaleLutLengthY);
            WriteScalar(writer, anim.ScaleLutLengthZ);
            WriteScalar(writer, anim.ScaleLutIndexX);
            WriteScalar(writer, anim.ScaleLutIndexY);
            WriteScalar(writer, anim.ScaleLutIndexZ);
            WriteScalar(writer, anim.RotateBlendX);
            WriteScalar(writer, anim.RotateBlendY);
            WriteScalar(writer, anim.RotateBlendZ);
            WriteScalar(writer, anim.Padding13);
            WriteScalar(writer, anim.RotateLutLengthX);
            WriteScalar(writer, anim.RotateLutLengthY);
            WriteScalar(writer, anim.RotateLutLengthZ);
            WriteScalar(writer, anim.RotateLutIndexX);
            WriteScalar(writer, anim.RotateLutIndexY);
            WriteScalar(writer, anim.RotateLutIndexZ);
            WriteScalar(writer, anim.TranslateBlendX);
            WriteScalar(writer, anim.TranslateBlendY);
            WriteScalar(writer, anim.TranslateBlendZ);
            WriteScalar(writer, anim.Padding23);
            WriteScalar(writer, anim.TranslateLutLengthX);
            WriteScalar(writer, anim.TranslateLutLengthY);
            WriteScalar(writer, anim.TranslateLutLengthZ);
            WriteScalar(writer, anim.TranslateLutIndexX);
            WriteScalar(writer, anim.TranslateLutIndexY);
            WriteScalar(writer, anim.TranslateLutIndexZ);
        }

        const std::int32_t groupOffset = ToIntCount(writer.Position());
        writer.Write(value.FrameCount);
        writer.Write(scaleOffset);
        writer.Write(rotOffset);
        writer.Write(transOffset);
        writer.Write(animOffset);
        return groupOffset;
    }

    std::int32_t Repack::WriteMatGroup(
        const std::shared_ptr<MaterialAnimationGroup>& group, bool fhPad, BinaryWriter& writer)
    {
        MaterialAnimationGroup& value = RequireReference(group);
        const std::uint8_t padByte = fhPad ? static_cast<std::uint8_t>(0xCC) : 0;

        const std::int32_t colorOffset = ToIntCount(writer.Position());
        for (float entry : RequireReference(value.Colors))
        {
            writer.Write(static_cast<std::uint8_t>(entry));
        }
        while (writer.Position() % 4 != 0)
        {
            writer.Write(padByte);
        }

        const std::int32_t animOffset = ToIntCount(writer.Position());
        for (const auto& pair : RequireReference(value.Animations))
        {
            const MaterialAnimation& anim = pair.second;
            for (std::uint8_t byte : anim.Name)
            {
                writer.Write(byte);
            }
            REPACK_MODEL_DEBUG_ASSERT(writer.Position() % 4 == 0);
            WriteScalar(writer, anim.Unused40);
            WriteScalar(writer, anim.DiffuseBlendR);
            WriteScalar(writer, anim.DiffuseBlendG);
            WriteScalar(writer, anim.DiffuseBlendB);
            WriteScalar(writer, anim.Unused47);
            WriteScalar(writer, anim.DiffuseLutLengthR);
            WriteScalar(writer, anim.DiffuseLutLengthG);
            WriteScalar(writer, anim.DiffuseLutLengthB);
            WriteScalar(writer, anim.DiffuseLutIndexR);
            WriteScalar(writer, anim.DiffuseLutIndexG);
            WriteScalar(writer, anim.DiffuseLutIndexB);
            WriteScalar(writer, anim.AmbientBlendR);
            WriteScalar(writer, anim.AmbientBlendG);
            WriteScalar(writer, anim.AmbientBlendB);
            WriteScalar(writer, anim.Unused57);
            WriteScalar(writer, anim.AmbientLutLengthR);
            WriteScalar(writer, anim.AmbientLutLengthG);
            WriteScalar(writer, anim.AmbientLutLengthB);
            WriteScalar(writer, anim.AmbientLutIndexR);
            WriteScalar(writer, anim.AmbientLutIndexG);
            WriteScalar(writer, anim.AmbientLutIndexB);
            WriteScalar(writer, anim.SpecularBlendR);
            WriteScalar(writer, anim.SpecularBlendG);
            WriteScalar(writer, anim.SpecularBlendB);
            WriteScalar(writer, anim.Unused67);
            WriteScalar(writer, anim.SpecularLutLengthR);
            WriteScalar(writer, anim.SpecularLutLengthG);
            WriteScalar(writer, anim.SpecularLutLengthB);
            WriteScalar(writer, anim.SpecularLutIndexR);
            WriteScalar(writer, anim.SpecularLutIndexG);
            WriteScalar(writer, anim.SpecularLutIndexB);
            WriteScalar(writer, anim.Unused74);
            WriteScalar(writer, anim.Unused78);
            WriteScalar(writer, anim.Unused7C);
            WriteScalar(writer, anim.Unused80);
            WriteScalar(writer, anim.AlphaBlend);
            WriteScalar(writer, anim.Unused85);
            WriteScalar(writer, anim.AlphaLutLength);
            WriteScalar(writer, anim.AlphaLutIndex);
            WriteScalar(writer, anim.MaterialId);
        }

        const std::int32_t groupOffset = ToIntCount(writer.Position());
        writer.Write(value.FrameCount);
        writer.Write(colorOffset);
        writer.Write(ToIntCount(RequireReference(value.Animations).size()));
        writer.Write(animOffset);
        writer.Write(static_cast<std::uint16_t>(value.CurrentFrame));
        writer.Write(static_cast<std::uint16_t>(value.UnusedFrame));
        return groupOffset;
    }

    std::int32_t Repack::WriteUvGroup(
        const std::shared_ptr<TexcoordAnimationGroup>& group, bool fhPad, BinaryWriter& writer)
    {
        TexcoordAnimationGroup& value = RequireReference(group);
        const std::uint16_t padShort = fhPad ? static_cast<std::uint16_t>(0xCCCC) : 0;

        const std::int32_t scaleOffset = ToIntCount(writer.Position());
        for (float entry : RequireReference(value.Scales))
        {
            WriteFloat(writer, entry);
        }
        const std::int32_t rotOffset = ToIntCount(writer.Position());
        for (float entry : RequireReference(value.Rotations))
        {
            WriteAngle(writer, entry);
        }
        while (writer.Position() % 4 != 0)
        {
            writer.Write(padShort);
        }
        const std::int32_t transOffset = ToIntCount(writer.Position());
        for (float entry : RequireReference(value.Translations))
        {
            WriteFloat(writer, entry);
        }

        const std::int32_t animOffset = ToIntCount(writer.Position());
        for (const auto& pair : RequireReference(value.Animations))
        {
            const TexcoordAnimation& anim = pair.second;
            for (std::uint8_t byte : anim.Name)
            {
                writer.Write(byte);
            }
            REPACK_MODEL_DEBUG_ASSERT(writer.Position() % 4 == 0);
            WriteScalar(writer, anim.ScaleBlendS);
            WriteScalar(writer, anim.ScaleBlendT);
            WriteScalar(writer, anim.ScaleLutLengthS);
            WriteScalar(writer, anim.ScaleLutLengthT);
            WriteScalar(writer, anim.ScaleLutIndexS);
            WriteScalar(writer, anim.ScaleLutIndexT);
            WriteScalar(writer, anim.RotateBlendZ);
            WriteScalar(writer, anim.Unused2B);
            WriteScalar(writer, anim.RotateLutLengthZ);
            WriteScalar(writer, anim.RotateLutIndexZ);
            WriteScalar(writer, anim.TranslateBlendS);
            WriteScalar(writer, anim.TranslateBlendT);
            WriteScalar(writer, anim.TranslateLutLengthS);
            WriteScalar(writer, anim.TranslateLutLengthT);
            WriteScalar(writer, anim.TranslateLutIndexS);
            WriteScalar(writer, anim.TranslateLutIndexT);
            writer.Write(padShort);
        }

        const std::int32_t groupOffset = ToIntCount(writer.Position());
        writer.Write(value.FrameCount);
        writer.Write(scaleOffset);
        writer.Write(rotOffset);
        writer.Write(transOffset);
        writer.Write(ToIntCount(RequireReference(value.Animations).size()));
        writer.Write(animOffset);
        writer.Write(static_cast<std::uint16_t>(value.CurrentFrame));
        writer.Write(static_cast<std::uint16_t>(value.UnusedFrame));
        return groupOffset;
    }

    std::int32_t Repack::WriteTexGroup(
        const std::shared_ptr<TextureAnimationGroup>& group, bool fhPad, BinaryWriter& writer)
    {
        TextureAnimationGroup& value = RequireReference(group);
        const std::uint16_t padShort = fhPad ? static_cast<std::uint16_t>(0xCCCC) : 0;

        const std::int32_t frameOffset = ToIntCount(writer.Position());
        for (std::uint16_t entry : RequireReference(value.FrameIndices))
        {
            writer.Write(entry);
        }
        const std::int32_t texOffset = ToIntCount(writer.Position());
        for (std::uint16_t entry : RequireReference(value.TextureIds))
        {
            writer.Write(entry);
        }
        const std::int32_t palOffset = ToIntCount(writer.Position());
        for (std::uint16_t entry : RequireReference(value.PaletteIds))
        {
            writer.Write(entry);
        }
        while (writer.Position() % 4 != 0)
        {
            writer.Write(padShort);
        }

        const std::int32_t animOffset = ToIntCount(writer.Position());
        for (const auto& pair : RequireReference(value.Animations))
        {
            const TextureAnimation& anim = pair.second;
            for (std::uint8_t byte : anim.Name)
            {
                writer.Write(byte);
            }
            REPACK_MODEL_DEBUG_ASSERT(writer.Position() % 4 == 0);
            WriteScalar(writer, anim.Count);
            WriteScalar(writer, anim.StartIndex);
            WriteScalar(writer, anim.MinimumPaletteId);
            WriteScalar(writer, anim.MaterialId);
            WriteScalar(writer, anim.MinimumTextureId);
            writer.Write(padShort);
        }

        const std::int32_t groupOffset = ToIntCount(writer.Position());
        writer.Write(static_cast<std::uint16_t>(value.FrameCount));
        writer.Write(static_cast<std::uint16_t>(RequireReference(value.FrameIndices).size()));
        writer.Write(static_cast<std::uint16_t>(RequireReference(value.TextureIds).size()));
        writer.Write(static_cast<std::uint16_t>(RequireReference(value.PaletteIds).size()));
        writer.Write(static_cast<std::uint16_t>(RequireReference(value.Animations).size()));
        writer.Write(value.UnusedA);
        writer.Write(frameOffset);
        writer.Write(texOffset);
        writer.Write(palOffset);
        writer.Write(animOffset);
        writer.Write(static_cast<std::uint16_t>(value.CurrentFrame));
        writer.Write(static_cast<std::uint16_t>(value.UnusedFrame));
        return groupOffset;
    }

    std::pair<Vector3i, Vector3i> Repack::CalculateBounds(
        const std::shared_ptr<const std::vector<std::shared_ptr<RenderInstruction>>>& insts)
    {
        const auto& instructions = RequireReference(insts);
        std::vector<Vector3i> verts;
        std::int32_t vtxX = 0;
        std::int32_t vtxY = 0;
        std::int32_t vtxZ = 0;

        const auto update = [&]()
        {
            verts.emplace_back(vtxX, vtxY, vtxZ);
        };

        for (const auto& instructionPtr : instructions)
        {
            const RenderInstruction& instruction = RequireReference(instructionPtr);
            switch (instruction.Code)
            {
            case InstructionCode::VTX_16:
            {
                const auto& arguments = RequireReference(instruction.Arguments);
                const std::uint32_t xy = ManagedListAt(arguments, 0);
                const std::int32_t x = Sign16(xy);
                const std::int32_t y = Sign16(xy >> 16);
                const std::int32_t z = Sign16(ManagedListAt(arguments, 1));
                vtxX = x;
                vtxY = y;
                vtxZ = z;
                update();
                break;
            }
            case InstructionCode::VTX_10:
            {
                const auto& arguments = RequireReference(instruction.Arguments);
                const std::uint32_t xyz = ManagedListAt(arguments, 0);
                const std::int32_t x = Sign10(xyz);
                const std::int32_t y = Sign10(xyz >> 10);
                const std::int32_t z = Sign10(xyz >> 20);
                vtxX = UncheckedMultiply(x, 64);
                vtxY = UncheckedMultiply(y, 64);
                vtxZ = UncheckedMultiply(z, 64);
                update();
                break;
            }
            case InstructionCode::VTX_XY:
            {
                const auto& arguments = RequireReference(instruction.Arguments);
                const std::uint32_t xy = ManagedListAt(arguments, 0);
                vtxX = Sign16(xy);
                vtxY = Sign16(xy >> 16);
                update();
                break;
            }
            case InstructionCode::VTX_XZ:
            {
                const auto& arguments = RequireReference(instruction.Arguments);
                const std::uint32_t xz = ManagedListAt(arguments, 0);
                vtxX = Sign16(xz);
                vtxZ = Sign16(xz >> 16);
                update();
                break;
            }
            case InstructionCode::VTX_YZ:
            {
                const auto& arguments = RequireReference(instruction.Arguments);
                const std::uint32_t yz = ManagedListAt(arguments, 0);
                vtxY = Sign16(yz);
                vtxZ = Sign16(yz >> 16);
                update();
                break;
            }
            case InstructionCode::VTX_DIFF:
            {
                const auto& arguments = RequireReference(instruction.Arguments);
                const std::uint32_t xyz = ManagedListAt(arguments, 0);
                vtxX = UncheckedAdd(vtxX, Sign10(xyz));
                vtxY = UncheckedAdd(vtxY, Sign10(xyz >> 10));
                vtxZ = UncheckedAdd(vtxZ, Sign10(xyz >> 20));
                update();
                break;
            }
            default:
                break;
            }
        }

        std::int32_t minX = std::numeric_limits<std::int32_t>::max();
        std::int32_t maxX = std::numeric_limits<std::int32_t>::min();
        std::int32_t minY = std::numeric_limits<std::int32_t>::max();
        std::int32_t maxY = std::numeric_limits<std::int32_t>::min();
        std::int32_t minZ = std::numeric_limits<std::int32_t>::max();
        std::int32_t maxZ = std::numeric_limits<std::int32_t>::min();
        for (const Vector3i& vert : verts)
        {
            minX = std::min(minX, vert.X);
            maxX = std::max(maxX, vert.X);
            minY = std::min(minY, vert.Y);
            maxY = std::max(maxY, vert.Y);
            minZ = std::min(minZ, vert.Z);
            maxZ = std::max(maxZ, vert.Z);
        }
        return {
            Vector3i(minX, minY, minZ),
            Vector3i(maxX, maxY, maxZ)
        };
    }

    std::pair<std::int32_t, std::int32_t> Repack::GetDlistCounts(
        const std::shared_ptr<const std::vector<std::shared_ptr<RenderInstruction>>>& dlist)
    {
        const auto& values = RequireReference(dlist);
        std::int32_t primitiveCount = 0;
        std::int32_t vertexCount = 0;
        std::int32_t vertexType = -1;
        std::int32_t currentVertexCount = 0;
        for (const auto& instructionPtr : values)
        {
            const RenderInstruction& instruction = RequireReference(instructionPtr);
            switch (instruction.Code)
            {
            case InstructionCode::BEGIN_VTXS:
                REPACK_MODEL_DEBUG_ASSERT(vertexType == -1 && currentVertexCount == 0);
                vertexType = static_cast<std::int32_t>(
                    ManagedListAt(RequireReference(instruction.Arguments), 0));
                break;
            case InstructionCode::VTX_16:
            case InstructionCode::VTX_10:
            case InstructionCode::VTX_XY:
            case InstructionCode::VTX_XZ:
            case InstructionCode::VTX_YZ:
            case InstructionCode::VTX_DIFF:
                vertexCount = UncheckedAdd(vertexCount, 1);
                currentVertexCount = UncheckedAdd(currentVertexCount, 1);
                break;
            case InstructionCode::END_VTXS:
                if (vertexType == 0)
                {
                    REPACK_MODEL_DEBUG_ASSERT(
                        currentVertexCount >= 3 && currentVertexCount % 3 == 0);
                    primitiveCount = UncheckedAdd(
                        primitiveCount, currentVertexCount / 3);
                }
                else if (vertexType == 1)
                {
                    REPACK_MODEL_DEBUG_ASSERT(
                        currentVertexCount >= 4 && currentVertexCount % 4 == 0);
                    primitiveCount = UncheckedAdd(
                        primitiveCount, currentVertexCount / 4);
                }
                else if (vertexType == 2)
                {
                    REPACK_MODEL_DEBUG_ASSERT(currentVertexCount >= 3);
                    primitiveCount = UncheckedAdd(
                        primitiveCount, UncheckedAdd(1, currentVertexCount - 3));
                }
                else if (vertexType == 3)
                {
                    REPACK_MODEL_DEBUG_ASSERT(
                        currentVertexCount >= 4 && currentVertexCount % 2 == 0);
                    primitiveCount = UncheckedAdd(
                        primitiveCount, UncheckedAdd(1, (currentVertexCount - 4) / 2));
                }
                vertexType = -1;
                currentVertexCount = 0;
                break;
            default:
                break;
            }
        }
        return {primitiveCount, vertexCount};
    }

    std::pair<std::vector<std::uint8_t>, std::vector<std::uint8_t>>
    Repack::PackModel(const std::shared_ptr<Model>& model)
    {
        Model& modelValue = RequireReference(model);
        const std::int32_t recolor = 0;
        const auto& recolors = RequireReference(modelValue.Recolors);
        const auto& recolorValue = RequireReference(ManagedListAt(recolors, recolor));

        auto textureInfo = std::make_shared<std::vector<std::shared_ptr<TextureInfo>>>();
        const auto& textures = RequireReference(recolorValue.Textures);
        textureInfo->reserve(textures.size());
        for (std::int32_t i = 0; i < ToIntCount(textures.size()); ++i)
        {
            const Texture& texture = ManagedListAt(textures, i);
            textureInfo->push_back(ConvertData(
                texture, ManagedListAt(RequireReference(recolorValue.TextureData), i)));
        }

        auto paletteInfo = std::make_shared<std::vector<std::shared_ptr<PaletteInfo>>>();
        const auto& paletteData = RequireReference(recolorValue.PaletteData);
        paletteInfo->reserve(paletteData.size());
        for (const auto& data : paletteData)
        {
            paletteInfo->push_back(std::make_shared<PaletteInfo>(PaletteWords(data)));
        }

        auto options = std::make_shared<RepackOptions>();
        options->Compare = false;
        options->ComputeBounds = ComputeBounds::None;
        options->IsRoom = false;
        options->Texture = RepackTexture::Inline;
        options->WriteFile = false;

        return PackModel(
            ManagedInt32FromFloating(modelValue.Scale.X),
            modelValue.NodeMatrixIds,
            modelValue.NodePosCounts,
            modelValue.Materials,
            std::const_pointer_cast<const std::vector<std::shared_ptr<TextureInfo>>>(textureInfo),
            std::const_pointer_cast<const std::vector<std::shared_ptr<PaletteInfo>>>(paletteInfo),
            modelValue.Nodes,
            modelValue.Meshes,
            modelValue.RenderInstructionLists,
            modelValue.DisplayLists,
            options);
    }

    std::pair<std::vector<std::uint8_t>, std::vector<std::uint8_t>>
    Repack::PackModel(
        std::int32_t scale,
        const std::shared_ptr<const std::vector<std::int32_t>>& nodeMtxIds,
        const std::shared_ptr<const std::vector<std::int32_t>>& nodePosScaleCounts,
        const std::shared_ptr<const std::vector<std::shared_ptr<Material>>>& materials,
        const std::shared_ptr<const std::vector<std::shared_ptr<TextureInfo>>>& textures,
        const std::shared_ptr<const std::vector<std::shared_ptr<PaletteInfo>>>& palettes,
        const std::shared_ptr<const std::vector<std::shared_ptr<Node>>>& nodes,
        const std::shared_ptr<const std::vector<std::shared_ptr<Mesh>>>& meshes,
        const std::shared_ptr<const std::vector<
            std::shared_ptr<const std::vector<std::shared_ptr<RenderInstruction>>>>>& renders,
        const std::shared_ptr<const std::vector<DisplayList>>& dlists,
        const std::shared_ptr<RepackOptions>& options)
    {
        RepackOptions& optionValues = RequireReference(options);

        constexpr std::uint8_t padByte = 0;
        constexpr std::uint16_t padShort = 0;
        constexpr std::uint32_t padInt = 0;

        BinaryWriter writer;
        BinaryWriter separateTexWriter;
        BinaryWriter* texWriter = optionValues.Texture == RepackTexture::Inline
            ? &writer : &separateTexWriter;

        std::int32_t primitiveCount = 0;
        std::int32_t vertexCount = 0;

        std::vector<Vector3i> dlistMin;
        std::vector<Vector3i> dlistMax;
        std::vector<Vector3i> nodeMin;
        std::vector<Vector3i> nodeMax;

        REPACK_MODEL_DEBUG_ASSERT(scale > 0);
        REPACK_MODEL_DEBUG_ASSERT(
            RequireReference(renders).size() == RequireReference(dlists).size());

        const auto& renderValues = RequireReference(renders);
        for (const auto& render : renderValues)
        {
            const auto [primitives, vertices] = GetDlistCounts(render);
            primitiveCount = UncheckedAdd(primitiveCount, primitives);
            vertexCount = UncheckedAdd(vertexCount, vertices);
        }

        const auto& nodeMtxIdValues = RequireReference(nodeMtxIds);
        if (!nodeMtxIdValues.empty())
        {
            optionValues.ComputeBounds = ComputeBounds::None;
        }

        if (optionValues.ComputeBounds == ComputeBounds::None)
        {
            const auto& dlistBoundsValues = RequireReference(dlists);
            dlistMin.reserve(dlistBoundsValues.size());
            dlistMax.reserve(dlistBoundsValues.size());
            for (const DisplayList& dlist : dlistBoundsValues)
            {
                dlistMin.push_back(dlist.MinBounds.ToIntVector());
                dlistMax.push_back(dlist.MaxBounds.ToIntVector());
            }
            const auto& nodeBoundsValues = RequireReference(nodes);
            nodeMin.reserve(nodeBoundsValues.size());
            nodeMax.reserve(nodeBoundsValues.size());
            for (const auto& nodePtr : nodeBoundsValues)
            {
                const Node& node = RequireReference(nodePtr);
                nodeMin.push_back(FixedVector(node.MinBounds));
                nodeMax.push_back(FixedVector(node.MaxBounds));
            }
        }
        else
        {
            std::vector<Vector3i> allMin;
            std::vector<Vector3i> allMax;
            allMin.reserve(renderValues.size());
            allMax.reserve(renderValues.size());
            for (const auto& insts : renderValues)
            {
                const auto bounds = CalculateBounds(insts);
                allMin.push_back(bounds.first);
                allMax.push_back(bounds.second);
            }

            const auto& nodeBoundsValues = RequireReference(nodes);
            nodeMin.reserve(nodeBoundsValues.size());
            nodeMax.reserve(nodeBoundsValues.size());
            for (const auto& nodePtr : nodeBoundsValues)
            {
                const Node& node = RequireReference(nodePtr);
                const auto ids = node.MeshCount == 0
                    ? node.GetAllMeshIds(nodes, true)
                    : node.GetMeshIds();
                bool any = false;
                for (std::int32_t id : ids)
                {
                    (void)id;
                    any = true;
                    break;
                }

                if (!any)
                {
                    nodeMin.emplace_back(0, 0, 0);
                    nodeMax.emplace_back(0, 0, 0);
                }
                else
                {
                    Vector3i minimum(
                        std::numeric_limits<std::int32_t>::max(),
                        std::numeric_limits<std::int32_t>::max(),
                        std::numeric_limits<std::int32_t>::max());
                    Vector3i maximum(
                        std::numeric_limits<std::int32_t>::min(),
                        std::numeric_limits<std::int32_t>::min(),
                        std::numeric_limits<std::int32_t>::min());
                    for (std::int32_t id : ids)
                    {
                        const std::int32_t dlistId
                            = RequireReference(ManagedListAt(RequireReference(meshes), id)).DlistId;
                        const Vector3i meshMin = ManagedListAt(allMin, dlistId);
                        const Vector3i meshMax = ManagedListAt(allMax, dlistId);
                        minimum.X = std::min(minimum.X, meshMin.X);
                        minimum.Y = std::min(minimum.Y, meshMin.Y);
                        minimum.Z = std::min(minimum.Z, meshMin.Z);
                        maximum.X = std::max(maximum.X, meshMax.X);
                        maximum.Y = std::max(maximum.Y, meshMax.Y);
                        maximum.Z = std::max(maximum.Z, meshMax.Z);
                    }
                    nodeMin.emplace_back(
                        UncheckedMultiply(minimum.X, scale),
                        UncheckedMultiply(minimum.Y, scale),
                        UncheckedMultiply(minimum.Z, scale));
                    nodeMax.emplace_back(
                        UncheckedMultiply(maximum.X, scale),
                        UncheckedMultiply(maximum.Y, scale),
                        UncheckedMultiply(maximum.Z, scale));
                }
            }

            const std::int32_t clampMin = UncheckedMultiply(
                std::numeric_limits<std::int16_t>::min(), scale);
            const std::int32_t clampMax = UncheckedMultiply(
                std::numeric_limits<std::int16_t>::max(), scale);
            for (std::size_t i = 0; i < allMin.size(); ++i)
            {
                const Vector3i minimum = allMin[i];
                const Vector3i maximum = allMax[i];
                if (optionValues.ComputeBounds == ComputeBounds::Capped)
                {
                    dlistMin.emplace_back(
                        MathClamp(UncheckedMultiply(minimum.X, scale), clampMin, clampMax),
                        MathClamp(UncheckedMultiply(minimum.Y, scale), clampMin, clampMax),
                        MathClamp(UncheckedMultiply(minimum.Z, scale), clampMin, clampMax));
                    dlistMax.emplace_back(
                        MathClamp(UncheckedMultiply(maximum.X, scale), clampMin, clampMax),
                        MathClamp(UncheckedMultiply(maximum.Y, scale), clampMin, clampMax),
                        MathClamp(UncheckedMultiply(maximum.Z, scale), clampMin, clampMax));
                }
                else
                {
                    dlistMin.push_back(minimum);
                    dlistMax.push_back(maximum);
                }
            }
        }

        writer.Position(static_cast<std::size_t>(Sizes::Header));

        const auto& nodeValues = RequireReference(nodes);
        const auto& nodePosScaleCountValues = RequireReference(nodePosScaleCounts);
        std::int32_t nodeMtxIdOffset = 0;
        std::int32_t actualOffset = Sizes::Header;
        if (nodeMtxIdValues.empty())
        {
            nodeMtxIdOffset = 0;
            if (nodePosScaleCountValues.empty())
            {
                actualOffset = UncheckedAdd(actualOffset, 4);
            }
            else
            {
                actualOffset = UncheckedAdd(actualOffset,
                    UncheckedMultiply(ToIntCount(nodePosScaleCountValues.size()), 4));
            }
        }
        else
        {
            nodeMtxIdOffset = actualOffset;
            actualOffset = UncheckedAdd(actualOffset,
                UncheckedMultiply(ToIntCount(nodeMtxIdValues.size()), 4));
        }

        const std::int32_t nodePosCountOffset
            = nodePosScaleCountValues.empty() ? 0 : actualOffset;

        if (nodeMtxIdValues.empty())
        {
            if (optionValues.IsRoom)
            {
                for (std::int32_t i = 0; i < ToIntCount(nodeValues.size()); ++i)
                {
                    if (RequireReference(ManagedListAt(nodeValues, i)).MeshCount > 0)
                    {
                        writer.Write(i);
                    }
                }
            }
            else
            {
                const std::int32_t padCount = nodePosScaleCountValues.empty()
                    ? 1 : ToIntCount(nodePosScaleCountValues.size());
                for (std::int32_t i = 0; i < padCount; ++i)
                {
                    writer.Write(nodeValues.size() <= 1 ? 0 : UncheckedAdd(i, 1));
                }
            }
        }
        else
        {
            for (std::int32_t value : nodeMtxIdValues)
            {
                writer.Write(value);
            }
        }

        for (std::int32_t value : nodePosScaleCountValues)
        {
            writer.Write(value);
        }

        const auto& textureValues = RequireReference(textures);
        std::vector<std::int32_t> textureDataOffsets;
        textureDataOffsets.reserve(textureValues.size());
        for (const auto& texturePtr : textureValues)
        {
            const TextureInfo& texture = RequireReference(texturePtr);
            textureDataOffsets.push_back(ToIntCount(texWriter->Position()));
            for (std::uint8_t data : RequireReference(texture.Data))
            {
                texWriter->Write(data);
            }
        }

        const std::int32_t texturesOffset = textureValues.empty()
            ? 0 : ToIntCount(writer.Position());
        for (std::int32_t i = 0; i < ToIntCount(textureValues.size()); ++i)
        {
            WriteTextureMeta(
                ManagedListAt(textureValues, i),
                ManagedListAt(textureDataOffsets, i),
                writer);
        }

        const auto& paletteValues = RequireReference(palettes);
        std::vector<std::int32_t> paletteDataOffsets;
        paletteDataOffsets.reserve(paletteValues.size());
        for (const auto& palettePtr : paletteValues)
        {
            const PaletteInfo& palette = RequireReference(palettePtr);
            paletteDataOffsets.push_back(ToIntCount(texWriter->Position()));
            for (std::uint16_t data : RequireReference(palette.Data))
            {
                texWriter->Write(data);
            }
        }

        const std::int32_t paletteOffset = paletteValues.empty()
            ? 0 : ToIntCount(writer.Position());
        for (std::int32_t i = 0; i < ToIntCount(paletteValues.size()); ++i)
        {
            const PaletteInfo& palette = RequireReference(ManagedListAt(paletteValues, i));
            writer.Write(ManagedListAt(paletteDataOffsets, i));
            writer.Write(UncheckedMultiply(ToIntCount(RequireReference(palette.Data).size()), 2));
            writer.Write(padInt);
            writer.Write(padInt);
        }

        std::vector<std::pair<std::int32_t, std::int32_t>> dlistResults;
        dlistResults.reserve(renderValues.size());
        for (const auto& render : renderValues)
        {
            const std::int32_t offset = ToIntCount(writer.Position());
            const std::int32_t size = WriteRenderInstructions(render, writer);
            dlistResults.emplace_back(offset, size);
        }

        const std::int32_t dlistsOffset = ToIntCount(writer.Position());
        const auto& dlistValues = RequireReference(dlists);
        for (std::int32_t i = 0; i < ToIntCount(dlistValues.size()); ++i)
        {
            const Vector3i minimum = ManagedListAt(dlistMin, i);
            const Vector3i maximum = ManagedListAt(dlistMax, i);
            const auto result = ManagedListAt(dlistResults, i);
            writer.Write(result.first);
            writer.Write(result.second);
            writer.Write(minimum.X);
            writer.Write(minimum.Y);
            writer.Write(minimum.Z);
            writer.Write(maximum.X);
            writer.Write(maximum.Y);
            writer.Write(maximum.Z);
        }

        const auto& materialValues = RequireReference(materials);
        std::int32_t matrixIdCount = 0;
        const std::int32_t materialsOffset = ToIntCount(writer.Position());
        for (const auto& material : materialValues)
        {
            const std::int32_t matrixId = GetTextureMatrixId(material, matrixIdCount);
            WriteMaterial(material, matrixId, writer);
        }

        const std::int32_t nodesOffset = ToIntCount(writer.Position());
        for (std::int32_t i = 0; i < ToIntCount(nodeValues.size()); ++i)
        {
            WriteNode(
                ManagedListAt(nodeValues, i),
                ManagedListAt(nodeMin, i),
                ManagedListAt(nodeMax, i),
                writer);
        }

        const std::int32_t meshesOffset = ToIntCount(writer.Position());
        const auto& meshValues = RequireReference(meshes);
        for (const auto& meshPtr : meshValues)
        {
            const Mesh& mesh = RequireReference(meshPtr);
            writer.Write(static_cast<std::uint16_t>(mesh.MaterialId));
            writer.Write(static_cast<std::uint16_t>(mesh.DlistId));
        }

        writer.Position(0);
        const std::int32_t scaleFactor = ManagedInt32FromFloating(
            std::log2(static_cast<double>(scale)));
        REPACK_MODEL_DEBUG_ASSERT(
            std::pow(2.0, static_cast<double>(scaleFactor))
            == static_cast<double>(scale));
        constexpr std::int32_t scaleBase = 4096;
        constexpr std::int32_t nodeAnimOffset = 0;
        constexpr std::int32_t uvAnimOffset = 0;
        constexpr std::int32_t matAnimOffset = 0;
        constexpr std::int32_t texAnimOffset = 0;

        writer.Write(scaleFactor);
        writer.Write(scaleBase);
        writer.Write(primitiveCount);
        writer.Write(vertexCount);
        writer.Write(materialsOffset);
        writer.Write(dlistsOffset);
        writer.Write(nodesOffset);
        writer.Write(static_cast<std::uint16_t>(nodeMtxIdValues.size()));
        writer.Write(padByte);
        writer.Write(padByte);
        writer.Write(nodeMtxIdOffset);
        writer.Write(meshesOffset);
        writer.Write(static_cast<std::uint16_t>(textureValues.size()));
        writer.Write(padShort);
        writer.Write(texturesOffset);
        writer.Write(static_cast<std::uint16_t>(paletteValues.size()));
        writer.Write(padShort);
        writer.Write(paletteOffset);
        writer.Write(nodePosCountOffset);
        writer.Write(padInt);
        writer.Write(padInt);
        writer.Write(padInt);
        writer.Write(static_cast<std::uint16_t>(materialValues.size()));
        writer.Write(static_cast<std::uint16_t>(nodeValues.size()));
        writer.Write(padInt);
        writer.Write(nodeAnimOffset);
        writer.Write(uvAnimOffset);
        writer.Write(matAnimOffset);
        writer.Write(texAnimOffset);
        writer.Write(static_cast<std::uint16_t>(meshValues.size()));
        writer.Write(static_cast<std::uint16_t>(matrixIdCount));

        REPACK_MODEL_DEBUG_ASSERT(
            writer.Position() == static_cast<std::size_t>(Sizes::Header));

        std::vector<std::uint8_t> modelBytes = writer.ToArray();
        if (optionValues.Texture == RepackTexture::Inline)
        {
            return {std::move(modelBytes), {}};
        }
        return {std::move(modelBytes), separateTexWriter.ToArray()};
    }

    std::shared_ptr<Repack::TextureInfo> Repack::ConvertData(
        const Texture& texture,
        const std::shared_ptr<const std::vector<TextureData>>& data)
    {
        auto imageData = std::make_shared<std::vector<std::uint8_t>>();

        if (texture.Format == TextureFormat::DirectRgb)
        {
            const auto& values = RequireReference(data);
            imageData->reserve(values.size() * 2);
            for (const TextureData& entry : values)
            {
                const std::uint16_t value = static_cast<std::uint16_t>(entry.Data);
                imageData->push_back(static_cast<std::uint8_t>(value & 0xFFU));
                imageData->push_back(static_cast<std::uint8_t>(value >> 8));
            }
        }
        else if (texture.Format == TextureFormat::PaletteA3I5)
        {
            const auto& values = RequireReference(data);
            imageData->reserve(values.size());
            for (const TextureData& entry : values)
            {
                std::uint8_t value = static_cast<std::uint8_t>(entry.Data);
                const std::uint8_t alpha = AlphaBits(entry.Alpha, 7.0F);
                value = static_cast<std::uint8_t>(
                    value | static_cast<std::uint8_t>(alpha << 5));
                imageData->push_back(value);
            }
        }
        else if (texture.Format == TextureFormat::PaletteA5I3)
        {
            const auto& values = RequireReference(data);
            imageData->reserve(values.size());
            for (const TextureData& entry : values)
            {
                std::uint8_t value = static_cast<std::uint8_t>(entry.Data);
                const std::uint8_t alpha = AlphaBits(entry.Alpha, 31.0F);
                value = static_cast<std::uint8_t>(
                    value | static_cast<std::uint8_t>(alpha << 3));
                imageData->push_back(value);
            }
        }
        else if (texture.Format == TextureFormat::Palette2Bit)
        {
            const auto& values = RequireReference(data);
            for (std::size_t i = 0; i < values.size(); i += 4)
            {
                std::uint8_t value = 0;
                for (std::size_t j = 0; j < 4 && i + j < values.size(); ++j)
                {
                    const std::uint32_t index = values[i + j].Data;
                    value = static_cast<std::uint8_t>(
                        value | static_cast<std::uint8_t>(index << (2 * j)));
                }
                imageData->push_back(value);
            }
        }
        else if (texture.Format == TextureFormat::Palette4Bit)
        {
            const auto& values = RequireReference(data);
            for (std::size_t i = 0; i < values.size(); i += 2)
            {
                std::uint8_t value = 0;
                for (std::size_t j = 0; j < 2 && i + j < values.size(); ++j)
                {
                    const std::uint32_t index = values[i + j].Data;
                    value = static_cast<std::uint8_t>(
                        value | static_cast<std::uint8_t>(index << (4 * j)));
                }
                imageData->push_back(value);
            }
        }
        else if (texture.Format == TextureFormat::Palette8Bit)
        {
            const auto& values = RequireReference(data);
            imageData->reserve(values.size());
            for (const TextureData& entry : values)
            {
                imageData->push_back(static_cast<std::uint8_t>(entry.Data));
            }
        }

        return std::make_shared<TextureInfo>(
            texture.Format, texture.Opaque != 0,
            texture.Height, texture.Width,
            std::const_pointer_cast<const std::vector<std::uint8_t>>(imageData));
    }

    std::pair<std::shared_ptr<Repack::TextureInfo>, std::shared_ptr<Repack::PaletteInfo>>
    Repack::ConvertImage(const std::shared_ptr<ImageInfo>& image)
    {
        const ImageInfo& value = RequireReference(image);
        const auto& pixels = RequireReference(value.Pixels);
        bool opaque = true;
        auto imageData = std::make_shared<std::vector<std::uint8_t>>();
        auto paletteData = std::make_shared<std::vector<std::uint16_t>>();

        REPACK_MODEL_DEBUG_ASSERT(
            pixels.size()
            == static_cast<std::size_t>(value.Width) * static_cast<std::size_t>(value.Height));

        const auto rgb5 = [](std::uint8_t component) -> std::uint16_t
        {
            return Color5(component);
        };

        if (value.Format == TextureFormat::DirectRgb)
        {
            imageData->reserve(pixels.size() * 2);
            for (const ColorRgba& pixel : pixels)
            {
                std::uint16_t packed = pixel.Alpha == 0 ? 0 : static_cast<std::uint16_t>(0x8000);
                const std::uint16_t red = rgb5(pixel.Red);
                const std::uint16_t green = rgb5(pixel.Green);
                const std::uint16_t blue = rgb5(pixel.Blue);
                packed = static_cast<std::uint16_t>(packed | red);
                packed = static_cast<std::uint16_t>(packed | static_cast<std::uint16_t>(green << 5));
                packed = static_cast<std::uint16_t>(packed | static_cast<std::uint16_t>(blue << 10));
                imageData->push_back(static_cast<std::uint8_t>(packed & 0xFFU));
                imageData->push_back(static_cast<std::uint8_t>(packed >> 8));
                if (pixel.Alpha != 255)
                {
                    opaque = false;
                }
            }
        }
        else
        {
            struct ColorEntry
            {
                std::uint8_t Red;
                std::uint8_t Green;
                std::uint8_t Blue;
            };
            std::vector<ColorEntry> colors;
            colors.reserve(pixels.size());

            const auto findColor = [&](const ColorRgba& pixel) -> std::int32_t
            {
                for (std::size_t i = 0; i < colors.size(); ++i)
                {
                    if (colors[i].Red == pixel.Red
                        && colors[i].Green == pixel.Green
                        && colors[i].Blue == pixel.Blue)
                    {
                        return ToIntCount(i);
                    }
                }
                return -1;
            };

            for (const ColorRgba& pixel : pixels)
            {
                if (value.PaletteOpaque || pixel.Alpha > 0)
                {
                    if (findColor(pixel) < 0)
                    {
                        colors.push_back(ColorEntry{pixel.Red, pixel.Green, pixel.Blue});
                    }
                }
            }

            const auto requireColor = [&](const ColorRgba& pixel) -> std::uint8_t
            {
                const std::int32_t index = findColor(pixel);
                if (index < 0)
                {
                    throw std::out_of_range("The given key was not present in the dictionary.");
                }
                return static_cast<std::uint8_t>(index);
            };

            if (value.Format == TextureFormat::PaletteA3I5)
            {
                REPACK_MODEL_DEBUG_ASSERT(colors.size() <= 32);
                imageData->reserve(pixels.size());
                for (const ColorRgba& pixel : pixels)
                {
                    std::uint8_t packed = requireColor(pixel);
                    const std::uint8_t alpha = AlphaBits(pixel.Alpha, 7.0F);
                    packed = static_cast<std::uint8_t>(
                        packed | static_cast<std::uint8_t>(alpha << 5));
                    imageData->push_back(packed);
                    if (alpha < 7)
                    {
                        opaque = false;
                    }
                }
            }
            else if (value.Format == TextureFormat::PaletteA5I3)
            {
                REPACK_MODEL_DEBUG_ASSERT(colors.size() <= 8);
                imageData->reserve(pixels.size());
                for (const ColorRgba& pixel : pixels)
                {
                    std::uint8_t packed = requireColor(pixel);
                    const std::uint8_t alpha = AlphaBits(pixel.Alpha, 31.0F);
                    packed = static_cast<std::uint8_t>(
                        packed | static_cast<std::uint8_t>(alpha << 3));
                    imageData->push_back(packed);
                    if (alpha < 31)
                    {
                        opaque = false;
                    }
                }
            }
            else if (value.Format == TextureFormat::Palette2Bit)
            {
                const bool allVisible = std::all_of(
                    pixels.begin(), pixels.end(),
                    [](const ColorRgba& pixel) { return pixel.Alpha > 0; });
                if (value.PaletteOpaque || allVisible)
                {
                    REPACK_MODEL_DEBUG_ASSERT(colors.size() <= 4);
                }
                else
                {
                    REPACK_MODEL_DEBUG_ASSERT(colors.size() <= 3);
                }

                for (std::size_t i = 0; i < pixels.size(); i += 4)
                {
                    std::uint8_t packed = 0;
                    for (std::size_t j = 0; j < 4 && i + j < pixels.size(); ++j)
                    {
                        const ColorRgba& pixel = pixels[i + j];
                        std::uint8_t index = 0;
                        if (value.PaletteOpaque || pixel.Alpha > 0)
                        {
                            index = requireColor(pixel);
                            if (!value.PaletteOpaque)
                            {
                                index = static_cast<std::uint8_t>(index + 1);
                            }
                        }
                        packed = static_cast<std::uint8_t>(
                            packed | static_cast<std::uint8_t>(index << (2 * j)));
                    }
                    imageData->push_back(packed);
                }
                opaque = value.PaletteOpaque;
            }
            else if (value.Format == TextureFormat::Palette4Bit)
            {
                const bool allVisible = std::all_of(
                    pixels.begin(), pixels.end(),
                    [](const ColorRgba& pixel) { return pixel.Alpha > 0; });
                if (value.PaletteOpaque || allVisible)
                {
                    REPACK_MODEL_DEBUG_ASSERT(colors.size() <= 16);
                }
                else
                {
                    REPACK_MODEL_DEBUG_ASSERT(colors.size() <= 15);
                }

                for (std::size_t i = 0; i < pixels.size(); i += 2)
                {
                    std::uint8_t packed = 0;
                    for (std::size_t j = 0; j < 2 && i + j < pixels.size(); ++j)
                    {
                        const ColorRgba& pixel = pixels[i + j];
                        std::uint8_t index = 0;
                        if (value.PaletteOpaque || pixel.Alpha > 0)
                        {
                            index = requireColor(pixel);
                            if (!value.PaletteOpaque)
                            {
                                index = static_cast<std::uint8_t>(index + 1);
                            }
                        }
                        packed = static_cast<std::uint8_t>(
                            packed | static_cast<std::uint8_t>(index << (4 * j)));
                    }
                    imageData->push_back(packed);
                }
                opaque = value.PaletteOpaque;
            }
            else if (value.Format == TextureFormat::Palette8Bit)
            {
                const bool allVisible = std::all_of(
                    pixels.begin(), pixels.end(),
                    [](const ColorRgba& pixel) { return pixel.Alpha > 0; });
                if (value.PaletteOpaque || allVisible)
                {
                    REPACK_MODEL_DEBUG_ASSERT(colors.size() <= 256);
                }
                else
                {
                    REPACK_MODEL_DEBUG_ASSERT(colors.size() <= 255);
                }

                imageData->reserve(pixels.size());
                for (const ColorRgba& pixel : pixels)
                {
                    if (value.PaletteOpaque || pixel.Alpha > 0)
                    {
                        std::uint8_t index = requireColor(pixel);
                        if (!value.PaletteOpaque)
                        {
                            index = static_cast<std::uint8_t>(index + 1);
                        }
                        imageData->push_back(index);
                    }
                    else
                    {
                        imageData->push_back(0);
                    }
                }
                opaque = value.PaletteOpaque;
            }

            paletteData->reserve(colors.size());
            for (const ColorEntry& color : colors)
            {
                std::uint16_t packed = 0;
                const std::uint16_t red = rgb5(color.Red);
                const std::uint16_t green = rgb5(color.Green);
                const std::uint16_t blue = rgb5(color.Blue);
                packed = static_cast<std::uint16_t>(packed | red);
                packed = static_cast<std::uint16_t>(
                    packed | static_cast<std::uint16_t>(green << 5));
                packed = static_cast<std::uint16_t>(
                    packed | static_cast<std::uint16_t>(blue << 10));
                paletteData->push_back(packed);
            }
        }

        auto texture = std::make_shared<TextureInfo>(
            value.Format, opaque, value.Height, value.Width,
            std::const_pointer_cast<const std::vector<std::uint8_t>>(imageData));
        auto palette = std::make_shared<PaletteInfo>(
            std::const_pointer_cast<const std::vector<std::uint16_t>>(paletteData));
        return {std::move(texture), std::move(palette)};
    }

    void Repack::WriteTextureMeta(
        const std::shared_ptr<TextureInfo>& texture, std::int32_t offset, BinaryWriter& writer)
    {
        const TextureInfo& value = RequireReference(texture);
        constexpr std::uint8_t padByte = 0;
        constexpr std::uint16_t padShort = 0;
        constexpr std::uint32_t padInt = 0;
        writer.Write(static_cast<std::uint8_t>(value.Format));
        writer.Write(padByte);
        writer.Write(value.Width);
        writer.Write(value.Height);
        writer.Write(padShort);
        writer.Write(offset);
        writer.Write(ToIntCount(RequireReference(value.Data).size()));
        writer.Write(padInt);
        writer.Write(padInt);
        writer.Write(padInt);
        WriteInt(writer, value.Opaque);
        writer.Write(padInt);
        writer.Write(padByte);
        writer.Write(padByte);
        writer.Write(padShort);
    }

    std::int32_t Repack::WriteRenderInstructions(
        const std::shared_ptr<const std::vector<std::shared_ptr<RenderInstruction>>>& list,
        BinaryWriter& writer)
    {
        const auto& values = RequireReference(list);
        std::int32_t bytesWritten = 0;
        std::vector<std::uint32_t> arguments;
        REPACK_MODEL_DEBUG_ASSERT(values.size() % 4 == 0);

        for (std::size_t i = 0; i < values.size(); i += 4)
        {
            arguments.clear();
            std::uint32_t packedCommands = 0;
            for (std::size_t j = 0; j < 4; ++j)
            {
                const RenderInstruction& inst = RequireReference(values[i + j]);
                const std::uint32_t code
                    = (static_cast<std::uint32_t>(inst.Code) - 0x400U) >> 2;
                packedCommands |= code << (8 * j);
                const auto& instArguments = RequireReference(inst.Arguments);
                arguments.insert(
                    arguments.end(), instArguments.begin(), instArguments.end());
            }
            writer.Write(packedCommands);
            bytesWritten = UncheckedAdd(bytesWritten, 4);
            for (std::uint32_t argument : arguments)
            {
                writer.Write(argument);
                bytesWritten = UncheckedAdd(bytesWritten, 4);
            }
        }
        return bytesWritten;
    }

    std::int32_t Repack::GetTextureMatrixId(
        const std::shared_ptr<Material>& material, std::int32_t& indexCount)
    {
        const Material& value = RequireReference(material);
        const std::int32_t scaleS = Fixed::ToInt(value.ScaleS);
        const std::int32_t scaleT = Fixed::ToInt(value.ScaleT);
        const std::uint16_t rotZ = AngleValue(value.RotateZ);
        const std::int32_t transS = Fixed::ToInt(value.TranslateS);
        const std::int32_t transT = Fixed::ToInt(value.TranslateT);
        if (scaleS == 4096 && scaleT == 4096 && rotZ == 0
            && transS == 0 && transT == 0 && value.TexgenMode == TexgenMode::None)
        {
            return -1;
        }
        const std::int32_t result = indexCount;
        indexCount = UncheckedAdd(indexCount, 1);
        return result;
    }

    void Repack::WriteMaterial(
        const std::shared_ptr<Material>& material, std::int32_t matrixId, BinaryWriter& writer)
    {
        const Material& value = RequireReference(material);
        constexpr std::uint8_t padByte = 0;
        constexpr std::uint16_t padShort = 0;

        WriteString(writer, value.Name, 64);
        writer.Write(value.Lighting);
        writer.Write(static_cast<std::uint8_t>(value.Culling));
        writer.Write(value.Alpha);
        writer.Write(value.Wireframe);
        writer.Write(static_cast<std::int16_t>(value.PaletteId));
        writer.Write(static_cast<std::int16_t>(value.TextureId));
        writer.Write(static_cast<std::uint8_t>(value.XRepeat));
        writer.Write(static_cast<std::uint8_t>(value.YRepeat));
        writer.Write(value.Diffuse.Red);
        writer.Write(value.Diffuse.Green);
        writer.Write(value.Diffuse.Blue);
        writer.Write(value.Ambient.Red);
        writer.Write(value.Ambient.Green);
        writer.Write(value.Ambient.Blue);
        writer.Write(value.Specular.Red);
        writer.Write(value.Specular.Green);
        writer.Write(value.Specular.Blue);
        writer.Write(padByte);
        writer.Write(static_cast<std::uint32_t>(value.PolygonMode));
        writer.Write(static_cast<std::uint8_t>(value.RenderMode));
        writer.Write(static_cast<std::uint8_t>(value.AnimationFlags));
        writer.Write(padShort);
        writer.Write(static_cast<std::uint32_t>(value.TexgenMode));
        writer.Write(padShort);
        writer.Write(padShort);
        writer.Write(matrixId == -1 ? 0 : matrixId);
        WriteFloat(writer, value.ScaleS);
        WriteFloat(writer, value.ScaleT);
        WriteAngle(writer, value.RotateZ);
        writer.Write(padShort);
        WriteFloat(writer, value.TranslateS);
        WriteFloat(writer, value.TranslateT);
        writer.Write(padShort);
        writer.Write(padShort);
        writer.Write(padByte);
        writer.Write(padByte);
        writer.Write(padShort);
    }

    void Repack::WriteNode(
        const std::shared_ptr<Node>& node,
        Vector3i minBounds, Vector3i maxBounds,
        BinaryWriter& writer)
    {
        const Node& value = RequireReference(node);
        constexpr std::uint8_t padByte = 0;
        constexpr std::uint16_t padShort = 0;
        constexpr std::uint32_t padInt = 0;

        WriteString(writer, value.Name, 64);
        writer.Write(static_cast<std::int16_t>(value.ParentIndex));
        writer.Write(static_cast<std::int16_t>(value.ChildIndex));
        writer.Write(static_cast<std::int16_t>(value.NextIndex));
        writer.Write(padShort);
        WriteInt(writer, value.Enabled);
        writer.Write(static_cast<std::int16_t>(value.MeshCount));
        writer.Write(static_cast<std::int16_t>(value.MeshId));
        WriteVector3(writer, value.Scale);
        WriteAngles(writer, value.Angle);
        writer.Write(padShort);
        WriteVector3(writer, value.Position);
        WriteFloat(writer, value.BoundingRadius);
        writer.Write(minBounds.X);
        writer.Write(minBounds.Y);
        writer.Write(minBounds.Z);
        writer.Write(maxBounds.X);
        writer.Write(maxBounds.Y);
        writer.Write(maxBounds.Z);
        writer.Write(static_cast<std::uint8_t>(value.BillboardMode));
        writer.Write(padByte);
        writer.Write(padShort);
        for (std::int32_t i = 0; i < 12; ++i)
        {
            writer.Write(padInt);
        }
        writer.Write(padInt);
        writer.Write(padInt);
        writer.Write(padInt);
        writer.Write(padInt);
        writer.Write(padInt);
        writer.Write(padInt);
        writer.Write(padInt);
        writer.Write(padInt);
        writer.Write(padInt);
        writer.Write(padInt);
        writer.Write(padInt);
        writer.Write(padInt);
    }

    void Repack::Nop()
    {
    }

    std::pair<std::vector<std::uint8_t>, std::vector<std::uint8_t>>
    Repack::RepackRoomModel(
        const std::string& room, bool separateTextures, RepackFilter filter)
    {
        const auto metaIterator = Metadata::RoomMetadata.find(room);
        if (metaIterator == Metadata::RoomMetadata.end())
        {
            throw std::out_of_range("The given key was not present in the dictionary.");
        }
        RoomMetadata& meta = RequireReference(metaIterator->second);
        if (separateTextures && meta.TexturePath.has_value())
        {
            throw ProgramException(
                "Room " + room + " already has a separate texture file.");
        }

        const std::shared_ptr<ModelInstance> instance = Read::GetRoomModelInstance(meta.Name);
        Model& modelValue = RequireReference(RequireReference(instance).Model());

        REPACK_MODEL_DEBUG_ASSERT(
            modelValue.Scale.X == modelValue.Scale.Y
            && modelValue.Scale.Y == modelValue.Scale.Z);
        REPACK_MODEL_DEBUG_ASSERT(
            modelValue.Scale.X == ManagedInt32FromFloating(modelValue.Scale.X));

        const auto& recolors = RequireReference(modelValue.Recolors);
        const auto& recolor = RequireReference(ManagedListAt(recolors, 0));

        auto textureInfo = std::make_shared<std::vector<std::shared_ptr<TextureInfo>>>();
        const auto& textureValues = RequireReference(recolor.Textures);
        textureInfo->reserve(textureValues.size());
        for (std::int32_t i = 0; i < ToIntCount(textureValues.size()); ++i)
        {
            const Texture& texture = ManagedListAt(textureValues, i);
            textureInfo->push_back(ConvertData(
                texture, ManagedListAt(RequireReference(recolor.TextureData), i)));
        }

        auto paletteInfo = std::make_shared<std::vector<std::shared_ptr<PaletteInfo>>>();
        const auto& paletteData = RequireReference(recolor.PaletteData);
        paletteInfo->reserve(paletteData.size());
        for (const auto& data : paletteData)
        {
            paletteInfo->push_back(std::make_shared<PaletteInfo>(PaletteWords(data)));
        }

        if (filter != RepackFilter::All)
        {
            const std::int32_t layerMask = filter == RepackFilter::Multiplayer
                ? SceneSetup::GetNodeLayer(GameMode::Battle, 0, 2)
                : SceneSetup::GetNodeLayer(GameMode::SinglePlayer, meta.NodeLayer, 1);
            modelValue.FilterNodes(layerMask);
        }

        auto options = std::make_shared<RepackOptions>();
        options->IsRoom = true;
        options->Texture = separateTextures
            ? RepackTexture::Separate : RepackTexture::Inline;
        options->ComputeBounds = ComputeBounds::None;

        return PackModel(
            ManagedInt32FromFloating(modelValue.Scale.X),
            modelValue.NodeMatrixIds,
            modelValue.NodePosCounts,
            modelValue.Materials,
            std::const_pointer_cast<const std::vector<std::shared_ptr<TextureInfo>>>(textureInfo),
            std::const_pointer_cast<const std::vector<std::shared_ptr<PaletteInfo>>>(paletteInfo),
            modelValue.Nodes,
            modelValue.Meshes,
            modelValue.RenderInstructionLists,
            modelValue.DisplayLists,
            options);
    }

    void Repack::TestAnimRepack(
        const std::shared_ptr<Model>& model,
        const std::string& animPath,
        bool firstHunt,
        bool writeFile)
    {
        Model& modelValue = RequireReference(model);
        const AnimationGroups& animationGroups = RequireReference(modelValue.AnimationGroups);
        const AnimationOffsets& offsets = RequireReference(animationGroups.Offsets);

        auto node = std::make_shared<std::vector<std::shared_ptr<NodeAnimationGroup>>>();
        auto mat = std::make_shared<std::vector<std::shared_ptr<MaterialAnimationGroup>>>();
        auto uv = std::make_shared<std::vector<std::shared_ptr<TexcoordAnimationGroup>>>();
        auto tex = std::make_shared<std::vector<std::shared_ptr<TextureAnimationGroup>>>();

        std::int32_t index = 0;
        for (std::uint32_t offset : RequireReference(offsets.Node))
        {
            node->push_back(offset == 0
                ? nullptr
                : ManagedListAt(RequireReference(animationGroups.Node), index++));
        }
        index = 0;
        for (std::uint32_t offset : RequireReference(offsets.Material))
        {
            mat->push_back(offset == 0
                ? nullptr
                : ManagedListAt(RequireReference(animationGroups.Material), index++));
        }
        index = 0;
        for (std::uint32_t offset : RequireReference(offsets.Texcoord))
        {
            uv->push_back(offset == 0
                ? nullptr
                : ManagedListAt(RequireReference(animationGroups.Texcoord), index++));
        }
        index = 0;
        for (std::uint32_t offset : RequireReference(offsets.Texture))
        {
            tex->push_back(offset == 0
                ? nullptr
                : ManagedListAt(RequireReference(animationGroups.Texture), index++));
        }

        const bool fhPad = EndsWith(animPath, "testlevel_Anim.bin");
        const std::vector<std::uint8_t> bytes = PackAnim(
            std::const_pointer_cast<const std::vector<std::shared_ptr<NodeAnimationGroup>>>(node),
            std::const_pointer_cast<const std::vector<std::shared_ptr<MaterialAnimationGroup>>>(mat),
            std::const_pointer_cast<const std::vector<std::shared_ptr<TexcoordAnimationGroup>>>(uv),
            std::const_pointer_cast<const std::vector<std::shared_ptr<TextureAnimationGroup>>>(tex),
            fhPad);

        const std::vector<std::uint8_t> fileBytes = FileReadAllBytes(
            Paths::Combine(firstHunt ? Paths::FhFileSystem() : Paths::FileSystem(), animPath));
        CompareAnims(modelValue.Name, bytes, fileBytes);

        if (writeFile)
        {
            FileWriteAllBytes(
                Paths::Combine(
                    Paths::Export(), "_pack",
                    "out_" + modelValue.Name + "_Anim.bin"),
                bytes);
        }
        Nop();
    }

    void Repack::TestModelRepack(
        const std::shared_ptr<Model>& model,
        std::int32_t recolor,
        const std::string& modelPath,
        const std::string* texPath,
        bool firstHunt,
        const std::shared_ptr<RepackOptions>& options)
    {
        RepackOptions& optionValues = RequireReference(options);
        if (optionValues.Texture == RepackTexture::Shared)
        {
            return;
        }
        Model& modelValue = RequireReference(model);

        const auto& recolors = RequireReference(modelValue.Recolors);
        const auto& recolorValue = RequireReference(ManagedListAt(recolors, recolor));

        auto textureInfo = std::make_shared<std::vector<std::shared_ptr<TextureInfo>>>();
        const auto& textureValues = RequireReference(recolorValue.Textures);
        textureInfo->reserve(textureValues.size());
        for (std::int32_t i = 0; i < ToIntCount(textureValues.size()); ++i)
        {
            const Texture& texture = ManagedListAt(textureValues, i);
            textureInfo->push_back(ConvertData(
                texture, ManagedListAt(RequireReference(recolorValue.TextureData), i)));
        }

        auto paletteInfo = std::make_shared<std::vector<std::shared_ptr<PaletteInfo>>>();
        const auto& paletteData = RequireReference(recolorValue.PaletteData);
        paletteInfo->reserve(paletteData.size());
        for (const auto& data : paletteData)
        {
            paletteInfo->push_back(std::make_shared<PaletteInfo>(PaletteWords(data)));
        }

        REPACK_MODEL_DEBUG_ASSERT(
            modelValue.Scale.X == modelValue.Scale.Y
            && modelValue.Scale.Y == modelValue.Scale.Z);
        REPACK_MODEL_DEBUG_ASSERT(
            modelValue.Scale.X == ManagedInt32FromFloating(modelValue.Scale.X));

        auto packed = PackModel(
            ManagedInt32FromFloating(modelValue.Scale.X),
            modelValue.NodeMatrixIds,
            modelValue.NodePosCounts,
            modelValue.Materials,
            std::const_pointer_cast<const std::vector<std::shared_ptr<TextureInfo>>>(textureInfo),
            std::const_pointer_cast<const std::vector<std::shared_ptr<PaletteInfo>>>(paletteInfo),
            modelValue.Nodes,
            modelValue.Meshes,
            modelValue.RenderInstructionLists,
            modelValue.DisplayLists,
            options);

        const std::vector<std::uint8_t> fileBytes = FileReadAllBytes(
            Paths::Combine(firstHunt ? Paths::FhFileSystem() : Paths::FileSystem(), modelPath));

        if (optionValues.Compare)
        {
            CompareModels(modelValue.Name, packed.first, fileBytes, options);
            if (optionValues.Texture == RepackTexture::Separate)
            {
                REPACK_MODEL_DEBUG_ASSERT(texPath != nullptr);
                if (texPath == nullptr)
                {
                    throw System::NullReferenceException();
                }
                const std::vector<std::uint8_t> texFile = FileReadAllBytes(
                    Paths::Combine(
                        firstHunt ? Paths::FhFileSystem() : Paths::FileSystem(), *texPath));
                REPACK_MODEL_DEBUG_ASSERT(packed.second.size() == texFile.size());
                REPACK_MODEL_DEBUG_ASSERT(packed.second == texFile);
            }
        }

        if (optionValues.WriteFile)
        {
            FileWriteAllBytes(
                Paths::Combine(
                    Paths::Export(), "_pack",
                    "out_" + modelValue.Name + "_" + recolorValue.Name + ".bin"),
                packed.first);
            if (optionValues.Texture == RepackTexture::Separate)
            {
                FileWriteAllBytes(
                    Paths::Combine(
                        Paths::Export(), "_pack",
                        "out_" + modelValue.Name + "_Tex.bin"),
                    packed.second);
            }
        }
        Nop();
    }

    void Repack::TestRepack()
    {
        Read::ApplyFixes = false;

        const auto testModels = [](const auto& models)
        {
            for (const auto& pair : models)
            {
                const ModelMetadata& meta = pair.second;
                const std::shared_ptr<ModelInstance> instance
                    = Read::GetModelInstance(meta.Name, meta.FirstHunt);
                const std::shared_ptr<Model> model = RequireReference(instance).Model();

                std::int32_t i = 0;
                for (const RecolorMetadata& recolor : meta.Recolors)
                {
                    if ((meta.Name == "samus_hi_yellow"
                            || meta.Name == "samus_low_yellow"
                            || meta.Name == "morphBall")
                        && i != 0)
                    {
                        break;
                    }

                    std::string modelPath = meta.ModelPath;
                    if (meta.Name == "arcWelder1")
                    {
                        modelPath = StringReplace(
                            modelPath, "arcWelder1",
                            "arcWelder" + std::to_string(i + 1));
                    }

                    auto options = std::make_shared<RepackOptions>();
                    options->IsRoom = false;
                    options->Texture = RepackTexture::Inline;
                    options->ComputeBounds = ComputeBounds::None;

                    if (meta.ModelPath != recolor.TexturePath
                        || meta.ModelPath != recolor.PalettePath)
                    {
                        options->Texture
                            = ToLowerAscii(recolor.TexturePath).find("share")
                                    != std::string::npos
                                || meta.ModelPath != recolor.PalettePath
                            ? RepackTexture::Shared
                            : RepackTexture::Separate;
                    }

                    const std::int32_t recolorIndex = i++;
                    TestModelRepack(
                        model, recolorIndex, modelPath,
                        std::addressof(recolor.TexturePath),
                        meta.FirstHunt, options);
                }

                if (meta.AnimationPath.has_value()
                    && !meta.AnimationShare.has_value())
                {
                    TestAnimRepack(
                        model, *meta.AnimationPath, meta.FirstHunt, false);
                }
            }
        };

        testModels(Metadata::ModelMetadata);
        testModels(Metadata::FirstHuntModels);

        for (const auto& pair : Metadata::RoomMetadata)
        {
            RoomMetadata& meta = RequireReference(pair.second);
            auto options = std::make_shared<RepackOptions>();
            options->IsRoom = true;
            options->Texture = !meta.TexturePath.has_value()
                    || meta.ModelPath == *meta.TexturePath
                ? RepackTexture::Inline
                : RepackTexture::Separate;
            options->ComputeBounds = ComputeBounds::None;

            const std::shared_ptr<Model> model
                = RequireReference(Read::GetRoomModelInstance(meta.Name)).Model();
            const std::string* texturePath = meta.TexturePath
                ? std::addressof(*meta.TexturePath) : nullptr;
            TestModelRepack(
                model, 0, meta.ModelPath, texturePath,
                meta.FirstHunt || meta.Hybrid, options);

            if (!meta.AnimationPath.empty())
            {
                TestAnimRepack(
                    model, meta.AnimationPath,
                    meta.FirstHunt || meta.Hybrid, options->WriteFile);
            }
        }

        Read::ApplyFixes = true;
    }

    void Repack::TestRepack(
        const std::string& name, std::int32_t recolor, bool firstHunt)
    {
        Read::ApplyFixes = false;
        auto options = std::make_shared<RepackOptions>();
        options->IsRoom = false;
        options->WriteFile = true;
        options->ComputeBounds = ComputeBounds::None;

        const auto& metadata = firstHunt
            ? Metadata::FirstHuntModels : Metadata::ModelMetadata;
        const auto iterator = metadata.find(name);
        if (iterator == metadata.end())
        {
            throw std::out_of_range("The given key was not present in the dictionary.");
        }
        const ModelMetadata& meta = iterator->second;
        const std::shared_ptr<Model> model
            = RequireReference(Read::GetModelInstance(meta.Name, meta.FirstHunt)).Model();

        const RecolorMetadata& recolorMeta = ManagedListAt(meta.Recolors, recolor);
        TestModelRepack(
            model, recolor, meta.ModelPath,
            std::addressof(recolorMeta.TexturePath),
            meta.FirstHunt, options);

        if (meta.AnimationPath.has_value() && !meta.AnimationShare.has_value())
        {
            TestAnimRepack(
                model, *meta.AnimationPath, meta.FirstHunt, options->WriteFile);
        }
        Read::ApplyFixes = true;
    }

    void Repack::CompareModels(
        const std::string& model1, const std::string& model2,
        const std::string& game1, const std::string& game2)
    {
        const auto first = Metadata::ModelMetadata.find(model1);
        const auto second = Metadata::ModelMetadata.find(model2);
        if (first == Metadata::ModelMetadata.end()
            || second == Metadata::ModelMetadata.end())
        {
            throw std::out_of_range("The given key was not present in the dictionary.");
        }

        const std::string parent
            = ::MphRead::NativeRuntime::PathToUtf8(::MphRead::NativeRuntime::PathFromUtf8(Paths::FileSystem()).parent_path());
        const std::string path1
            = Paths::Combine(parent, game1, first->second.ModelPath);
        const std::string path2
            = Paths::Combine(parent, game2, second->second.ModelPath);

        auto options = std::make_shared<RepackOptions>();
        options->Texture = RepackTexture::Separate;
        CompareModels(
            model1, FileReadAllBytes(path1), FileReadAllBytes(path2), options);
        Nop();
    }

    void Repack::CompareAnims(
        const std::string& model1, const std::string& model2,
        const std::string& game1, const std::string& game2)
    {
        const auto first = Metadata::ModelMetadata.find(model1);
        const auto second = Metadata::ModelMetadata.find(model2);
        if (first == Metadata::ModelMetadata.end()
            || second == Metadata::ModelMetadata.end())
        {
            throw std::out_of_range("The given key was not present in the dictionary.");
        }

        const ModelMetadata& meta1 = first->second;
        const ModelMetadata& meta2 = second->second;
        if (meta1.AnimationPath.has_value() && meta2.AnimationPath.has_value())
        {
            const std::string parent
                = ::MphRead::NativeRuntime::PathToUtf8(::MphRead::NativeRuntime::PathFromUtf8(Paths::FileSystem()).parent_path());
            const std::string path1
                = Paths::Combine(parent, game1, *meta1.AnimationPath);
            const std::string path2
                = Paths::Combine(parent, game2, *meta2.AnimationPath);
            CompareAnims(
                model1, FileReadAllBytes(path1), FileReadAllBytes(path2));
        }
        Nop();
    }

    void Repack::CompareModels(
        const std::string& name,
        const std::vector<std::uint8_t>& bytes,
        const std::vector<std::uint8_t>& otherBytes,
        const std::shared_ptr<RepackOptions>& options)
    {
        const std::string temp = name;
        (void)temp;
        const RepackOptions& optionValues = RequireReference(options);
        REPACK_MODEL_DEBUG_ASSERT(bytes.size() == otherBytes.size());

        const std::span<const std::uint8_t> first(bytes.data(), bytes.size());
        const std::span<const std::uint8_t> second(otherBytes.data(), otherBytes.size());

        const Header header = Read::ReadStruct<Header>(first);
        const Header other = Read::ReadStruct<Header>(second);

#if defined(DEBUG)
        REPACK_MODEL_DEBUG_ASSERT(header.ScaleFactor == other.ScaleFactor);
        REPACK_MODEL_DEBUG_ASSERT(header.ScaleBase.Value == other.ScaleBase.Value);
        REPACK_MODEL_DEBUG_ASSERT(header.PrimitiveCount == other.PrimitiveCount);
        REPACK_MODEL_DEBUG_ASSERT(header.VertexCount == other.VertexCount);
        REPACK_MODEL_DEBUG_ASSERT(header.MaterialOffset == other.MaterialOffset);
        REPACK_MODEL_DEBUG_ASSERT(header.DlistOffset == other.DlistOffset);
        REPACK_MODEL_DEBUG_ASSERT(header.NodeOffset == other.NodeOffset);
        REPACK_MODEL_DEBUG_ASSERT(header.NodeWeightCount == other.NodeWeightCount);
        REPACK_MODEL_DEBUG_ASSERT(header.NodeWeightOffset == other.NodeWeightOffset);
        REPACK_MODEL_DEBUG_ASSERT(header.MeshOffset == other.MeshOffset);
        REPACK_MODEL_DEBUG_ASSERT(header.TextureCount == other.TextureCount);
        REPACK_MODEL_DEBUG_ASSERT(header.TextureOffset == other.TextureOffset);
        REPACK_MODEL_DEBUG_ASSERT(header.PaletteCount == other.PaletteCount);
        REPACK_MODEL_DEBUG_ASSERT(header.PaletteOffset == other.PaletteOffset);
        REPACK_MODEL_DEBUG_ASSERT(header.NodePosCounts == other.NodePosCounts);
        REPACK_MODEL_DEBUG_ASSERT(header.NodePosScales == other.NodePosScales);
        REPACK_MODEL_DEBUG_ASSERT(header.NodeInitialPosition == other.NodeInitialPosition);
        REPACK_MODEL_DEBUG_ASSERT(header.NodePosition == other.NodePosition);
        REPACK_MODEL_DEBUG_ASSERT(header.MaterialCount == other.MaterialCount);
        REPACK_MODEL_DEBUG_ASSERT(header.NodeCount == other.NodeCount);
        REPACK_MODEL_DEBUG_ASSERT(header.NodeAnimationOffset == other.NodeAnimationOffset);
        REPACK_MODEL_DEBUG_ASSERT(
            header.TextureCoordinateAnimations == other.TextureCoordinateAnimations);
        REPACK_MODEL_DEBUG_ASSERT(header.MaterialAnimations == other.MaterialAnimations);
        REPACK_MODEL_DEBUG_ASSERT(header.TextureAnimations == other.TextureAnimations);
        REPACK_MODEL_DEBUG_ASSERT(header.MeshCount == other.MeshCount);
        REPACK_MODEL_DEBUG_ASSERT(header.TextureMatrixCount == other.TextureMatrixCount);
#endif

        const auto texes = Read::DoOffsets<Texture>(
            first, header.TextureOffset, static_cast<std::int32_t>(header.TextureCount));
        const auto otherTexes = Read::DoOffsets<Texture>(
            second, other.TextureOffset, static_cast<std::int32_t>(other.TextureCount));
        for (std::int32_t i = 0; i < ToIntCount(RequireReference(texes).size()); ++i)
        {
            const Texture& tex = ManagedListAt(RequireReference(texes), i);
            const Texture& otherTex = ManagedListAt(RequireReference(otherTexes), i);
#if defined(DEBUG)
            REPACK_MODEL_DEBUG_ASSERT(tex.Format == otherTex.Format);
            REPACK_MODEL_DEBUG_ASSERT(tex.Width == otherTex.Width);
            REPACK_MODEL_DEBUG_ASSERT(tex.Height == otherTex.Height);
            REPACK_MODEL_DEBUG_ASSERT(tex.ImageOffset == otherTex.ImageOffset);
            REPACK_MODEL_DEBUG_ASSERT(tex.ImageSize == otherTex.ImageSize);
            REPACK_MODEL_DEBUG_ASSERT(tex.Opaque == otherTex.Opaque);
#endif
            if (optionValues.Texture == RepackTexture::Inline)
            {
                const auto texData = Read::DoOffsets<std::uint8_t>(
                    first, tex.ImageOffset, tex.ImageSize);
                const auto otherTexData = Read::DoOffsets<std::uint8_t>(
                    second, otherTex.ImageOffset, otherTex.ImageSize);
                REPACK_MODEL_DEBUG_ASSERT(
                    RequireReference(texData) == RequireReference(otherTexData));
            }
        }

        const auto pals = Read::DoOffsets<Palette>(
            first, header.PaletteOffset, static_cast<std::int32_t>(header.PaletteCount));
        const auto otherPals = Read::DoOffsets<Palette>(
            second, other.PaletteOffset, static_cast<std::int32_t>(other.PaletteCount));
        for (std::int32_t i = 0; i < ToIntCount(RequireReference(pals).size()); ++i)
        {
            const Palette& pal = ManagedListAt(RequireReference(pals), i);
            const Palette& otherPal = ManagedListAt(RequireReference(otherPals), i);
#if defined(DEBUG)
            REPACK_MODEL_DEBUG_ASSERT(pal.Offset == otherPal.Offset);
            REPACK_MODEL_DEBUG_ASSERT(pal.Size == otherPal.Size);
#endif
            if (optionValues.Texture == RepackTexture::Inline)
            {
                const auto palData = Read::DoOffsets<std::uint8_t>(
                    first, pal.Offset, pal.Size);
                const auto otherPalData = Read::DoOffsets<std::uint8_t>(
                    second, otherPal.Offset, otherPal.Size);
                REPACK_MODEL_DEBUG_ASSERT(
                    RequireReference(palData) == RequireReference(otherPalData));
            }
        }

        const auto mats = Read::DoOffsets<RawMaterial>(
            first, header.MaterialOffset, static_cast<std::int32_t>(header.MaterialCount));
        const auto otherMats = Read::DoOffsets<RawMaterial>(
            second, other.MaterialOffset, static_cast<std::int32_t>(other.MaterialCount));
        for (std::int32_t i = 0; i < ToIntCount(RequireReference(mats).size()); ++i)
        {
            const RawMaterial& mat = ManagedListAt(RequireReference(mats), i);
            const RawMaterial& otherMat = ManagedListAt(RequireReference(otherMats), i);
            REPACK_MODEL_DEBUG_ASSERT(mat.Name.WireBytes() == otherMat.Name.WireBytes());
            REPACK_MODEL_DEBUG_ASSERT(mat.Alpha == otherMat.Alpha);
            REPACK_MODEL_DEBUG_ASSERT(mat.Diffuse.Red == otherMat.Diffuse.Red);
            REPACK_MODEL_DEBUG_ASSERT(mat.Diffuse.Green == otherMat.Diffuse.Green);
            REPACK_MODEL_DEBUG_ASSERT(mat.Diffuse.Blue == otherMat.Diffuse.Blue);
            REPACK_MODEL_DEBUG_ASSERT(mat.Ambient.Red == otherMat.Ambient.Red);
            REPACK_MODEL_DEBUG_ASSERT(mat.Ambient.Green == otherMat.Ambient.Green);
            REPACK_MODEL_DEBUG_ASSERT(mat.Ambient.Blue == otherMat.Ambient.Blue);
            REPACK_MODEL_DEBUG_ASSERT(mat.Specular.Red == otherMat.Specular.Red);
            REPACK_MODEL_DEBUG_ASSERT(mat.Specular.Green == otherMat.Specular.Green);
            REPACK_MODEL_DEBUG_ASSERT(mat.Specular.Blue == otherMat.Specular.Blue);
            REPACK_MODEL_DEBUG_ASSERT(mat.AnimationFlags == otherMat.AnimationFlags);
            REPACK_MODEL_DEBUG_ASSERT(mat.Culling == otherMat.Culling);
            REPACK_MODEL_DEBUG_ASSERT(mat.Lighting == otherMat.Lighting);
            REPACK_MODEL_DEBUG_ASSERT(mat.MatrixId == otherMat.MatrixId);
            REPACK_MODEL_DEBUG_ASSERT(mat.PaletteId == otherMat.PaletteId);
            REPACK_MODEL_DEBUG_ASSERT(mat.PolygonMode == otherMat.PolygonMode);
            REPACK_MODEL_DEBUG_ASSERT(mat.RenderMode == otherMat.RenderMode);
            REPACK_MODEL_DEBUG_ASSERT(mat.RotateZ == otherMat.RotateZ);
            REPACK_MODEL_DEBUG_ASSERT(mat.ScaleS.Value == otherMat.ScaleS.Value);
            REPACK_MODEL_DEBUG_ASSERT(mat.ScaleT.Value == otherMat.ScaleT.Value);
            REPACK_MODEL_DEBUG_ASSERT(mat.TranslateS.Value == otherMat.TranslateS.Value);
            REPACK_MODEL_DEBUG_ASSERT(mat.TranslateT.Value == otherMat.TranslateT.Value);
            REPACK_MODEL_DEBUG_ASSERT(mat.TexcoordTransformMode == otherMat.TexcoordTransformMode);
            REPACK_MODEL_DEBUG_ASSERT(mat.TextureId == otherMat.TextureId);
            REPACK_MODEL_DEBUG_ASSERT(mat.Wireframe == otherMat.Wireframe);
            REPACK_MODEL_DEBUG_ASSERT(mat.XRepeat == otherMat.XRepeat);
            REPACK_MODEL_DEBUG_ASSERT(mat.YRepeat == otherMat.YRepeat);
        }

        const auto rawNodes = Read::DoOffsets<RawNode>(
            first, header.NodeOffset, static_cast<std::int32_t>(header.NodeCount));
        const auto otherRawNodes = Read::DoOffsets<RawNode>(
            second, other.NodeOffset, static_cast<std::int32_t>(other.NodeCount));
        for (std::int32_t i = 0; i < ToIntCount(RequireReference(rawNodes).size()); ++i)
        {
            const RawNode& node = ManagedListAt(RequireReference(rawNodes), i);
            const RawNode& otherNode = ManagedListAt(RequireReference(otherRawNodes), i);
            REPACK_MODEL_DEBUG_ASSERT(node.Name.WireBytes() == otherNode.Name.WireBytes());
            REPACK_MODEL_DEBUG_ASSERT(node.AngleX == otherNode.AngleX);
            REPACK_MODEL_DEBUG_ASSERT(node.AngleY == otherNode.AngleY);
            REPACK_MODEL_DEBUG_ASSERT(node.AngleZ == otherNode.AngleZ);
            REPACK_MODEL_DEBUG_ASSERT(node.BillboardMode == otherNode.BillboardMode);
            REPACK_MODEL_DEBUG_ASSERT(node.BoundingRadius.Value == otherNode.BoundingRadius.Value);
            REPACK_MODEL_DEBUG_ASSERT(node.ChildId == otherNode.ChildId);
            REPACK_MODEL_DEBUG_ASSERT(node.NextId == otherNode.NextId);
            REPACK_MODEL_DEBUG_ASSERT(node.ParentId == otherNode.ParentId);
            REPACK_MODEL_DEBUG_ASSERT(node.Enabled == otherNode.Enabled);
            REPACK_MODEL_DEBUG_ASSERT(node.MinBounds.X.Value == otherNode.MinBounds.X.Value);
            REPACK_MODEL_DEBUG_ASSERT(node.MinBounds.Y.Value == otherNode.MinBounds.Y.Value);
            REPACK_MODEL_DEBUG_ASSERT(node.MinBounds.Z.Value == otherNode.MinBounds.Z.Value);
            REPACK_MODEL_DEBUG_ASSERT(node.MaxBounds.X.Value == otherNode.MaxBounds.X.Value);
            REPACK_MODEL_DEBUG_ASSERT(node.MaxBounds.Y.Value == otherNode.MaxBounds.Y.Value);
            REPACK_MODEL_DEBUG_ASSERT(node.MaxBounds.Z.Value == otherNode.MaxBounds.Z.Value);
            REPACK_MODEL_DEBUG_ASSERT(node.MeshCount == otherNode.MeshCount);
            REPACK_MODEL_DEBUG_ASSERT(node.MeshId == otherNode.MeshId);
            REPACK_MODEL_DEBUG_ASSERT(node.Position.X.Value == otherNode.Position.X.Value);
            REPACK_MODEL_DEBUG_ASSERT(node.Position.Y.Value == otherNode.Position.Y.Value);
            REPACK_MODEL_DEBUG_ASSERT(node.Position.Z.Value == otherNode.Position.Z.Value);
            REPACK_MODEL_DEBUG_ASSERT(node.Scale.X.Value == otherNode.Scale.X.Value);
            REPACK_MODEL_DEBUG_ASSERT(node.Scale.Y.Value == otherNode.Scale.Y.Value);
            REPACK_MODEL_DEBUG_ASSERT(node.Scale.Z.Value == otherNode.Scale.Z.Value);
        }

        const auto rawMeshes = Read::DoOffsets<RawMesh>(
            first, header.MeshOffset, static_cast<std::int32_t>(header.MeshCount));
        const auto otherRawMeshes = Read::DoOffsets<RawMesh>(
            second, other.MeshOffset, static_cast<std::int32_t>(other.MeshCount));
        for (std::int32_t i = 0; i < ToIntCount(RequireReference(rawMeshes).size()); ++i)
        {
            const RawMesh& mesh = ManagedListAt(RequireReference(rawMeshes), i);
            const RawMesh& otherMesh = ManagedListAt(RequireReference(otherRawMeshes), i);
            REPACK_MODEL_DEBUG_ASSERT(mesh.MaterialId == otherMesh.MaterialId);
            REPACK_MODEL_DEBUG_ASSERT(mesh.DlistId == otherMesh.DlistId);
        }

        const auto dlists = Read::DoOffsets<DisplayList>(
            first, header.DlistOffset, static_cast<std::int32_t>(header.MeshCount));
        const auto otherDlists = Read::DoOffsets<DisplayList>(
            second, other.DlistOffset, static_cast<std::int32_t>(other.MeshCount));
        for (std::int32_t i = 0; i < ToIntCount(RequireReference(dlists).size()); ++i)
        {
            const DisplayList& dlist = ManagedListAt(RequireReference(dlists), i);
            const DisplayList& otherDlist = ManagedListAt(RequireReference(otherDlists), i);
            REPACK_MODEL_DEBUG_ASSERT(dlist.Offset == otherDlist.Offset);
            REPACK_MODEL_DEBUG_ASSERT(dlist.Size == otherDlist.Size);
            REPACK_MODEL_DEBUG_ASSERT(dlist.MinBounds.X.Value == otherDlist.MinBounds.X.Value);
            REPACK_MODEL_DEBUG_ASSERT(dlist.MinBounds.Y.Value == otherDlist.MinBounds.Y.Value);
            REPACK_MODEL_DEBUG_ASSERT(dlist.MinBounds.Z.Value == otherDlist.MinBounds.Z.Value);
            REPACK_MODEL_DEBUG_ASSERT(dlist.MaxBounds.X.Value == otherDlist.MaxBounds.X.Value);
            REPACK_MODEL_DEBUG_ASSERT(dlist.MaxBounds.Y.Value == otherDlist.MaxBounds.Y.Value);
            REPACK_MODEL_DEBUG_ASSERT(dlist.MaxBounds.Z.Value == otherDlist.MaxBounds.Z.Value);
            const auto dlistData = Read::DoOffsets<std::uint8_t>(
                first, dlist.Offset, dlist.Size);
            const auto otherDlistData = Read::DoOffsets<std::uint8_t>(
                second, otherDlist.Offset, otherDlist.Size);
            REPACK_MODEL_DEBUG_ASSERT(
                RequireReference(dlistData) == RequireReference(otherDlistData));
        }

        REPACK_MODEL_DEBUG_ASSERT(bytes == otherBytes);
        Nop();
    }

    void Repack::CompareAnims(
        const std::string& name,
        const std::vector<std::uint8_t>& bytes,
        const std::vector<std::uint8_t>& otherBytes)
    {
        const std::string temp = name;
        (void)temp;
        REPACK_MODEL_DEBUG_ASSERT(bytes.size() == otherBytes.size());

        const std::span<const std::uint8_t> first(bytes.data(), bytes.size());
        const std::span<const std::uint8_t> second(otherBytes.data(), otherBytes.size());

        const AnimationHeader header = Read::ReadStruct<AnimationHeader>(first);
        const AnimationHeader other = Read::ReadStruct<AnimationHeader>(second);
#if defined(DEBUG)
        REPACK_MODEL_DEBUG_ASSERT(header.Count == other.Count);
        REPACK_MODEL_DEBUG_ASSERT(header.NodeGroupOffset == other.NodeGroupOffset);
        REPACK_MODEL_DEBUG_ASSERT(header.UnusedGroupOffset == other.UnusedGroupOffset);
        REPACK_MODEL_DEBUG_ASSERT(header.MaterialGroupOffset == other.MaterialGroupOffset);
        REPACK_MODEL_DEBUG_ASSERT(header.TexcoordGroupOffset == other.TexcoordGroupOffset);
        REPACK_MODEL_DEBUG_ASSERT(header.TextureGroupOffset == other.TextureGroupOffset);
#endif

        const auto nodes = Read::DoOffsets<std::uint32_t>(
            first, header.NodeGroupOffset, static_cast<std::int32_t>(header.Count));
        const auto otherNodes = Read::DoOffsets<std::uint32_t>(
            second, other.NodeGroupOffset, static_cast<std::int32_t>(other.Count));
        const auto mats = Read::DoOffsets<std::uint32_t>(
            first, header.MaterialGroupOffset, static_cast<std::int32_t>(header.Count));
        const auto otherMats = Read::DoOffsets<std::uint32_t>(
            second, other.MaterialGroupOffset, static_cast<std::int32_t>(other.Count));
        const auto uvs = Read::DoOffsets<std::uint32_t>(
            first, header.TexcoordGroupOffset, static_cast<std::int32_t>(header.Count));
        const auto otherUvs = Read::DoOffsets<std::uint32_t>(
            second, other.TexcoordGroupOffset, static_cast<std::int32_t>(other.Count));
        const auto texes = Read::DoOffsets<std::uint32_t>(
            first, header.TextureGroupOffset, static_cast<std::int32_t>(header.Count));
        const auto otherTexes = Read::DoOffsets<std::uint32_t>(
            second, other.TextureGroupOffset, static_cast<std::int32_t>(other.Count));

        for (std::int32_t i = 0; i < static_cast<std::int32_t>(header.Count); ++i)
        {
            const std::uint32_t node = ManagedListAt(RequireReference(nodes), i);
            const std::uint32_t otherNode = ManagedListAt(RequireReference(otherNodes), i);
            REPACK_MODEL_DEBUG_ASSERT(node == otherNode);
            if (node != 0)
            {
                const RawNodeAnimationGroup group
                    = Read::DoOffset<RawNodeAnimationGroup>(first, node);
                const RawNodeAnimationGroup otherGroup
                    = Read::DoOffset<RawNodeAnimationGroup>(second, otherNode);
                REPACK_MODEL_DEBUG_ASSERT(group.FrameCount == otherGroup.FrameCount);
                REPACK_MODEL_DEBUG_ASSERT(group.ScaleLutOffset == otherGroup.ScaleLutOffset);
                REPACK_MODEL_DEBUG_ASSERT(group.RotateLutOffset == otherGroup.RotateLutOffset);
                REPACK_MODEL_DEBUG_ASSERT(group.TranslateLutOffset == otherGroup.TranslateLutOffset);
                REPACK_MODEL_DEBUG_ASSERT(group.AnimationOffset == otherGroup.AnimationOffset);
                const std::int32_t scaleCount
                    = static_cast<std::int32_t>(
                        group.RotateLutOffset - group.ScaleLutOffset) / 4;
                const std::int32_t rotCount
                    = static_cast<std::int32_t>(
                        group.TranslateLutOffset - group.RotateLutOffset) / 2;
                const std::int32_t transCount
                    = static_cast<std::int32_t>(
                        group.AnimationOffset - group.TranslateLutOffset) / 4;
                const auto scales = Read::DoOffsets<std::uint32_t>(
                    first, group.ScaleLutOffset, scaleCount);
                const auto otherScales = Read::DoOffsets<std::uint32_t>(
                    second, otherGroup.ScaleLutOffset, scaleCount);
                REPACK_MODEL_DEBUG_ASSERT(RequireReference(scales) == RequireReference(otherScales));
                const auto rots = Read::DoOffsets<std::uint16_t>(
                    first, group.RotateLutOffset, rotCount);
                const auto otherRots = Read::DoOffsets<std::uint16_t>(
                    second, otherGroup.RotateLutOffset, rotCount);
                REPACK_MODEL_DEBUG_ASSERT(RequireReference(rots) == RequireReference(otherRots));
                const auto poses = Read::DoOffsets<std::uint32_t>(
                    first, group.TranslateLutOffset, transCount);
                const auto otherPoses = Read::DoOffsets<std::uint32_t>(
                    second, otherGroup.TranslateLutOffset, transCount);
                REPACK_MODEL_DEBUG_ASSERT(RequireReference(poses) == RequireReference(otherPoses));
                if (group.AnimationOffset != node)
                {
                    REPACK_MODEL_DEBUG_ASSERT(node > group.AnimationOffset);
                    REPACK_MODEL_DEBUG_ASSERT(
                        (node - group.AnimationOffset)
                            % static_cast<std::uint32_t>(Sizes::NodeAnimation) == 0);
                    const std::int32_t animCount
                        = static_cast<std::int32_t>(node - group.AnimationOffset)
                        / Sizes::NodeAnimation;
                    const auto anims = Read::DoOffsets<NodeAnimation>(
                        first, group.AnimationOffset, animCount);
                    const auto otherAnims = Read::DoOffsets<NodeAnimation>(
                        second, otherGroup.AnimationOffset, animCount);
                    for (std::int32_t j = 0; j < ToIntCount(RequireReference(anims).size()); ++j)
                    {
                        const NodeAnimation& anim = ManagedListAt(RequireReference(anims), j);
                        const NodeAnimation& otherAnim = ManagedListAt(RequireReference(otherAnims), j);
                        REPACK_MODEL_DEBUG_ASSERT(anim.ScaleBlendX == otherAnim.ScaleBlendX);
                        REPACK_MODEL_DEBUG_ASSERT(anim.ScaleBlendY == otherAnim.ScaleBlendY);
                        REPACK_MODEL_DEBUG_ASSERT(anim.ScaleBlendZ == otherAnim.ScaleBlendZ);
                        REPACK_MODEL_DEBUG_ASSERT(anim.Flags == otherAnim.Flags);
                        REPACK_MODEL_DEBUG_ASSERT(anim.ScaleLutLengthX == otherAnim.ScaleLutLengthX);
                        REPACK_MODEL_DEBUG_ASSERT(anim.ScaleLutLengthY == otherAnim.ScaleLutLengthY);
                        REPACK_MODEL_DEBUG_ASSERT(anim.ScaleLutLengthZ == otherAnim.ScaleLutLengthZ);
                        REPACK_MODEL_DEBUG_ASSERT(anim.ScaleLutIndexX == otherAnim.ScaleLutIndexX);
                        REPACK_MODEL_DEBUG_ASSERT(anim.ScaleLutIndexY == otherAnim.ScaleLutIndexY);
                        REPACK_MODEL_DEBUG_ASSERT(anim.ScaleLutIndexZ == otherAnim.ScaleLutIndexZ);
                        REPACK_MODEL_DEBUG_ASSERT(anim.RotateBlendX == otherAnim.RotateBlendX);
                        REPACK_MODEL_DEBUG_ASSERT(anim.RotateBlendY == otherAnim.RotateBlendY);
                        REPACK_MODEL_DEBUG_ASSERT(anim.RotateBlendZ == otherAnim.RotateBlendZ);
                        REPACK_MODEL_DEBUG_ASSERT(anim.RotateLutLengthX == otherAnim.RotateLutLengthX);
                        REPACK_MODEL_DEBUG_ASSERT(anim.RotateLutLengthY == otherAnim.RotateLutLengthY);
                        REPACK_MODEL_DEBUG_ASSERT(anim.RotateLutLengthZ == otherAnim.RotateLutLengthZ);
                        REPACK_MODEL_DEBUG_ASSERT(anim.RotateLutIndexX == otherAnim.RotateLutIndexX);
                        REPACK_MODEL_DEBUG_ASSERT(anim.RotateLutIndexY == otherAnim.RotateLutIndexY);
                        REPACK_MODEL_DEBUG_ASSERT(anim.RotateLutIndexZ == otherAnim.RotateLutIndexZ);
                        REPACK_MODEL_DEBUG_ASSERT(anim.TranslateBlendX == otherAnim.TranslateBlendX);
                        REPACK_MODEL_DEBUG_ASSERT(anim.TranslateBlendY == otherAnim.TranslateBlendY);
                        REPACK_MODEL_DEBUG_ASSERT(anim.TranslateBlendZ == otherAnim.TranslateBlendZ);
                        REPACK_MODEL_DEBUG_ASSERT(anim.TranslateLutLengthX == otherAnim.TranslateLutLengthX);
                        REPACK_MODEL_DEBUG_ASSERT(anim.TranslateLutLengthY == otherAnim.TranslateLutLengthY);
                        REPACK_MODEL_DEBUG_ASSERT(anim.TranslateLutLengthZ == otherAnim.TranslateLutLengthZ);
                        REPACK_MODEL_DEBUG_ASSERT(anim.TranslateLutIndexX == otherAnim.TranslateLutIndexX);
                        REPACK_MODEL_DEBUG_ASSERT(anim.TranslateLutIndexY == otherAnim.TranslateLutIndexY);
                        REPACK_MODEL_DEBUG_ASSERT(anim.TranslateLutIndexZ == otherAnim.TranslateLutIndexZ);
                    }
                }
            }

            const std::uint32_t mat = ManagedListAt(RequireReference(mats), i);
            const std::uint32_t otherMat = ManagedListAt(RequireReference(otherMats), i);
            REPACK_MODEL_DEBUG_ASSERT(mat == otherMat);
            if (mat != 0)
            {
                const RawMaterialAnimationGroup group
                    = Read::DoOffset<RawMaterialAnimationGroup>(first, mat);
                const RawMaterialAnimationGroup otherGroup
                    = Read::DoOffset<RawMaterialAnimationGroup>(second, otherMat);
                REPACK_MODEL_DEBUG_ASSERT(group.FrameCount == otherGroup.FrameCount);
                REPACK_MODEL_DEBUG_ASSERT(group.ColorLutOffset == otherGroup.ColorLutOffset);
                REPACK_MODEL_DEBUG_ASSERT(group.AnimationCount == otherGroup.AnimationCount);
                REPACK_MODEL_DEBUG_ASSERT(group.AnimationOffset == otherGroup.AnimationOffset);
                REPACK_MODEL_DEBUG_ASSERT(group.AnimationFrame == otherGroup.AnimationFrame);
                REPACK_MODEL_DEBUG_ASSERT(group.Unused12 == otherGroup.Unused12);
                const std::int32_t colorCount = static_cast<std::int32_t>(
                    group.AnimationOffset - group.ColorLutOffset);
                const auto colors = Read::DoOffsets<std::uint8_t>(
                    first, group.ColorLutOffset, colorCount);
                const auto otherColors = Read::DoOffsets<std::uint8_t>(
                    second, otherGroup.ColorLutOffset, colorCount);
                REPACK_MODEL_DEBUG_ASSERT(RequireReference(colors) == RequireReference(otherColors));
                const auto anims = Read::DoOffsets<MaterialAnimation>(
                    first, group.AnimationOffset, group.AnimationCount);
                const auto otherAnims = Read::DoOffsets<MaterialAnimation>(
                    second, otherGroup.AnimationOffset, otherGroup.AnimationCount);
                for (std::int32_t j = 0; j < ToIntCount(RequireReference(anims).size()); ++j)
                {
                    const MaterialAnimation& anim = ManagedListAt(RequireReference(anims), j);
                    const MaterialAnimation& otherAnim = ManagedListAt(RequireReference(otherAnims), j);
                    REPACK_MODEL_DEBUG_ASSERT(anim.Name.WireBytes() == otherAnim.Name.WireBytes());
                    REPACK_MODEL_DEBUG_ASSERT(anim.Unused40 == otherAnim.Unused40);
                    REPACK_MODEL_DEBUG_ASSERT(anim.DiffuseBlendR == otherAnim.DiffuseBlendR);
                    REPACK_MODEL_DEBUG_ASSERT(anim.DiffuseBlendG == otherAnim.DiffuseBlendG);
                    REPACK_MODEL_DEBUG_ASSERT(anim.DiffuseBlendB == otherAnim.DiffuseBlendB);
                    REPACK_MODEL_DEBUG_ASSERT(anim.Unused47 == otherAnim.Unused47);
                    REPACK_MODEL_DEBUG_ASSERT(anim.DiffuseLutLengthR == otherAnim.DiffuseLutLengthR);
                    REPACK_MODEL_DEBUG_ASSERT(anim.DiffuseLutLengthG == otherAnim.DiffuseLutLengthG);
                    REPACK_MODEL_DEBUG_ASSERT(anim.DiffuseLutLengthB == otherAnim.DiffuseLutLengthB);
                    REPACK_MODEL_DEBUG_ASSERT(anim.DiffuseLutIndexR == otherAnim.DiffuseLutIndexR);
                    REPACK_MODEL_DEBUG_ASSERT(anim.DiffuseLutIndexG == otherAnim.DiffuseLutIndexG);
                    REPACK_MODEL_DEBUG_ASSERT(anim.DiffuseLutIndexB == otherAnim.DiffuseLutIndexB);
                    REPACK_MODEL_DEBUG_ASSERT(anim.AmbientBlendR == otherAnim.AmbientBlendR);
                    REPACK_MODEL_DEBUG_ASSERT(anim.AmbientBlendG == otherAnim.AmbientBlendG);
                    REPACK_MODEL_DEBUG_ASSERT(anim.AmbientBlendB == otherAnim.AmbientBlendB);
                    REPACK_MODEL_DEBUG_ASSERT(anim.Unused57 == otherAnim.Unused57);
                    REPACK_MODEL_DEBUG_ASSERT(anim.AmbientLutLengthR == otherAnim.AmbientLutLengthR);
                    REPACK_MODEL_DEBUG_ASSERT(anim.AmbientLutLengthG == otherAnim.AmbientLutLengthG);
                    REPACK_MODEL_DEBUG_ASSERT(anim.AmbientLutLengthB == otherAnim.AmbientLutLengthB);
                    REPACK_MODEL_DEBUG_ASSERT(anim.AmbientLutIndexR == otherAnim.AmbientLutIndexR);
                    REPACK_MODEL_DEBUG_ASSERT(anim.AmbientLutIndexG == otherAnim.AmbientLutIndexG);
                    REPACK_MODEL_DEBUG_ASSERT(anim.AmbientLutIndexB == otherAnim.AmbientLutIndexB);
                    REPACK_MODEL_DEBUG_ASSERT(anim.SpecularBlendR == otherAnim.SpecularBlendR);
                    REPACK_MODEL_DEBUG_ASSERT(anim.SpecularBlendG == otherAnim.SpecularBlendG);
                    REPACK_MODEL_DEBUG_ASSERT(anim.SpecularBlendB == otherAnim.SpecularBlendB);
                    REPACK_MODEL_DEBUG_ASSERT(anim.Unused67 == otherAnim.Unused67);
                    REPACK_MODEL_DEBUG_ASSERT(anim.SpecularLutLengthR == otherAnim.SpecularLutLengthR);
                    REPACK_MODEL_DEBUG_ASSERT(anim.SpecularLutLengthG == otherAnim.SpecularLutLengthG);
                    REPACK_MODEL_DEBUG_ASSERT(anim.SpecularLutLengthB == otherAnim.SpecularLutLengthB);
                    REPACK_MODEL_DEBUG_ASSERT(anim.SpecularLutIndexR == otherAnim.SpecularLutIndexR);
                    REPACK_MODEL_DEBUG_ASSERT(anim.SpecularLutIndexG == otherAnim.SpecularLutIndexG);
                    REPACK_MODEL_DEBUG_ASSERT(anim.SpecularLutIndexB == otherAnim.SpecularLutIndexB);
                    REPACK_MODEL_DEBUG_ASSERT(anim.Unused74 == otherAnim.Unused74);
                    REPACK_MODEL_DEBUG_ASSERT(anim.Unused78 == otherAnim.Unused78);
                    REPACK_MODEL_DEBUG_ASSERT(anim.Unused7C == otherAnim.Unused7C);
                    REPACK_MODEL_DEBUG_ASSERT(anim.Unused80 == otherAnim.Unused80);
                    REPACK_MODEL_DEBUG_ASSERT(anim.AlphaBlend == otherAnim.AlphaBlend);
                    REPACK_MODEL_DEBUG_ASSERT(anim.Unused85 == otherAnim.Unused85);
                    REPACK_MODEL_DEBUG_ASSERT(anim.AlphaLutLength == otherAnim.AlphaLutLength);
                    REPACK_MODEL_DEBUG_ASSERT(anim.AlphaLutIndex == otherAnim.AlphaLutIndex);
                    REPACK_MODEL_DEBUG_ASSERT(anim.MaterialId == otherAnim.MaterialId);
                }
            }

            const std::uint32_t uv = ManagedListAt(RequireReference(uvs), i);
            const std::uint32_t otherUv = ManagedListAt(RequireReference(otherUvs), i);
            REPACK_MODEL_DEBUG_ASSERT(uv == otherUv);
            if (uv != 0)
            {
                const RawTexcoordAnimationGroup group
                    = Read::DoOffset<RawTexcoordAnimationGroup>(first, uv);
                const RawTexcoordAnimationGroup otherGroup
                    = Read::DoOffset<RawTexcoordAnimationGroup>(second, otherUv);
                REPACK_MODEL_DEBUG_ASSERT(group.FrameCount == otherGroup.FrameCount);
                REPACK_MODEL_DEBUG_ASSERT(group.ScaleLutOffset == otherGroup.ScaleLutOffset);
                REPACK_MODEL_DEBUG_ASSERT(group.RotateLutOffset == otherGroup.RotateLutOffset);
                REPACK_MODEL_DEBUG_ASSERT(group.TranslateLutOffset == otherGroup.TranslateLutOffset);
                REPACK_MODEL_DEBUG_ASSERT(group.AnimationCount == otherGroup.AnimationCount);
                REPACK_MODEL_DEBUG_ASSERT(group.AnimationOffset == otherGroup.AnimationOffset);
                REPACK_MODEL_DEBUG_ASSERT(group.AnimationFrame == otherGroup.AnimationFrame);
                REPACK_MODEL_DEBUG_ASSERT(group.Unused1A == otherGroup.Unused1A);
                const std::int32_t scaleCount
                    = static_cast<std::int32_t>(
                        group.RotateLutOffset - group.ScaleLutOffset) / 4;
                const std::int32_t rotCount
                    = static_cast<std::int32_t>(
                        group.TranslateLutOffset - group.RotateLutOffset) / 2;
                const std::int32_t transCount
                    = static_cast<std::int32_t>(
                        group.AnimationOffset - group.TranslateLutOffset) / 4;
                const auto scales = Read::DoOffsets<std::uint32_t>(
                    first, group.ScaleLutOffset, scaleCount);
                const auto otherScales = Read::DoOffsets<std::uint32_t>(
                    second, otherGroup.ScaleLutOffset, scaleCount);
                REPACK_MODEL_DEBUG_ASSERT(RequireReference(scales) == RequireReference(otherScales));
                const auto rots = Read::DoOffsets<std::uint16_t>(
                    first, group.RotateLutOffset, rotCount);
                const auto otherRots = Read::DoOffsets<std::uint16_t>(
                    second, otherGroup.RotateLutOffset, rotCount);
                REPACK_MODEL_DEBUG_ASSERT(RequireReference(rots) == RequireReference(otherRots));
                const auto poses = Read::DoOffsets<std::uint32_t>(
                    first, group.TranslateLutOffset, transCount);
                const auto otherPoses = Read::DoOffsets<std::uint32_t>(
                    second, otherGroup.TranslateLutOffset, transCount);
                REPACK_MODEL_DEBUG_ASSERT(RequireReference(poses) == RequireReference(otherPoses));
                const auto anims = Read::DoOffsets<TexcoordAnimation>(
                    first, group.AnimationOffset, group.AnimationCount);
                const auto otherAnims = Read::DoOffsets<TexcoordAnimation>(
                    second, otherGroup.AnimationOffset, otherGroup.AnimationCount);
                for (std::int32_t j = 0; j < ToIntCount(RequireReference(anims).size()); ++j)
                {
                    const TexcoordAnimation& anim = ManagedListAt(RequireReference(anims), j);
                    const TexcoordAnimation& otherAnim = ManagedListAt(RequireReference(otherAnims), j);
                    REPACK_MODEL_DEBUG_ASSERT(anim.Name.WireBytes() == otherAnim.Name.WireBytes());
                    REPACK_MODEL_DEBUG_ASSERT(anim.ScaleBlendS == otherAnim.ScaleBlendS);
                    REPACK_MODEL_DEBUG_ASSERT(anim.ScaleBlendT == otherAnim.ScaleBlendT);
                    REPACK_MODEL_DEBUG_ASSERT(anim.ScaleLutLengthS == otherAnim.ScaleLutLengthS);
                    REPACK_MODEL_DEBUG_ASSERT(anim.ScaleLutLengthT == otherAnim.ScaleLutLengthT);
                    REPACK_MODEL_DEBUG_ASSERT(anim.ScaleLutIndexS == otherAnim.ScaleLutIndexS);
                    REPACK_MODEL_DEBUG_ASSERT(anim.ScaleLutIndexT == otherAnim.ScaleLutIndexT);
                    REPACK_MODEL_DEBUG_ASSERT(anim.RotateBlendZ == otherAnim.RotateBlendZ);
                    REPACK_MODEL_DEBUG_ASSERT(anim.Unused2B == otherAnim.Unused2B);
                    REPACK_MODEL_DEBUG_ASSERT(anim.RotateLutLengthZ == otherAnim.RotateLutLengthZ);
                    REPACK_MODEL_DEBUG_ASSERT(anim.RotateLutIndexZ == otherAnim.RotateLutIndexZ);
                    REPACK_MODEL_DEBUG_ASSERT(anim.TranslateBlendS == otherAnim.TranslateBlendS);
                    REPACK_MODEL_DEBUG_ASSERT(anim.TranslateBlendT == otherAnim.TranslateBlendT);
                    REPACK_MODEL_DEBUG_ASSERT(anim.TranslateLutLengthS == otherAnim.TranslateLutLengthS);
                    REPACK_MODEL_DEBUG_ASSERT(anim.TranslateLutLengthT == otherAnim.TranslateLutLengthT);
                    REPACK_MODEL_DEBUG_ASSERT(anim.TranslateLutIndexS == otherAnim.TranslateLutIndexS);
                    REPACK_MODEL_DEBUG_ASSERT(anim.TranslateLutIndexT == otherAnim.TranslateLutIndexT);
                }
            }

            const std::uint32_t tex = ManagedListAt(RequireReference(texes), i);
            const std::uint32_t otherTex = ManagedListAt(RequireReference(otherTexes), i);
            REPACK_MODEL_DEBUG_ASSERT(tex == otherTex);
            if (tex != 0)
            {
                const RawTextureAnimationGroup group
                    = Read::DoOffset<RawTextureAnimationGroup>(first, tex);
                const RawTextureAnimationGroup otherGroup
                    = Read::DoOffset<RawTextureAnimationGroup>(second, otherTex);
                REPACK_MODEL_DEBUG_ASSERT(group.FrameCount == otherGroup.FrameCount);
                REPACK_MODEL_DEBUG_ASSERT(group.FrameIndexCount == otherGroup.FrameIndexCount);
                REPACK_MODEL_DEBUG_ASSERT(group.TextureIdCount == otherGroup.TextureIdCount);
                REPACK_MODEL_DEBUG_ASSERT(group.PaletteIdCount == otherGroup.PaletteIdCount);
                REPACK_MODEL_DEBUG_ASSERT(group.AnimationCount == otherGroup.AnimationCount);
                REPACK_MODEL_DEBUG_ASSERT(group.UnusedA == otherGroup.UnusedA);
                REPACK_MODEL_DEBUG_ASSERT(group.FrameIndexOffset == otherGroup.FrameIndexOffset);
                REPACK_MODEL_DEBUG_ASSERT(group.TextureIdOffset == otherGroup.TextureIdOffset);
                REPACK_MODEL_DEBUG_ASSERT(group.PaletteIdOffset == otherGroup.PaletteIdOffset);
                REPACK_MODEL_DEBUG_ASSERT(group.AnimationOffset == otherGroup.AnimationOffset);
                REPACK_MODEL_DEBUG_ASSERT(group.AnimationFrame == otherGroup.AnimationFrame);
                REPACK_MODEL_DEBUG_ASSERT(group.Unused1C == otherGroup.Unused1C);
                const auto frameIds = Read::DoOffsets<std::uint16_t>(
                    first, group.FrameIndexOffset, group.FrameIndexCount);
                const auto otherFrameIds = Read::DoOffsets<std::uint16_t>(
                    second, otherGroup.FrameIndexOffset, otherGroup.FrameIndexCount);
                REPACK_MODEL_DEBUG_ASSERT(RequireReference(frameIds) == RequireReference(otherFrameIds));
                const auto texIds = Read::DoOffsets<std::uint16_t>(
                    first, group.TextureIdOffset, group.TextureIdCount);
                const auto otherTexIds = Read::DoOffsets<std::uint16_t>(
                    second, otherGroup.TextureIdOffset, otherGroup.TextureIdCount);
                REPACK_MODEL_DEBUG_ASSERT(RequireReference(texIds) == RequireReference(otherTexIds));
                const auto palIds = Read::DoOffsets<std::uint16_t>(
                    first, group.PaletteIdOffset, group.PaletteIdCount);
                const auto otherPalIds = Read::DoOffsets<std::uint16_t>(
                    second, otherGroup.PaletteIdOffset, otherGroup.PaletteIdCount);
                REPACK_MODEL_DEBUG_ASSERT(RequireReference(palIds) == RequireReference(otherPalIds));
                const auto anims = Read::DoOffsets<TextureAnimation>(
                    first, group.AnimationOffset, group.AnimationCount);
                const auto otherAnims = Read::DoOffsets<TextureAnimation>(
                    second, otherGroup.AnimationOffset, otherGroup.AnimationCount);
                for (std::int32_t j = 0; j < ToIntCount(RequireReference(anims).size()); ++j)
                {
                    const TextureAnimation& anim = ManagedListAt(RequireReference(anims), j);
                    const TextureAnimation& otherAnim = ManagedListAt(RequireReference(otherAnims), j);
                    REPACK_MODEL_DEBUG_ASSERT(anim.Name.WireBytes() == otherAnim.Name.WireBytes());
                    REPACK_MODEL_DEBUG_ASSERT(anim.Count == otherAnim.Count);
                    REPACK_MODEL_DEBUG_ASSERT(anim.StartIndex == otherAnim.StartIndex);
                    REPACK_MODEL_DEBUG_ASSERT(anim.MinimumPaletteId == otherAnim.MinimumPaletteId);
                    REPACK_MODEL_DEBUG_ASSERT(anim.MaterialId == otherAnim.MaterialId);
                    REPACK_MODEL_DEBUG_ASSERT(anim.MinimumTextureId == otherAnim.MinimumTextureId);
                }
            }
        }

        REPACK_MODEL_DEBUG_ASSERT(bytes == otherBytes);
        Nop();
    }
}
