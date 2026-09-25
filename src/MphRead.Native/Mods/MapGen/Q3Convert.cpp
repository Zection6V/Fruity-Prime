#include "Q3Convert.hpp"
#include "NativeRuntime/System/Charconv.hpp"

#include "../../Formats/Enums.hpp"
#include "CustomRooms.hpp"
#include "MapBuilder.hpp"
#include "MapDefinition.hpp"
#include "MapTextureBake.hpp"
#include "Q3Bsp.hpp"
#include "../../Formats/Types.hpp"
#include "../../NativeRuntime/System/Encoding.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/IO.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "NativeRuntime/System/Globalization.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <memory>
#include <numbers>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#endif

using ::MphRead::NativeRuntime::AppendUtf8;
using ::MphRead::NativeRuntime::DecodeUtf8Scalar;
using ::MphRead::NativeRuntime::DirectoryCreateDirectory;
using ::MphRead::NativeRuntime::FileExists;
using ::MphRead::NativeRuntime::ManagedAt;
using ::MphRead::NativeRuntime::ManagedListAt;
using ::MphRead::NativeRuntime::MathMax;
using ::MphRead::NativeRuntime::MathMin;
using ::MphRead::NativeRuntime::PathCombine;
using ::MphRead::NativeRuntime::PathFromUtf8;
using ::MphRead::NativeRuntime::PathGetFileName;
using ::MphRead::NativeRuntime::PathGetFullPath;
using ::MphRead::NativeRuntime::PathToUtf8;
using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::NativeRuntime::RoundToEven;
using ::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::Utf16ToUtf8;
using ::MphRead::NativeRuntime::Utf8Scalar;
using ::MphRead::NativeRuntime::WideToUtf8;

namespace
{
    using MphRead::ItemType;
    using MphRead::Mods::MapGen::MapDefinition;
    using MphRead::Mods::MapGen::Q3Entity;
    using MphRead::Mods::MapGen::Q3StringEqual;

    void ValidatePathText(const std::string& path)
    {
        if (path.find('\0') != std::string::npos)
        {
            throw System::ArgumentException();
        }
    }

    [[nodiscard]] bool WindowsEffectivelyEmpty(
        const std::string& path) noexcept
    {
#if defined(_WIN32)
        return !path.empty()
            && std::all_of(
                path.begin(), path.end(),
                [](char ch) noexcept { return ch == ' '; });
#else
        (void)path;
        return false;
#endif
    }

    void CopyFile(
        const std::string& source,
        const std::string& destination)
    {
        (void)std::filesystem::copy_file(
            PathFromUtf8(source),
            PathFromUtf8(destination),
            std::filesystem::copy_options::overwrite_existing);
    }

    [[nodiscard]] const std::string* EntityValue(
        const Q3Entity* entity,
        std::string_view key)
    {
        if (entity == nullptr)
        {
            throw System::NullReferenceException();
        }
        for (const auto& pair : *entity)
        {
            if (::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase(pair.first, key))
            {
                return std::addressof(pair.second);
            }
        }
        return nullptr;
    }

    [[nodiscard]] double AverageAxis(
        const std::vector<std::shared_ptr<std::vector<float>>>& values,
        std::size_t axis)
    {
        if (values.empty())
        {
            throw System::InvalidOperationException("Sequence contains no elements");
        }

        double sum = static_cast<double>(
            ManagedAt(&RequireReference(values.front()), axis));
        std::int64_t count = 1;
        for (std::size_t i = 1; i < values.size(); ++i)
        {
            sum += static_cast<double>(
                ManagedAt(&RequireReference(values[i]), axis));
            count = std::bit_cast<std::int64_t>(
                static_cast<std::uint64_t>(count) + 1U);
        }
        return sum / static_cast<double>(count);
    }

    [[nodiscard]] std::string JoinMultiplayerItems(
        const MphRead::Mods::MapGen::ItemTypeHashSet& values)
    {
        std::string result;
        bool first = true;
        for (const ItemType value : values)
        {
            if (!first)
            {
                result += ", ";
            }
            first = false;
            result += ::MphRead::ToString(value);
        }
        return result;
    }

