#include "Q3Import.hpp"
#include "NativeRuntime/System/Charconv.hpp"

#include "../../Formats/Formats.hpp"
#include "../../Program.hpp"
#include "../../Read.hpp"
#include "BuiltMap.hpp"
#include "CustomRooms.hpp"
#include "MapBuilder.hpp"
#include "MapDefinition.hpp"
#include "MapTextureBake.hpp"
#include "MapTexturePack.hpp"
#include "Q3Bsp.hpp"
#include "../../NativeRuntime/System/IO.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

using ::MphRead::NativeRuntime::ConvertToInt32Net9;
using ::MphRead::NativeRuntime::MathMax;
using ::MphRead::NativeRuntime::PathCombine;
using ::MphRead::NativeRuntime::PathFromUtf8;
using ::MphRead::NativeRuntime::PathToUtf8;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::UncheckedMultiply;
using ::OpenTK::Mathematics::Add;
using ::OpenTK::Mathematics::ComponentMax;
using ::OpenTK::Mathematics::ComponentMin;
using ::OpenTK::Mathematics::Divide;
using ::OpenTK::Mathematics::LengthSquared;
using ::OpenTK::Mathematics::Multiply;
using ::OpenTK::Mathematics::Subtract;

namespace
{
    using MphRead::ItemType;
    using MphRead::ManagedArray;
    using MphRead::Mods::MapGen::BuiltFace;
    using MphRead::Mods::MapGen::Q3Entity;
    using MphRead::Mods::MapGen::Q3StringEqual;
    using OpenTK::Mathematics::Vector2;
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Vector4;

    [[noreturn]] void NullReference()
    {
        throw System::NullReferenceException();
    }

