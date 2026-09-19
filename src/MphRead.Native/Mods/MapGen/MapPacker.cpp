#include "MapPacker.hpp"

#include "../../Formats/Collision.hpp"
#include "../../Formats/Model.hpp"
#include "../../Program.hpp"
#include "../../Read.hpp"
#include "BuiltMap.hpp"
#include "MapBuilder.hpp"
#include "MapCollisionPacker.hpp"
#include "MapDefinition.hpp"
#include "MapNodePacker.hpp"
#include "MapTexturePack.hpp"
#include "Q3Import.hpp"
#include "RawStructs.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#include <locale.h>
#include <wchar.h>
#endif

namespace MphRead::Utility
{
    // RepackAccess.cs deliberately exposes the otherwise-private entity packer.
    // Native currently carries that access shim separately from Repack.hpp, so
    // this translation unit binds the exact members used by MapPacker.cs without
    // including two competing Native declarations of the same C# partial class.
    class Repack final
    {
    public:
        enum class RepackTexture : std::int32_t
        {
            Inline = 0,
            Separate = 1,
            Shared = 2
        };

        enum class ComputeBounds : std::int32_t
        {
            None = 0,
            Capped = 1,
            Uncapped = 2
        };

        class RepackOptions
        {
        public:
            RepackTexture Texture = RepackTexture::Inline;
            bool IsRoom = false;
            Repack::ComputeBounds ComputeBounds = Repack::ComputeBounds::None;
            bool WriteFile = false;
            bool Compare = false;
        };

        class TextureInfo
        {
        public:
            const TextureFormat Format;
            const bool Opaque;
            const std::uint16_t Height;
            const std::uint16_t Width;
            const std::shared_ptr<const std::vector<std::uint8_t>> Data;

            TextureInfo(
                TextureFormat format,
                bool opaque,
                std::uint16_t height,
                std::uint16_t width,
                std::shared_ptr<const std::vector<std::uint8_t>> data);
        };

        class PaletteInfo
        {
        public:
            std::shared_ptr<const std::vector<std::uint16_t>> Data;

            explicit PaletteInfo(
                std::shared_ptr<const std::vector<std::uint16_t>> data);
        };

        [[nodiscard]] static std::vector<std::uint8_t> PackEntities(
            std::span<Editor::EntityEditorBase* const> entities);

        [[nodiscard]] static std::pair<std::vector<std::uint8_t>, std::vector<std::uint8_t>>
            PackModel(
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
                const std::shared_ptr<RepackOptions>& options);

        [[nodiscard]] static std::shared_ptr<TextureInfo> ConvertData(
            const Texture& texture,
            const std::shared_ptr<const std::vector<TextureData>>& data);

        Repack() = delete;
    };

    // This is the existing RepackCollision Native layout used by
    // MapCollisionPacker. Only the C# members touched by MapPacker are invoked.
    class CollisionDataEditor
    {
    public:
        const std::shared_ptr<std::vector<OpenTK::Mathematics::Vector3>> Points
            = std::make_shared<std::vector<OpenTK::Mathematics::Vector3>>();
        OpenTK::Mathematics::Vector4 Plane{};
        std::uint16_t LayerMask = 0;
        MphRead::Formats::Collision::CollisionFlags Flags
            = MphRead::Formats::Collision::CollisionFlags::None;

        CollisionDataEditor() = default;
        CollisionDataEditor(const CollisionDataEditor&) = delete;
        CollisionDataEditor& operator=(const CollisionDataEditor&) = delete;
        CollisionDataEditor(CollisionDataEditor&&) = delete;
        CollisionDataEditor& operator=(CollisionDataEditor&&) = delete;

        [[nodiscard]] bool Damaging() const noexcept;
        void Damaging(bool value) noexcept;
        [[nodiscard]] bool Reflect() const noexcept;
        void Reflect(bool value) noexcept;
        [[nodiscard]] bool Players() const noexcept;
        void Players(bool value) noexcept;
        [[nodiscard]] bool Beams() const noexcept;
        void Beams(bool value) noexcept;
        [[nodiscard]] bool Scan() const noexcept;
        void Scan(bool value) noexcept;
        [[nodiscard]] std::int32_t Slipperiness() const noexcept;
        void Slipperiness(std::int32_t value);
        [[nodiscard]] MphRead::Terrain Terrain() const noexcept;
        void Terrain(MphRead::Terrain value);

    private:
        [[nodiscard]] bool Check(
            MphRead::Formats::Collision::CollisionFlags flag) const noexcept;
        void Update(
            MphRead::Formats::Collision::CollisionFlags flag,
            bool value) noexcept;
    };
}

namespace
{
    using MphRead::ColorRgb;
    using MphRead::DisplayList;
    using MphRead::Fixed;
    using MphRead::InstructionCode;
    using MphRead::ManagedArray;
    using MphRead::Material;
    using MphRead::Mesh;
    using MphRead::Node;
    using MphRead::ProgramException;
    using MphRead::RenderInstruction;
    using MphRead::RepeatMode;
    using MphRead::TextureFormat;
    using MphRead::Mods::MapGen::BuiltFace;
    using MphRead::Mods::MapGen::BuiltMap;
    using MphRead::Mods::MapGen::MapDefinition;
    using MphRead::Mods::MapGen::MapImport;
    using MphRead::Mods::MapGen::MapMaterial;
    using MphRead::Mods::MapGen::MapTexturePack;
    using MphRead::Utility::CollisionDataEditor;
    using MphRead::Utility::Repack;
    using OpenTK::Mathematics::Vector2;
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Vector4;