    [[nodiscard]] std::string JoinStrings(
        const std::vector<std::string>& values,
        std::size_t count)
    {
        std::string result;
        const std::size_t limit = std::min(count, values.size());
        for (std::size_t i = 0; i < limit; ++i)
        {
            if (i != 0)
            {
                result += ", ";
            }
            result += values[i];
        }
        return result;
    }
}

namespace MphRead::Mods::MapGen
{
    std::int32_t Q3Convert::Run(
        const std::string& source,
        const std::optional<std::string>& mapName,
        const std::optional<std::string>& roomName,
        const std::optional<std::string>& outputDir,
        bool dropClip,
        const std::optional<float>& forcedScale,
        std::int32_t textureSize)
    {
        if (!FileExists(source))
        {
            std::cout
                << "No such file: "
                << source
                << '\n';
            return 1;
        }

        const std::string& sourceValue = source;
        std::shared_ptr<Q3Bsp> bsp;
        try
        {
            bsp = Q3Bsp::Load(sourceValue, mapName);
        }
        catch (const std::exception& exception)
        {
            std::cout << exception.what() << '\n';
            return 1;
        }

        std::optional<std::string> selectedMapName = mapName;
        if (!selectedMapName.has_value())
        {
            const std::vector<std::string> maps
                = Q3Bsp::ListMaps(sourceValue);
            if (!maps.empty())
            {
                selectedMapName = maps.front();
            }
        }

        const std::string roomValue = roomName.has_value()
            ? *roomName
            : selectedMapName.has_value()
                ? *selectedMapName
                : "CUSTOM";
        const std::string room = ::MphRead::NativeRuntime::ToUpperInvariant(roomValue);
        const std::string prefix = ::MphRead::NativeRuntime::ToLowerInvariant(room);
        const std::string directory = outputDir.has_value()
            ? *outputDir
            : PathCombine(CustomRooms::MapDirectory(), prefix);
        DirectoryCreateDirectory(directory);

        std::shared_ptr<std::vector<float>> min;
        std::shared_ptr<std::vector<float>> max;
        Bounds(&RequireReference(bsp), min, max, false);
        if (ManagedAt(min.get(), 0) > ManagedAt(max.get(), 0))
        {
            std::cout
                << (selectedMapName.has_value()
                    ? *selectedMapName
                    : std::string())
                << " has no drawn surfaces.\n";
            return 1;
        }

        const float xExtent
            = ManagedAt(max.get(), 0) - ManagedAt(min.get(), 0);
        const float yExtent
            = ManagedAt(max.get(), 1) - ManagedAt(min.get(), 1);
        const float zExtent
            = ManagedAt(max.get(), 2) - ManagedAt(min.get(), 2);
        const float widest = MathMax(
            xExtent,
            MathMax(yExtent, zExtent));
        const float unit = forcedScale.has_value()
            ? *forcedScale
            : RoundToEven(MathMax(
                35.0F,
                widest / TargetExtent));

        std::shared_ptr<std::vector<float>> reachMin;
        std::shared_ptr<std::vector<float>> reachMax;
        Bounds(&RequireReference(bsp), reachMin, reachMax, true);

        const std::string levelName = PathGetFileName(sourceValue);
        const std::string beside
            = PathCombine(directory, levelName);
        const std::string besideFullPath = PathGetFullPath(beside);
        const std::string sourceFullPath = PathGetFullPath(sourceValue);
        if (besideFullPath != sourceFullPath)
        {
            CopyFile(sourceValue, beside);
        }

        const std::string texturePath
            = PathCombine(directory, prefix + ".tex");
        auto archivePaths = std::make_shared<
            std::vector<std::optional<std::string>>>();
        archivePaths->push_back(sourceValue);
        const std::shared_ptr<MapTextureBake::Result> baked
            = MapTextureBake::Bake(
                bsp,
                archivePaths,
                std::optional<std::string>(texturePath),
                textureSize);
        MapTextureBake::Result* bakedValue = &RequireReference(baked);

        std::cout
            << "  " << bakedValue->Baked
            << " textures at "
            << ::MphRead::NativeRuntime::ToString(textureSize) << 'x' << ::MphRead::NativeRuntime::ToString(textureSize)
            << " -> " << ::MphRead::NativeRuntime::ToString(bakedValue->Bytes, "N0")
            << " B  " << PathGetFileName(texturePath)
            << '\n';
        if (!RequireReference(bakedValue->Missing).empty())
        {
            std::cout
                << "  no image for " << RequireReference(bakedValue->Missing).size()
                << ": " << JoinStrings(*(&RequireReference(bakedValue->Missing)), 6)
                << (RequireReference(bakedValue->Missing).size() > 6 ? " ..." : "")
                << '\n';
            std::cout
                << "  those surfaces are dropped rather than painted with somebody else's"
                << " texture; pass another .pk3 in the same folder if it has them\n";
        }

        auto definition = std::make_shared<MapDefinition>();
        definition->Name(room);
        definition->InGameName(
            roomName.has_value()
                ? roomName
                : selectedMapName.has_value()
                    ? selectedMapName
                    : std::optional<std::string>(room));
        definition->ScaleFactor(
            ScaleFactor(reachMin.get(), reachMax.get(), unit));
        definition->KillHeight(
            RoundToEven(ManagedAt(min.get(), 2) / unit)
            - 5.0F);
        definition->FarClip(
            RoundToEven(MathMin(
                400.0F,
                widest / unit * 1.2F)));

        auto import = std::make_shared<MapImport>();
        import->Source(levelName);
        import->MapName(selectedMapName);
        import->UnitsPerUnit(unit);
        import->Textures(PathGetFileName(texturePath));
        import->KeepSky(true);
        import->KeepClip(!dropClip);
        import->KeepSpawns(true);
        definition->Import(import);

        std::int32_t clipBrushes = 0;
        for (const std::shared_ptr<Q3Brush>& brushRef
            : RequireReference(bsp).Brushes())
        {
            Q3Brush* brush = &RequireReference(brushRef);
            Q3Texture* solidTexture = &RequireReference(
                ManagedListAt(
                    RequireReference(bsp).Textures(),
                    brush->Texture()));
            if ((solidTexture->Contents()
                    & Q3Bsp::ContentsSolid) != 0)
            {
                continue;
            }
            Q3Texture* clipTexture = &RequireReference(
                ManagedListAt(
                    RequireReference(bsp).Textures(),
                    brush->Texture()));
            if ((clipTexture->Contents()
                    & Q3Bsp::ContentsPlayerClip) != 0)
            {
                if (clipBrushes
                    == std::numeric_limits<std::int32_t>::max())
                {
                    throw System::OverflowException();
                }
                ++clipBrushes;
            }
        }

        AddSpawns(
            &RequireReference(definition),
            &RequireReference(bsp),
            unit);

        const std::string path
            = PathCombine(directory, prefix + ".json");
        definition->Save(path);

        MapDefinition::SpawnList* outputSpawns
            = definition->Spawns();
        if (outputSpawns == nullptr)
        {
            throw System::NullReferenceException();
        }

        std::cout
            << "  " << outputSpawns->size()
            << " spawn points, "
            << ::MphRead::NativeRuntime::ToString(unit, "0.#")
            << " Quake units per unit"
            << " -> "
            << ::MphRead::NativeRuntime::ToString((ManagedAt(max.get(), 0)
                    - ManagedAt(min.get(), 0)) / unit, "0")
            << " x "
            << ::MphRead::NativeRuntime::ToString((ManagedAt(max.get(), 2)
                    - ManagedAt(min.get(), 2)) / unit, "0")
            << " x "
            << ::MphRead::NativeRuntime::ToString((ManagedAt(max.get(), 1)
                    - ManagedAt(min.get(), 1)) / unit, "0")
            << " units\n";
        std::cout << "  wrote " << path << '\n';

        MapDefinition::SpawnList* checkSpawns
            = definition->Spawns();
        if (checkSpawns == nullptr)
        {
            throw System::NullReferenceException();
        }
        if (checkSpawns->size() < 4)
        {
            MapDefinition::SpawnList* warningSpawns
                = definition->Spawns();
            if (warningSpawns == nullptr)
            {
                throw System::NullReferenceException();
            }
            std::cout
                << "  only " << warningSpawns->size()
                << " places to appear: this level was not"
                << " built for a deathmatch. Add spawns to the map file before playing it with a full house.\n";
        }
        if (clipBrushes > 0 && !dropClip)
        {
            std::cout
                << "  " << clipBrushes
                << " player-clip brushes kept. They are the level's invisible"
                << " walls; on a race map they fence the route. -noclip converts without them.\n";
        }
        std::cout
            << "  no weapons or powerups were placed: where those go decides how the map"
            << " plays. Add them under \"items\", from:\n";
        std::cout
            << "  "
            << JoinMultiplayerItems(MapBuilder::MultiplayerItems)
            << '\n';
        std::cout
            << "  then: FruityPrime -mapgen \""
            << room
            << "\"\n";
        return 0;
    }

