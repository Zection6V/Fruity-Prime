#include "TestPrint.hpp"

#include "../Formats/Formats.hpp"
#include "../Formats/Model.hpp"
#include "../Formats/Types.hpp"
#include "../NativeRuntime/System/Console.hpp"
#include "../NativeRuntime/System/Encoding.hpp"
#include "../NativeRuntime/System/Globalization.hpp"
#include "../NativeRuntime/System/Managed.hpp"
#include "NativeRuntime/System/Globalization.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

using ::MphRead::NativeRuntime::CharIsWhiteSpace;
using ::MphRead::NativeRuntime::DecodeUtf8Scalar;
using ::MphRead::NativeRuntime::EnvironmentNewLine;
using ::MphRead::NativeRuntime::Int32ToUInt32;
using ::MphRead::NativeRuntime::ManagedAt;
using ::MphRead::NativeRuntime::MathMax;
using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::NativeRuntime::StringIsNullOrWhiteSpace;
using ::MphRead::NativeRuntime::StringReplace;
using ::MphRead::NativeRuntime::StringTrim;
using ::MphRead::NativeRuntime::UInt32ToInt32;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::UncheckedMultiply;
using ::MphRead::NativeRuntime::Utf8Scalar;

namespace
{
    class ManagedIndexOutOfRangeException final : public std::out_of_range
    {
    public:
        ManagedIndexOutOfRangeException()
            : std::out_of_range("Index was outside the bounds of the array.")
        {
        }
    };

    [[nodiscard]] const std::string& InterpolationString(
        const std::optional<std::string>& value) noexcept
    {
        static const std::string empty;
        return value ? *value : empty;
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

    [[nodiscard]] std::string UpperFirstInvariant(std::string_view part)
    {
        // part[0].ToString().ToUpperInvariant() + part[1..]: part[0] is one
        // UTF-16 unit, so a character outside the BMP is half of one and
        // stays as it is.
        if (part.empty())
        {
            throw ManagedIndexOutOfRangeException();
        }
        const Utf8Scalar first = DecodeUtf8Scalar(part, 0);
        if (!first.Valid() || first.Value > 0xFFFFU)
        {
            return std::string(part);
        }
        std::string result;
        ::MphRead::NativeRuntime::AppendUtf8(result, ::MphRead::NativeRuntime::ToUpperInvariant(first.Value));
        result.append(part.substr(first.Length));
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
        return ::MphRead::NativeRuntime::ToString(static_cast<std::uint32_t>(value));
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
        return ::MphRead::NativeRuntime::ToString(static_cast<std::uint32_t>(value));
    }

// External parity blocker: no shared Native System.Diagnostics.DebugProvider /
    // CLR debugger contract exists yet. Keep the existing compile-compatible fallbacks
    // private to this translation unit; they are not claimed as managed-equivalent.
#if defined(DEBUG)
    void DebugWriteLineFallback(const std::string& value)
    {
        std::clog << value << '\n';
    }

    [[noreturn]] void DebugAssertFailedFallback() noexcept
    {
        std::abort();
    }

    void DebugAssertFallback(bool condition) noexcept
    {
        if (!condition)
        {
            DebugAssertFailedFallback();
        }
    }
#endif

#if defined(DEBUG)
#define MPH_TESTPRINT_DEBUG_WRITE_LINE(value) DebugWriteLineFallback(value)
#define MPH_TESTPRINT_DEBUG_ASSERT(condition) DebugAssertFallback(condition)
#else
#define MPH_TESTPRINT_DEBUG_WRITE_LINE(value) do { } while (false)
#define MPH_TESTPRINT_DEBUG_ASSERT(condition) do { } while (false)
#endif

    void DebuggerBreakFallback()
    {
#if defined(_WIN32)
        __debugbreak();
#else
        std::raise(SIGTRAP);
#endif
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
            std::cout << "  int field_" << ::MphRead::NativeRuntime::ToString(offset, "X1") << ";\n";
            offset = UncheckedAdd(offset, 4);
        }
        std::cout << "}\n";
    }

