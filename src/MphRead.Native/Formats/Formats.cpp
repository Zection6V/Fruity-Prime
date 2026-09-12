#include "Formats.hpp"

#include "../Metadata/Metadata.hpp"
#include "../Program.hpp"
#include "../Strings.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <new>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace
{
    using MphRead::AreaVolumeEntityData;
    using MphRead::CollisionVolume;
    using MphRead::EntityType;
    using MphRead::FhAreaVolumeEntityData;
    using MphRead::FhRawCollisionVolume;
    using MphRead::FhTriggerVolumeEntityData;
    using MphRead::FhVolumeType;
    using MphRead::InstructionCode;
    using MphRead::PointModuleEntityData;
    using MphRead::RawCollisionVolume;
    using MphRead::TriggerVolumeEntityData;
    using MphRead::VolumeType;
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Vector4;

    template <typename T>
    T& AssignReadonly(T& self, const T& other) noexcept
    {
        if (std::addressof(self) != std::addressof(other))
        {
            self.~T();
            ::new (static_cast<void*>(std::addressof(self))) T(other);
        }
        return self;
    }

    template <typename T>
    [[nodiscard]] T& Require(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    template <typename T>
    [[nodiscard]] const T& Require(const std::shared_ptr<const T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    [[nodiscard]] constexpr Vector3 Multiply(Vector3 value, float scalar) noexcept
    {
        return Vector3(value.X * scalar, value.Y * scalar, value.Z * scalar);
    }

    [[nodiscard]] constexpr Vector3 Multiply(float scalar, Vector3 value) noexcept
    {
        return Multiply(value, scalar);
    }

    [[nodiscard]] float Length(Vector3 value)
    {
        return std::sqrt(value.X * value.X + value.Y * value.Y + value.Z * value.Z);
    }

    [[nodiscard]] constexpr std::int32_t ManagedInt32(std::uint32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(value);
    }

    template <typename TArray>
    [[nodiscard]] std::string MarshalString(const TArray& array)
    {
        std::string result;
        for (const auto raw : array)
        {
            const std::uint32_t value = static_cast<std::uint32_t>(
                static_cast<std::make_unsigned_t<std::remove_cv_t<decltype(raw)>>>(raw));
            if (value == 0)
            {
                break;
            }

            // MarshalString(byte[]) maps each byte directly to a UTF-16 char.
            // Native strings in this tree are UTF-8, so retain the same scalar
            // value when materializing the managed string.
            if (value <= 0x7FU)
            {
                result.push_back(static_cast<char>(value));
            }
            else if (value <= 0x7FFU)
            {
                result.push_back(static_cast<char>(0xC0U | (value >> 6)));
                result.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
            }
            else
            {
                result.push_back(static_cast<char>(0xE0U | (value >> 12)));
                result.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
                result.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
            }
        }
        return result;
    }

    template <typename TArray>
    [[nodiscard]] std::shared_ptr<const std::string> MarshalManagedString(const TArray& array)
    {
        return std::make_shared<const std::string>(MarshalString(array));
    }

    template <typename TArray>
    [[nodiscard]] std::string ReverseMarshalString(const TArray& array)
    {
        std::string result;
        for (auto iterator = std::rbegin(array); iterator != std::rend(array); ++iterator)
        {
            const std::uint32_t value = static_cast<std::uint32_t>(
                static_cast<std::make_unsigned_t<std::remove_cv_t<std::remove_reference_t<decltype(*iterator)>>>>(*iterator));
            if (value == 0)
            {
                break;
            }
            if (value <= 0x7FU)
            {
                result.push_back(static_cast<char>(value));
            }
            else if (value <= 0x7FFU)
            {
                result.push_back(static_cast<char>(0xC0U | (value >> 6)));
                result.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
            }
            else
            {
                result.push_back(static_cast<char>(0xE0U | (value >> 12)));
                result.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
                result.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
            }
        }
        return result;
    }

    [[nodiscard]] bool IsDefinedEntityType(EntityType type) noexcept
    {
        switch (type)
        {
        case EntityType::Platform:
        case EntityType::Object:
        case EntityType::PlayerSpawn:
        case EntityType::Door:
        case EntityType::ItemSpawn:
        case EntityType::ItemInstance:
        case EntityType::EnemySpawn:
        case EntityType::TriggerVolume:
        case EntityType::AreaVolume:
        case EntityType::JumpPad:
        case EntityType::PointModule:
        case EntityType::MorphCamera:
        case EntityType::OctolithFlag:
        case EntityType::FlagBase:
        case EntityType::Teleporter:
        case EntityType::NodeDefense:
        case EntityType::LightSource:
        case EntityType::Artifact:
        case EntityType::CameraSequence:
        case EntityType::ForceField:
        case EntityType::BeamEffect:
        case EntityType::Bomb:
        case EntityType::EnemyInstance:
        case EntityType::Halfturret:
        case EntityType::Player:
        case EntityType::BeamProjectile:
        case EntityType::ListHead:
        case EntityType::FhUnknown0:
        case EntityType::FhPlayerSpawn:
        case EntityType::FhUnknown2:
        case EntityType::FhDoor:
        case EntityType::FhItemSpawn:
        case EntityType::FhItemInstance:
        case EntityType::FhEnemySpawn:
        case EntityType::FhEffectInstance:
        case EntityType::FhBomb:
        case EntityType::FhTriggerVolume:
        case EntityType::FhAreaVolume:
        case EntityType::FhPlatform:
        case EntityType::FhJumpPad:
        case EntityType::FhPointModule:
        case EntityType::FhMorphCamera:
        case EntityType::FhEnemyInstance:
        case EntityType::FhPlayer:
        case EntityType::FhBeamProjectile:
        case EntityType::Room:
        case EntityType::Model:
        case EntityType::All:
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] std::string EntityTypeString(EntityType type)
    {
        return std::to_string(static_cast<std::uint16_t>(type));
    }

    [[nodiscard]] EntityType ValidateEntityType(EntityType type)
    {
        if (!IsDefinedEntityType(type))
        {
            throw MphRead::ProgramException(
                "Invalid entity type " + EntityTypeString(type));
        }
        return type;
    }

    [[nodiscard]] std::string VolumeTypeString(VolumeType type)
    {
        return std::to_string(static_cast<std::uint32_t>(type));
    }

    [[nodiscard]] std::string FhVolumeTypeString(FhVolumeType type)
    {
        return std::to_string(static_cast<std::uint32_t>(type));
    }

    [[nodiscard]] bool IsDefinedInstructionCode(InstructionCode code) noexcept
    {
        switch (code)
        {
        case InstructionCode::NOP:
        case InstructionCode::MTX_RESTORE:
        case InstructionCode::COLOR:
        case InstructionCode::NORMAL:
        case InstructionCode::TEXCOORD:
        case InstructionCode::VTX_16:
        case InstructionCode::VTX_10:
        case InstructionCode::VTX_XY:
        case InstructionCode::VTX_XZ:
        case InstructionCode::VTX_YZ:
        case InstructionCode::VTX_DIFF:
        case InstructionCode::DIF_AMB:
        case InstructionCode::BEGIN_VTXS:
        case InstructionCode::END_VTXS:
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] std::string InstructionCodeString(InstructionCode code)
    {
        switch (code)
        {
        case InstructionCode::NOP: return "NOP";
        case InstructionCode::MTX_RESTORE: return "MTX_RESTORE";
        case InstructionCode::COLOR: return "COLOR";
        case InstructionCode::NORMAL: return "NORMAL";
        case InstructionCode::TEXCOORD: return "TEXCOORD";
        case InstructionCode::VTX_16: return "VTX_16";
        case InstructionCode::VTX_10: return "VTX_10";
        case InstructionCode::VTX_XY: return "VTX_XY";
        case InstructionCode::VTX_XZ: return "VTX_XZ";
        case InstructionCode::VTX_YZ: return "VTX_YZ";
        case InstructionCode::VTX_DIFF: return "VTX_DIFF";
        case InstructionCode::DIF_AMB: return "DIF_AMB";
        case InstructionCode::BEGIN_VTXS: return "BEGIN_VTXS";
        case InstructionCode::END_VTXS: return "END_VTXS";
        default:
            return std::to_string(static_cast<std::uint32_t>(code));
        }
    }

    template <typename TMap>
    [[nodiscard]] std::shared_ptr<const TMap> FreezeMap(
        const std::shared_ptr<const TMap>& source)
    {
        if (!source)
        {
            throw System::ArgumentNullException("source");
        }
        return std::make_shared<const TMap>(*source);
    }

    template <typename TMap>
    [[nodiscard]] std::shared_ptr<const TMap> EmptyFrozenMap()
    {
        static const auto empty = std::make_shared<const TMap>();
        return empty;
    }

    [[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view value);
    [[nodiscard]] std::string PathToUtf8(const std::filesystem::path& path);

    [[nodiscard]] std::string GetFileNameWithoutExtension(std::string_view name)
    {
        return PathToUtf8(PathFromUtf8(name).stem());
    }

    [[nodiscard]] std::string ReplaceAll(
        std::string value, std::string_view oldValue, std::string_view newValue)
    {
        if (oldValue.empty())
        {
            return value;
        }
        std::size_t position = 0;
        while ((position = value.find(oldValue, position)) != std::string::npos)
        {
            value.replace(position, oldValue.size(), newValue);
            position += newValue.size();
        }
        return value;
    }

    [[nodiscard]] std::int64_t TickCount64Milliseconds() noexcept
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(
            steady_clock::now().time_since_epoch()).count();
    }

    [[nodiscard]] std::shared_ptr<std::vector<std::uint32_t>>
        CopyRenderArguments(
            InstructionCode code,
            const std::shared_ptr<std::vector<std::uint32_t>>& arguments)
    {
        const auto& source = Require(arguments);
        const std::int32_t arity = MphRead::RenderInstruction::GetArity(code);
        if (source.size() != static_cast<std::size_t>(arity))
        {
            throw MphRead::ProgramException(
                "Incorrect number of arguments for code "
                + InstructionCodeString(code) + ".");
        }
        return std::make_shared<std::vector<std::uint32_t>>(source);
    }

    struct Utf8CodePoint
    {
        std::uint32_t Value;
        std::size_t Length;
    };

    [[nodiscard]] bool IsDotNetWhitespace(std::uint32_t codePoint) noexcept
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

    [[nodiscard]] std::optional<Utf8CodePoint> DecodeUtf8Forward(
        std::string_view text, std::size_t position) noexcept
    {
        if (position >= text.size())
        {
            return std::nullopt;
        }

        const auto first = static_cast<unsigned char>(text[position]);
        if (first <= 0x7FU)
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
            minimum = 0x80U;
        }
        else if ((first & 0xF0U) == 0xE0U)
        {
            value = first & 0x0FU;
            length = 3;
            minimum = 0x800U;
        }
        else if ((first & 0xF8U) == 0xF0U)
        {
            value = first & 0x07U;
            length = 4;
            minimum = 0x10000U;
        }
        else
        {
            return std::nullopt;
        }

        if (position + length > text.size())
        {
            return std::nullopt;
        }
        for (std::size_t index = 1; index < length; ++index)
        {
            const auto next = static_cast<unsigned char>(text[position + index]);
            if ((next & 0xC0U) != 0x80U)
            {
                return std::nullopt;
            }
            value = (value << 6) | (next & 0x3FU);
        }
        if (value < minimum || value > 0x10FFFFU
            || (value >= 0xD800U && value <= 0xDFFFU))
        {
            return std::nullopt;
        }
        return Utf8CodePoint{value, length};
    }

    [[nodiscard]] std::optional<std::pair<Utf8CodePoint, std::size_t>>
        DecodeUtf8Backward(std::string_view text, std::size_t end) noexcept
    {
        if (end == 0 || end > text.size())
        {
            return std::nullopt;
        }
        std::size_t start = end - 1;
        while (start > 0
            && (static_cast<unsigned char>(text[start]) & 0xC0U) == 0x80U)
        {
            start--;
        }
        const auto decoded = DecodeUtf8Forward(text, start);
        if (!decoded.has_value() || start + decoded->Length != end)
        {
            return std::nullopt;
        }
        return std::make_pair(*decoded, start);
    }

    [[nodiscard]] std::string TrimDotNetWhitespace(std::string value)
    {
        std::size_t first = 0;
        std::size_t last = value.size();

        while (first < last)
        {
            const auto decoded = DecodeUtf8Forward(
                std::string_view(value).substr(0, last), first);
            if (!decoded.has_value() || !IsDotNetWhitespace(decoded->Value))
            {
                break;
            }
            first += decoded->Length;
        }

        while (last > first)
        {
            const auto decoded = DecodeUtf8Backward(value, last);
            if (!decoded.has_value()
                || !IsDotNetWhitespace(decoded->first.Value))
            {
                break;
            }
            last = decoded->second;
        }

        return value.substr(first, last - first);
    }

    [[nodiscard]] std::vector<std::string> SplitEquals(const std::string& value)
    {
        std::vector<std::string> result;
        std::size_t start = 0;
        while (true)
        {
            const std::size_t index = value.find('=', start);
            if (index == std::string::npos)
            {
                result.push_back(value.substr(start));
                break;
            }
            result.push_back(value.substr(start, index - start));
            start = index + 1;
        }
        return result;
    }

    [[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view value)
    {
#if defined(__cpp_char8_t)
        std::u8string converted;
        converted.reserve(value.size());
        for (unsigned char byte : value)
        {
            converted.push_back(static_cast<char8_t>(byte));
        }
        return std::filesystem::path(converted);
#else
        return std::filesystem::u8path(value.begin(), value.end());
#endif
    }

    [[nodiscard]] std::string PathToUtf8(const std::filesystem::path& path)
    {
#if defined(__cpp_char8_t)
        const std::u8string value = path.u8string();
        std::string result;
        result.reserve(value.size());
        for (char8_t byte : value)
        {
            result.push_back(static_cast<char>(byte));
        }
        return result;
#else
        return path.u8string();
#endif
    }

    [[nodiscard]] bool FileExists(std::string_view path) noexcept
    {
        try
        {
            std::error_code error;
            const auto status = std::filesystem::status(PathFromUtf8(path), error);
            return !error && std::filesystem::is_regular_file(status);
        }
        catch (...)
        {
            return false;
        }
    }

    void AppendUtf8(std::string& output, std::uint32_t codePoint)
    {
        if (codePoint > 0x10FFFFU
            || (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
        {
            codePoint = 0xFFFDU;
        }

        if (codePoint <= 0x7FU)
        {
            output.push_back(static_cast<char>(codePoint));
        }
        else if (codePoint <= 0x7FFU)
        {
            output.push_back(static_cast<char>(0xC0U | (codePoint >> 6)));
            output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        }
        else if (codePoint <= 0xFFFFU)
        {
            output.push_back(static_cast<char>(0xE0U | (codePoint >> 12)));
            output.push_back(static_cast<char>(
                0x80U | ((codePoint >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        }
        else
        {
            output.push_back(static_cast<char>(0xF0U | (codePoint >> 18)));
            output.push_back(static_cast<char>(
                0x80U | ((codePoint >> 12) & 0x3FU)));
            output.push_back(static_cast<char>(
                0x80U | ((codePoint >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        }
    }

    [[nodiscard]] std::string DecodeUtf8Text(std::string_view bytes)
    {
        std::string output;
        output.reserve(bytes.size());
        for (std::size_t index = 0; index < bytes.size();)
        {
            const auto decoded = DecodeUtf8Forward(bytes, index);
            if (decoded.has_value())
            {
                output.append(bytes.substr(index, decoded->Length));
                index += decoded->Length;
            }
            else
            {
                AppendUtf8(output, 0xFFFDU);
                index++;
            }
        }
        return output;
    }

    [[nodiscard]] std::string DecodeUtf16Text(
        std::string_view bytes, bool bigEndian)
    {
        std::string output;
        output.reserve(bytes.size());

        const auto readUnit = [&](std::size_t index)
        {
            const auto first = static_cast<unsigned char>(bytes[index]);
            const auto second = static_cast<unsigned char>(bytes[index + 1]);
            return bigEndian
                ? static_cast<std::uint16_t>((first << 8) | second)
                : static_cast<std::uint16_t>(first | (second << 8));
        };

        std::size_t index = 0;
        while (index + 1 < bytes.size())
        {
            const std::uint16_t first = readUnit(index);
            index += 2;
            if (first >= 0xD800U && first <= 0xDBFFU)
            {
                if (index + 1 < bytes.size())
                {
                    const std::uint16_t second = readUnit(index);
                    if (second >= 0xDC00U && second <= 0xDFFFU)
                    {
                        index += 2;
                        const std::uint32_t codePoint
                            = 0x10000U
                            + ((static_cast<std::uint32_t>(first) - 0xD800U) << 10)
                            + (static_cast<std::uint32_t>(second) - 0xDC00U);
                        AppendUtf8(output, codePoint);
                        continue;
                    }
                }
                AppendUtf8(output, 0xFFFDU);
            }
            else if (first >= 0xDC00U && first <= 0xDFFFU)
            {
                AppendUtf8(output, 0xFFFDU);
            }
            else
            {
                AppendUtf8(output, first);
            }
        }

        if (index < bytes.size())
        {
            AppendUtf8(output, 0xFFFDU);
        }
        return output;
    }

    [[nodiscard]] std::string DecodeUtf32Text(
        std::string_view bytes, bool bigEndian)
    {
        std::string output;
        output.reserve(bytes.size());

        std::size_t index = 0;
        while (index + 3 < bytes.size())
        {
            const auto b0 = static_cast<unsigned char>(bytes[index]);
            const auto b1 = static_cast<unsigned char>(bytes[index + 1]);
            const auto b2 = static_cast<unsigned char>(bytes[index + 2]);
            const auto b3 = static_cast<unsigned char>(bytes[index + 3]);
            index += 4;

            const std::uint32_t codePoint = bigEndian
                ? (static_cast<std::uint32_t>(b0) << 24)
                    | (static_cast<std::uint32_t>(b1) << 16)
                    | (static_cast<std::uint32_t>(b2) << 8)
                    | static_cast<std::uint32_t>(b3)
                : static_cast<std::uint32_t>(b0)
                    | (static_cast<std::uint32_t>(b1) << 8)
                    | (static_cast<std::uint32_t>(b2) << 16)
                    | (static_cast<std::uint32_t>(b3) << 24);
            AppendUtf8(output, codePoint);
        }

        if (index < bytes.size())
        {
            AppendUtf8(output, 0xFFFDU);
        }
        return output;
    }

    [[nodiscard]] std::vector<std::string> SplitManagedLines(
        const std::string& text)
    {
        std::vector<std::string> lines;
        std::size_t start = 0;
        std::size_t index = 0;
        while (index < text.size())
        {
            if (text[index] == '\r' || text[index] == '\n')
            {
                lines.push_back(text.substr(start, index - start));
                if (text[index] == '\r'
                    && index + 1 < text.size()
                    && text[index + 1] == '\n')
                {
                    index++;
                }
                index++;
                start = index;
                continue;
            }
            index++;
        }
        if (start < text.size())
        {
            lines.push_back(text.substr(start));
        }
        return lines;
    }

    [[nodiscard]] std::vector<std::string> ReadAllLines(std::string_view path)
    {
        std::ifstream stream(PathFromUtf8(path), std::ios::in | std::ios::binary);
        if (!stream.is_open())
        {
            throw std::ios_base::failure(
                "Could not open file for reading: " + std::string(path));
        }
        const std::string bytes{
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
        if (stream.bad())
        {
            throw std::ios_base::failure(
                "Could not read file: " + std::string(path));
        }

        const auto byteAt = [&](std::size_t index)
        {
            return static_cast<unsigned char>(bytes[index]);
        };

        std::string text;
        if (bytes.size() >= 4
            && byteAt(0) == 0xFFU && byteAt(1) == 0xFEU
            && byteAt(2) == 0x00U && byteAt(3) == 0x00U)
        {
            text = DecodeUtf32Text(std::string_view(bytes).substr(4), false);
        }
        else if (bytes.size() >= 4
            && byteAt(0) == 0x00U && byteAt(1) == 0x00U
            && byteAt(2) == 0xFEU && byteAt(3) == 0xFFU)
        {
            text = DecodeUtf32Text(std::string_view(bytes).substr(4), true);
        }
        else if (bytes.size() >= 3
            && byteAt(0) == 0xEFU && byteAt(1) == 0xBBU
            && byteAt(2) == 0xBFU)
        {
            text = DecodeUtf8Text(std::string_view(bytes).substr(3));
        }
        else if (bytes.size() >= 2
            && byteAt(0) == 0xFFU && byteAt(1) == 0xFEU)
        {
            text = DecodeUtf16Text(std::string_view(bytes).substr(2), false);
        }
        else if (bytes.size() >= 2
            && byteAt(0) == 0xFEU && byteAt(1) == 0xFFU)
        {
            text = DecodeUtf16Text(std::string_view(bytes).substr(2), true);
        }
        else
        {
            text = DecodeUtf8Text(bytes);
        }
        return SplitManagedLines(text);
    }

    [[nodiscard]] std::string CombinePaths(
        const std::vector<std::string>& paths)
    {
        std::filesystem::path result;
        bool hasComponent = false;
        for (const std::string& path : paths)
        {
            if (path.empty())
            {
                continue;
            }
            if (!hasComponent)
            {
                result = PathFromUtf8(path);
                hasComponent = true;
            }
            else
            {
                result /= PathFromUtf8(path);
            }
        }
        return hasComponent ? PathToUtf8(result) : std::string();
    }

    template <typename T>
    [[nodiscard]] const T& EntityDataOf(const std::shared_ptr<MphRead::EntityOf<T>>& entity)
    {
        return Require(entity).Data;
    }

    [[nodiscard]] MphRead::NativeRuntime::CoroutineSequence<std::int32_t>
        EnumerateMeshIds(const MphRead::Node* node)
    {
        const std::int32_t start = node->MeshId / 2;
        for (std::int32_t index = 0; index < node->MeshCount; ++index)
        {
            co_yield start + index;
        }
    }

    [[nodiscard]] MphRead::NativeRuntime::CoroutineSequence<std::int32_t>
        EnumerateAllMeshIds(
            const MphRead::Node* node,
            const std::vector<std::shared_ptr<MphRead::Node>>* nodes,
            bool root)
    {
        const std::int32_t start = node->MeshId / 2;
        for (std::int32_t index = 0; index < node->MeshCount; ++index)
        {
            co_yield start + index;
        }

        if (!root && node->NextIndex != -1)
        {
            const MphRead::Node& next = Require(
                nodes->at(static_cast<std::size_t>(node->NextIndex)));
            for (std::int32_t value : next.GetAllMeshIds(*nodes, false))
            {
                co_yield value;
            }
        }

        if (node->ChildIndex != -1)
        {
            const MphRead::Node& child = Require(
                nodes->at(static_cast<std::size_t>(node->ChildIndex)));
            for (std::int32_t value : child.GetAllMeshIds(*nodes, false))
            {
                co_yield value;
            }
        }
    }

}

namespace MphRead
{
    Node::Node(RawNode raw)
        : Name(MarshalString(raw.Name)),
          ParentIndex(raw.ParentId),
          ChildIndex(raw.ChildId),
          NextIndex(raw.NextId),
          Enabled(raw.Enabled != 0),
          MeshCount(raw.MeshCount),
          MeshId(raw.MeshId),
          Scale(raw.Scale.ToFloatVector()),
          Angle(
              static_cast<float>(raw.AngleX) / 65536.0F
                  * 2.0F * std::numbers::pi_v<float>,
              static_cast<float>(raw.AngleY) / 65536.0F
                  * 2.0F * std::numbers::pi_v<float>,
              static_cast<float>(raw.AngleZ) / 65536.0F
                  * 2.0F * std::numbers::pi_v<float>),
          Position(raw.Position.ToFloatVector()),
          BoundingRadius(raw.BoundingRadius.FloatValue()),
          MinBounds(raw.MinBounds.ToFloatVector()),
          MaxBounds(raw.MaxBounds.ToFloatVector()),
          BillboardMode(raw.BillboardMode)
    {
        (*Bounds)[0] = MinBounds.X;
        (*Bounds)[1] = MinBounds.Y;
        (*Bounds)[2] = MinBounds.Z;
        (*Bounds)[3] = MaxBounds.X;
        (*Bounds)[4] = MaxBounds.Y;
        (*Bounds)[5] = MaxBounds.Z;
    }

    Enumerable<std::int32_t> Node::GetMeshIds() const
    {
        return Enumerable<std::int32_t>([this]()
        {
            return EnumerateMeshIds(this);
        });
    }

    Enumerable<std::int32_t> Node::GetAllMeshIds(
        const std::vector<std::shared_ptr<Node>>& nodes, bool root) const
    {
        return Enumerable<std::int32_t>([this, nodes = std::addressof(nodes), root]()
        {
            return EnumerateAllMeshIds(this, nodes, root);
        });
    }

    Mesh::Mesh(RawMesh raw)
        : MaterialId(raw.MaterialId),
          DlistId(raw.DlistId)
    {
    }

    std::optional<Vector4> Mesh::OverrideColor() const
    {
        const auto getFactor = []()
        {
            const std::int64_t milliseconds = TickCount64Milliseconds();
            float percentage = static_cast<float>(milliseconds % 1000) / 1000.0F;
            if (((milliseconds / 1000) % 10) % 2 == 0)
            {
                percentage = 1.0F - percentage;
            }
            return percentage;
        };

        if (Selection == SelectionType::Selected)
        {
            const float factor = getFactor();
            return Vector4(factor, factor, factor, 1.0F);
        }
        if (Selection == SelectionType::Parent)
        {
            const float factor = getFactor();
            return Vector4(factor, 0.0F, 0.0F, 1.0F);
        }
        if (Selection == SelectionType::Child)
        {
            const float factor = getFactor();
            return Vector4(0.0F, 0.0F, factor, 1.0F);
        }
        return std::nullopt;
    }

    Material::Material(RawMaterial raw)
        : Name(MarshalString(raw.Name)),
          Lighting(raw.Lighting),
          InitLighting(raw.Lighting),
          Culling(raw.Culling),
          Alpha(raw.Alpha),
          CurrentAlpha(static_cast<float>(raw.Alpha) / 31.0F),
          Wireframe(raw.Wireframe),
          TextureId(raw.TextureId),
          PaletteId(raw.PaletteId),
          CurrentTextureId(raw.TextureId),
          CurrentPaletteId(raw.PaletteId),
          XRepeat(raw.XRepeat),
          YRepeat(raw.YRepeat),
          Diffuse(raw.Diffuse),
          Ambient(raw.Ambient),
          Specular(raw.Specular),
          CurrentDiffuse(raw.Diffuse / 31.0F),
          CurrentAmbient(raw.Ambient / 31.0F),
          CurrentSpecular(raw.Specular / 31.0F),
          PolygonMode(raw.PolygonMode),
          RenderMode(raw.RenderMode),
          AnimationFlags(static_cast<MatAnimFlags>(raw.AnimationFlags)),
          TexgenMode(raw.TexcoordTransformMode),
          TexcoordAnimationId(raw.TexcoordAnimationId),
          MatrixId(ManagedInt32(raw.MatrixId)),
          ScaleS(raw.ScaleS.FloatValue()),
          ScaleT(raw.ScaleT.FloatValue()),
          TranslateS(raw.TranslateS.FloatValue()),
          TranslateT(raw.TranslateT.FloatValue()),
          RotateZ(static_cast<float>(raw.RotateZ) / 65536.0F
              * 2.0F * std::numbers::pi_v<float>)
    {
    }

    TextureData& TextureData::operator=(const TextureData& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    PaletteData& PaletteData::operator=(const PaletteData& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    NodeAnimationGroup::NodeAnimationGroup()
        : Scales(std::make_shared<const std::vector<float>>()),
          Rotations(Scales),
          Translations(Scales),
          Animations(EmptyFrozenMap<NodeAnimationDictionary>())
    {
    }

    NodeAnimationGroup::NodeAnimationGroup(
        RawNodeAnimationGroup raw,
        std::shared_ptr<const std::vector<float>> scales,
        std::shared_ptr<const std::vector<float>> rotations,
        std::shared_ptr<const std::vector<float>> translations,
        std::shared_ptr<const NodeAnimationDictionary> animations)
        : FrameCount(ManagedInt32(raw.FrameCount)),
          Count(animations
              ? static_cast<std::int32_t>(animations->size())
              : 0),
          Scales(std::move(scales)),
          Rotations(std::move(rotations)),
          Translations(std::move(translations)),
          Animations(FreezeMap(animations))
    {
    }

    std::shared_ptr<NodeAnimationGroup> NodeAnimationGroup::Empty()
    {
        return std::shared_ptr<NodeAnimationGroup>(new NodeAnimationGroup());
    }

    TexcoordAnimationGroup::TexcoordAnimationGroup()
        : Scales(std::make_shared<const std::vector<float>>()),
          Rotations(Scales),
          Translations(Scales),
          Animations(EmptyFrozenMap<TexcoordAnimationDictionary>())
    {
    }

    TexcoordAnimationGroup::TexcoordAnimationGroup(
        RawTexcoordAnimationGroup raw,
        std::shared_ptr<const std::vector<float>> scales,
        std::shared_ptr<const std::vector<float>> rotations,
        std::shared_ptr<const std::vector<float>> translations,
        std::shared_ptr<const TexcoordAnimationDictionary> animations)
        : FrameCount(ManagedInt32(raw.FrameCount)),
          CurrentFrame(raw.AnimationFrame),
          UnusedFrame(raw.Unused1A),
          Count(ManagedInt32(raw.AnimationCount)),
          Scales(std::move(scales)),
          Rotations(std::move(rotations)),
          Translations(std::move(translations)),
          Animations(FreezeMap(animations))
    {
        assert(Count == static_cast<std::int32_t>(Animations->size()));
    }

    std::shared_ptr<TexcoordAnimationGroup> TexcoordAnimationGroup::Empty()
    {
        return std::shared_ptr<TexcoordAnimationGroup>(new TexcoordAnimationGroup());
    }

    TextureAnimationGroup::TextureAnimationGroup()
        : FrameIndices(std::make_shared<const std::vector<std::uint16_t>>()),
          TextureIds(FrameIndices),
          PaletteIds(FrameIndices),
          Animations(EmptyFrozenMap<TextureAnimationDictionary>())
    {
    }

    TextureAnimationGroup::TextureAnimationGroup(
        RawTextureAnimationGroup raw,
        std::shared_ptr<const std::vector<std::uint16_t>> frameIndices,
        std::shared_ptr<const std::vector<std::uint16_t>> textureIds,
        std::shared_ptr<const std::vector<std::uint16_t>> paletteIds,
        std::shared_ptr<const TextureAnimationDictionary> animations)
        : FrameCount(raw.FrameCount),
          CurrentFrame(raw.AnimationFrame),
          UnusedFrame(raw.Unused1C),
          Count(raw.AnimationCount),
          FrameIndices(std::move(frameIndices)),
          TextureIds(std::move(textureIds)),
          PaletteIds(std::move(paletteIds)),
          Animations(FreezeMap(animations)),
          UnusedA(raw.UnusedA)
    {
        assert(Count == static_cast<std::int32_t>(Animations->size()));
    }

    std::shared_ptr<TextureAnimationGroup> TextureAnimationGroup::Empty()
    {
        return std::shared_ptr<TextureAnimationGroup>(new TextureAnimationGroup());
    }

    MaterialAnimationGroup::MaterialAnimationGroup()
        : Colors(std::make_shared<const std::vector<float>>()),
          Animations(EmptyFrozenMap<MaterialAnimationDictionary>())
    {
    }

    MaterialAnimationGroup::MaterialAnimationGroup(
        RawMaterialAnimationGroup raw,
        std::shared_ptr<const std::vector<float>> colors,
        std::shared_ptr<const MaterialAnimationDictionary> animations)
        : FrameCount(ManagedInt32(raw.FrameCount)),
          CurrentFrame(raw.AnimationFrame),
          UnusedFrame(raw.Unused12),
          Count(ManagedInt32(raw.AnimationCount)),
          Colors(std::move(colors)),
          Animations(FreezeMap(animations))
    {
        assert(Count == static_cast<std::int32_t>(Animations->size()));
    }

    std::shared_ptr<MaterialAnimationGroup> MaterialAnimationGroup::Empty()
    {
        return std::shared_ptr<MaterialAnimationGroup>(new MaterialAnimationGroup());
    }

    FxFuncInfo::FxFuncInfo(
        std::uint32_t funcId,
        std::shared_ptr<const std::vector<std::int32_t>> parameters)
        : FuncId(funcId),
          Parameters(std::move(parameters))
    {
        assert(funcId > 0);
    }

    Effect::Effect(
        std::int32_t id,
        RawEffect raw,
        std::shared_ptr<const Effects::EffectFuncDictionary> funcs,
        std::shared_ptr<const std::vector<std::uint32_t>> list2,
        std::shared_ptr<const std::vector<std::shared_ptr<EffectElement>>> elements,
        std::string name)
        : Id(id),
          Name(ReplaceAll(GetFileNameWithoutExtension(name), "_PS", "")),
          Field0(raw.Field0),
          Funcs(FreezeMap(funcs)),
          List2(std::move(list2)),
          Elements(std::move(elements))
    {
    }

    struct EffectElement::Init
    {
        std::string Name{};
        std::string ModelName{};
        std::shared_ptr<const std::vector<std::shared_ptr<Particle>>> Particles{};
        Effects::EffElemFlags Flags{};
        Vector3 Acceleration{};
        std::uint32_t ChildEffectId = 0;
        float Lifespan = 0.0F;
        float DrainTime = 0.0F;
        float BufferTime = 0.0F;
        std::int32_t DrawType = 0;
        std::shared_ptr<const Effects::EffectActionDictionary> Actions{};
        std::shared_ptr<const Effects::EffectFuncDictionary> Funcs{};
    };

    EffectElement::Init EffectElement::BuildInit(
        RawEffectElement raw,
        std::shared_ptr<const std::vector<std::shared_ptr<Particle>>> particles,
        std::shared_ptr<const Effects::EffectFuncDictionary> funcs,
        std::shared_ptr<const Effects::EffectActionDictionary> actions)
    {
        Init init{};
        init.Name = MarshalString(raw.Name);
        init.ModelName = MarshalString(raw.ModelName);
        init.Flags = raw.Flags;
        init.Acceleration = raw.Acceleration.ToFloatVector();
        init.ChildEffectId = raw.ChildEffectId;
        init.Lifespan = raw.Lifespan.FloatValue();
        init.DrainTime = raw.DrainTime.FloatValue();
        init.BufferTime = raw.BufferTime.FloatValue();
        init.DrawType = raw.DrawType;
        init.Particles = std::move(particles);
        init.Funcs = FreezeMap(funcs);
        init.Actions = FreezeMap(actions);
        return init;
    }

    EffectElement::EffectElement(
        RawEffectElement raw,
        std::shared_ptr<const std::vector<std::shared_ptr<Particle>>> particles,
        std::shared_ptr<const Effects::EffectFuncDictionary> funcs,
        std::shared_ptr<const Effects::EffectActionDictionary> actions)
        : EffectElement(BuildInit(
            raw,
            std::move(particles),
            std::move(funcs),
            std::move(actions)))
    {
    }

    EffectElement::EffectElement(Init init)
        : Name(std::move(init.Name)),
          ModelName(std::move(init.ModelName)),
          Particles(std::move(init.Particles)),
          Flags(init.Flags),
          Acceleration(init.Acceleration),
          ChildEffectId(init.ChildEffectId),
          Lifespan(init.Lifespan),
          DrainTime(init.DrainTime),
          BufferTime(init.BufferTime),
          DrawType(init.DrawType),
          Actions(std::move(init.Actions)),
          Funcs(std::move(init.Funcs))
    {
    }

    Particle::Particle(
        std::string name,
        std::shared_ptr<MphRead::Model> model,
        std::shared_ptr<MphRead::Node> node,
        std::int32_t materialId)
        : Name(std::move(name)),
          Model(std::move(model)),
          Node(std::move(node)),
          MaterialId(materialId)
    {
    }

    StringTableEntry::StringTableEntry(
        RawStringTableEntry raw,
        char16_t prefix,
        std::string value1,
        std::string value2)
        : Id(ReverseMarshalString(raw.Id)),
          Prefix(prefix),
          Value1(value1),
          Value2(value2),
          Speed(raw.Speed),
          Category(static_cast<char16_t>(raw.Category)),
          String1(Text::Strings::ReplaceNonAscii(value1)),
          String2(Text::Strings::ReplaceNonAscii(value2))
    {
    }

    StringTableEntry::StringTableEntry(
        std::string id,
        char16_t prefix,
        std::string value1,
        std::string value2,
        std::uint8_t speed,
        char16_t category)
        : Id(std::move(id)),
          Prefix(prefix),
          Value1(value1),
          Value2(value2),
          Speed(speed),
          Category(category),
          String1(Text::Strings::ReplaceNonAscii(value1)),
          String2(Text::Strings::ReplaceNonAscii(value2))
    {
    }

    Entity::Entity(
        EntityEntry entry,
        EntityType type,
        std::int16_t entityId,
        EntityDataHeader header)
        : NodeName(MarshalManagedString(entry.NodeName)),
          LayerMask(entry.LayerMask),
          Length(entry.Length),
          Type(ValidateEntityType(type)),
          EntityId(entityId),
          FirstHunt(false),
          Position(header.Position.ToFloatVector()),
          UpVector(header.UpVector.ToFloatVector()),
          FacingVector(header.FacingVector.ToFloatVector())
    {
    }

    Entity::Entity(
        FhEntityEntry entry,
        EntityType type,
        std::int16_t entityId,
        EntityDataHeader header)
        : NodeName(MarshalManagedString(entry.NodeName)),
          LayerMask(0),
          Length(0),
          Type(ValidateEntityType(type)),
          EntityId(entityId),
          FirstHunt(true),
          Position(header.Position.ToFloatVector()),
          UpVector(header.UpVector.ToFloatVector()),
          FacingVector(header.FacingVector.ToFloatVector())
    {
    }

    std::int16_t Entity::GetParentId()
    {
        return -1;
    }

    std::int16_t Entity::GetChildId()
    {
        return -1;
    }

    CollisionVolume::CollisionVolume() noexcept
    {
        std::memset(this, 0, sizeof(*this));
    }

    CollisionVolume::CollisionVolume(RawCollisionVolume raw)
    {
        std::memset(this, 0, sizeof(*this));
        Type = raw.Type;
        if (Type == VolumeType::Box)
        {
            BoxVector1 = raw.BoxVector1.ToFloatVector();
            BoxVector2 = raw.BoxVector2.ToFloatVector();
            BoxVector3 = raw.BoxVector3.ToFloatVector();
            BoxPosition = raw.BoxPosition.ToFloatVector();
            BoxDot1 = raw.BoxDot1.FloatValue();
            BoxDot2 = raw.BoxDot2.FloatValue();
            BoxDot3 = raw.BoxDot3.FloatValue();
        }
        else if (Type == VolumeType::Cylinder)
        {
            CylinderVector = raw.CylinderVector.ToFloatVector();
            CylinderPosition = raw.CylinderPosition.ToFloatVector();
            CylinderRadius = raw.CylinderRadius.FloatValue();
            CylinderDot = raw.CylinderDot.FloatValue();
        }
        else if (Type == VolumeType::Sphere)
        {
            SpherePosition = raw.SpherePosition.ToFloatVector();
            SphereRadius = raw.SphereRadius.FloatValue();
        }
        else
        {
            throw ProgramException(
                "Invalid volume type " + VolumeTypeString(raw.Type) + ".");
        }
    }

    CollisionVolume::CollisionVolume(FhRawCollisionVolume raw)
    {
        std::memset(this, 0, sizeof(*this));
        if (raw.Type == FhVolumeType::Box)
        {
            Type = VolumeType::Box;
            BoxVector1 = raw.BoxVector1.ToFloatVector();
            BoxVector2 = raw.BoxVector2.ToFloatVector();
            BoxVector3 = raw.BoxVector3.ToFloatVector();
            BoxPosition = raw.BoxPosition.ToFloatVector();
            BoxDot1 = raw.BoxDot1.FloatValue();
            BoxDot2 = raw.BoxDot2.FloatValue();
            BoxDot3 = raw.BoxDot3.FloatValue();
        }
        else if (raw.Type == FhVolumeType::Cylinder)
        {
            Type = VolumeType::Cylinder;
            CylinderVector = raw.CylinderVector.ToFloatVector();
            CylinderPosition = raw.CylinderPosition.ToFloatVector();
            CylinderRadius = raw.CylinderRadius.FloatValue();
            CylinderDot = raw.CylinderDot.FloatValue();
        }
        else if (raw.Type == FhVolumeType::Sphere)
        {
            Type = VolumeType::Sphere;
            SpherePosition = raw.SpherePosition.ToFloatVector();
            SphereRadius = raw.SphereRadius.FloatValue();
        }
        else
        {
            throw ProgramException(
                "Invalid volume type " + FhVolumeTypeString(raw.Type) + ".");
        }
    }

    CollisionVolume::CollisionVolume(
        Vector3 vec1,
        Vector3 vec2,
        Vector3 vec3,
        Vector3 pos,
        float dot1,
        float dot2,
        float dot3) noexcept
    {
        std::memset(this, 0, sizeof(*this));
        Type = VolumeType::Box;
        BoxVector1 = vec1;
        BoxVector2 = vec2;
        BoxVector3 = vec3;
        BoxPosition = pos;
        BoxDot1 = dot1;
        BoxDot2 = dot2;
        BoxDot3 = dot3;
    }

    CollisionVolume::CollisionVolume(
        Vector3 vec,
        Vector3 pos,
        float rad,
        float dot) noexcept
    {
        std::memset(this, 0, sizeof(*this));
        Type = VolumeType::Cylinder;
        CylinderVector = vec;
        CylinderPosition = pos;
        CylinderRadius = rad;
        CylinderDot = dot;
    }

    CollisionVolume::CollisionVolume(Vector3 pos, float rad) noexcept
    {
        std::memset(this, 0, sizeof(*this));
        Type = VolumeType::Sphere;
        SpherePosition = pos;
        SphereRadius = rad;
    }

    CollisionVolume::CollisionVolume(const CollisionVolume& other) noexcept
    {
        std::memcpy(this, std::addressof(other), sizeof(*this));
    }

    CollisionVolume& CollisionVolume::operator=(
        const CollisionVolume& other) noexcept
    {
        if (this != std::addressof(other))
        {
            std::memcpy(this, std::addressof(other), sizeof(*this));
        }
        return *this;
    }

    CollisionVolume CollisionVolume::Transform(
        CollisionVolume volume, Matrix4 transform)
    {
        if (volume.Type == VolumeType::Box)
        {
            return CollisionVolume(
                Matrix::Vec3MultMtx3(volume.BoxVector1, transform),
                Matrix::Vec3MultMtx3(volume.BoxVector2, transform),
                Matrix::Vec3MultMtx3(volume.BoxVector3, transform),
                Matrix::Vec3MultMtx4(volume.BoxPosition, transform),
                volume.BoxDot1,
                volume.BoxDot2,
                volume.BoxDot3);
        }
        if (volume.Type == VolumeType::Cylinder)
        {
            return CollisionVolume(
                Matrix::Vec3MultMtx3(volume.CylinderVector, transform),
                Matrix::Vec3MultMtx4(volume.CylinderPosition, transform),
                volume.CylinderRadius,
                volume.CylinderDot);
        }
        if (volume.Type == VolumeType::Sphere)
        {
            return CollisionVolume(
                Matrix::Vec3MultMtx4(volume.SpherePosition, transform),
                volume.SphereRadius);
        }
        throw ProgramException(
            "Invalid volume type " + VolumeTypeString(volume.Type) + ".");
    }

    CollisionVolume CollisionVolume::Transform(
        RawCollisionVolume volume, Matrix4 transform)
    {
        return Transform(CollisionVolume(volume), transform);
    }

    CollisionVolume CollisionVolume::Transform(
        FhRawCollisionVolume volume, Matrix4 transform)
    {
        return Transform(CollisionVolume(volume), transform);
    }

    CollisionVolume CollisionVolume::Move(
        CollisionVolume volume, Vector3 position)
    {
        if (volume.Type == VolumeType::Box)
        {
            return CollisionVolume(
                volume.BoxVector1,
                volume.BoxVector2,
                volume.BoxVector3,
                volume.BoxPosition + position,
                volume.BoxDot1,
                volume.BoxDot2,
                volume.BoxDot3);
        }
        if (volume.Type == VolumeType::Cylinder)
        {
            return CollisionVolume(
                volume.CylinderVector,
                volume.CylinderPosition + position,
                volume.CylinderRadius,
                volume.CylinderDot);
        }
        if (volume.Type == VolumeType::Sphere)
        {
            return CollisionVolume(
                volume.SpherePosition + position,
                volume.SphereRadius);
        }
        throw ProgramException(
            "Invalid volume type " + VolumeTypeString(volume.Type) + ".");
    }

    CollisionVolume CollisionVolume::Move(
        RawCollisionVolume volume, Vector3 position)
    {
        return Move(CollisionVolume(volume), position);
    }

    CollisionVolume CollisionVolume::Move(
        FhRawCollisionVolume volume, Vector3 position)
    {
        return Move(CollisionVolume(volume), position);
    }

    bool CollisionVolume::TestPoint(Vector3 point) const
    {
        if (Type == VolumeType::Box)
        {
            const Vector3 difference = point - BoxPosition;
            const float dot1 = Vector3::Dot(BoxVector1, difference);
            if (dot1 >= 0.0F && dot1 <= BoxDot1)
            {
                const float dot2 = Vector3::Dot(BoxVector2, difference);
                if (dot2 >= 0.0F && dot2 <= BoxDot2)
                {
                    const float dot3 = Vector3::Dot(BoxVector3, difference);
                    return dot3 >= 0.0F && dot3 <= BoxDot3;
                }
            }
        }
        else if (Type == VolumeType::Cylinder)
        {
            const Vector3 bottom = CylinderPosition;
            const Vector3 top = bottom + Multiply(CylinderVector, CylinderDot);
            const Vector3 axis = top - bottom;
            if (Vector3::Dot(point - bottom, axis) >= 0.0F)
            {
                if (Vector3::Dot(point - top, axis) <= 0.0F)
                {
                    return Length(Vector3::Cross(point - bottom, axis))
                        / Length(axis) <= CylinderRadius;
                }
            }
        }
        else if (Type == VolumeType::Sphere)
        {
            return Vector3::Distance(SpherePosition, point) <= SphereRadius;
        }
        return false;
    }

    Vector3 CollisionVolume::GetCenter() const
    {
        if (Type == VolumeType::Box)
        {
            return BoxPosition
                + Multiply(BoxDot1 / 2.0F, BoxVector1)
                + Multiply(BoxDot2 / 2.0F, BoxVector2)
                + Multiply(BoxDot3 / 2.0F, BoxVector3);
        }
        if (Type == VolumeType::Cylinder)
        {
            return Multiply(CylinderDot / 2.0F, CylinderVector)
                + CylinderPosition;
        }
        if (Type == VolumeType::Sphere)
        {
            return SpherePosition;
        }
        return Vector3::Zero;
    }

    DisplayVolume::DisplayVolume(
        RawCollisionVolume volume, Matrix4 transform)
        : Volume(CollisionVolume::Transform(volume, transform))
    {
    }

    DisplayVolume::DisplayVolume(
        FhRawCollisionVolume volume, Matrix4 transform)
        : Volume(CollisionVolume::Transform(volume, transform))
    {
    }

    DisplayVolume::DisplayVolume(
        CollisionVolume volume, Matrix4 transform)
        : Volume(CollisionVolume::Transform(volume, transform))
    {
    }

    DisplayVolume::DisplayVolume(
        RawCollisionVolume volume, Vector3 position)
        : Volume(CollisionVolume::Move(volume, position))
    {
    }

    DisplayVolume::DisplayVolume(
        FhRawCollisionVolume volume, Vector3 position)
        : Volume(CollisionVolume::Move(volume, position))
    {
    }

    DisplayVolume::DisplayVolume(
        CollisionVolume volume, Vector3 position)
        : Volume(CollisionVolume::Move(volume, position))
    {
    }

    MorphCameraDisplay::MorphCameraDisplay(
        std::shared_ptr<EntityOf<MorphCameraEntityData>> entity,
        Vector3 position)
        : DisplayVolume(EntityDataOf(entity).Volume, position)
    {
        SetColor1(Vector3(1.0F, 1.0F, 0.0F));
    }

    MorphCameraDisplay::MorphCameraDisplay(
        std::shared_ptr<EntityOf<FhMorphCameraEntityData>> entity,
        Vector3 position)
        : DisplayVolume(EntityDataOf(entity).Volume, position)
    {
        SetColor1(Vector3(1.0F, 1.0F, 0.0F));
    }

    std::optional<Vector3> MorphCameraDisplay::GetColor(
        std::int32_t index) const
    {
        if (index == 7)
        {
            return Color1;
        }
        return std::nullopt;
    }

    JumpPadDisplay::JumpPadDisplay(
        std::shared_ptr<EntityOf<JumpPadEntityData>> entity,
        Vector3 position)
        : DisplayVolume(EntityDataOf(entity).Volume, position),
          Vector(EntityDataOf(entity).BeamVector.ToFloatVector()),
          Speed(EntityDataOf(entity).Speed.FloatValue()),
          Active(EntityDataOf(entity).Active != 0)
    {
        SetColor1(Vector3(0.0F, 1.0F, 0.0F));
    }

    JumpPadDisplay::JumpPadDisplay(
        std::shared_ptr<EntityOf<FhJumpPadEntityData>> entity,
        Vector3 position)
        : DisplayVolume(EntityDataOf(entity).ActiveVolume(), position),
          Vector(EntityDataOf(entity).BeamVector.ToFloatVector()),
          Speed(EntityDataOf(entity).Speed.FloatValue()),
          Active(true)
    {
        SetColor1(Vector3(0.0F, 1.0F, 0.0F));
    }

    std::optional<Vector3> JumpPadDisplay::GetColor(
        std::int32_t index) const
    {
        if (index == 8)
        {
            return Color1;
        }
        return std::nullopt;
    }

    ObjectDisplay::ObjectDisplay(
        std::shared_ptr<EntityOf<ObjectEntityData>> entity,
        Matrix4 transform)
        : DisplayVolume(EntityDataOf(entity).Volume, transform)
    {
        SetColor1(Vector3(1.0F, 0.0F, 0.0F));
    }

    std::optional<Vector3> ObjectDisplay::GetColor(
        std::int32_t index) const
    {
        if (index == 9)
        {
            return Color1;
        }
        return std::nullopt;
    }

    FlagBaseDisplay::FlagBaseDisplay(
        std::shared_ptr<EntityOf<FlagBaseEntityData>> entity,
        Vector3 position)
        : DisplayVolume(EntityDataOf(entity).Volume, position)
    {
        SetColor1(Vector3(1.0F, 1.0F, 1.0F));
    }

    std::optional<Vector3> FlagBaseDisplay::GetColor(
        std::int32_t index) const
    {
        if (index == 10)
        {
            return Color1;
        }
        return std::nullopt;
    }

    NodeDefenseDisplay::NodeDefenseDisplay(
        std::shared_ptr<EntityOf<NodeDefenseEntityData>> entity,
        Vector3 position)
        : DisplayVolume(EntityDataOf(entity).Volume, position)
    {
        SetColor1(Vector3(1.0F, 1.0F, 1.0F));
    }

    std::optional<Vector3> NodeDefenseDisplay::GetColor(
        std::int32_t index) const
    {
        if (index == 11)
        {
            return Color1;
        }
        return std::nullopt;
    }

    TriggerVolumeDisplay::TriggerVolumeDisplay(
        std::shared_ptr<EntityOf<TriggerVolumeEntityData>> entity,
        Vector3 position)
        : DisplayVolume(EntityDataOf(entity).Volume, position)
    {
        SetColor1(Metadata::GetEventColor(EntityDataOf(entity).ParentMessage));
        SetColor2(Metadata::GetEventColor(EntityDataOf(entity).ChildMessage));
    }

    TriggerVolumeDisplay::TriggerVolumeDisplay(
        std::shared_ptr<EntityOf<FhTriggerVolumeEntityData>> entity,
        Vector3 position)
        : DisplayVolume(EntityDataOf(entity).ActiveVolume(), position)
    {
        SetColor1(Metadata::GetEventColor(EntityDataOf(entity).ParentMessage));
        SetColor2(Metadata::GetEventColor(EntityDataOf(entity).ChildMessage));
    }

    std::optional<Vector3> TriggerVolumeDisplay::GetColor(
        std::int32_t index) const
    {
        if (index == 3)
        {
            return Color1;
        }
        if (index == 4)
        {
            return Color2;
        }
        return std::nullopt;
    }

    AreaVolumeDisplay::AreaVolumeDisplay(
        std::shared_ptr<EntityOf<AreaVolumeEntityData>> entity,
        Vector3 position)
        : DisplayVolume(EntityDataOf(entity).Volume, position)
    {
        SetColor1(Metadata::GetEventColor(EntityDataOf(entity).InsideMessage));
        SetColor2(Metadata::GetEventColor(EntityDataOf(entity).ExitMessage));
    }

    AreaVolumeDisplay::AreaVolumeDisplay(
        std::shared_ptr<EntityOf<FhAreaVolumeEntityData>> entity,
        Vector3 position)
        : DisplayVolume(EntityDataOf(entity).ActiveVolume(), position)
    {
        SetColor1(Metadata::GetEventColor(EntityDataOf(entity).InsideMessage));
        SetColor2(Metadata::GetEventColor(EntityDataOf(entity).ExitMessage));
    }

    std::optional<Vector3> AreaVolumeDisplay::GetColor(
        std::int32_t index) const
    {
        if (index == 5)
        {
            return Color1;
        }
        if (index == 6)
        {
            return Color2;
        }
        return std::nullopt;
    }

    LightSource::LightSource(
        std::shared_ptr<EntityOf<LightSourceEntityData>> entity,
        Vector3 position)
        : DisplayVolume(EntityDataOf(entity).Volume, position),
          Light1Enabled(EntityDataOf(entity).Light1Enabled != 0),
          Light1Vector(EntityDataOf(entity).Light1Vector.ToFloatVector()),
          Light2Enabled(EntityDataOf(entity).Light2Enabled != 0),
          Light2Vector(EntityDataOf(entity).Light2Vector.ToFloatVector())
    {
        SetColor1(EntityDataOf(entity).Light1Color.AsVector3());
        SetColor2(EntityDataOf(entity).Light2Color.AsVector3());
    }

    std::optional<Vector3> LightSource::GetColor(
        std::int32_t index) const
    {
        if (index == 1)
        {
            return Light1Enabled ? Color1 : Vector3::Zero;
        }
        if (index == 2)
        {
            return Light2Enabled ? Color2 : Vector3::Zero;
        }
        return std::nullopt;
    }

    CameraSequenceKeyframe::CameraSequenceKeyframe(
        RawCameraSequenceKeyframe raw)
        : Position(raw.Position.ToFloatVector()),
          ToTarget(raw.ToTarget.ToFloatVector()),
          Roll(raw.Roll.FloatValue()),
          Fov(raw.Fov.FloatValue()),
          MoveTime(raw.MoveTime.FloatValue()),
          HoldTime(raw.HoldTime.FloatValue()),
          FadeInTime(raw.FadeInTime.FloatValue()),
          FadeOutTime(raw.FadeOutTime.FloatValue()),
          FadeInType(raw.FadeInType),
          FadeOutType(raw.FadeOutType),
          PrevFrameInfluence(raw.PrevFrameInfluence),
          AfterFrameInfluence(raw.AfterFrameInfluence),
          UseEntityTransform(raw.UseEntityTransform != 0),
          PosEntityType(raw.PosEntityType),
          PosEntityId(raw.PosEntityId),
          TargetEntityType(raw.TargetEntityType),
          TargetEntityId(raw.TargetEntityId),
          MessageTargetType(raw.MessageTargetType),
          MessageTargetId(raw.MessageTargetId),
          MessageId(raw.MessageId),
          MessageParam(raw.MessageParam),
          Easing(raw.Easing.FloatValue()),
          NodeName(MarshalString(raw.NodeName))
    {
    }

    const std::shared_ptr<const std::unordered_map<InstructionCode, std::int32_t>>
        RenderInstruction::_arityMap = Frozen::Create<InstructionCode, std::int32_t>({
            {InstructionCode::NOP, 0},
            {InstructionCode::MTX_RESTORE, 1},
            {InstructionCode::COLOR, 1},
            {InstructionCode::NORMAL, 1},
            {InstructionCode::TEXCOORD, 1},
            {InstructionCode::VTX_16, 2},
            {InstructionCode::VTX_10, 1},
            {InstructionCode::VTX_XY, 1},
            {InstructionCode::VTX_XZ, 1},
            {InstructionCode::VTX_YZ, 1},
            {InstructionCode::VTX_DIFF, 1},
            {InstructionCode::DIF_AMB, 1},
            {InstructionCode::BEGIN_VTXS, 1},
            {InstructionCode::END_VTXS, 0}});

    RenderInstruction::RenderInstruction(
        InstructionCode code,
        std::shared_ptr<std::vector<std::uint32_t>> arguments)
        : Code(code),
          Arguments(CopyRenderArguments(code, arguments))
    {
    }

    std::int32_t RenderInstruction::GetArity(InstructionCode code)
    {
        if (!IsDefinedInstructionCode(code))
        {
            throw ProgramException(
                "Invalid code arity " + InstructionCodeString(code));
        }
        assert(_arityMap->find(code) != _arityMap->end());
        return _arityMap->at(code);
    }

    std::string Paths::MphKey = Ver::AMHE0;
    std::string Paths::FhKey = Ver::AMFE0;
    std::unordered_map<std::string, std::string> Paths::_allPaths{};

    const std::string& Paths::FileSystem()
    {
        return _allPaths.at(MphKey);
    }

    const std::string& Paths::FhFileSystem()
    {
        return _allPaths.at(FhKey);
    }

    const std::string& Paths::Export()
    {
        return _allPaths.at("Export");
    }

    bool Paths::IsMphAmericas() noexcept
    {
        return MphKey == Ver::AMHE0 || MphKey == Ver::AMHE1;
    }

    bool Paths::IsMphEurope() noexcept
    {
        return MphKey == Ver::AMHP0 || MphKey == Ver::AMHP1;
    }

    bool Paths::IsMphJapan() noexcept
    {
        return MphKey == Ver::AMHJ0 || MphKey == Ver::AMHJ1;
    }

    bool Paths::IsMphKorea() noexcept
    {
        return MphKey == Ver::AMHK0;
    }

    const std::unordered_map<std::string, std::string>& Paths::AllPaths()
    {
        if (_allPaths.empty())
        {
            UpdatePaths();
        }
        return _allPaths;
    }

    void Paths::SetPath(std::string key, std::string path)
    {
        if (_allPaths.empty())
        {
            UpdatePaths();
        }
        _allPaths[std::move(key)] = std::move(path);
    }

    void Paths::UpdatePaths()
    {
        _allPaths.clear();
        _allPaths.emplace(Ver::AMFE0, "");
        _allPaths.emplace(Ver::AMFP0, "");
        _allPaths.emplace(Ver::A76E0, "");
        _allPaths.emplace(Ver::AMHE0, "");
        _allPaths.emplace(Ver::AMHE1, "");
        _allPaths.emplace(Ver::AMHJ0, "");
        _allPaths.emplace(Ver::AMHJ1, "");
        _allPaths.emplace(Ver::AMHP0, "");
        _allPaths.emplace(Ver::AMHP1, "");
        _allPaths.emplace(Ver::AMHK0, "");
        _allPaths.emplace("Export", "");

        if (FileExists("paths.txt"))
        {
            const std::vector<std::string> lines = ReadAllLines("paths.txt");
            for (const std::string& rawLine : lines)
            {
                const std::string line = TrimDotNetWhitespace(rawLine);
                const std::vector<std::string> split = SplitEquals(line);
                const std::string key
                    = TrimDotNetWhitespace(split.at(0));
                if (split.size() == 2
                    && _allPaths.find(key) != _allPaths.end())
                {
                    _allPaths[key]
                        = TrimDotNetWhitespace(split[1]);
                }
            }
        }
    }

    void Paths::ChooseMphPath()
    {
        const auto choose = [](const std::string& key)
        {
            if (Paths::_allPaths.at(key) != "")
            {
                Paths::MphKey = key;
                return true;
            }
            return false;
        };

        if (choose(Ver::AMHE1)) return;
        if (choose(Ver::AMHP1)) return;
        if (choose(Ver::AMHJ1)) return;
        if (choose(Ver::AMHK0)) return;
        if (choose(Ver::AMHE0)) return;
        if (choose(Ver::AMHP0)) return;
        if (choose(Ver::AMHJ0)) return;
        if (choose(Ver::A76E0)) return;
        MphKey = Ver::AMHE0;
    }

    void Paths::ChooseFhPath()
    {
        const auto choose = [](const std::string& key)
        {
            if (Paths::_allPaths.at(key) != "")
            {
                Paths::FhKey = key;
                return true;
            }
            return false;
        };

        if (choose(Ver::AMFE0)) return;
        if (choose(Ver::AMFP0)) return;
        FhKey = Ver::AMFE0;
    }

    std::string Paths::Replace(std::string path)
    {
        const char separator = std::filesystem::path::preferred_separator;
        if (separator == '\\')
        {
            std::replace(path.begin(), path.end(), '/', '\\');
        }
        else if (separator == '/')
        {
            std::replace(path.begin(), path.end(), '\\', '/');
        }
        return path;
    }

    std::string Paths::Combine(std::string path1, std::string path2)
    {
        return CombinePaths({Replace(std::move(path1)), Replace(std::move(path2))});
    }

    std::string Paths::Combine(
        std::string path1, std::string path2, std::string path3)
    {
        return CombinePaths({
            Replace(std::move(path1)),
            Replace(std::move(path2)),
            Replace(std::move(path3))});
    }

    std::string Paths::Combine(
        std::string path1,
        std::string path2,
        std::string path3,
        std::string path4)
    {
        return CombinePaths({
            Replace(std::move(path1)),
            Replace(std::move(path2)),
            Replace(std::move(path3)),
            Replace(std::move(path4))});
    }

    std::string Paths::Combine(std::vector<std::string>& paths)
    {
        for (std::string& path : paths)
        {
            path = Replace(std::move(path));
        }
        return CombinePaths(paths);
    }

    [[noreturn]] void CollectionExtensions::ThrowSpanRange()
    {
        throw std::out_of_range(
            "Specified argument was out of the range of valid values.");
    }

    [[noreturn]] void CollectionExtensions::ThrowSpanIndex()
    {
        throw std::out_of_range(
            "Index was outside the bounds of the span.");
    }
}