    void Q3Convert::Bounds(
        Q3Bsp* bsp,
        std::shared_ptr<std::vector<float>>& min,
        std::shared_ptr<std::vector<float>>& max,
        bool sky)
    {
        if (bsp == nullptr)
        {
            throw System::NullReferenceException();
        }

        min = std::make_shared<std::vector<float>>(
            std::initializer_list<float>{
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max()});
        max = std::make_shared<std::vector<float>>(
            std::initializer_list<float>{
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest()});

        for (const std::shared_ptr<Q3Face>& faceRef
            : bsp->Faces())
        {
            Q3Face* face = &RequireReference(faceRef);
            if (face->Type() != 1
                && face->Type() != 2
                && face->Type() != 3)
            {
                continue;
            }

            Q3Texture* texture = &RequireReference(
                ManagedListAt(bsp->Textures(), face->Texture()));
            if ((texture->Flags()
                    & (Q3Bsp::SurfaceNoDraw
                        | Q3Bsp::SurfaceHint
                        | Q3Bsp::SurfaceSkip)) != 0
                || (!sky
                    && (texture->Flags()
                        & Q3Bsp::SurfaceSky) != 0))
            {
                continue;
            }

            for (std::int32_t i = face->Vertex();
                i < UncheckedAdd(
                    face->Vertex(),
                    face->VertexCount());
                i = UncheckedAdd(i, 1))
            {
                Q3Vertex* vertex = &RequireReference(
                    ManagedListAt(bsp->Vertices(), i));
                const std::vector<float>* position
                    = &RequireReference(vertex->Position());
                for (std::size_t axis = 0; axis < 3; ++axis)
                {
                    ManagedAt(min.get(), axis) = MathMin(
                        ManagedAt(min.get(), axis),
                        ManagedAt(position, axis));
                    ManagedAt(max.get(), axis) = MathMax(
                        ManagedAt(max.get(), axis),
                        ManagedAt(position, axis));
                }
            }
        }
    }