    void TestPrint::GetPolygonAttrs(
        const std::shared_ptr<MphRead::Model>& model,
        std::int32_t polygonId)
    {
        Model& modelRef = RequireReference(model);
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
        Model& modelRef = RequireReference(model);
        Material& materialRef = RequireReference(material);

        const std::uint32_t v19 = polygonId == 1 ? 0x4000U : 0U;
        const std::uint32_t v20 = v19 | 0x8000U;
        const std::uint32_t polygonModeBits =
            Int32ToUInt32(UncheckedMultiply(
                16,
                UInt32ToInt32(static_cast<std::uint32_t>(materialRef.PolygonMode))));
        const std::uint32_t cullingBits =
            static_cast<std::uint32_t>(materialRef.Culling) << 6U;
        const std::uint32_t polygonBits = Int32ToUInt32(polygonId) << 24U;
        const std::uint32_t alphaBits =
            static_cast<std::uint32_t>(materialRef.Alpha) << 16U;
        const std::uint32_t attr = v20
            | static_cast<std::uint32_t>(materialRef.Lighting)
            | polygonModeBits | cullingBits | polygonBits | alphaBits;

        std::cout << modelRef.Name << " - " << materialRef.Name << '\n';
        std::cout << "light = "
                  << ::MphRead::NativeRuntime::ToString(materialRef.Lighting)
                  << ", mode = "
                  << ::MphRead::NativeRuntime::ToString(UInt32ToInt32(static_cast<std::uint32_t>(materialRef.PolygonMode)))
                  << " (" << PolygonModeName(materialRef.PolygonMode) << "), cull = "
                  << ::MphRead::NativeRuntime::ToString(static_cast<std::uint32_t>(materialRef.Culling))
                  << " (" << CullingModeName(materialRef.Culling) << "), alpha = "
                  << ::MphRead::NativeRuntime::ToString(materialRef.Alpha)
                  << ", id = " << ::MphRead::NativeRuntime::ToString(polygonId) << '\n';
        DumpPolygonAttr(attr);
    }