    [[noreturn]] void ArrayBounds()
    {
        throw System::IndexOutOfRangeException();
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
            throw System::ArgumentOutOfRangeException();
        }
        return values[static_cast<std::size_t>(index)];
    }

    template <typename T>
    [[nodiscard]] const T& ListAt(const std::vector<T>& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= values.size())
        {
            throw System::ArgumentOutOfRangeException();
        }
        return values[static_cast<std::size_t>(index)];
    }

    template <typename T>
    [[nodiscard]] const T& ArrayAt(const std::vector<T>* values, std::size_t index)
    {
        if (values == nullptr)
        {
            NullReference();
        }
        if (index >= values->size())
        {
            ArrayBounds();
        }
        return (*values)[index];
    }

    template <typename T>
    [[nodiscard]] T& ArrayAt(std::vector<T>* values, std::size_t index)
    {
        if (values == nullptr)
        {
            NullReference();
        }
        if (index >= values->size())
        {
            ArrayBounds();
        }
        return (*values)[index];
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

    [[nodiscard]] bool IsAsciiWhitespace(unsigned char value) noexcept
    {
        return value == 0x20U || (value >= 0x09U && value <= 0x0DU);
    }

    [[nodiscard]] bool EqualsIgnoreCaseAscii(
        std::string_view left, std::string_view right) noexcept
    {
        if (left.size() != right.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < left.size(); ++i)
        {
            unsigned char a = static_cast<unsigned char>(left[i]);
            unsigned char b = static_cast<unsigned char>(right[i]);
            if (a >= 'a' && a <= 'z')
            {
                a = static_cast<unsigned char>(a - ('a' - 'A'));
            }
            if (b >= 'a' && b <= 'z')
            {
                b = static_cast<unsigned char>(b - ('a' - 'A'));
            }
            if (a != b)
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool IsFloatNumber(
        std::string_view text, bool& negative) noexcept
    {
        negative = false;
        if (text.empty())
        {
            return false;
        }
        std::size_t position = 0;
        if (text[position] == '+' || text[position] == '-')
        {
            negative = text[position] == '-';
            ++position;
        }
        if (position == text.size())
        {
            return false;
        }

        bool digits = false;
        while (position < text.size()
            && text[position] >= '0' && text[position] <= '9')
        {
            digits = true;
            ++position;
        }
        if (position < text.size() && text[position] == '.')
        {
            ++position;
            while (position < text.size()
                && text[position] >= '0' && text[position] <= '9')
            {
                digits = true;
                ++position;
            }
        }
        if (!digits)
        {
            return false;
        }
        if (position < text.size()
            && (text[position] == 'e' || text[position] == 'E'))
        {
            ++position;
            if (position < text.size()
                && (text[position] == '+' || text[position] == '-'))
            {
                ++position;
            }
            const std::size_t exponentStart = position;
            while (position < text.size()
                && text[position] >= '0' && text[position] <= '9')
            {
                ++position;
            }
            if (position == exponentStart)
            {
                return false;
            }
        }
        return position == text.size();
    }

    [[nodiscard]] bool DecimalIsBelowOne(std::string_view text) noexcept
    {
        std::size_t position = 0;
        if (!text.empty() && (text.front() == '+' || text.front() == '-'))
        {
            position = 1;
        }

        std::int64_t digitsBeforeDecimal = 0;
        std::int64_t digitIndex = 0;
        std::int64_t firstNonzero = -1;
        bool beforeDecimal = true;
        while (position < text.size()
            && text[position] != 'e' && text[position] != 'E')
        {
            const char ch = text[position++];
            if (ch == '.')
            {
                beforeDecimal = false;
                continue;
            }
            if (beforeDecimal)
            {
                ++digitsBeforeDecimal;
            }
            if (firstNonzero < 0 && ch != '0')
            {
                firstNonzero = digitIndex;
            }
            ++digitIndex;
        }
        if (firstNonzero < 0)
        {
            return true;
        }

        std::int64_t exponent = 0;
        if (position < text.size())
        {
            ++position;
            bool exponentNegative = false;
            if (position < text.size()
                && (text[position] == '+' || text[position] == '-'))
            {
                exponentNegative = text[position] == '-';
                ++position;
            }
            constexpr std::int64_t Limit = 1'000'000;
            while (position < text.size())
            {
                const std::int64_t digit = text[position++] - '0';
                exponent = std::min(Limit, exponent * 10 + digit);
            }
            if (exponentNegative)
            {
                exponent = -exponent;
            }
        }

        const std::int64_t scientificExponent
            = digitsBeforeDecimal - firstNonzero - 1 + exponent;
        return scientificExponent < 0;
    }

    [[nodiscard]] bool TryParseSingleInvariant(
        std::string_view text, float& result) noexcept
    {
        result = 0.0F;
        while (!text.empty()
            && IsAsciiWhitespace(static_cast<unsigned char>(text.front())))
        {
            text.remove_prefix(1);
        }
        if (text.empty())
        {
            return false;
        }

        // Number.TryParseFloat first parses NumberStyles.Float, then checks
        // the culture's NaN/infinity symbols against a whitespace-trimmed
        // view. InvariantCulture uses +, -, NaN and Infinity. All signed NaN
        // spellings return Single.NaN, whose canonical .NET bit pattern is
        // 0xFFC00000.
        std::string_view special = text;
        while (!special.empty()
            && IsAsciiWhitespace(static_cast<unsigned char>(special.back())))
        {
            special.remove_suffix(1);
        }
        if (EqualsIgnoreCaseAscii(special, "NaN")
            || EqualsIgnoreCaseAscii(special, "+NaN")
            || EqualsIgnoreCaseAscii(special, "-NaN"))
        {
            result = std::bit_cast<float>(std::uint32_t{0xFFC00000U});
            return true;
        }
        if (EqualsIgnoreCaseAscii(special, "Infinity")
            || EqualsIgnoreCaseAscii(special, "+Infinity"))
        {
            result = std::numeric_limits<float>::infinity();
            return true;
        }
        if (EqualsIgnoreCaseAscii(special, "-Infinity"))
        {
            result = -std::numeric_limits<float>::infinity();
            return true;
        }

        // TryStringToNumber permits trailing whitespace and, for compatibility,
        // then permits only embedded NUL characters to the end of the input.
        // Strip in that order from the outside: NULs first, then whitespace.
        // A NUL followed by whitespace must remain invalid.
        while (!text.empty() && text.back() == '\0')
        {
            text.remove_suffix(1);
        }
        while (!text.empty()
            && IsAsciiWhitespace(static_cast<unsigned char>(text.back())))
        {
            text.remove_suffix(1);
        }
        if (text.empty())
        {
            return false;
        }

        bool negative = false;
        if (!IsFloatNumber(text, negative))
        {
            return false;
        }
        std::string_view magnitude = text;
        if (magnitude.front() == '+' || magnitude.front() == '-')
        {
            magnitude.remove_prefix(1);
        }

        float parsed = 0.0F;
        const char* first = magnitude.data();
        const char* last = first + magnitude.size();
        const auto conversion = ::MphRead::NativeRuntime::FromChars(
            first, last, parsed, std::chars_format::general);
        if (conversion.ptr == last && conversion.ec == std::errc{})
        {
            result = negative ? -parsed : parsed;
            return true;
        }
        if (conversion.ptr != last
            || conversion.ec != std::errc::result_out_of_range)
        {
            return false;
        }

        long double wide = 0.0L;
        const auto wideConversion = ::MphRead::NativeRuntime::FromChars(
            first, last, wide, std::chars_format::general);
        if (wideConversion.ptr == last && wideConversion.ec == std::errc{})
        {
            parsed = static_cast<float>(wide);
            result = negative ? -parsed : parsed;
            return true;
        }
        if (wideConversion.ptr != last
            || wideConversion.ec != std::errc::result_out_of_range)
        {
            return false;
        }

        if (DecimalIsBelowOne(text))
        {
            result = negative ? -0.0F : 0.0F;
        }
        else
        {
            result = negative
                ? -std::numeric_limits<float>::infinity()
                : std::numeric_limits<float>::infinity();
        }
        return true;
    }

    [[nodiscard]] bool TryParseInt32(std::string_view text, std::int32_t& result) noexcept
    {
        result = 0;
        std::size_t position = 0;
        while (position < text.size()
            && IsAsciiWhitespace(static_cast<unsigned char>(text[position])))
        {
            ++position;
        }
        if (position == text.size())
        {
            return false;
        }

        bool negative = false;
        if (text[position] == '+' || text[position] == '-')
        {
            negative = text[position] == '-';
            ++position;
        }
        const std::size_t digitsStart = position;
        const std::uint64_t limit = negative
            ? static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) + 1U
            : static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
        std::uint64_t magnitude = 0;
        while (position < text.size()
            && text[position] >= '0' && text[position] <= '9')
        {
            const std::uint64_t digit
                = static_cast<std::uint64_t>(text[position] - '0');
            if (magnitude > (limit - digit) / 10U)
            {
                return false;
            }
            magnitude = magnitude * 10U + digit;
            ++position;
        }
        if (position == digitsStart)
        {
            return false;
        }
        while (position < text.size()
            && IsAsciiWhitespace(static_cast<unsigned char>(text[position])))
        {
            ++position;
        }
        while (position < text.size() && text[position] == '\0')
        {
            ++position;
        }
        if (position != text.size())
        {
            return false;
        }

        if (negative)
        {
            result = magnitude == limit
                ? std::numeric_limits<std::int32_t>::min()
                : -static_cast<std::int32_t>(magnitude);
        }
        else
        {
            result = static_cast<std::int32_t>(magnitude);
        }
        return true;
    }

    [[nodiscard]] bool EqualsOrdinalIgnoreCase(
        const std::string& left, const std::string& right) noexcept
    {
        return Q3StringEqual{}(left, right);
    }

    [[nodiscard]] bool StartsWithOrdinalIgnoreCase(
        const std::string& value, const std::string& prefix) noexcept
    {
        if (prefix.size() > value.size())
        {
            return false;
        }
        return Q3StringEqual{}(
            value.substr(0, prefix.size()), prefix);
    }

    [[nodiscard]] std::string FileName(const std::string& path)
    {
        return PathToUtf8(PathFromUtf8(path).filename());
    }

    [[nodiscard]] std::string FormatOneOptional(float value)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(1) << value;
        std::string text = stream.str();
        if (text.size() >= 2
            && text[text.size() - 2] == '.'
            && text.back() == '0')
        {
            text.resize(text.size() - 2);
        }
        return text;
    }

    [[nodiscard]] std::string FormatOneRequired(float value)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(1) << value;
        return stream.str();
    }

    [[nodiscard]] std::string ItemTypeToString(ItemType value)
    {
        switch (value)
        {
        case ItemType::None: return "None";
        case ItemType::HealthMedium: return "HealthMedium";
        case ItemType::HealthSmall: return "HealthSmall";
        case ItemType::HealthBig: return "HealthBig";
        case ItemType::DoubleDamage: return "DoubleDamage";
        case ItemType::EnergyTank: return "EnergyTank";
        case ItemType::VoltDriver: return "VoltDriver";
        case ItemType::MissileExpansion: return "MissileExpansion";
        case ItemType::Battlehammer: return "Battlehammer";
        case ItemType::Imperialist: return "Imperialist";
        case ItemType::Judicator: return "Judicator";
        case ItemType::Magmaul: return "Magmaul";
        case ItemType::ShockCoil: return "ShockCoil";
        case ItemType::OmegaCannon: return "OmegaCannon";
        case ItemType::UASmall: return "UASmall";
        case ItemType::UABig: return "UABig";
        case ItemType::MissileSmall: return "MissileSmall";
        case ItemType::MissileBig: return "MissileBig";
        case ItemType::Cloak: return "Cloak";
        case ItemType::UAExpansion: return "UAExpansion";
        case ItemType::ArtifactKey: return "ArtifactKey";
        case ItemType::Deathalt: return "Deathalt";
        case ItemType::AffinityWeapon: return "AffinityWeapon";
        case ItemType::PickWpnMissile: return "PickWpnMissile";
        }
        return std::to_string(static_cast<std::int32_t>(value));
    }

    [[nodiscard]] const std::string* EntityValue(
        const Q3Entity* entity, const std::string& key)
    {
        if (entity == nullptr)
        {
            NullReference();
        }
        const auto iterator = entity->find(key);
        if (iterator == entity->end())
        {
            return nullptr;
        }
        return std::addressof(iterator->second);
    }

    [[nodiscard]] std::size_t RectIndex(
        std::int32_t dimension, std::int32_t a, std::int32_t b)
    {
        if (dimension < 0 || a < 0 || b < 0
            || a >= dimension || b >= dimension)
        {
            ArrayBounds();
        }
        return static_cast<std::size_t>(a)
            * static_cast<std::size_t>(dimension)
            + static_cast<std::size_t>(b);
    }
}

namespace MphRead::Mods::MapGen
{
    bool Q3Import::CellKey::operator==(const CellKey& other) const noexcept
    {
        return X == other.X && Y == other.Y && Z == other.Z;
    }

    std::size_t Q3Import::CellKeyHash::operator()(const CellKey& value) const noexcept
    {
        std::size_t hash = std::hash<std::int32_t>{}(value.X);
        hash ^= std::hash<std::int32_t>{}(value.Y)
            + static_cast<std::size_t>(0x9e3779b9U) + (hash << 6U) + (hash >> 2U);
        hash ^= std::hash<std::int32_t>{}(value.Z)
            + static_cast<std::size_t>(0x9e3779b9U) + (hash << 6U) + (hash >> 2U);
        return hash;
    }