    std::int32_t Q3Convert::ScaleFactor(
        const std::vector<float>* min,
        const std::vector<float>* max,
        float unit)
    {
        float reach = 0.0F;
        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            reach = MathMax(
                reach,
                MathMax(
                    std::fabs(ManagedAt(min, axis)),
                    std::fabs(ManagedAt(max, axis)))
                    / unit);
        }

        std::int32_t factor = 0;
        while (8.0F
                * std::pow(
                    2.0F,
                    static_cast<float>(factor))
                < reach
            && factor < 10)
        {
            factor = UncheckedAdd(factor, 1);
        }
        return factor;
    }

    void Q3Convert::AddSpawns(
        MapDefinition* definition,
        Q3Bsp* bsp,
        float unit)
    {
        if (definition == nullptr || bsp == nullptr)
        {
            throw System::NullReferenceException();
        }

        std::vector<std::shared_ptr<std::vector<float>>> starts;
        std::vector<std::shared_ptr<std::vector<float>>> fallbacks;

        for (const std::shared_ptr<Q3Entity>& entityRef
            : bsp->Entities())
        {
            const Q3Entity* entity = &RequireReference(entityRef);
            const std::string* classname
                = EntityValue(entity, "classname");
            if (classname == nullptr)
            {
                continue;
            }
            const std::string* origin
                = EntityValue(entity, "origin");
            if (origin == nullptr)
            {
                continue;
            }

            std::shared_ptr<std::vector<float>> position
                = ParseVector(*origin);
            if (::MphRead::NativeRuntime::StringStartsWithOrdinalIgnoreCase(*classname, "info_player_deathmatch")
                || StringEqualsOrdinalIgnoreCase(
                    *classname,
                    "info_player_start"))
            {
                starts.push_back(std::move(position));
            }
            else if (::MphRead::NativeRuntime::StringStartsWithOrdinalIgnoreCase(*classname, "target_")
                || StringEqualsOrdinalIgnoreCase(
                    *classname,
                    "info_player_intermission"))
            {
                fallbacks.push_back(std::move(position));
            }
        }

        std::vector<std::shared_ptr<std::vector<float>>> concatenated;
        const std::vector<std::shared_ptr<std::vector<float>>>* chosen;
        if (starts.size() >= 4)
        {
            chosen = &starts;
        }
        else
        {
            if (starts.size()
                    > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())
                || fallbacks.size()
                    > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())
                        - starts.size())
            {
                throw System::OverflowException();
            }
            const std::size_t count = starts.size() + fallbacks.size();
            concatenated.reserve(count);
            concatenated.insert(
                concatenated.end(),
                starts.begin(),
                starts.end());
            concatenated.insert(
                concatenated.end(),
                fallbacks.begin(),
                fallbacks.end());
            chosen = &concatenated;
        }

        MapImport* import = definition->Import();
        if (import == nullptr)
        {
            throw System::NullReferenceException();
        }
        import->KeepSpawns(starts.size() >= 4);

        import = definition->Import();
        if (import == nullptr)
        {
            throw System::NullReferenceException();
        }
        if (import->KeepSpawns())
        {
            return;
        }

        auto centre = std::make_shared<std::vector<float>>(
            std::initializer_list<float>{
                chosen->empty()
                    ? 0.0F
                    : static_cast<float>(
                        AverageAxis(*chosen, 0)),
                chosen->empty()
                    ? 0.0F
                    : static_cast<float>(
                        AverageAxis(*chosen, 1))});

        for (const std::shared_ptr<std::vector<float>>& positionRef
            : *chosen)
        {
            const std::vector<float>* position
                = &RequireReference(positionRef);
            const float x = ManagedAt(position, 0) / unit;
            const float y = ManagedAt(position, 2) / unit
                - 24.0F / unit;
            const float z = -ManagedAt(position, 1) / unit;
            const float toCentre = std::atan2(
                ManagedAt(centre.get(), 0) / unit - x,
                -ManagedAt(centre.get(), 1) / unit - z);

            auto spawn = std::make_shared<MapSpawn>();
            spawn->Position(
                std::make_shared<std::vector<float>>(
                    std::initializer_list<float>{
                        Round(x),
                        Round(y),
                        Round(z)}));
            spawn->Yaw(
                Round(
                    toCentre
                    * 180.0F
                    / std::numbers::pi_v<float>));

            MapDefinition::SpawnList* spawns
                = definition->Spawns();
            if (spawns == nullptr)
            {
                throw System::NullReferenceException();
            }
            spawns->push_back(std::move(spawn));
        }
    }

    float Q3Convert::Round(float value) noexcept
    {
        constexpr float Power = 100.0F;
        constexpr float RoundLimit = 1.0e8F;
        if (std::fabs(value) < RoundLimit)
        {
            return RoundToEven(value * Power) / Power;
        }
        return value;
    }

    std::shared_ptr<std::vector<float>> Q3Convert::ParseVector(
        const std::string& value)
    {
        auto result = std::make_shared<std::vector<float>>(
            3, 0.0F);

        std::vector<std::string_view> parts;
        std::size_t position = 0;
        while (position < value.size())
        {
            while (position < value.size()
                && value[position] == ' ')
            {
                ++position;
            }
            if (position >= value.size())
            {
                break;
            }
            const std::size_t start = position;
            while (position < value.size()
                && value[position] != ' ')
            {
                ++position;
            }
            parts.emplace_back(
                value.data() + start,
                position - start);
        }

        const std::size_t count
            = std::min<std::size_t>(3, parts.size());
        for (std::size_t i = 0; i < count; ++i)
        {
            float parsed = 0.0F;
            (void)::MphRead::NativeRuntime::SingleTryParseInvariant(parts[i], parsed);
            ManagedAt(result.get(), i) = parsed;
        }
        return result;
    }
}
