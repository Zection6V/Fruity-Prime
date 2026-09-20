#include "TestPrint.hpp"

#include "../Formats/Formats.hpp"
#include "../Formats/Model.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace
{
    [[nodiscard]] constexpr std::uint32_t ManagedUInt32(std::int32_t value) noexcept
    {
        return std::bit_cast<std::uint32_t>(value);
    }

    [[nodiscard]] constexpr std::int32_t ManagedInt32(std::uint32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(value);
    }

    [[nodiscard]] constexpr std::int32_t AddInt32(
        std::int32_t left, std::int32_t right) noexcept
    {
        return ManagedInt32(ManagedUInt32(left) + ManagedUInt32(right));
    }

    [[nodiscard]] constexpr std::int32_t MultiplyInt32(
        std::int32_t left, std::int32_t right) noexcept
    {
        return ManagedInt32(ManagedUInt32(left) * ManagedUInt32(right));
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
    [[nodiscard]] const T& ManagedAt(const std::vector<T>& values, std::size_t index)
    {
        if (index >= values.size())
        {
            throw System::IndexOutOfRangeException();
        }
        return values[index];
    }

    [[nodiscard]] const std::string& RequireString(const std::optional<std::string>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    [[nodiscard]] const std::string& InterpolationString(
        const std::optional<std::string>& value) noexcept
    {
        static const std::string empty;
        return value ? *value : empty;
    }

    struct Utf8CodePoint final
    {
        char32_t Value;
        std::size_t Length;
    };

    [[nodiscard]] Utf8CodePoint DecodeUtf8(
        std::string_view value, std::size_t offset) noexcept
    {
        const auto first = static_cast<unsigned char>(value[offset]);
        if (first < 0x80U)
        {
            return {first, 1};
        }

        auto continuation = [&](std::size_t index) noexcept -> int
        {
            if (index >= value.size())
            {
                return -1;
            }
            const auto byte = static_cast<unsigned char>(value[index]);
            return (byte & 0xC0U) == 0x80U ? static_cast<int>(byte & 0x3FU) : -1;
        };

        if ((first & 0xE0U) == 0xC0U)
        {
            const int c1 = continuation(offset + 1);
            if (c1 >= 0)
            {
                const char32_t cp = static_cast<char32_t>(((first & 0x1FU) << 6) | c1);
                if (cp >= 0x80)
                {
                    return {cp, 2};
                }
            }
        }
        else if ((first & 0xF0U) == 0xE0U)
        {
            const int c1 = continuation(offset + 1);
            const int c2 = continuation(offset + 2);
            if (c1 >= 0 && c2 >= 0)
            {
                const char32_t cp = static_cast<char32_t>(
                    ((first & 0x0FU) << 12) | (c1 << 6) | c2);
                if (cp >= 0x800 && !(cp >= 0xD800 && cp <= 0xDFFF))
                {
                    return {cp, 3};
                }
            }
        }
        else if ((first & 0xF8U) == 0xF0U)
        {
            const int c1 = continuation(offset + 1);
            const int c2 = continuation(offset + 2);
            const int c3 = continuation(offset + 3);
            if (c1 >= 0 && c2 >= 0 && c3 >= 0)
            {
                const char32_t cp = static_cast<char32_t>(
                    ((first & 0x07U) << 18) | (c1 << 12) | (c2 << 6) | c3);
                if (cp >= 0x10000 && cp <= 0x10FFFF)
                {
                    return {cp, 4};
                }
            }
        }
        return {first, 1};
    }

    [[nodiscard]] constexpr bool ManagedCharIsWhiteSpace(char32_t value) noexcept
    {
        return value == 0x0009 || value == 0x000A || value == 0x000B
            || value == 0x000C || value == 0x000D || value == 0x0020
            || value == 0x0085 || value == 0x00A0 || value == 0x1680
            || (value >= 0x2000 && value <= 0x200A)
            || value == 0x2028 || value == 0x2029 || value == 0x202F
            || value == 0x205F || value == 0x3000;
    }

    [[nodiscard]] bool IsNullOrWhiteSpace(const std::optional<std::string>& value) noexcept
    {
        if (!value || value->empty())
        {
            return true;
        }
        std::size_t offset = 0;
        while (offset < value->size())
        {
            const Utf8CodePoint cp = DecodeUtf8(*value, offset);
            if (!ManagedCharIsWhiteSpace(cp.Value))
            {
                return false;
            }
            offset += cp.Length;
        }
        return true;
    }

    [[nodiscard]] std::string TrimManaged(std::string_view value)
    {
        struct Span final
        {
            std::size_t Offset;
            std::size_t Length;
            char32_t Value;
        };

        std::vector<Span> spans;
        spans.reserve(value.size());
        for (std::size_t offset = 0; offset < value.size();)
        {
            const Utf8CodePoint cp = DecodeUtf8(value, offset);
            spans.push_back({offset, cp.Length, cp.Value});
            offset += cp.Length;
        }

        std::size_t first = 0;
        while (first < spans.size() && ManagedCharIsWhiteSpace(spans[first].Value))
        {
            first++;
        }
        if (first == spans.size())
        {
            return {};
        }

        std::size_t last = spans.size();
        while (last > first && ManagedCharIsWhiteSpace(spans[last - 1].Value))
        {
            last--;
        }

        const std::size_t start = spans[first].Offset;
        const std::size_t end = spans[last - 1].Offset + spans[last - 1].Length;
        return std::string(value.substr(start, end - start));
    }

    [[nodiscard]] std::string ReplaceAll(
        std::string value, std::string_view oldValue, std::string_view newValue)
    {
        if (oldValue.empty())
        {
            throw std::invalid_argument("String cannot be of zero length. (Parameter 'oldValue')");
        }
        std::size_t offset = 0;
        while ((offset = value.find(oldValue, offset)) != std::string::npos)
        {
            value.replace(offset, oldValue.size(), newValue);
            offset += newValue.size();
        }
        return value;
    }

    [[nodiscard]] std::vector<std::string> SplitChar(std::string_view value, char separator)
    {
        std::vector<std::string> result;
        std::size_t start = 0;
        while (true)
        {
            const std::size_t found = value.find(separator, start);
            if (found == std::string_view::npos)
            {
                result.emplace_back(value.substr(start));
                return result;
            }
            result.emplace_back(value.substr(start, found - start));
            start = found + 1;
        }
    }

    [[nodiscard]] std::vector<std::string> SplitString(
        std::string_view value, std::string_view separator)
    {
        std::vector<std::string> result;
        std::size_t start = 0;
        while (true)
        {
            const std::size_t found = value.find(separator, start);
            if (found == std::string_view::npos)
            {
                result.emplace_back(value.substr(start));
                return result;
            }
            result.emplace_back(value.substr(start, found - start));
            start = found + separator.size();
        }
    }

    [[nodiscard]] constexpr std::string_view EnvironmentNewLine() noexcept
    {
#ifdef _WIN32
        return "\r\n";
#else
        return "\n";
#endif
    }

    [[nodiscard]] std::int32_t ParseInt32(std::string_view value)
    {
        std::size_t first = 0;
        while (first < value.size())
        {
            const Utf8CodePoint cp = DecodeUtf8(value, first);
            if (!ManagedCharIsWhiteSpace(cp.Value))
            {
                break;
            }
            first += cp.Length;
        }

        bool negative = false;
        if (first < value.size() && (value[first] == '+' || value[first] == '-'))
        {
            negative = value[first] == '-';
            first++;
        }

        const std::size_t digitStart = first;
        std::uint64_t magnitude = 0;
        const std::uint64_t limit = negative ? 2147483648ULL : 2147483647ULL;
        while (first < value.size() && value[first] >= '0' && value[first] <= '9')
        {
            const unsigned digit = static_cast<unsigned>(value[first] - '0');
            if (magnitude > (limit - digit) / 10ULL)
            {
                throw System::OverflowException();
            }
            magnitude = magnitude * 10ULL + digit;
            first++;
        }
        if (first == digitStart)
        {
            throw System::FormatException();
        }

        while (first < value.size())
        {
            const Utf8CodePoint cp = DecodeUtf8(value, first);
            if (!ManagedCharIsWhiteSpace(cp.Value))
            {
                throw System::FormatException();
            }
            first += cp.Length;
        }

        if (negative)
        {
            if (magnitude == 2147483648ULL)
            {
                return std::numeric_limits<std::int32_t>::min();
            }
            return -static_cast<std::int32_t>(magnitude);
        }
        return static_cast<std::int32_t>(magnitude);
    }

    [[nodiscard]] std::string UpperFirstInvariant(std::string_view part)
    {
        if (part.empty())
        {
            throw System::IndexOutOfRangeException();
        }
        std::string result(part);
        const unsigned char first = static_cast<unsigned char>(result[0]);
        if (first >= static_cast<unsigned char>('a')
            && first <= static_cast<unsigned char>('z'))
        {
            result[0] = static_cast<char>(first - ('a' - 'A'));
        }
        return result;
    }

    [[nodiscard]] std::string FormatHexUInt32(std::uint32_t value, int minimumDigits)
    {
        std::ostringstream stream;
        stream << std::uppercase << std::hex << std::setfill('0')
               << std::setw(minimumDigits) << value;
        return stream.str();
    }

    [[nodiscard]] std::string FormatHexInt32(std::int32_t value, int minimumDigits)
    {
        if (value < 0)
        {
            minimumDigits = std::max(minimumDigits, 8);
        }
        return FormatHexUInt32(ManagedUInt32(value), minimumDigits);
    }

    [[nodiscard]] std::string FormatBinary(std::uint32_t value)
    {
        if (value == 0)
        {
            return "0";
        }
        std::string result;
        result.reserve(32);
        bool started = false;
        for (int bit = 31; bit >= 0; --bit)
        {
            const bool set = (value & (std::uint32_t{1} << bit)) != 0;
            if (set)
            {
                started = true;
            }
            if (started)
            {
                result.push_back(set ? '1' : '0');
            }
        }
        return result;
    }

    [[nodiscard]] std::string PolygonModeName(MphRead::PolygonMode value)
    {
        switch (value)
        {
        case MphRead::PolygonMode::Modulate:
            return "Modulate";
        case MphRead::PolygonMode::Decal:
            return "Decal";
        case MphRead::PolygonMode::Toon:
            return "Toon";
        case MphRead::PolygonMode::Shadow:
            return "Shadow";
        }
        return std::to_string(static_cast<std::uint32_t>(value));
    }

    [[nodiscard]] std::string CullingModeName(MphRead::CullingMode value)
    {
        switch (value)
        {
        case MphRead::CullingMode::Neither:
            return "Neither";
        case MphRead::CullingMode::Front:
            return "Front";
        case MphRead::CullingMode::Back:
            return "Back";
        }
        return std::to_string(static_cast<unsigned>(value));
    }

#if defined(DEBUG)
    void DebugWriteLine(const std::string& value)
    {
        std::clog << value << '\n';
    }

    [[noreturn]] void DebugAssertFailed() noexcept
    {
        std::abort();
    }

    void DebugAssert(bool condition) noexcept
    {
        if (!condition)
        {
            DebugAssertFailed();
        }
    }
#endif

#if defined(DEBUG)
#define MPH_TESTPRINT_DEBUG_WRITE_LINE(value) DebugWriteLine(value)
#define MPH_TESTPRINT_DEBUG_ASSERT(condition) DebugAssert(condition)
#else
#define MPH_TESTPRINT_DEBUG_WRITE_LINE(value) do { } while (false)
#define MPH_TESTPRINT_DEBUG_ASSERT(condition) do { } while (false)
#endif

    void DebugBreak()
    {
#if defined(_MSC_VER)
        __debugbreak();
#else
        std::raise(SIGTRAP);
#endif
    }

    [[nodiscard]] float ManagedMax(float left, float right) noexcept
    {
        if (std::isnan(left))
        {
            return left;
        }
        if (std::isnan(right))
        {
            return right;
        }
        return left > right ? left : right;
    }
}

namespace System
{
    const Type::PropertyList& Type::GetProperties() const
    {
        if (!Properties)
        {
            throw NullReferenceException();
        }
        return *Properties;
    }
}

namespace MphRead::Testing
{
    void TestPrint::PrintStruct(
        const std::optional<std::string>& name, std::int32_t size)
    {
        MPH_TESTPRINT_DEBUG_ASSERT(size > 0 && size % 4 == 0);
        std::cout << "struct " << InterpolationString(name) << '\n';
        std::cout << "{\n";
        std::int32_t offset = 0;
        while (offset < size)
        {
            std::cout << "  int field_" << FormatHexInt32(offset, 1) << ";\n";
            offset = AddInt32(offset, 4);
        }
        std::cout << "}\n";
    }

    void TestPrint::GetPolygonAttrs(
        const std::shared_ptr<MphRead::Model>& model,
        std::int32_t polygonId)
    {
        Model& modelRef = Require(model);
        if (!modelRef.Materials)
        {
            throw System::NullReferenceException();
        }
        for (const std::shared_ptr<Material>& material : *modelRef.Materials)
        {
            GetPolygonAttrs(model, material, polygonId);
        }
    }

    void TestPrint::GetPolygonAttrs(
        const std::shared_ptr<MphRead::Model>& model,
        const std::shared_ptr<MphRead::Material>& material,
        std::int32_t polygonId)
    {
        MPH_TESTPRINT_DEBUG_ASSERT(polygonId >= 0);
        Model& modelRef = Require(model);
        Material& materialRef = Require(material);

        const std::uint32_t v19 = polygonId == 1 ? 0x4000U : 0U;
        const std::uint32_t v20 = v19 | 0x8000U;
        const std::uint32_t polygonModeBits =
            ManagedUInt32(MultiplyInt32(
                16,
                ManagedInt32(static_cast<std::uint32_t>(materialRef.PolygonMode))));
        const std::uint32_t cullingBits =
            static_cast<std::uint32_t>(materialRef.Culling) << 6U;
        const std::uint32_t polygonBits = ManagedUInt32(polygonId) << 24U;
        const std::uint32_t alphaBits =
            static_cast<std::uint32_t>(materialRef.Alpha) << 16U;
        const std::uint32_t attr = v20
            | static_cast<std::uint32_t>(materialRef.Lighting)
            | polygonModeBits | cullingBits | polygonBits | alphaBits;

        std::cout << modelRef.Name << " - " << materialRef.Name << '\n';
        std::cout << "light = " << static_cast<unsigned>(materialRef.Lighting)
                  << ", mode = "
                  << ManagedInt32(static_cast<std::uint32_t>(materialRef.PolygonMode))
                  << " (" << PolygonModeName(materialRef.PolygonMode) << "), cull = "
                  << static_cast<unsigned>(materialRef.Culling)
                  << " (" << CullingModeName(materialRef.Culling) << "), alpha = "
                  << static_cast<unsigned>(materialRef.Alpha)
                  << ", id = " << polygonId << '\n';
        DumpPolygonAttr(attr);
    }

    void TestPrint::DumpPolygonAttr(std::uint32_t attr)
    {
        std::cout << "0x" << FormatHexUInt32(attr, 2) << '\n';
        std::cout << FormatBinary(attr) << '\n';
        std::cout << "light1: " << (attr & 0x1U) << '\n';
        std::cout << "light2: " << ((attr >> 1U) & 0x1U) << '\n';
        std::cout << "light3: " << ((attr >> 2U) & 0x1U) << '\n';
        std::cout << "light4: " << ((attr >> 3U) & 0x1U) << '\n';
        std::cout << "mode: " << ((attr >> 4U) & 0x2U) << '\n';
        std::cout << "back: " << ((attr >> 6U) & 0x1U) << '\n';
        std::cout << "front: " << ((attr >> 7U) & 0x1U) << '\n';
        std::cout << "clear: " << ((attr >> 11U) & 0x1U) << '\n';
        std::cout << "far: " << ((attr >> 12U) & 0x1U) << '\n';
        std::cout << "1dot: " << ((attr >> 13U) & 0x1U) << '\n';
        std::cout << "depth: " << ((attr >> 14U) & 0x1U) << '\n';
        std::cout << "fog: " << ((attr >> 15U) & 0x1U) << '\n';
        std::cout << "alpha: " << ((attr >> 16U) & 0x1FU) << '\n';
        std::cout << "id: " << ((attr >> 24U) & 0x3FU) << '\n';
        std::cout << '\n';
    }

    OpenTK::Mathematics::Vector3 TestPrint::LightCalc(
        OpenTK::Mathematics::Vector3 light_vec,
        OpenTK::Mathematics::Vector3 light_col,
        OpenTK::Mathematics::Vector3 normal_vec,
        OpenTK::Mathematics::Vector3 dif_col,
        OpenTK::Mathematics::Vector3 amb_col,
        OpenTK::Mathematics::Vector3 spe_col)
    {
        using OpenTK::Mathematics::Vector3;

        const Vector3 sight_vec(0.0F, 0.0F, -1.0F);
        const float dif_factor = ManagedMax(
            0.0F, -Vector3::Dot(light_vec, normal_vec));
        const Vector3 half_vec(
            (light_vec.X + sight_vec.X) / 2.0F,
            (light_vec.Y + sight_vec.Y) / 2.0F,
            (light_vec.Z + sight_vec.Z) / 2.0F);
        float spe_factor = ManagedMax(
            0.0F,
            Vector3::Dot(
                Vector3(-half_vec.X, -half_vec.Y, -half_vec.Z), normal_vec));
        spe_factor *= spe_factor;

        const Vector3 spe_out(
            spe_col.X * light_col.X * spe_factor,
            spe_col.Y * light_col.Y * spe_factor,
            spe_col.Z * light_col.Z * spe_factor);
        const Vector3 dif_out(
            dif_col.X * light_col.X * dif_factor,
            dif_col.Y * light_col.Y * dif_factor,
            dif_col.Z * light_col.Z * dif_factor);
        const Vector3 amb_out(
            amb_col.X * light_col.X,
            amb_col.Y * light_col.Y,
            amb_col.Z * light_col.Z);

        return Vector3(
            (spe_out.X + dif_out.X) + amb_out.X,
            (spe_out.Y + dif_out.Y) + amb_out.Y,
            (spe_out.Z + dif_out.Z) + amb_out.Z);
    }

    void TestPrint::PrintEntityEditor(const std::shared_ptr<System::Type>& type)
    {
        System::Type& typeRef = Require(type);
        const std::string& name = typeRef.Name;
        const std::string rawName = ReplaceAll(name, "Editor", "Data");
        std::cout << "public " << name << "(Entity header, " << rawName
                  << " raw) : base(header)\n";
        std::cout << "        {\n";
        for (const std::shared_ptr<System::Reflection::PropertyInfo>& prop
            : typeRef.GetProperties())
        {
            System::Reflection::PropertyInfo& propRef = Require(prop);
            const std::string& propName = propRef.Name;
            const std::string& srcName = propRef.Name;
            if (propRef.PropertyType == System::Type::Of<bool>())
            {
                std::cout << "            " << propName << " = raw." << srcName
                          << " != 0;\n";
            }
            else if (propRef.PropertyType == System::Type::Of<MphRead::CollisionVolume>())
            {
                std::cout << "            " << propName
                          << " = new CollisionVolume(raw." << srcName << ");\n";
            }
            else
            {
                std::string prefix;
                std::string suffix;
                if (propRef.PropertyType
                    == System::Type::Of<OpenTK::Mathematics::Vector3>())
                {
                    suffix = ".ToFloatVector()";
                }
                else if (propRef.PropertyType == System::Type::Of<std::string>())
                {
                    suffix = ".MarshalString()";
                }
                if (propName == "Id" || propName == "Type" || propName == "LayerMask"
                    || propName == "NodeName" || propName == "Position"
                    || propName == "Up" || propName == "Facing")
                {
                    continue;
                }
                std::cout << "            " << propName << " = " << prefix
                          << "raw." << srcName << suffix << ";\n";
            }
        }
        std::cout << "        }\n";
        Nop();
    }

    void TestPrint::ParseStruct(
        const std::optional<std::string>& className,
        const std::optional<std::string>& baseClass,
        const std::optional<std::string>& data)
    {
#if !defined(DEBUG)
        (void)className;
#endif
        if (IsNullOrWhiteSpace(data))
        {
            return;
        }

        std::int32_t index = 0;
        std::int32_t offset = 0;
        if (baseClass && *baseClass == "CEntity")
        {
            offset = 0x18;
        }
        else if (baseClass && *baseClass == "CEnemyBase")
        {
            offset = 0x170;
        }

        const std::unordered_map<std::string, std::string> byteEnums{
            {"ENEMY_TYPE", "EnemyType"},
            {"HUNTER", "Hunter"},
            {"GAME_MODE", "GameMode"}
        };
        const std::unordered_map<std::string, std::string> ushortEnums{
            {"ITEM_TYPE", "ItemType"}
        };
        const std::unordered_map<std::string, std::string> uintEnums{
            {"EVENT_TYPE", "Message"},
            {"DOOR_TYPE", "DoorType"},
            {"COLLISION_VOLUME_TYPE", "VolumeType"}
        };

        if (baseClass)
        {
            MPH_TESTPRINT_DEBUG_WRITE_LINE("public class " + InterpolationString(className)
                + " : " + *baseClass);
        }
        else
        {
            MPH_TESTPRINT_DEBUG_WRITE_LINE("public class " + InterpolationString(className)
                + " : MemoryClass");
        }
        MPH_TESTPRINT_DEBUG_WRITE_LINE("    {");

        std::vector<std::string> news;
        for (const std::string& line : SplitString(RequireString(data), EnvironmentNewLine()))
        {
            std::string normalized = TrimManaged(line);
            normalized = ReplaceAll(std::move(normalized), "signed ", "signed");
            normalized = ReplaceAll(std::move(normalized), " *", "* ");
            normalized = ReplaceAll(std::move(normalized), ";", "");
            std::vector<std::string> split = SplitChar(normalized, ' ');
            MPH_TESTPRINT_DEBUG_ASSERT(split.size() == 2);

            std::string name;
            for (const std::string& part : SplitChar(ManagedAt(split, 1), '_'))
            {
                name += UpperFirstInvariant(part);
            }

            std::string comment;
            std::string type;
            std::string getter;
            std::string setter;
            std::int32_t size = 0;
            bool enums = false;
            bool embed = false;
            std::string cast;
            const std::string& sourceType = ManagedAt(split, 0);

            if (sourceType.find('*') != std::string::npos)
            {
                type = "IntPtr";
                getter = "ReadPointer";
                setter = "WritePointer";
                size = 4;
                comment = " // " + sourceType;
            }
            else if (sourceType == "EntityPtrUnion")
            {
                type = "IntPtr";
                getter = "ReadPointer";
                setter = "WritePointer";
                size = 4;
                comment = " // CEntity*";
            }
            else if (sourceType == "EntityIdOrRef")
            {
                type = "IntPtr";
                getter = "ReadPointer";
                setter = "WritePointer";
                size = 4;
                comment = " // EntityIdOrRef";
            }
            else if (sourceType == "int")
            {
                type = "int";
                getter = "ReadInt32";
                setter = "WriteInt32";
                size = 4;
            }
            else if (sourceType == "unsignedint")
            {
                type = "uint";
                getter = "ReadUInt32";
                setter = "WriteUInt32";
                size = 4;
            }
            else if (sourceType == "signed__int16")
            {
                type = "short";
                getter = "ReadInt16";
                setter = "WriteInt16";
                size = 2;
            }
            else if (sourceType == "__int16" || sourceType == "unsigned__int16")
            {
                type = "ushort";
                getter = "ReadUInt16";
                setter = "WriteUInt16";
                size = 2;
            }
            else if (sourceType == "char" || sourceType == "__int8"
                || sourceType == "unsigned__int8")
            {
                type = "byte";
                getter = "ReadByte";
                setter = "WriteByte";
                size = 1;
            }
            else if (sourceType == "signed__int8")
            {
                type = "sbyte";
                getter = "ReadSByte";
                setter = "WriteSByte";
                size = 1;
            }
            else if (sourceType == "Color3")
            {
                type = "ColorRgb";
                getter = "ReadColor3";
                setter = "WriteColor3";
                size = 3;
            }
            else if (sourceType == "VecFx32")
            {
                type = "Vector3";
                getter = "ReadVec3";
                setter = "WriteVec3";
                size = 12;
            }
            else if (sourceType == "Vec4")
            {
                type = "Vector4";
                getter = "ReadVec4";
                setter = "WriteVec4";
                size = 16;
            }
            else if (sourceType == "MtxFx43")
            {
                type = "Matrix4x3";
                getter = "ReadMtx43";
                setter = "WriteMtx43";
                size = 48;
            }
            else if (sourceType == "RoomState")
            {
                type = "RoomState";
                size = 60;
                embed = true;
            }
            else if (sourceType == "CModel")
            {
                type = "CModel";
                size = 0x48;
                embed = true;
            }
            else if (sourceType == "BeamInfo")
            {
                type = "BeamInfo";
                size = 0x14;
                embed = true;
            }
            else if (sourceType == "EntityCollision")
            {
                type = "EntityCollision";
                size = 0xB4;
                embed = true;
            }
            else if (sourceType == "SFXParameters")
            {
                type = "SfxParameters";
                size = 4;
                embed = true;
            }
            else if (sourceType == "CollisionVolume")
            {
                type = "CollisionVolume";
                size = 0x40;
                embed = true;
            }
            else if (sourceType == "Light")
            {
                type = "Light";
                size = 0xF;
                embed = true;
            }
            else if (sourceType == "LightInfo")
            {
                type = "LightInfo";
                size = 0x1F;
                embed = true;
            }
            else if (sourceType == "CameraInfo")
            {
                type = "CameraInfo";
                size = 0x11C;
                embed = true;
            }
            else if (sourceType == "PlayerControls")
            {
                type = "PlayerControls";
                size = 0x9C;
                embed = true;
            }
            else if (sourceType == "ButtonControlUnion")
            {
                type = "ButtonControlUnion";
                size = 4;
                embed = true;
            }
            else if (sourceType == "PlayerInput")
            {
                type = "PlayerInput";
                size = 0x48;
                embed = true;
            }
            else if (sourceType == "CBeamProjectile")
            {
                type = "CBeamProjectile";
                size = 0x158;
                embed = true;
            }
            else if (sourceType == "EquipInfo")
            {
                type = "EquipInfo";
                size = 0x14;
                embed = true;
            }
            else if (sourceType == "AIButton")
            {
                type = "AiButton";
                size = 6;
                embed = true;
            }
            else if (const auto found = byteEnums.find(sourceType); found != byteEnums.end())
            {
                type = found->second;
                getter = "ReadByte";
                setter = "WriteByte";
                size = 1;
                enums = true;
                cast = "byte";
            }
            else if (const auto found = ushortEnums.find(sourceType); found != ushortEnums.end())
            {
                type = found->second;
                getter = "ReadUInt16";
                setter = "WriteUInt16";
                size = 2;
                enums = true;
                cast = "ushort";
            }
            else if (const auto found = uintEnums.find(sourceType); found != uintEnums.end())
            {
                type = found->second;
                getter = "ReadUInt32";
                setter = "WriteUInt32";
                size = 4;
                enums = true;
                cast = "uint";
            }
            else
            {
                type = sourceType;
                getter = "Read";
                setter = "Write";
                size = 4;
                embed = true;
                DebugBreak();
            }

            std::int32_t number = 0;
            const bool array = name.find('[') != std::string::npos;
            std::string param;
            if (array)
            {
                split = SplitChar(name, '[');
                const std::vector<std::string> closeSplit =
                    SplitChar(ManagedAt(split, 1), ']');
                number = ParseInt32(ManagedAt(closeSplit, 0));
                MPH_TESTPRINT_DEBUG_ASSERT(number > 1);
                name = ManagedAt(split, 0);
                if (comment.empty())
                {
                    comment = " // " + type;
                }
                comment += "[" + std::to_string(number) + "]";
                if (embed)
                {
                    param = "," + std::string(EnvironmentNewLine())
                        + "                " + std::to_string(size)
                        + ", (Memory m, int a) => new " + type + "(m, a)";
                    type = "StructArray<" + type + ">";
                }
                else if (enums && size == 1)
                {
                    type = "U8EnumArray<" + type + ">";
                }
                else if (enums && size == 2)
                {
                    type = "U16EnumArray<" + type + ">";
                }
                else if (enums && size == 4)
                {
                    type = "U32EnumArray<" + type + ">";
                }
                else
                {
                    type = ReplaceAll(getter, "Read", "");
                    type = ReplaceAll(std::move(type), "Pointer", "IntPtr") + "Array";
                }
                size = MultiplyInt32(size, number);
            }

            MPH_TESTPRINT_DEBUG_WRITE_LINE("        private const int _off" + std::to_string(index)
                + " = 0x" + FormatHexInt32(offset, 1) + ";" + comment);
            if (array)
            {
                MPH_TESTPRINT_DEBUG_WRITE_LINE("        public " + type + " " + name + " { get; }");
                news.push_back("            " + name + " = new " + type
                    + "(memory, address + _off" + std::to_string(index) + ", "
                    + std::to_string(number) + param + ");");
            }
            else if (embed)
            {
                MPH_TESTPRINT_DEBUG_WRITE_LINE("        public " + type + " " + name + " { get; }");
                news.push_back("            " + name + " = new " + type
                    + "(memory, address + _off" + std::to_string(index) + ");");
            }
            else if (enums)
            {
                MPH_TESTPRINT_DEBUG_WRITE_LINE("        public " + type + " " + name
                    + " { get => (" + type + ")" + getter + "(_off"
                    + std::to_string(index) + "); set => " + setter + "(_off"
                    + std::to_string(index) + ", (" + cast + ")value); }");
            }
            else
            {
                MPH_TESTPRINT_DEBUG_WRITE_LINE("        public " + type + " " + name
                    + " { get => " + getter + "(_off" + std::to_string(index)
                    + "); set => " + setter + "(_off" + std::to_string(index)
                    + ", value); }");
            }
            MPH_TESTPRINT_DEBUG_WRITE_LINE("");
            index = AddInt32(index, 1);
            offset = AddInt32(offset, size);
        }

        MPH_TESTPRINT_DEBUG_WRITE_LINE("        public " + InterpolationString(className)
            + "(Memory memory, int address) : base(memory, address)");
        MPH_TESTPRINT_DEBUG_WRITE_LINE("        {");
        for (const std::string& line : news)
        {
#if !defined(DEBUG)
            (void)line;
#endif
            MPH_TESTPRINT_DEBUG_WRITE_LINE(line);
        }
        MPH_TESTPRINT_DEBUG_WRITE_LINE("        }");
        MPH_TESTPRINT_DEBUG_WRITE_LINE("");
        MPH_TESTPRINT_DEBUG_WRITE_LINE("        public " + InterpolationString(className)
            + "(Memory memory, IntPtr address) : base(memory, address)");
        MPH_TESTPRINT_DEBUG_WRITE_LINE("        {");
        for (const std::string& line : news)
        {
#if !defined(DEBUG)
            (void)line;
#endif
            MPH_TESTPRINT_DEBUG_WRITE_LINE(line);
        }
        MPH_TESTPRINT_DEBUG_WRITE_LINE("        }");
        MPH_TESTPRINT_DEBUG_WRITE_LINE("    }");
        DebugBreak();
    }

    void TestPrint::Nop() noexcept
    {
    }
}

#undef MPH_TESTPRINT_DEBUG_WRITE_LINE
#undef MPH_TESTPRINT_DEBUG_ASSERT