    std::shared_ptr<BuiltMap> Q3Import::Build(MapDefinition* def, bool verbose)
    {
        if (def == nullptr)
        {
            NullReference();
        }

        MapImport* import = def->Import();
        if (import == nullptr)
        {
            throw ProgramException("Map " + def->Name() + " has no import settings.");
        }

        std::optional<std::string> resolved = import->Resolve();
        const std::string source = resolved.has_value()
            ? *resolved
            : import->Source();
        const std::optional<std::string> mapName = import->MapName();
        std::shared_ptr<Q3Bsp> bsp = Q3Bsp::Load(source, mapName);

        auto map = std::make_shared<BuiltMap>(def);
        const float unit = import->UnitsPerUnit();

        std::shared_ptr<MapTexturePack> packOwner = import->LoadTexturePack();
        std::unique_ptr<MapTexturePack> bakedPack;
        MapTexturePack* pack = packOwner.get();
        if (pack == nullptr)
        {
            const std::optional<std::string> baked = BakeTextures(bsp, import, verbose);
            if (baked.has_value())
            {
                bakedPack.reset(new MapTexturePack(MapTexturePack::Load(*baked)));
                pack = bakedPack.get();
            }
        }

        std::vector<std::pair<std::int32_t, std::int32_t>> textureSizes;
        if (pack == nullptr)
        {
            textureSizes = GetTextureSizes(def);
        }
        else
        {
            textureSizes.reserve(pack->Entries().size());
            for (const MapTexturePack::Entry& entry : pack->Entries())
            {
                textureSizes.emplace_back(
                    static_cast<std::int32_t>(entry.Width()),
                    static_cast<std::int32_t>(entry.Height()));
            }
        }

        if (textureSizes.empty())
        {
            const std::optional<std::string>& textures = import->Textures();
            if (!textures.has_value() || textures->empty())
            {
                throw ProgramException(
                    def->Name()
                    + " names no texture pack and maps no shaders onto a shipped "
                    + "room's materials, so it has no materials at all. A bundle cooked "
                    + "without its baked textures is the usual cause.");
            }
            throw ProgramException(
                def->Name() + " has no textures: " + *textures
                + " is not beside its recipe, not in its bundle, and could not be baked from the level.");
        }

        std::int32_t unpainted = 0;

        float skySpan = 100.0F;
        if (!bsp)
        {
            NullReference();
        }
        if (!bsp->Models().empty())
        {
            Q3Model* model = Require(bsp->Models().front());
            const std::vector<float>* mins = Require(model->Mins());
            const std::vector<float>* maxs = Require(model->Maxs());
            const float spanX = ArrayAt(maxs, 0) - ArrayAt(mins, 0);
            const float spanY = ArrayAt(maxs, 1) - ArrayAt(mins, 1);
            skySpan = MathMax(spanX, spanY) / unit;
        }

        std::int32_t skipped = 0;
        std::int32_t patches = 0;
        std::int32_t patchTriangles = 0;
        Vector3 drawnMin(
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max());
        Vector3 drawnMax(
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest());

        for (const std::shared_ptr<Q3Face>& faceValue : bsp->Faces())
        {
            Q3Face* face = Require(faceValue);
            if (face->Type() != 1 && face->Type() != 2 && face->Type() != 3)
            {
                ++skipped;
                continue;
            }

            Q3Texture* texture = Require(ListAt(bsp->Textures(), face->Texture()));
            if ((texture->Flags()
                & (Q3Bsp::SurfaceNoDraw | Q3Bsp::SurfaceHint | Q3Bsp::SurfaceSkip)) != 0)
            {
                ++skipped;
                continue;
            }

            const bool sky = (texture->Flags() & Q3Bsp::SurfaceSky) != 0;
            if (sky && !import->KeepSky())
            {
                ++skipped;
                continue;
            }

            std::int32_t material = 0;
            if (pack == nullptr)
            {
                material = MatchMaterial(import, texture->Name().Value());
            }
            else if (!pack->BySourceIndex().TryGetValue(face->Texture(), material))
            {
                ++unpainted;
                continue;
            }

            if (material < 0
                || static_cast<std::size_t>(material) >= textureSizes.size())
            {
                throw System::ArgumentOutOfRangeException();
            }
            const auto [width, height]
                = textureSizes[static_cast<std::size_t>(material)];

            const bool patch = face->Type() == 2;
            if (patch)
            {
                ++patches;
            }

            MphRead::Enumerable<BuiltFace*> builtFaces = patch
                ? Tessellate(
                    bsp.get(), face, unit, width, height,
                    material, sky, import->PatchLevel())
                : Triangles(
                    bsp.get(), face, unit, width, height,
                    material, sky);

            for (BuiltFace* built : builtFaces)
            {
                if (sky)
                {
                    ProjectSky(
                        built,
                        static_cast<float>(width) * SkyTiles
                            / MathMax(1.0F, skySpan));
                }

                map->Faces().push_back(built);
                if (patch)
                {
                    ++patchTriangles;
                }

                if (!sky)
                {
                    ManagedArray<Vector3>* points = FacePoints(built);
                    for (std::size_t i = 0; i < points->Length(); ++i)
                    {
                        drawnMin = ComponentMin(drawnMin, (*points)[i]);
                        drawnMax = ComponentMax(drawnMax, (*points)[i]);
                    }
                }

                if (patch
                    && (texture->Contents() & Q3Bsp::ContentsSolid) != 0)
                {
                    map->Solid().push_back(built);
                }
            }
        }

        const Vector3 margin(6.0F, 6.0F, 6.0F);
        const Vector3 keepMin = Subtract(drawnMin, margin);
        const Vector3 keepMax = Add(drawnMax, margin);
        std::int32_t solidBrushes = 0;
        std::int32_t shellBrushes = 0;
        std::int32_t clipBrushes = 0;
        std::vector<std::vector<Vector4>> brushPlanes;
        std::vector<BrushBounds> brushBounds;
        std::vector<BrushSideEntry> sides;

        std::int32_t firstBrush = 0;
        std::int32_t brushCount = static_cast<std::int32_t>(bsp->Brushes().size());
        if (!bsp->Models().empty())
        {
            Q3Model* levelModel = Require(bsp->Models().front());
            firstBrush = levelModel->Brush();
            brushCount = levelModel->BrushCount();
        }

        const std::int32_t brushEnd = UncheckedAdd(firstBrush, brushCount);
        for (std::int32_t brushIndex = firstBrush;
            brushIndex < brushEnd;
            brushIndex = UncheckedAdd(brushIndex, 1))
        {
            Q3Brush* brush = Require(ListAt(bsp->Brushes(), brushIndex));
            Q3Texture* texture = Require(ListAt(bsp->Textures(), brush->Texture()));

            const bool solid = (texture->Contents() & Q3Bsp::ContentsSolid) != 0;
            const bool clip = (texture->Contents() & Q3Bsp::ContentsPlayerClip) != 0;
            if (!solid && clip && !import->KeepClip())
            {
                ++clipBrushes;
                continue;
            }
            if (!solid && !clip)
            {
                continue;
            }
            if (IsSky(bsp.get(), brush))
            {
                ++shellBrushes;
                continue;
            }

            std::vector<BrushPolygon> polygons = BrushSides(bsp.get(), brush);
            if (polygons.empty())
            {
                continue;
            }

            bool allOutside = true;
            for (const BrushPolygon& polygon : polygons)
            {
                for (const Vector3 point : polygon.Points)
                {
                    if (Inside(ToWorld(point, unit), keepMin, keepMax))
                    {
                        allOutside = false;
                        break;
                    }
                }
                if (!allOutside)
                {
                    break;
                }
            }
            if (allOutside)
            {
                ++shellBrushes;
                continue;
            }

            ++solidBrushes;
            if (brush->SideCount() < 0)
            {
                throw System::OverflowException();
            }
            std::vector<Vector4> planes(
                static_cast<std::size_t>(brush->SideCount()));
            for (std::int32_t i = 0; i < brush->SideCount(); ++i)
            {
                const std::int32_t sideIndex = UncheckedAdd(brush->FirstSide(), i);
                Q3BrushSide* side = Require(ListAt(bsp->BrushSides(), sideIndex));
                Q3Plane* plane = Require(ListAt(bsp->Planes(), side->Plane()));
                planes[static_cast<std::size_t>(i)] = Vector4(
                    plane->X(), plane->Y(), plane->Z(), plane->Distance());
            }

            Vector3 brushMin(
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max());
            Vector3 brushMax(
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest());
            for (const BrushPolygon& polygon : polygons)
            {
                for (const Vector3 point : polygon.Points)
                {
                    brushMin = ComponentMin(brushMin, point);
                    brushMax = ComponentMax(brushMax, point);
                }
            }

            const std::int32_t index
                = static_cast<std::int32_t>(brushPlanes.size());
            brushPlanes.push_back(std::move(planes));
            brushBounds.push_back(BrushBounds{brushMin, brushMax});
            for (const BrushPolygon& polygon : polygons)
            {
                sides.push_back(
                    BrushSideEntry{index, polygon.Points, polygon.Normal});
            }
        }

        const BrushLookup lookup = BuildBrushLookup(brushBounds);
        std::int32_t buried = 0;
        for (const BrushSideEntry& side : sides)
        {
            if (IsBuried(
                side.Points, side.Normal, side.Brush,
                brushPlanes, brushBounds, lookup))
            {
                ++buried;
                continue;
            }

            auto* world = new ManagedArray<Vector3>(side.Points.size());
            auto* texcoords = new ManagedArray<Vector2>(side.Points.size());
            for (std::size_t i = 0; i < side.Points.size(); ++i)
            {
                (*world)[i] = ToWorld(side.Points[i], unit);
            }
            const std::vector<float> direction{
                side.Normal.X, side.Normal.Y, side.Normal.Z};
            map->Solid().push_back(new BuiltFace(
                BuiltPoints(world),
                BuiltTexcoords(texcoords),
                ToDirection(std::addressof(direction)),
                0,
                1.0F));
        }

        AddEntities(map.get(), def, bsp.get(), import, verbose);

        if (def->Preview() == nullptr)
        {
            Vector3 low;
            Vector3 high;
            Bounds(map.get(), low, high);
            const Vector3 centre = Divide(Add(low, high), 2.0F);
            const Vector3 size = Subtract(high, low);

            auto preview = std::make_shared<MapPreview>();
            preview->Position(std::make_shared<std::vector<float>>(
                std::initializer_list<float>{
                    centre.X + size.X * 0.6F,
                    high.Y + size.Y * 0.5F,
                    centre.Z + size.Z * 0.6F}));
            preview->Target(std::make_shared<std::vector<float>>(
                std::initializer_list<float>{
                    centre.X, centre.Y, centre.Z}));
            def->Preview(preview);

            if (verbose)
            {
                std::cout
                    << "  suggested preview (paste into the map file to keep it):\n";
                const std::vector<float>* position = preview->Position();
                const std::vector<float>* target = preview->Target();
                std::cout
                    << "  \"preview\": { \"position\": ["
                    << FormatOneOptional(ArrayAt(position, 0)) << ", "
                    << FormatOneOptional(ArrayAt(position, 1)) << ", "
                    << FormatOneOptional(ArrayAt(position, 2))
                    << "], \"target\": ["
                    << FormatOneOptional(ArrayAt(target, 0)) << ", "
                    << FormatOneOptional(ArrayAt(target, 1)) << ", "
                    << FormatOneOptional(ArrayAt(target, 2))
                    << "] },\n";
            }
        }

        if (verbose)
        {
            if (pack != nullptr)
            {
                std::cout << "  " << pack->Entries().size() << " baked textures";
                if (unpainted > 0)
                {
                    std::cout << ", " << unpainted
                        << " surfaces dropped for want of one";
                }
                std::cout << '\n';
            }

            std::cout
                << "  imported " << bsp->Faces().size()
                << " surfaces -> " << map->Faces().size()
                << " triangles (" << patches
                << " patches tessellated to " << patchTriangles
                << ", " << skipped
                << " non-drawing surfaces skipped)\n";

            std::cout
                << "  " << solidBrushes
                << " solid brushes -> " << map->Solid().size()
                << " collision faces (" << shellBrushes
                << " shell brushes outside the level left out, "
                << buried << " sides buried inside other brushes";
            if (clipBrushes > 0)
            {
                std::cout << ", " << clipBrushes
                    << " invisible-wall brushes dropped";
            }
            std::cout << ")\n";

            Vector3 min;
            Vector3 max;
            Bounds(map.get(), min, max);
            std::cout
                << "  extent " << FormatOneRequired(max.X - min.X)
                << " x " << FormatOneRequired(max.Y - min.Y)
                << " x " << FormatOneRequired(max.Z - min.Z)
                << " units at " << FormatOneOptional(unit)
                << " Quake units each\n";
        }

        return map;
    }