    [[noreturn]] void NullReference()
    {
        throw System::NullReferenceException();
    }

    [[noreturn]] void ListBounds()
    {
        throw std::out_of_range(
            "Index was out of range. Must be non-negative and less than the size of the collection. (Parameter 'index')");
    }

    [[noreturn]] void ArrayBounds()
    {
        throw std::out_of_range("Index was outside the bounds of the array.");
    }

    template <typename T>
    [[nodiscard]] T* Require(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            NullReference();
        }
        return value.get();
    }

    template <typename T>
    [[nodiscard]] const T* Require(const std::shared_ptr<const T>& value)
    {
        if (!value)
        {
            NullReference();
        }
        return value.get();
    }

    template <typename T>
    [[nodiscard]] T& ListAt(std::vector<T>& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= values.size())
        {
            ListBounds();
        }
        return values[static_cast<std::size_t>(index)];
    }

    template <typename T>
    [[nodiscard]] const T& ListAt(const std::vector<T>& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= values.size())
        {
            ListBounds();
        }
        return values[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] std::int32_t ListCount(std::size_t count)
    {
        if (count > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw std::length_error("List count exceeds Int32.MaxValue.");
        }
        return static_cast<std::int32_t>(count);
    }

    [[nodiscard]] constexpr std::int32_t WrapAdd(
        std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] std::int32_t ConvertToInt32Net(float value) noexcept
    {
        if (std::isnan(value))
        {
            return 0;
        }
        const double wide = static_cast<double>(value);
        if (wide < static_cast<double>(std::numeric_limits<std::int32_t>::min()))
        {
            return std::numeric_limits<std::int32_t>::min();
        }
        if (wide > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
        {
            return std::numeric_limits<std::int32_t>::max();
        }
        return static_cast<std::int32_t>(std::trunc(wide));
    }

    [[nodiscard]] float RoundToEven(float value) noexcept
    {
        if (!std::isfinite(value) || value == 0.0F)
        {
            return value;
        }
        const float lower = std::floor(value);
        const float fraction = value - lower;
        if (fraction < 0.5F)
        {
            return lower;
        }
        if (fraction > 0.5F)
        {
            return lower + 1.0F;
        }
        const float magnitude = std::fabs(lower);
        const bool even = std::fmod(magnitude, 2.0F) == 0.0F;
        return even ? lower : lower + 1.0F;
    }

    [[nodiscard]] std::int32_t RoundedInt32(float value) noexcept
    {
        return ConvertToInt32Net(RoundToEven(value));
    }

    [[nodiscard]] ManagedArray<Vector3>* FacePoints(BuiltFace* face)
    {
        if (face == nullptr)
        {
            NullReference();
        }
        auto* points = reinterpret_cast<ManagedArray<Vector3>*>(face->Points());
        if (points == nullptr)
        {
            NullReference();
        }
        return points;
    }

    [[nodiscard]] ManagedArray<Vector2>* FaceTexcoords(BuiltFace* face)
    {
        if (face == nullptr)
        {
            NullReference();
        }
        auto* texcoords = reinterpret_cast<ManagedArray<Vector2>*>(face->Texcoords());
        if (texcoords == nullptr)
        {
            NullReference();
        }
        return texcoords;
    }

    template <typename T>
    [[nodiscard]] const T& ArrayAt(const ManagedArray<T>* array, std::size_t index)
    {
        if (array == nullptr)
        {
            NullReference();
        }
        if (index >= array->Length())
        {
            ArrayBounds();
        }
        return (*array)[index];
    }

    [[nodiscard]] MphRead::Interop::ManagedArray<Vector3>* BuiltPoints(
        ManagedArray<Vector3>* points) noexcept
    {
        return reinterpret_cast<MphRead::Interop::ManagedArray<Vector3>*>(points);
    }

    [[nodiscard]] MphRead::Interop::ManagedArray<Vector2>* BuiltTexcoords(
        ManagedArray<Vector2>* texcoords) noexcept
    {
        return reinterpret_cast<MphRead::Interop::ManagedArray<Vector2>*>(texcoords);
    }

    void AppendUtf8(std::string& output, std::uint32_t scalar)
    {
        if (scalar <= 0x7FU)
        {
            output.push_back(static_cast<char>(scalar));
        }
        else if (scalar <= 0x7FFU)
        {
            output.push_back(static_cast<char>(0xC0U | (scalar >> 6)));
            output.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
        }
        else if (scalar <= 0xFFFFU)
        {
            output.push_back(static_cast<char>(0xE0U | (scalar >> 12)));
            output.push_back(static_cast<char>(0x80U | ((scalar >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
        }
        else
        {
            output.push_back(static_cast<char>(0xF0U | (scalar >> 18)));
            output.push_back(static_cast<char>(0x80U | ((scalar >> 12) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | ((scalar >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
        }
    }

    enum class Utf8Status
    {
        Done,
        NeedMore,
        Invalid
    };

    [[nodiscard]] Utf8Status DecodeUtf8Scalar(
        const std::uint8_t* data,
        std::size_t size,
        std::size_t& position,
        std::uint32_t& scalar) noexcept
    {
        const std::size_t start = position;
        if (start >= size)
        {
            scalar = 0xFFFDU;
            return Utf8Status::NeedMore;
        }
        const std::uint32_t first = data[start];
        if (first <= 0x7FU)
        {
            scalar = first;
            position = start + 1;
            return Utf8Status::Done;
        }
        if (first < 0xC2U || first > 0xF4U)
        {
            scalar = 0xFFFDU;
            position = start + 1;
            return Utf8Status::Invalid;
        }
        if (start + 1 >= size)
        {
            scalar = 0xFFFDU;
            position = size;
            return Utf8Status::NeedMore;
        }
        const std::uint32_t second = data[start + 1];
        if ((second & 0xC0U) != 0x80U)
        {
            scalar = 0xFFFDU;
            position = start + 1;
            return Utf8Status::Invalid;
        }
        if (first <= 0xDFU)
        {
            scalar = ((first & 0x1FU) << 6) | (second & 0x3FU);
            position = start + 2;
            return Utf8Status::Done;
        }
        if ((first == 0xE0U && second < 0xA0U)
            || (first == 0xEDU && second >= 0xA0U)
            || (first == 0xF0U && second < 0x90U)
            || (first == 0xF4U && second >= 0x90U))
        {
            scalar = 0xFFFDU;
            position = start + 1;
            return Utf8Status::Invalid;
        }
        if (start + 2 >= size)
        {
            scalar = 0xFFFDU;
            position = size;
            return Utf8Status::NeedMore;
        }
        const std::uint32_t third = data[start + 2];
        if ((third & 0xC0U) != 0x80U)
        {
            scalar = 0xFFFDU;
            position = start + 2;
            return Utf8Status::Invalid;
        }
        if (first <= 0xEFU)
        {
            scalar = ((first & 0x0FU) << 12)
                | ((second & 0x3FU) << 6)
                | (third & 0x3FU);
            position = start + 3;
            return Utf8Status::Done;
        }
        if (start + 3 >= size)
        {
            scalar = 0xFFFDU;
            position = size;
            return Utf8Status::NeedMore;
        }
        const std::uint32_t fourth = data[start + 3];
        if ((fourth & 0xC0U) != 0x80U)
        {
            scalar = 0xFFFDU;
            position = start + 3;
            return Utf8Status::Invalid;
        }
        scalar = ((first & 0x07U) << 18)
            | ((second & 0x3FU) << 12)
            | ((third & 0x3FU) << 6)
            | (fourth & 0x3FU);
        position = start + 4;
        return Utf8Status::Done;
    }

    [[nodiscard]] std::vector<std::uint32_t> DecodeUtf8(const std::string& value)
    {
        std::vector<std::uint32_t> result;
        result.reserve(value.size());
        std::size_t position = 0;
        while (position < value.size())
        {
            std::uint32_t scalar = 0;
            (void)DecodeUtf8Scalar(
                reinterpret_cast<const std::uint8_t*>(value.data()),
                value.size(),
                position,
                scalar);
            result.push_back(scalar);
        }
        return result;
    }

    [[nodiscard]] std::u16string Utf8ToUtf16(const std::string& value)
    {
        std::u16string result;
        const std::vector<std::uint32_t> scalars = DecodeUtf8(value);
        result.reserve(scalars.size());
        for (std::uint32_t scalar : scalars)
        {
            if (scalar <= 0xFFFFU)
            {
                result.push_back(static_cast<char16_t>(scalar));
            }
            else
            {
                scalar -= 0x10000U;
                result.push_back(static_cast<char16_t>(0xD800U + (scalar >> 10)));
                result.push_back(static_cast<char16_t>(0xDC00U + (scalar & 0x3FFU)));
            }
        }
        return result;
    }

#if !defined(_WIN32)
    [[nodiscard]] void* FindVersionedIcuSymbol(void* library, const char* base) noexcept
    {
        if (library == nullptr)
        {
            return nullptr;
        }
        if (void* symbol = dlsym(library, base); symbol != nullptr)
        {
            return symbol;
        }
        char name[96]{};
        for (int version = 99; version >= 50; --version)
        {
            const int count = std::snprintf(name, sizeof(name), "%s_%d", base, version);
            if (count <= 0 || static_cast<std::size_t>(count) >= sizeof(name))
            {
                continue;
            }
            if (void* symbol = dlsym(library, name); symbol != nullptr)
            {
                return symbol;
            }
        }
        return nullptr;
    }

    [[nodiscard]] std::uint32_t IcuLower(std::uint32_t scalar) noexcept
    {
        using LowerFunction = std::int32_t (*)(std::int32_t);
        static LowerFunction lower = []() noexcept -> LowerFunction
        {
            void* library = dlopen("libicuuc.so", RTLD_LAZY | RTLD_LOCAL);
#if defined(__APPLE__)
            if (library == nullptr)
            {
                library = dlopen("/usr/lib/libicucore.A.dylib", RTLD_LAZY | RTLD_LOCAL);
            }
#endif
            return reinterpret_cast<LowerFunction>(
                FindVersionedIcuSymbol(library, "u_tolower"));
        }();
        if (lower == nullptr || scalar > 0x10FFFFU)
        {
            return scalar;
        }
        const std::int32_t mapped = lower(static_cast<std::int32_t>(scalar));
        return mapped < 0 ? scalar : static_cast<std::uint32_t>(mapped);
    }
#endif

    [[nodiscard]] std::uint32_t InvariantLower(std::uint32_t scalar) noexcept
    {
        if (scalar >= 'A' && scalar <= 'Z')
        {
            return scalar + static_cast<std::uint32_t>('a' - 'A');
        }
#if defined(_WIN32)
        if (scalar <= 0xFFFFU)
        {
            const wchar_t source = static_cast<wchar_t>(scalar);
            wchar_t target = source;
            if (LCMapStringEx(
                    LOCALE_NAME_INVARIANT,
                    LCMAP_LOWERCASE,
                    &source,
                    1,
                    &target,
                    1,
                    nullptr,
                    nullptr,
                    0) == 1)
            {
                return static_cast<std::uint32_t>(target);
            }
        }
#else
        const std::uint32_t icuMapped = IcuLower(scalar);
        if (icuMapped != scalar)
        {
            return icuMapped;
        }
        static locale_t locale = []() noexcept
        {
            locale_t value = newlocale(LC_CTYPE_MASK, "C.UTF-8", nullptr);
            if (value == nullptr)
            {
                value = newlocale(LC_CTYPE_MASK, "en_US.UTF-8", nullptr);
            }
            return value;
        }();
        if (locale != nullptr && scalar <= static_cast<std::uint32_t>(WCHAR_MAX))
        {
            const wint_t mapped = towlower_l(static_cast<wint_t>(scalar), locale);
            if (mapped != WEOF)
            {
                return static_cast<std::uint32_t>(mapped);
            }
        }
#endif
        if (scalar >= 0x00C0U && scalar <= 0x00D6U) return scalar + 0x20U;
        if (scalar >= 0x00D8U && scalar <= 0x00DEU) return scalar + 0x20U;
        if (scalar == 0x0178U) return 0x00FFU;
        if (scalar >= 0x0391U && scalar <= 0x03A1U) return scalar + 0x20U;
        if (scalar >= 0x03A3U && scalar <= 0x03ABU) return scalar + 0x20U;
        if (scalar >= 0x0410U && scalar <= 0x042FU) return scalar + 0x20U;
        return scalar;
    }

    [[nodiscard]] std::string ToLowerInvariant(const std::string& value)
    {
        std::string result;
        result.reserve(value.size());
        for (std::uint32_t scalar : DecodeUtf8(value))
        {
            AppendUtf8(result, InvariantLower(scalar));
        }
        return result;
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

    [[nodiscard]] std::string PathToUtf8(const std::filesystem::path& path)
    {
#if defined(__cpp_char8_t)
        const std::u8string value = path.u8string();
        std::string result;
        result.reserve(value.size());
        for (char8_t ch : value)
        {
            result.push_back(static_cast<char>(ch));
        }
        return result;
#else
        return path.u8string();
#endif
    }

    [[nodiscard]] std::string CombinePath(
        const std::string& left,
        const std::string& right)
    {
        return PathToUtf8(PathFromUtf8(left) / PathFromUtf8(right));
    }

    void CreateDirectory(const std::string& path)
    {
        if (path.empty())
        {
            throw std::invalid_argument(
                "The value cannot be an empty string. (Parameter 'path')");
        }
        std::filesystem::create_directories(PathFromUtf8(path));
    }

    void WriteAllBytes(
        const std::string& path,
        const std::vector<std::uint8_t>& bytes)
    {
        std::ofstream stream(
            PathFromUtf8(path),
            std::ios::binary | std::ios::out | std::ios::trunc);
        if (!stream)
        {
            throw std::ios_base::failure("Could not open file for writing: " + path);
        }
        if (!bytes.empty())
        {
            stream.write(
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }
        if (!stream)
        {
            throw std::ios_base::failure("I/O error while writing file: " + path);
        }
        stream.close();
        if (!stream)
        {
            throw std::ios_base::failure("I/O error while closing file: " + path);
        }
    }

    [[nodiscard]] std::string FormatN0(std::size_t value)
    {
        std::ostringstream stream;
        try
        {
            stream.imbue(std::locale(""));
        }
        catch (...)
        {
        }
        stream << value;
        return stream.str();
    }

    [[nodiscard]] std::shared_ptr<RenderInstruction> Instruction(
        InstructionCode code,
        std::initializer_list<std::uint32_t> arguments = {})
    {
        return std::make_shared<RenderInstruction>(
            code,
            std::make_shared<std::vector<std::uint32_t>>(arguments));
    }

    struct FanFaceOwner final
    {
        std::unique_ptr<ManagedArray<Vector3>> Points;
        std::unique_ptr<ManagedArray<Vector2>> Texcoords;
        std::unique_ptr<BuiltFace> Face;

        FanFaceOwner(
            Vector3 p0,
            Vector3 p1,
            Vector3 p2,
            Vector2 t0,
            Vector2 t1,
            Vector2 t2,
            Vector3 normal,
            std::int32_t material,
            float shade)
            : Points(std::make_unique<ManagedArray<Vector3>>(3)),
              Texcoords(std::make_unique<ManagedArray<Vector2>>(3))
        {
            (*Points)[0] = p0;
            (*Points)[1] = p1;
            (*Points)[2] = p2;
            (*Texcoords)[0] = t0;
            (*Texcoords)[1] = t1;
            (*Texcoords)[2] = t2;
            Face = std::make_unique<BuiltFace>(
                BuiltPoints(Points.get()),
                BuiltTexcoords(Texcoords.get()),
                normal,
                material,
                shade);
        }
    };

    [[nodiscard]] std::vector<std::unique_ptr<FanFaceOwner>> Fan(BuiltFace* face)
    {
        ManagedArray<Vector3>* points = FacePoints(face);
        const std::size_t length = points->Length();
        std::vector<std::unique_ptr<FanFaceOwner>> result;
        if (length <= 2)
        {
            return result;
        }
        result.reserve(length - 2);
        for (std::size_t i = 1; i + 1 < length; ++i)
        {
            const Vector3 p0 = ArrayAt(points, 0);
            const Vector3 p1 = ArrayAt(points, i);
            const Vector3 p2 = ArrayAt(points, i + 1);
            ManagedArray<Vector2>* texcoords = FaceTexcoords(face);
            const Vector2 t0 = ArrayAt(texcoords, 0);
            const Vector2 t1 = ArrayAt(texcoords, i);
            const Vector2 t2 = ArrayAt(texcoords, i + 1);
            const Vector3 normal = face->Normal();
            const std::int32_t material = face->Material();
            const float shade = face->Shade();
            result.push_back(std::make_unique<FanFaceOwner>(
                p0, p1, p2, t0, t1, t2, normal, material, shade));
        }
        return result;
    }

    [[nodiscard]] std::uint32_t PackColor(float shade) noexcept
    {
        const std::int32_t rounded = RoundedInt32(31.0F * shade);
        const std::int32_t clamped = std::clamp(rounded, 0, 31);
        const std::uint32_t value = static_cast<std::uint32_t>(clamped);
        return value | (value << 5U) | (value << 10U);
    }

    [[nodiscard]] std::uint32_t PackNormal(Vector3 normal) noexcept
    {
        const auto component = [](float value) noexcept -> std::uint32_t
        {
            const std::int32_t rounded = RoundedInt32(value * 512.0F);
            const std::int32_t packed = std::clamp(rounded, -512, 511);
            return static_cast<std::uint32_t>(packed) & 0x3FFU;
        };
        return component(normal.X)
            | (component(normal.Y) << 10U)
            | (component(normal.Z) << 20U);
    }

    [[nodiscard]] std::uint32_t PackTexcoord(float u, float v) noexcept
    {
        const auto component = [](float value) noexcept -> std::uint32_t
        {
            const std::int32_t rounded = RoundedInt32(value * 16.0F);
            const std::int32_t packed = std::clamp(
                rounded,
                static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::min()),
                static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::max()));
            return static_cast<std::uint32_t>(packed) & 0xFFFFU;
        };
        return component(u) | (component(v) << 16U);
    }

    [[nodiscard]] std::shared_ptr<RenderInstruction> PackVertex(
        Vector3 point,
        float scale)
    {
        const auto component = [scale](float value) -> std::uint32_t
        {
            const std::int32_t packed = Fixed::ToInt(value / scale);
            if (packed < std::numeric_limits<std::int16_t>::min()
                || packed > std::numeric_limits<std::int16_t>::max())
            {
                std::ostringstream message;
                message << "Vertex " << value << " does not fit at scale " << scale
                    << "; raise the map's scaleFactor.";
                throw ProgramException(message.str());
            }
            return static_cast<std::uint32_t>(packed) & 0xFFFFU;
        };
        const std::uint32_t x = component(point.X);
        const std::uint32_t y = component(point.Y);
        const std::uint32_t z = component(point.Z);
        return Instruction(InstructionCode::VTX_16, {x | (y << 16U), z});
    }

    [[nodiscard]] std::int32_t EmitPrimitives(
        std::vector<std::shared_ptr<RenderInstruction>>& instructions,
        const std::vector<BuiltFace*>& faces,
        std::uint32_t primitiveType,
        float scale)
    {
        if (faces.empty())
        {
            return 0;
        }
        std::int32_t vertexCount = 0;
        instructions.push_back(
            Instruction(InstructionCode::BEGIN_VTXS, {primitiveType}));
        for (BuiltFace* face : faces)
        {
            if (face == nullptr)
            {
                NullReference();
            }
            instructions.push_back(
                Instruction(InstructionCode::COLOR, {PackColor(face->Shade())}));
            instructions.push_back(
                Instruction(InstructionCode::NORMAL, {PackNormal(face->Normal())}));
            ManagedArray<Vector3>* points = FacePoints(face);
            for (std::size_t i = 0; i < points->Length(); ++i)
            {
                ManagedArray<Vector2>* texcoords = FaceTexcoords(face);
                const Vector2 texcoord = ArrayAt(texcoords, i);
                instructions.push_back(Instruction(
                    InstructionCode::TEXCOORD,
                    {PackTexcoord(texcoord.X, texcoord.Y)}));
                instructions.push_back(PackVertex(ArrayAt(points, i), scale));
                vertexCount = WrapAdd(vertexCount, 1);
            }
        }
        instructions.push_back(Instruction(InstructionCode::END_VTXS));
        return vertexCount;
    }

    [[nodiscard]] std::pair<std::vector<std::uint8_t>, std::int32_t> Assemble(
        BuiltMap* map,
        MapDefinition* def,
        const std::shared_ptr<std::vector<std::shared_ptr<Material>>>& materials,
        const std::shared_ptr<std::vector<std::shared_ptr<Repack::TextureInfo>>>& textures,
        const std::shared_ptr<std::vector<std::shared_ptr<Repack::PaletteInfo>>>& palettes)
    {
        const float scale = std::pow(2.0F, static_cast<float>(def->ScaleFactor()));
        auto renders = std::make_shared<std::vector<
            std::shared_ptr<const std::vector<std::shared_ptr<RenderInstruction>>>>>();
        auto meshes = std::make_shared<std::vector<std::shared_ptr<Mesh>>>();
        std::int32_t vertexCount = 0;
        const std::int32_t materialCount = ListCount(materials->size());
        for (std::int32_t materialId = 0; materialId < materialCount; ++materialId)
        {
            std::vector<BuiltFace*> group;
            for (BuiltFace* face : map->Faces())
            {
                if (face == nullptr)
                {
                    NullReference();
                }
                if (face->Material() == materialId)
                {
                    group.push_back(face);
                }
            }
            if (group.empty())
            {
                continue;
            }

            auto instructions = std::make_shared<std::vector<std::shared_ptr<RenderInstruction>>>();

            std::vector<BuiltFace*> triangles;
            for (BuiltFace* face : group)
            {
                if (FacePoints(face)->Length() == 3)
                {
                    triangles.push_back(face);
                }
            }
            vertexCount = WrapAdd(
                vertexCount,
                EmitPrimitives(*instructions, triangles, 0U, scale));

            std::vector<BuiltFace*> quads;
            for (BuiltFace* face : group)
            {
                if (FacePoints(face)->Length() == 4)
                {
                    quads.push_back(face);
                }
            }
            vertexCount = WrapAdd(
                vertexCount,
                EmitPrimitives(*instructions, quads, 1U, scale));

            std::vector<std::unique_ptr<FanFaceOwner>> fanOwners;
            std::vector<BuiltFace*> fanFaces;
            for (BuiltFace* face : group)
            {
                if (FacePoints(face)->Length() > 4)
                {
                    std::vector<std::unique_ptr<FanFaceOwner>> current = Fan(face);
                    for (auto& owner : current)
                    {
                        fanFaces.push_back(owner->Face.get());
                        fanOwners.push_back(std::move(owner));
                    }
                }
            }
            vertexCount = WrapAdd(
                vertexCount,
                EmitPrimitives(*instructions, fanFaces, 0U, scale));

            while (instructions->size() % 4U != 0U)
            {
                instructions->push_back(Instruction(InstructionCode::NOP));
            }
            meshes->push_back(MphRead::Mods::MapGen::RawStructs::MakeMesh(
                materialId,
                ListCount(renders->size())));
            renders->push_back(instructions);
        }

        auto nodes = std::make_shared<std::vector<std::shared_ptr<Node>>>();
        const std::u16string rootName = u"rmMain";
        const std::u16string geometryName = u"geo1";
        nodes->push_back(MphRead::Mods::MapGen::RawStructs::MakeNode(
            std::u16string_view(rootName),
            0,
            0,
            -1,
            1));
        nodes->push_back(MphRead::Mods::MapGen::RawStructs::MakeNode(
            std::u16string_view(geometryName),
            ListCount(meshes->size()),
            0,
            0));

        auto dlists = std::make_shared<std::vector<DisplayList>>(renders->size());
        auto options = std::make_shared<Repack::RepackOptions>();
        options->IsRoom = true;
        options->Texture = Repack::RepackTexture::Inline;
        options->ComputeBounds = Repack::ComputeBounds::Capped;

        const auto emptyIds = std::make_shared<const std::vector<std::int32_t>>();
        const auto packed = Repack::PackModel(
            ConvertToInt32Net(scale),
            emptyIds,
            emptyIds,
            materials,
            textures,
            palettes,
            nodes,
            meshes,
            renders,
            dlists,
            options);
        return {packed.first, vertexCount};
    }

    [[nodiscard]] std::pair<std::vector<std::uint8_t>, std::int32_t> BuildModel(
        BuiltMap* map,
        MapTexturePack* pack)
    {
        if (map == nullptr || pack == nullptr)
        {
            NullReference();
        }
        MapDefinition* def = map->Definition();
        if (def == nullptr)
        {
            NullReference();
        }

        auto textures = std::make_shared<std::vector<std::shared_ptr<Repack::TextureInfo>>>();
        auto palettes = std::make_shared<std::vector<std::shared_ptr<Repack::PaletteInfo>>>();
        auto materials = std::make_shared<std::vector<std::shared_ptr<Material>>>();
        for (const MapTexturePack::Entry& entry : pack->Entries())
        {
            auto pixels = std::shared_ptr<const std::vector<std::uint8_t>>(
                std::addressof(entry.Pixels()),
                [](const std::vector<std::uint8_t>*) {});
            auto palette = std::shared_ptr<const std::vector<std::uint16_t>>(
                std::addressof(entry.Palette()),
                [](const std::vector<std::uint16_t>*) {});
            textures->push_back(std::make_shared<Repack::TextureInfo>(
                TextureFormat::Palette8Bit,
                true,
                entry.Height(),
                entry.Width(),
                std::move(pixels)));
            palettes->push_back(
                std::make_shared<Repack::PaletteInfo>(std::move(palette)));

            std::u16string name = Utf8ToUtf16(entry.Name());
            if (name.size() > 30U)
            {
                name = name.substr(name.size() - 30U);
            }
            materials->push_back(MphRead::Mods::MapGen::RawStructs::MakeMaterial(
                std::u16string_view(name),
                ListCount(textures->size()) - 1,
                ListCount(palettes->size()) - 1,
                RepeatMode::Repeat,
                RepeatMode::Repeat,
                false,
                ColorRgb(31, 31, 31),
                ColorRgb(0, 0, 0)));
        }
        if (materials->empty())
        {
            throw ProgramException("The texture pack is empty.");
        }
        return Assemble(map, def, materials, textures, palettes);
    }

    [[nodiscard]] std::pair<std::vector<std::uint8_t>, std::int32_t> BuildModel(
        BuiltMap* map)
    {
        if (map == nullptr)
        {
            NullReference();
        }
        MapDefinition* def = map->Definition();
        if (def == nullptr)
        {
            NullReference();
        }
        MapImport* import = def->Import();
        std::shared_ptr<MapTexturePack> own = import == nullptr
            ? std::shared_ptr<MapTexturePack>{}
            : import->LoadTexturePack();
        if (own)
        {
            return BuildModel(map, own.get());
        }

        const std::shared_ptr<MphRead::ModelInstance> instance
            = MphRead::Read::GetRoomModelInstance(def->TextureSource());
        MphRead::ModelInstance* instanceValue = Require(instance);
        const std::shared_ptr<MphRead::Model> source = instanceValue->Model();
        MphRead::Model* sourceValue = Require(source);
        const auto* recolors = Require(sourceValue->Recolors);
        const std::shared_ptr<MphRead::Recolor>& recolorValue
            = ListAt(*recolors, 0);
        MphRead::Recolor* recolor = Require(recolorValue);

        auto textures = std::make_shared<std::vector<std::shared_ptr<Repack::TextureInfo>>>();
        auto palettes = std::make_shared<std::vector<std::shared_ptr<Repack::PaletteInfo>>>();
        std::unordered_map<std::int32_t, std::int32_t> textureMap;
        std::unordered_map<std::int32_t, std::int32_t> paletteMap;
        auto materials = std::make_shared<std::vector<std::shared_ptr<Material>>>();

        const MapDefinition::MaterialList* sourceMaterials = def->Materials();
        if (sourceMaterials == nullptr)
        {
            NullReference();
        }
        for (const std::shared_ptr<MapMaterial>& mapMaterialValue : *sourceMaterials)
        {
            MapMaterial* mapMaterial = Require(mapMaterialValue);
            const std::int32_t sourceMaterial = mapMaterial->SourceMaterial();
            if (sourceMaterial < 0)
            {
                throw ProgramException(
                    def->TextureSource() + " has no material "
                    + std::to_string(sourceMaterial) + ".");
            }
            const auto* modelMaterials = Require(sourceValue->Materials);
            if (static_cast<std::size_t>(sourceMaterial) >= modelMaterials->size())
            {
                throw ProgramException(
                    def->TextureSource() + " has no material "
                    + std::to_string(sourceMaterial) + ".");
            }
            const std::shared_ptr<Material>& srcMaterialValue
                = (*modelMaterials)[static_cast<std::size_t>(sourceMaterial)];
            Material* srcMaterial = Require(srcMaterialValue);
            if (srcMaterial->TextureId < 0 || srcMaterial->PaletteId < 0)
            {
                throw ProgramException(
                    "Material " + std::to_string(sourceMaterial)
                    + " of " + def->TextureSource() + " has no texture.");
            }

            std::int32_t textureId = 0;
            const auto textureFound = textureMap.find(srcMaterial->TextureId);
            if (textureFound == textureMap.end())
            {
                textureId = ListCount(textures->size());
                const auto* recolorTextures = Require(recolor->Textures);
                const MphRead::Texture& texture
                    = ListAt(*recolorTextures, srcMaterial->TextureId);
                const auto* textureDataLists = Require(recolor->TextureData);
                const std::shared_ptr<const std::vector<MphRead::TextureData>>& textureData
                    = ListAt(*textureDataLists, srcMaterial->TextureId);
                textures->push_back(Repack::ConvertData(texture, textureData));
                textureMap.emplace(srcMaterial->TextureId, textureId);
            }
            else
            {
                textureId = textureFound->second;
            }

            std::int32_t paletteId = 0;
            const auto paletteFound = paletteMap.find(srcMaterial->PaletteId);
            if (paletteFound == paletteMap.end())
            {
                paletteId = ListCount(palettes->size());
                const auto* paletteDataLists = Require(recolor->PaletteData);
                const std::shared_ptr<const std::vector<MphRead::PaletteData>>& sourcePalette
                    = ListAt(*paletteDataLists, srcMaterial->PaletteId);
                if (!sourcePalette)
                {
                    throw System::ArgumentNullException("source");
                }
                const auto* paletteValues = sourcePalette.get();
                auto data = std::make_shared<std::vector<std::uint16_t>>();
                data->reserve(paletteValues->size());
                for (const MphRead::PaletteData& value : *paletteValues)
                {
                    data->push_back(value.Data);
                }
                palettes->push_back(std::make_shared<Repack::PaletteInfo>(data));
                paletteMap.emplace(srcMaterial->PaletteId, paletteId);
            }
            else
            {
                paletteId = paletteFound->second;
            }

            const std::u16string name = Utf8ToUtf16(mapMaterial->Name());
            materials->push_back(MphRead::Mods::MapGen::RawStructs::MakeMaterial(
                std::u16string_view(name),
                textureId,
                paletteId,
                RepeatMode::Repeat,
                RepeatMode::Repeat,
                false,
                ColorRgb(31, 31, 31),
                ColorRgb(0, 0, 0)));
        }
        if (materials->empty())
        {
            throw ProgramException("A map needs at least one material.");
        }
        return Assemble(map, def, materials, textures, palettes);
    }

    [[nodiscard]] std::vector<std::uint8_t> BuildCollision(BuiltMap* map)
    {
        if (map == nullptr)
        {
            NullReference();
        }
        auto editors = std::make_shared<
            std::vector<std::shared_ptr<CollisionDataEditor>>>();
        for (BuiltFace* face : map->Solid())
        {
            if (face == nullptr)
            {
                NullReference();
            }
            ManagedArray<Vector3>* facePoints = FacePoints(face);
            if (facePoints->Length() <= 10U)
            {
                auto editor = std::make_shared<CollisionDataEditor>();
                const Vector3 normal = face->Normal();
                editor->LayerMask = static_cast<std::uint16_t>(
                    4 | MphRead::Mods::MapGen::MapPacker::GetPrimaryAxis(normal));
                editor->Plane = Vector4(
                    normal,
                    Vector3::Dot(normal, ArrayAt(facePoints, 0)));
                editor->Damaging(face->Damaging());
                editor->Terrain(face->Terrain());
                for (std::size_t i = 0; i < facePoints->Length(); ++i)
                {
                    editor->Points->push_back(ArrayAt(facePoints, i));
                }
                editors->push_back(std::move(editor));
            }
            else
            {
                std::vector<std::unique_ptr<FanFaceOwner>> parts = Fan(face);
                for (const std::unique_ptr<FanFaceOwner>& owner : parts)
                {
                    BuiltFace* part = owner->Face.get();
                    auto editor = std::make_shared<CollisionDataEditor>();
                    const Vector3 normal = part->Normal();
                    ManagedArray<Vector3>* partPoints = FacePoints(part);
                    editor->LayerMask = static_cast<std::uint16_t>(
                        4 | MphRead::Mods::MapGen::MapPacker::GetPrimaryAxis(normal));
                    editor->Plane = Vector4(
                        normal,
                        Vector3::Dot(normal, ArrayAt(partPoints, 0)));
                    editor->Damaging(face->Damaging());
                    editor->Terrain(face->Terrain());
                    for (std::size_t i = 0; i < partPoints->Length(); ++i)
                    {
                        editor->Points->push_back(ArrayAt(partPoints, i));
                    }
                    editors->push_back(std::move(editor));
                }
            }
        }
        if (editors->empty())
        {
            throw ProgramException("A map needs at least one solid face.");
        }
        return MphRead::Mods::MapGen::MapCollisionPacker::Pack(editors);
    }
}

namespace MphRead::Mods::MapGen
{
    void MapPacker::Generate(
        BuiltMap* map,
        const std::string& archiveDir,
        const std::string& entityDir,
        const std::string& nodeDir,
        bool verbose)
    {
        if (map == nullptr)
        {
            NullReference();
        }
        MapDefinition* def = map->Definition();
        CreateDirectory(archiveDir);
        CreateDirectory(entityDir);
        if (def == nullptr)
        {
            NullReference();
        }
        const std::string prefix = ToLowerInvariant(def->Name());

        auto [model, vertices] = BuildModel(map);
        std::vector<std::uint8_t> collision = BuildCollision(map);
        std::vector<Editor::EntityEditorBase*>& entityList = map->Entities();
        std::vector<std::uint8_t> entities = Repack::PackEntities(
            std::span<Editor::EntityEditorBase* const>(
                entityList.data(), entityList.size()));
        auto [nodes, nodeCount, edges] = MapNodePacker::Pack(&map->Solid());

        const std::string modelPath = CombinePath(archiveDir, prefix + "_Model.bin");
        WriteAllBytes(modelPath, model);
        const std::string animPath = CombinePath(archiveDir, prefix + "_Anim.bin");
        const std::vector<std::uint8_t> emptyAnim(24U);
        WriteAllBytes(animPath, emptyAnim);
        const std::string collisionPath = CombinePath(archiveDir, prefix + "_Collision.bin");
        WriteAllBytes(collisionPath, collision);
        const std::string entityPath = CombinePath(entityDir, prefix + "_Ent.bin");
        WriteAllBytes(entityPath, entities);
        CreateDirectory(nodeDir);
        const std::string nodePath = CombinePath(nodeDir, prefix + "_Node.bin");
        WriteAllBytes(nodePath, nodes);

        if (verbose)
        {
            std::cout
                << def->Name() << ": " << ListCount(map->Faces().size())
                << " polygons (" << vertices << " vertices), "
                << ListCount(map->Solid().size()) << " collision faces, "
                << ListCount(map->Entities().size()) << " entities\n";
            std::cout
                << "  " << nodeCount << " bot waypoints, " << edges
                << " routes between them\n";
            std::cout
                << "  model " << FormatN0(model.size())
                << " B, collision " << FormatN0(collision.size())
                << " B, entities " << FormatN0(entities.size())
                << " B, nodes " << FormatN0(nodes.size()) << " B\n";
        }
    }

    void MapPacker::Generate(
        MapDefinition* def,
        const std::string& archiveDir,
        const std::string& entityDir,
        const std::string& nodeDir,
        bool verbose)
    {
        if (def == nullptr)
        {
            NullReference();
        }
        std::shared_ptr<BuiltMap> map = def->Import() == nullptr
            ? MapBuilder::Build(def)
            : Q3Import::Build(def, verbose);
        Generate(map.get(), archiveDir, entityDir, nodeDir, verbose);
    }

    std::int32_t MapPacker::GetPrimaryAxis(Vector3 normal) noexcept
    {
        const float x = std::fabs(normal.X);
        const float y = std::fabs(normal.Y);
        const float z = std::fabs(normal.Z);
        if (y > x && y >= z)
        {
            return 1;
        }
        if (z > x && z > y)
        {
            return 2;
        }
        return 0;
    }
}