    void TestPrint::DumpPolygonAttr(std::uint32_t attr)
    {
        std::cout << "0x" << ::MphRead::NativeRuntime::ToString(attr, "X2") << '\n';
        std::cout << ::MphRead::NativeRuntime::ToString(attr, "B") << '\n';
        std::cout << "light1: " << ::MphRead::NativeRuntime::ToString(attr & 0x1U) << '\n';
        std::cout << "light2: " << ::MphRead::NativeRuntime::ToString((attr >> 1U) & 0x1U) << '\n';
        std::cout << "light3: " << ::MphRead::NativeRuntime::ToString((attr >> 2U) & 0x1U) << '\n';
        std::cout << "light4: " << ::MphRead::NativeRuntime::ToString((attr >> 3U) & 0x1U) << '\n';
        std::cout << "mode: " << ::MphRead::NativeRuntime::ToString((attr >> 4U) & 0x2U) << '\n';
        std::cout << "back: " << ::MphRead::NativeRuntime::ToString((attr >> 6U) & 0x1U) << '\n';
        std::cout << "front: " << ::MphRead::NativeRuntime::ToString((attr >> 7U) & 0x1U) << '\n';
        std::cout << "clear: " << ::MphRead::NativeRuntime::ToString((attr >> 11U) & 0x1U) << '\n';
        std::cout << "far: " << ::MphRead::NativeRuntime::ToString((attr >> 12U) & 0x1U) << '\n';
        std::cout << "1dot: " << ::MphRead::NativeRuntime::ToString((attr >> 13U) & 0x1U) << '\n';
        std::cout << "depth: " << ::MphRead::NativeRuntime::ToString((attr >> 14U) & 0x1U) << '\n';
        std::cout << "fog: " << ::MphRead::NativeRuntime::ToString((attr >> 15U) & 0x1U) << '\n';
        std::cout << "alpha: " << ::MphRead::NativeRuntime::ToString((attr >> 16U) & 0x1FU) << '\n';
        std::cout << "id: " << ::MphRead::NativeRuntime::ToString((attr >> 24U) & 0x3FU) << '\n';
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
        const float dif_factor = MathMax(
            0.0F, -Vector3::Dot(light_vec, normal_vec));
        const Vector3 half_vec(
            (light_vec.X + sight_vec.X) / 2.0F,
            (light_vec.Y + sight_vec.Y) / 2.0F,
            (light_vec.Z + sight_vec.Z) / 2.0F);
        float spe_factor = MathMax(
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

    // PrintEntityEditor is intentionally not defined here. TestPrint.cs requires the
    // CLR System.Type / PropertyInfo metadata contract, and no shared Native reflection
    // owner exists yet. The public declaration is preserved without a pair-local fake facade.

    void TestPrint::ParseStruct(
        const std::optional<std::string>& className,
        const std::optional<std::string>& baseClass,
        const std::optional<std::string>& data)
    {
#if !defined(DEBUG)
        (void)className;
#endif
        if (StringIsNullOrWhiteSpace(data))
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
        for (const std::string& line : SplitString(RequireReference(data), EnvironmentNewLine()))
        {
            std::string normalized = StringTrim(line);
            normalized = StringReplace(std::move(normalized), "signed ", "signed");
            normalized = StringReplace(std::move(normalized), " *", "* ");
            normalized = StringReplace(std::move(normalized), ";", "");
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
                DebuggerBreakFallback();
            }

            std::int32_t number = 0;
            const bool array = name.find('[') != std::string::npos;
            std::string param;
            if (array)
            {
                split = SplitChar(name, '[');
                const std::vector<std::string> closeSplit =
                    SplitChar(ManagedAt(split, 1), ']');
                number = ::MphRead::NativeRuntime::ParseInteger<std::int32_t>(ManagedAt(closeSplit, 0), ::MphRead::NativeRuntime::NumberStyles::Integer, ::MphRead::NativeRuntime::NumberFormatInfo::CurrentInfo());
                MPH_TESTPRINT_DEBUG_ASSERT(number > 1);
                name = ManagedAt(split, 0);
                if (comment.empty())
                {
                    comment = " // " + type;
                }
                comment += "[" + ::MphRead::NativeRuntime::ToString(number) + "]";
                if (embed)
                {
                    param = "," + std::string(EnvironmentNewLine())
                        + "                " + ::MphRead::NativeRuntime::ToString(size)
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
                    type = StringReplace(getter, "Read", "");
                    type = StringReplace(std::move(type), "Pointer", "IntPtr") + "Array";
                }
                size = UncheckedMultiply(size, number);
            }

            MPH_TESTPRINT_DEBUG_WRITE_LINE("        private const int _off" + ::MphRead::NativeRuntime::ToString(index)
                + " = 0x" + ::MphRead::NativeRuntime::ToString(offset, "X1") + ";" + comment);
            if (array)
            {
                MPH_TESTPRINT_DEBUG_WRITE_LINE("        public " + type + " " + name + " { get; }");
                news.push_back("            " + name + " = new " + type
                    + "(memory, address + _off" + ::MphRead::NativeRuntime::ToString(index) + ", "
                    + ::MphRead::NativeRuntime::ToString(number) + param + ");");
            }
            else if (embed)
            {
                MPH_TESTPRINT_DEBUG_WRITE_LINE("        public " + type + " " + name + " { get; }");
                news.push_back("            " + name + " = new " + type
                    + "(memory, address + _off" + ::MphRead::NativeRuntime::ToString(index) + ");");
            }
            else if (enums)
            {
                MPH_TESTPRINT_DEBUG_WRITE_LINE("        public " + type + " " + name
                    + " { get => (" + type + ")" + getter + "(_off"
                    + ::MphRead::NativeRuntime::ToString(index) + "); set => " + setter + "(_off"
                    + ::MphRead::NativeRuntime::ToString(index) + ", (" + cast + ")value); }");
            }
            else
            {
                MPH_TESTPRINT_DEBUG_WRITE_LINE("        public " + type + " " + name
                    + " { get => " + getter + "(_off" + ::MphRead::NativeRuntime::ToString(index)
                    + "); set => " + setter + "(_off" + ::MphRead::NativeRuntime::ToString(index)
                    + ", value); }");
            }
            MPH_TESTPRINT_DEBUG_WRITE_LINE("");
            index = UncheckedAdd(index, 1);
            offset = UncheckedAdd(offset, size);
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
        DebuggerBreakFallback();
    }

    void TestPrint::Nop() noexcept
    {
    }
}

#undef MPH_TESTPRINT_DEBUG_WRITE_LINE
#undef MPH_TESTPRINT_DEBUG_ASSERT