    MphRead::Enumerable<BuiltFace*> Q3Import::Triangles(
        Q3Bsp* bsp,
        Q3Face* face,
        float unit,
        std::int32_t width,
        std::int32_t height,
        std::int32_t material,
        bool sky)
    {
        return MphRead::Enumerable<BuiltFace*>(
            [bsp, face, unit, width, height, material, sky]()
                -> MphRead::NativeRuntime::CoroutineSequence<BuiltFace*>
            {
                if (face == nullptr || bsp == nullptr)
                {
                    NullReference();
                }

                for (std::int32_t i = 0;
                    UncheckedAdd(i, 2) < face->MeshVertCount();
                    i = UncheckedAdd(i, 3))
                {
                    std::vector<Vector3> points(3);
                    std::vector<Vector2> uvs(3);
                    Vector3 normal(0.0F, 0.0F, 0.0F);
                    float shade = 0.0F;

                    for (std::int32_t j = 0; j < 3; ++j)
                    {
                        const std::int32_t meshIndex = UncheckedAdd(
                            UncheckedAdd(face->MeshVert(), i), j);
                        const std::int32_t vertexOffset
                            = ListAt(bsp->MeshVerts(), meshIndex);
                        const std::int32_t vertexIndex
                            = UncheckedAdd(face->Vertex(), vertexOffset);
                        Q3Vertex* vertex
                            = Require(ListAt(bsp->Vertices(), vertexIndex));

                        points[static_cast<std::size_t>(j)]
                            = ToWorld(Require(vertex->Position()), unit);
                        const std::vector<float>* surface
                            = Require(vertex->Surface());
                        uvs[static_cast<std::size_t>(j)] = Vector2(
                            ArrayAt(surface, 0) * static_cast<float>(width),
                            ArrayAt(surface, 1) * static_cast<float>(height));

                        normal = Add(
                            normal,
                            ToDirection(Require(vertex->Normal())));

                        const std::vector<std::uint8_t>* color
                            = Require(vertex->Color());
                        const std::int32_t colorSum
                            = static_cast<std::int32_t>(ArrayAt(color, 0))
                            + static_cast<std::int32_t>(ArrayAt(color, 1))
                            + static_cast<std::int32_t>(ArrayAt(color, 2));
                        shade += static_cast<float>(colorSum)
                            / (3.0F * 255.0F);
                    }

                    if (LengthSquared(normal) < 0.0001F)
                    {
                        normal = ToDirection(Require(face->Normal()));
                    }
                    normal = normal.Normalized();
                    co_yield MakeFace(
                        std::move(points),
                        std::move(uvs),
                        normal,
                        material,
                        Shade(shade / 3.0F, sky));
                }
            });
    }

    MphRead::Enumerable<BuiltFace*> Q3Import::Tessellate(
        Q3Bsp* bsp,
        Q3Face* face,
        float unit,
        std::int32_t width,
        std::int32_t height,
        std::int32_t material,
        bool sky,
        std::int32_t level)
    {
        return MphRead::Enumerable<BuiltFace*>(
            [bsp, face, unit, width, height, material, sky, level]()
                -> MphRead::NativeRuntime::CoroutineSequence<BuiltFace*>
            {
                if (face == nullptr || bsp == nullptr)
                {
                    NullReference();
                }

                const std::vector<std::int32_t>* size = Require(face->Size());
                const std::int32_t w = ArrayAt(size, 0);
                const std::int32_t h = ArrayAt(size, 1);
                if (w < 3 || h < 3 || w % 2 == 0 || h % 2 == 0)
                {
                    co_return;
                }

                const std::int32_t tessLevel = std::clamp(level, 1, 8);
                const std::int32_t dimension = tessLevel + 1;
                const std::size_t valueCount
                    = static_cast<std::size_t>(dimension)
                    * static_cast<std::size_t>(dimension);

                for (std::int32_t py = 0; UncheckedAdd(py, 2) < h; py = UncheckedAdd(py, 2))
                {
                    for (std::int32_t px = 0; UncheckedAdd(px, 2) < w; px = UncheckedAdd(px, 2))
                    {
                        std::vector<Vector3> points(valueCount);
                        std::vector<Vector2> uvs(valueCount);
                        std::vector<Vector3> normals(valueCount);
                        std::vector<float> shades(valueCount);

                        for (std::int32_t a = 0; a <= tessLevel; ++a)
                        {
                            const float v
                                = static_cast<float>(a)
                                / static_cast<float>(tessLevel);
                            const auto [v0, v1, v2] = Weights(v);

                            for (std::int32_t b = 0; b <= tessLevel; ++b)
                            {
                                const float u
                                    = static_cast<float>(b)
                                    / static_cast<float>(tessLevel);
                                const auto [u0, u1, u2] = Weights(u);

                                Vector3 position(0.0F, 0.0F, 0.0F);
                                Vector2 uv(0.0F, 0.0F);
                                Vector3 normal(0.0F, 0.0F, 0.0F);
                                float shade = 0.0F;

                                for (std::int32_t r = 0; r < 3; ++r)
                                {
                                    const float rw
                                        = r == 0 ? v0 : r == 1 ? v1 : v2;
                                    for (std::int32_t c = 0; c < 3; ++c)
                                    {
                                        const float cw
                                            = c == 0 ? u0 : c == 1 ? u1 : u2;
                                        const float weight = rw * cw;

                                        const std::int32_t row
                                            = UncheckedAdd(py, r);
                                        const std::int32_t rowOffset
                                            = UncheckedMultiply(row, w);
                                        const std::int32_t controlOffset
                                            = UncheckedAdd(
                                                UncheckedAdd(rowOffset, px), c);
                                        const std::int32_t vertexIndex
                                            = UncheckedAdd(
                                                face->Vertex(),
                                                controlOffset);
                                        Q3Vertex* vertex = Require(
                                            ListAt(
                                                bsp->Vertices(),
                                                vertexIndex));

                                        position = Add(
                                            position,
                                            Multiply(
                                                ToWorld(
                                                    Require(vertex->Position()),
                                                    unit),
                                                weight));

                                        const std::vector<float>* surface
                                            = Require(vertex->Surface());
                                        uv = Add(
                                            uv,
                                            Multiply(
                                                Vector2(
                                                    ArrayAt(surface, 0)
                                                        * static_cast<float>(width),
                                                    ArrayAt(surface, 1)
                                                        * static_cast<float>(height)),
                                                weight));

                                        normal = Add(
                                            normal,
                                            Multiply(
                                                ToDirection(
                                                    Require(vertex->Normal())),
                                                weight));

                                        const std::vector<std::uint8_t>* color
                                            = Require(vertex->Color());
                                        const std::int32_t colorSum
                                            = static_cast<std::int32_t>(
                                                ArrayAt(color, 0))
                                            + static_cast<std::int32_t>(
                                                ArrayAt(color, 1))
                                            + static_cast<std::int32_t>(
                                                ArrayAt(color, 2));
                                        shade += (
                                            static_cast<float>(colorSum)
                                            / (3.0F * 255.0F))
                                            * weight;
                                    }
                                }

                                const std::size_t index
                                    = RectIndex(dimension, a, b);
                                points[index] = position;
                                uvs[index] = uv;
                                normals[index] = LengthSquared(normal) < 0.0001F
                                    ? ToDirection(Require(face->Normal()))
                                    : normal.Normalized();
                                shades[index] = shade;
                            }
                        }

                        for (std::int32_t a = 0; a < tessLevel; ++a)
                        {
                            for (std::int32_t b = 0; b < tessLevel; ++b)
                            {
                                co_yield Cell(
                                    points, uvs, normals, shades,
                                    dimension, material, sky,
                                    GridPoint{a, b},
                                    GridPoint{a, b + 1},
                                    GridPoint{a + 1, b + 1});
                                co_yield Cell(
                                    points, uvs, normals, shades,
                                    dimension, material, sky,
                                    GridPoint{a, b},
                                    GridPoint{a + 1, b + 1},
                                    GridPoint{a + 1, b});
                            }
                        }
                    }
                }
            });
    }

    std::tuple<float, float, float> Q3Import::Weights(float t) noexcept
    {
        const float inverse = 1.0F - t;
        return std::tuple<float, float, float>(
            inverse * inverse,
            2.0F * t * inverse,
            t * t);
    }

    BuiltFace* Q3Import::Cell(
        const std::vector<Vector3>& points,
        const std::vector<Vector2>& uvs,
        const std::vector<Vector3>& normals,
        const std::vector<float>& shades,
        std::int32_t dimension,
        std::int32_t material,
        bool sky,
        GridPoint p0,
        GridPoint p1,
        GridPoint p2)
    {
        const std::size_t i0 = RectIndex(dimension, p0.A, p0.B);
        const std::size_t i1 = RectIndex(dimension, p1.A, p1.B);
        const std::size_t i2 = RectIndex(dimension, p2.A, p2.B);

        std::vector<Vector3> corners{
            points.at(i0), points.at(i1), points.at(i2)};
        std::vector<Vector2> texcoords{
            uvs.at(i0), uvs.at(i1), uvs.at(i2)};

        Vector3 normal = Add(
            Add(normals.at(i0), normals.at(i1)),
            normals.at(i2));
        normal = LengthSquared(normal) < 0.0001F
            ? Vector3(0.0F, 1.0F, 0.0F)
            : normal.Normalized();

        const float shade
            = (shades.at(i0) + shades.at(i1) + shades.at(i2))
            / 3.0F;
        return MakeFace(
            std::move(corners),
            std::move(texcoords),
            normal,
            material,
            Shade(shade, sky));
    }

    float Q3Import::Shade(float baked, bool sky) noexcept
    {
        return sky
            ? 1.0F
            : std::clamp(0.62F + baked * 0.75F, 0.55F, 1.0F);
    }

    BuiltFace* Q3Import::MakeFace(
        std::vector<Vector3> points,
        std::vector<Vector2> uvs,
        Vector3 normal,
        std::int32_t material,
        float shade)
    {
        const Vector3 point1 = ArrayAt(&points, 1);
        const Vector3 point0a = ArrayAt(&points, 0);
        const Vector3 point2 = ArrayAt(&points, 2);
        const Vector3 point0b = ArrayAt(&points, 0);
        const Vector3 wound = Vector3::Cross(
            Subtract(point1, point0a),
            Subtract(point2, point0b));
        if (Vector3::Dot(wound, normal) < 0.0F)
        {
            (void)ArrayAt(&points, 1);
            (void)ArrayAt(&points, 2);
            (void)ArrayAt(&uvs, 1);
            (void)ArrayAt(&uvs, 2);
            std::swap(points[1], points[2]);
            std::swap(uvs[1], uvs[2]);
        }

        auto* pointArray = new ManagedArray<Vector3>(points.size());
        auto* uvArray = new ManagedArray<Vector2>(uvs.size());
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            (*pointArray)[i] = points[i];
        }
        for (std::size_t i = 0; i < uvs.size(); ++i)
        {
            (*uvArray)[i] = uvs[i];
        }

        return new BuiltFace(
            BuiltPoints(pointArray),
            BuiltTexcoords(uvArray),
            normal,
            material,
            shade);
    }

    void Q3Import::ProjectSky(BuiltFace* face, float texelsPerUnit)
    {
        ManagedArray<Vector3>* points = FacePoints(face);
        ManagedArray<Vector2>* texcoords = FaceTexcoords(face);
        const Vector3 origin = (*points)[0];
        const Vector3 normal = face->Normal();
        const float ax = std::fabs(normal.X);
        const float ay = std::fabs(normal.Y);
        const float az = std::fabs(normal.Z);

        for (std::size_t i = 0; i < points->Length(); ++i)
        {
            const Vector3 offset = Multiply(
                Subtract((*points)[i], origin),
                texelsPerUnit);
            (*texcoords)[i] = ay > ax && ay >= az
                ? Vector2(offset.X, offset.Z)
                : ax >= az
                    ? Vector2(offset.Z, -offset.Y)
                    : Vector2(offset.X, -offset.Y);
        }
    }

    std::optional<std::string> Q3Import::BakeTextures(
        const std::shared_ptr<Q3Bsp>& bsp,
        MapImport* import,
        bool verbose)
    {
        if (import == nullptr)
        {
            NullReference();
        }

        const std::optional<std::string> level = import->Resolve();
        const std::optional<std::string>& textures = import->Textures();
        if (!textures.has_value() || textures->empty() || !level.has_value())
        {
            return std::nullopt;
        }

        const std::optional<std::string>& baseDirectoryValue
            = import->BaseDirectory();
        const std::string& baseDirectory = baseDirectoryValue.has_value()
            ? *baseDirectoryValue
            : CustomRooms::MapDirectory();
        const std::string target = PathCombine(baseDirectory, *textures);

        try
        {
            auto archivePaths = std::make_shared<
                std::vector<std::optional<std::string>>>();
            archivePaths->push_back(*level);

            std::shared_ptr<MapTextureBake::Result> result
                = MapTextureBake::Bake(
                    bsp,
                    archivePaths,
                    std::optional<std::string>(target));
            if (!result)
            {
                NullReference();
            }

            if (result->Baked == 0)
            {
                std::filesystem::remove(PathFromUtf8(target));
                return std::nullopt;
            }

            if (verbose)
            {
                std::cout
                    << "  baked " << result->Baked
                    << " textures from " << FileName(*level)
                    << " -> " << FileName(target) << '\n';
            }
            return target;
        }
        catch (const std::exception& exception)
        {
            std::cout
                << "[mapgen] could not bake textures from "
                << FileName(*level) << ": "
                << exception.what() << '\n';
            return std::nullopt;
        }
    }

    bool Q3Import::Inside(
        Vector3 point, Vector3 min, Vector3 max) noexcept
    {
        return point.X >= min.X && point.X <= max.X
            && point.Y >= min.Y && point.Y <= max.Y
            && point.Z >= min.Z && point.Z <= max.Z;
    }

    bool Q3Import::IsSky(Q3Bsp* bsp, Q3Brush* brush)
    {
        if (bsp == nullptr || brush == nullptr)
        {
            NullReference();
        }

        for (std::int32_t i = 0; i < brush->SideCount(); ++i)
        {
            const std::int32_t sideIndex
                = UncheckedAdd(brush->FirstSide(), i);
            Q3BrushSide* side
                = Require(ListAt(bsp->BrushSides(), sideIndex));
            Q3Texture* texture
                = Require(ListAt(bsp->Textures(), side->Texture()));
            if ((texture->Flags() & Q3Bsp::SurfaceSky) == 0)
            {
                return false;
            }
        }
        return brush->SideCount() > 0;
    }

    void Q3Import::Bounds(BuiltMap* map, Vector3& min, Vector3& max)
    {
        if (map == nullptr)
        {
            NullReference();
        }

        min = Vector3(
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max());
        max = Vector3(
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest());

        const auto accumulate = [&min, &max](const std::vector<BuiltFace*>& faces)
        {
            for (BuiltFace* face : faces)
            {
                ManagedArray<Vector3>* points = FacePoints(face);
                for (std::size_t i = 0; i < points->Length(); ++i)
                {
                    min = ComponentMin(min, (*points)[i]);
                    max = ComponentMax(max, (*points)[i]);
                }
            }
        };
        accumulate(map->Faces());
        accumulate(map->Solid());
    }

    std::vector<Vector2> Q3Import::Rebase(const std::vector<Vector2>& uvs)
    {
        if (uvs.empty())
        {
            throw System::InvalidOperationException("Sequence contains no elements");
        }

        float minU = uvs.front().X;
        for (std::size_t i = 1; i < uvs.size(); ++i)
        {
            if (std::isnan(uvs[i].X) || uvs[i].X < minU)
            {
                minU = uvs[i].X;
                if (std::isnan(minU))
                {
                    break;
                }
            }
        }

        float minV = uvs.front().Y;
        for (std::size_t i = 1; i < uvs.size(); ++i)
        {
            if (std::isnan(uvs[i].Y) || uvs[i].Y < minV)
            {
                minV = uvs[i].Y;
                if (std::isnan(minV))
                {
                    break;
                }
            }
        }

        const Vector2 offset(std::floor(minU), std::floor(minV));
        std::vector<Vector2> result;
        result.reserve(uvs.size());
        for (const Vector2 uv : uvs)
        {
            result.push_back(Subtract(uv, offset));
        }
        return result;
    }

    std::vector<std::pair<std::int32_t, std::int32_t>>
        Q3Import::GetTextureSizes(MapDefinition* def)
    {
        if (def == nullptr)
        {
            NullReference();
        }

        std::shared_ptr<ModelInstance> instance
            = Read::GetRoomModelInstance(def->TextureSource());
        ModelInstance* requiredInstance = Require(instance);
        std::shared_ptr<Model> source = requiredInstance->Model();
        Model* requiredSource = Require(source);

        if (!requiredSource->Recolors)
        {
            NullReference();
        }
        Recolor* recolor
            = Require(ListAt(*requiredSource->Recolors, 0));

        if (!requiredSource->Materials)
        {
            NullReference();
        }
        if (!recolor->Textures)
        {
            NullReference();
        }

        MapDefinition::MaterialList* materials = def->Materials();
        if (materials == nullptr)
        {
            NullReference();
        }

        std::vector<std::pair<std::int32_t, std::int32_t>> results;
        results.reserve(materials->size());
        for (const std::shared_ptr<MapMaterial>& materialValue : *materials)
        {
            MapMaterial* material = Require(materialValue);
            const std::int32_t sourceMaterialId = material->SourceMaterial();
            Material* sourceMaterial = Require(
                ListAt(*requiredSource->Materials, sourceMaterialId));
            const Texture& texture
                = ListAt(*recolor->Textures, sourceMaterial->TextureId);
            results.emplace_back(
                static_cast<std::int32_t>(texture.Width),
                static_cast<std::int32_t>(texture.Height));
        }
        return results;
    }

    std::int32_t Q3Import::MatchMaterial(
        MapImport* import, const std::string& shader)
    {
        if (import == nullptr)
        {
            NullReference();
        }
        MapImport::ShaderMaterialDictionary* materials
            = import->ShaderMaterials();
        if (materials == nullptr)
        {
            NullReference();
        }

        std::int32_t exact = 0;
        if (materials->TryGetValue(shader, exact))
        {
            return exact;
        }

        std::optional<std::string> best;
        for (const std::string& key : materials->Keys())
        {
            if (StartsWithOrdinalIgnoreCase(shader, key)
                && (!best.has_value() || key.size() > best->size()))
            {
                best = key;
            }
        }
        return best.has_value()
            ? (*materials)[*best]
            : import->DefaultMaterial();
    }

    std::vector<Q3Import::BrushPolygon> Q3Import::BrushSides(
        Q3Bsp* bsp, Q3Brush* brush)
    {
        if (bsp == nullptr || brush == nullptr)
        {
            NullReference();
        }

        std::vector<BrushPolygon> result;
        for (std::int32_t i = 0; i < brush->SideCount(); ++i)
        {
            const std::int32_t firstIndex
                = UncheckedAdd(brush->FirstSide(), i);
            Q3BrushSide* firstSide
                = Require(ListAt(bsp->BrushSides(), firstIndex));
            Q3Plane* plane
                = Require(ListAt(bsp->Planes(), firstSide->Plane()));
            const Vector3 normal(
                plane->X(), plane->Y(), plane->Z());

            std::vector<Vector3> points
                = MakeSheet(normal, plane->Distance());
            for (std::int32_t j = 0;
                j < brush->SideCount() && points.size() >= 3;
                ++j)
            {
                if (j == i)
                {
                    continue;
                }

                const std::int32_t otherIndex
                    = UncheckedAdd(brush->FirstSide(), j);
                Q3BrushSide* otherSide
                    = Require(ListAt(bsp->BrushSides(), otherIndex));
                Q3Plane* other
                    = Require(ListAt(bsp->Planes(), otherSide->Plane()));
                points = Clip(
                    points,
                    Vector3(other->X(), other->Y(), other->Z()),
                    other->Distance());
            }

            if (points.size() < 3)
            {
                continue;
            }
            points = Weld(points);
            if (points.size() < 3)
            {
                continue;
            }
            result.push_back(
                BrushPolygon{std::move(points), normal});
        }
        return result;
    }

    Q3Import::BrushLookup Q3Import::BuildBrushLookup(
        const std::vector<BrushBounds>& bounds)
    {
        BrushLookup lookup;
        for (std::size_t i = 0; i < bounds.size(); ++i)
        {
            const BrushBounds& bound = bounds[i];
            const std::int32_t startX = Cell(bound.Min.X);
            const std::int32_t endX = Cell(bound.Max.X);
            for (std::int32_t x = startX; x <= endX; x = UncheckedAdd(x, 1))
            {
                const std::int32_t startY = Cell(bound.Min.Y);
                const std::int32_t endY = Cell(bound.Max.Y);
                for (std::int32_t y = startY; y <= endY; y = UncheckedAdd(y, 1))
                {
                    const std::int32_t startZ = Cell(bound.Min.Z);
                    const std::int32_t endZ = Cell(bound.Max.Z);
                    for (std::int32_t z = startZ; z <= endZ; z = UncheckedAdd(z, 1))
                    {
                        lookup[CellKey{x, y, z}].push_back(
                            static_cast<std::int32_t>(i));
                    }
                }
            }
        }
        return lookup;
    }

    std::int32_t Q3Import::Cell(float value) noexcept
    {
        return ConvertToInt32Net9(std::floor(value / BrushCellSize));
    }

    bool Q3Import::IsBuried(
        const std::vector<Vector3>& points,
        Vector3 normal,
        std::int32_t owner,
        const std::vector<std::vector<Vector4>>& brushes,
        const std::vector<BrushBounds>& bounds,
        const BrushLookup& lookup)
    {
        Vector3 centre(0.0F, 0.0F, 0.0F);
        for (const Vector3 point : points)
        {
            centre = Add(centre, point);
        }
        centre = Divide(
            centre, static_cast<float>(points.size()));
        const Vector3 offset = normal.Normalized();

        for (std::size_t i = 0; i < points.size(); ++i)
        {
            const Vector3 edge = Divide(
                Add(points[i], points[(i + 1) % points.size()]),
                2.0F);

            const Vector3 cornerSample = Add(
                Add(
                    points[i],
                    Multiply(
                        Subtract(centre, points[i]),
                        0.15F)),
                offset);
            const Vector3 edgeSample = Add(
                Add(
                    edge,
                    Multiply(
                        Subtract(centre, edge),
                        0.15F)),
                offset);

            if (!Covered(
                    cornerSample, owner,
                    brushes, bounds, lookup)
                || !Covered(
                    edgeSample, owner,
                    brushes, bounds, lookup))
            {
                return false;
            }
        }
        return Covered(
            Add(centre, offset),
            owner, brushes, bounds, lookup);
    }

    bool Q3Import::Covered(
        Vector3 point,
        std::int32_t owner,
        const std::vector<std::vector<Vector4>>& brushes,
        const std::vector<BrushBounds>& bounds,
        const BrushLookup& lookup)
    {
        const auto iterator = lookup.find(
            CellKey{Cell(point.X), Cell(point.Y), Cell(point.Z)});
        if (iterator == lookup.end())
        {
            return false;
        }

        for (const std::int32_t index : iterator->second)
        {
            if (index == owner)
            {
                continue;
            }

            const BrushBounds& bound = ListAt(bounds, index);
            if (point.X < bound.Min.X - 1.0F
                || point.X > bound.Max.X + 1.0F
                || point.Y < bound.Min.Y - 1.0F
                || point.Y > bound.Max.Y + 1.0F
                || point.Z < bound.Min.Z - 1.0F
                || point.Z > bound.Max.Z + 1.0F)
            {
                continue;
            }

            bool inside = true;
            const std::vector<Vector4>& planes
                = ListAt(brushes, index);
            for (const Vector4 plane : planes)
            {
                if (plane.X * point.X
                    + plane.Y * point.Y
                    + plane.Z * point.Z
                    - plane.W > -0.1F)
                {
                    inside = false;
                    break;
                }
            }
            if (inside)
            {
                return true;
            }
        }
        return false;
    }

    std::vector<Vector3> Q3Import::MakeSheet(
        Vector3 normal, float distance)
    {
        const Vector3 axis = std::fabs(normal.Z) < 0.9F
            ? Vector3(0.0F, 0.0F, 1.0F)
            : Vector3(1.0F, 0.0F, 0.0F);
        const Vector3 right
            = Vector3::Cross(axis, normal).Normalized();
        const Vector3 up
            = Vector3::Cross(normal, right).Normalized();
        const Vector3 centre = Multiply(normal, distance);
        constexpr float Extent = 65536.0F;

        return std::vector<Vector3>{
            Subtract(Subtract(centre, Multiply(right, Extent)), Multiply(up, Extent)),
            Subtract(Add(centre, Multiply(right, Extent)), Multiply(up, Extent)),
            Add(Add(centre, Multiply(right, Extent)), Multiply(up, Extent)),
            Add(Subtract(centre, Multiply(right, Extent)), Multiply(up, Extent))
        };
    }

    std::vector<Vector3> Q3Import::Clip(
        const std::vector<Vector3>& points,
        Vector3 normal,
        float distance)
    {
        constexpr float Epsilon = 0.01F;
        std::vector<Vector3> result;
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            const Vector3 current = points[i];
            const Vector3 next
                = points[(i + 1) % points.size()];
            const float distCurrent
                = Vector3::Dot(normal, current) - distance;
            const float distNext
                = Vector3::Dot(normal, next) - distance;

            if (distCurrent <= Epsilon)
            {
                result.push_back(current);
            }
            if ((distCurrent > Epsilon) != (distNext > Epsilon)
                && std::fabs(distCurrent - distNext) > 1.0e-6F)
            {
                result.push_back(Add(
                    current,
                    Multiply(
                        Subtract(next, current),
                        distCurrent / (distCurrent - distNext))));
            }
        }
        return result;
    }

    std::vector<Vector3> Q3Import::Weld(
        const std::vector<Vector3>& points)
    {
        std::vector<Vector3> result;
        for (const Vector3 point : points)
        {
            bool found = false;
            for (const Vector3 existing : result)
            {
                if (LengthSquared(
                        Subtract(existing, point)) < 0.0004F)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                result.push_back(point);
            }
        }
        return result;
    }

    void Q3Import::AddEntities(
        BuiltMap* map,
        MapDefinition* def,
        Q3Bsp* bsp,
        MapImport* import,
        bool verbose)
    {
        if (map == nullptr || def == nullptr
            || bsp == nullptr || import == nullptr)
        {
            NullReference();
        }

        std::unordered_map<
            std::string,
            Vector3,
            Q3StringHash,
            Q3StringEqual> targets;

        for (const std::shared_ptr<Q3Entity>& entityValue : bsp->Entities())
        {
            const Q3Entity* entity = Require(entityValue);
            const std::string* name
                = EntityValue(entity, "targetname");
            const std::string* origin
                = EntityValue(entity, "origin");
            if (name != nullptr && origin != nullptr)
            {
                targets[*name] = ToWorld(
                    ParseVector(*origin).get(),
                    import->UnitsPerUnit());
            }
        }

        std::int32_t pads = 0;
        std::int32_t items = 0;
        for (const std::shared_ptr<Q3Entity>& entityValue : bsp->Entities())
        {
            const Q3Entity* entity = Require(entityValue);
            const std::string* classname
                = EntityValue(entity, "classname");
            if (classname == nullptr)
            {
                continue;
            }

            if (EqualsOrdinalIgnoreCase(
                    *classname, "info_player_deathmatch")
                || EqualsOrdinalIgnoreCase(
                    *classname, "info_player_start"))
            {
                const std::string* origin = nullptr;
                if (!import->KeepSpawns()
                    || (origin = EntityValue(entity, "origin")) == nullptr)
                {
                    continue;
                }

                float angle = 0.0F;
                const std::string* value
                    = EntityValue(entity, "angle");
                float parsed = 0.0F;
                if (value != nullptr
                    && TryParseSingleInvariant(*value, parsed))
                {
                    angle = parsed;
                }

                const Vector3 position = ToWorld(
                    ParseVector(*origin).get(),
                    import->UnitsPerUnit());

                MapDefinition::SpawnList* spawns = def->Spawns();
                if (spawns == nullptr)
                {
                    NullReference();
                }
                auto spawn = std::make_shared<MapSpawn>();
                spawn->Position(std::make_shared<std::vector<float>>(
                    std::initializer_list<float>{
                        position.X,
                        position.Y - 24.0F / import->UnitsPerUnit(),
                        position.Z}));
                spawn->Yaw(90.0F + angle);
                spawns->push_back(std::move(spawn));
            }
            else if (EqualsOrdinalIgnoreCase(
                *classname, "trigger_push"))
            {
                const std::string* model
                    = EntityValue(entity, "model");
                const std::string* target
                    = EntityValue(entity, "target");
                if (model == nullptr
                    || model->empty()
                    || model->front() != '*'
                    || target == nullptr)
                {
                    continue;
                }

                const auto targetIterator = targets.find(*target);
                if (targetIterator == targets.end())
                {
                    continue;
                }
                const Vector3 destination = targetIterator->second;

                std::int32_t modelIndex = 0;
                if (!TryParseInt32(
                        std::string_view(*model).substr(1),
                        modelIndex)
                    || modelIndex < 0
                    || static_cast<std::size_t>(modelIndex)
                        >= bsp->Models().size())
                {
                    continue;
                }

                Q3Model* volume
                    = Require(ListAt(bsp->Models(), modelIndex));
                const Vector3 min = ToWorld(
                    Require(volume->Mins()),
                    import->UnitsPerUnit());
                const Vector3 max = ToWorld(
                    Require(volume->Maxs()),
                    import->UnitsPerUnit());
                const Vector3 lower = ComponentMin(min, max);
                const Vector3 upper = ComponentMax(min, max);
                const Vector3 centre(
                    (lower.X + upper.X) / 2.0F,
                    lower.Y,
                    (lower.Z + upper.Z) / 2.0F);

                MapDefinition::JumpPadList* jumpPads
                    = def->JumpPads();
                if (jumpPads == nullptr)
                {
                    NullReference();
                }
                auto pad = std::make_shared<MapJumpPad>();
                pad->Position(std::make_shared<std::vector<float>>(
                    std::initializer_list<float>{
                        centre.X, centre.Y, centre.Z}));
                pad->Target(std::make_shared<std::vector<float>>(
                    std::initializer_list<float>{
                        destination.X, destination.Y, destination.Z}));
                pad->Size(std::make_shared<std::vector<float>>(
                    std::initializer_list<float>{
                        MathMax(upper.X - lower.X, 0.8F),
                        MathMax(upper.Y - lower.Y, 0.8F),
                        MathMax(upper.Z - lower.Z, 0.8F)}));
                jumpPads->push_back(std::move(pad));
                ++pads;
            }
            else
            {
                const std::string* itemOrigin
                    = EntityValue(entity, "origin");
                if (itemOrigin == nullptr)
                {
                    continue;
                }

                const ItemType type
                    = MapItemType(*classname);
                if (type == ItemType::None)
                {
                    continue;
                }

                const Vector3 position = ToWorld(
                    ParseVector(*itemOrigin).get(),
                    import->UnitsPerUnit());

                MapDefinition::ItemList* mapItems = def->Items();
                if (mapItems == nullptr)
                {
                    NullReference();
                }
                auto item = std::make_shared<MapItem>();
                item->Position(std::make_shared<std::vector<float>>(
                    std::initializer_list<float>{
                        position.X, position.Y, position.Z}));
                item->Type(ItemTypeToString(type));
                mapItems->push_back(std::move(item));
                ++items;
            }
        }

        if (verbose)
        {
            MapDefinition::SpawnList* spawns = def->Spawns();
            if (spawns == nullptr)
            {
                NullReference();
            }
            std::cout
                << "  " << spawns->size()
                << " spawns, " << pads
                << " jump pads, " << items
                << " items\n";
        }

        MapBuilder::AddEntities(map, def);
    }

    ItemType Q3Import::MapItemType(
        const std::string& classname) noexcept
    {
        const auto equal = [&classname](const char* value) noexcept
        {
            return Q3StringEqual{}(classname, value);
        };

        if (equal("weapon_railgun")) return ItemType::Imperialist;
        if (equal("weapon_rocketlauncher")) return ItemType::Magmaul;
        if (equal("weapon_lightning")) return ItemType::ShockCoil;
        if (equal("weapon_plasmagun")) return ItemType::VoltDriver;
        if (equal("weapon_shotgun")) return ItemType::Battlehammer;
        if (equal("weapon_grenadelauncher")) return ItemType::Judicator;
        if (equal("weapon_bfg")) return ItemType::OmegaCannon;
        if (equal("item_quad")) return ItemType::DoubleDamage;
        if (equal("item_invis")) return ItemType::Cloak;
        if (equal("item_health")) return ItemType::HealthMedium;
        if (equal("item_health_small")) return ItemType::HealthSmall;
        if (equal("item_health_large")) return ItemType::HealthBig;
        if (equal("item_health_mega")) return ItemType::HealthBig;
        if (equal("item_armor_shard")) return ItemType::UASmall;
        if (equal("item_armor_combat")) return ItemType::UABig;
        if (equal("item_armor_body")) return ItemType::UABig;
        if (equal("ammo_rockets")) return ItemType::MissileBig;
        if (equal("ammo_slugs")) return ItemType::UABig;
        if (equal("ammo_cells")) return ItemType::UASmall;
        if (equal("ammo_shells")) return ItemType::UASmall;
        if (equal("ammo_bullets")) return ItemType::UASmall;
        if (equal("ammo_grenades")) return ItemType::MissileSmall;
        if (equal("ammo_lightning")) return ItemType::UASmall;
        return ItemType::None;
    }

    std::shared_ptr<std::vector<float>> Q3Import::ParseVector(
        const std::string& value)
    {
        auto result = std::make_shared<std::vector<float>>(3, 0.0F);

        std::size_t index = 0;
        std::int32_t partIndex = 0;
        while (index < value.size() && partIndex < 3)
        {
            while (index < value.size() && value[index] == ' ')
            {
                ++index;
            }
            if (index >= value.size())
            {
                break;
            }

            const std::size_t start = index;
            while (index < value.size() && value[index] != ' ')
            {
                ++index;
            }

            float parsed = 0.0F;
            (void)TryParseSingleInvariant(
                std::string_view(value).substr(start, index - start),
                parsed);
            (*result)[static_cast<std::size_t>(partIndex)] = parsed;
            ++partIndex;
        }
        return result;
    }

    Vector3 Q3Import::ToWorld(
        const std::vector<float>* position, float unit)
    {
        const float x = ArrayAt(position, 0);
        const float y = ArrayAt(position, 2);
        const float z = -ArrayAt(position, 1);
        return Vector3(x / unit, y / unit, z / unit);
    }

    Vector3 Q3Import::ToWorld(
        Vector3 position, float unit) noexcept
    {
        return Vector3(
            position.X / unit,
            position.Z / unit,
            -position.Y / unit);
    }

    Vector3 Q3Import::ToDirection(
        const std::vector<float>* direction)
    {
        const Vector3 result(
            ArrayAt(direction, 0),
            ArrayAt(direction, 2),
            -ArrayAt(direction, 1));
        return LengthSquared(result) < 0.0001F
            ? Vector3(0.0F, 1.0F, 0.0F)
            : result.Normalized();
    }
}
