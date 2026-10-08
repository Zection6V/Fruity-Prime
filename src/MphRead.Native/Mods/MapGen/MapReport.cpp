#include "MapReport.hpp"

#include "CustomRooms.hpp"
#include "MapDefinition.hpp"
#include "Q3Bsp.hpp"
#include "Q3Convert.hpp"
#include "Q3Import.hpp"
#include "../../NativeRuntime/System/IO.hpp"
#include "../../NativeRuntime/System/Number.hpp"
#include "../../Formats/Enums.hpp"
#include "../../Formats/Model.hpp"
#include "../../Read.hpp"
#include "../../Formats/Types.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/Encoding.hpp"
#include "../../NativeRuntime/System/ExceptionText.hpp"
#include "NativeRuntime/System/Globalization.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::NativeRuntime::UncheckedAdd;

namespace
{
    struct ShaderCount final
    {
        std::string Name;
        std::int32_t Count;
    };

    void AppendRightAligned(
        std::string& output, std::string_view value, std::size_t width)
    {
        output += ::MphRead::NativeRuntime::StringPadLeft(std::string(value), width);
    }

    void AppendLeftAligned(
        std::string& output, std::string_view value, std::size_t width)
    {
        output += ::MphRead::NativeRuntime::StringPadRight(std::string(value), width);
    }

    [[nodiscard]] std::string FormatTextureFormat(MphRead::TextureFormat value)
    {
        return ::MphRead::ToString(value);
    }

    [[nodiscard]] std::string FormatRenderMode(MphRead::RenderMode value)
    {
        return ::MphRead::ToString(value);
    }

    void WriteLine(std::string_view value)
    {
        ::MphRead::NativeRuntime::ConsoleWriteLine(value);
    }

    void TrimManagedWhitespaceEnd(std::string& value)
    {
        while (!value.empty())
        {
            const ::MphRead::NativeRuntime::Utf8Scalar last
                = ::MphRead::NativeRuntime::DecodeLastUtf8Scalar(value, value.size());
            if (!::MphRead::NativeRuntime::CharIsWhiteSpace(last.Value))
            {
                break;
            }
            value.resize(value.size() - last.Length);
        }
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
        catch (const std::exception&)
        {
            WriteLine(::MphRead::NativeRuntime::ExceptionMessage(std::current_exception()));
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
                = ::MphRead::NativeRuntime::ManagedListAt(bsp->Textures(), face.Texture());
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
        catch (const std::exception&)
        {
            std::string message = "Could not load ";
            message += room;
            message += ": ";
            message += ::MphRead::NativeRuntime::ExceptionMessage(std::current_exception());
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
            = ::MphRead::NativeRuntime::ManagedListAt(*model->Recolors, 0);

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
                = ::MphRead::NativeRuntime::ManagedListAt(*model->Materials, i);
            const Material& material = RequireReference(materialRef);

            std::string size = "no texture";
            std::string format;
            if (material.TextureId >= 0
                && material.TextureId
                    < static_cast<std::int32_t>(recolor->Textures->size()))
            {
                const Texture& texture
                    = ::MphRead::NativeRuntime::ManagedListAt(*recolor->Textures, material.TextureId);
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
            // How the texture is wrapped and placed: what a smeared surface is about.
            const auto repeat = [](RepeatMode mode)
            { return mode == RepeatMode::Clamp ? "clamp" : mode == RepeatMode::Mirror ? "mirror" : "repeat"; };
            line += std::string(" wrap ") + repeat(material.XRepeat) + "/" + repeat(material.YRepeat)
                + " texgen " + std::to_string(static_cast<int>(material.TexgenMode))
                + " scale " + ::MphRead::NativeRuntime::ToString(material.ScaleS) + "," + ::MphRead::NativeRuntime::ToString(material.ScaleT)
                + " anim " + std::to_string(material.TexcoordAnimationId);
            WriteLine(line);
        }
        return 0;
    }

    std::int32_t MapReport::ListItems(const std::string& target, std::optional<std::string> mapName,
        std::optional<float> forcedScale)
    {
        namespace Runtime = ::MphRead::NativeRuntime;
        MapDefinition* def = nullptr;
        for (const std::shared_ptr<MapDefinition>& candidate : CustomRooms::Definitions())
        {
            MapDefinition& definition = Runtime::RequireReference(candidate);
            if (Runtime::StringEqualsOrdinalIgnoreCase(definition.Name(), target)
                && definition.Import() != nullptr)
            {
                def = std::addressof(definition);
                break;
            }
        }
        std::string source;
        float unit = 0;
        if (def != nullptr)
        {
            MapImport* import = def->Import();
            const std::optional<std::string> resolved = import->Resolve();
            source = resolved.has_value() ? *resolved : import->Source();
            if (!mapName.has_value())
            {
                mapName = import->MapName();
            }
            unit = forcedScale.has_value() ? *forcedScale : import->UnitsPerUnit();
        }
        else
        {
            source = target;
        }
        std::shared_ptr<Q3Bsp> bsp;
        try
        {
            bsp = Q3Bsp::Load(source, mapName);
        }
        catch (const std::exception&)
        {
            WriteLine(Runtime::ExceptionMessage(std::current_exception()));
            return 1;
        }
        if (!bsp)
        {
            throw System::NullReferenceException();
        }
        if (unit <= 0)
        {
            unit = forcedScale.has_value() ? *forcedScale : Q3Convert::AutoScale(Q3Convert::WidestExtent(bsp.get()));
        }
        const std::vector<Q3Import::Q3Pickup> pickups = Q3Import::Pickups(bsp.get(), unit);
        const std::int32_t pickupCount = static_cast<std::int32_t>(pickups.size());
        const std::string label = def != nullptr ? def->Name() : mapName.has_value() ? *mapName : source;
        WriteLine(label + ": " + Runtime::ToString(pickupCount) + " pickups in "
            + (mapName.has_value() ? *mapName : Runtime::PathGetFileName(source)) + " at "
            + Runtime::ToString(unit, "0.#") + " Quake units per unit");
        if (def != nullptr)
        {
            const MapDefinition::ItemList& items = Runtime::RequireReference(def->Items());
            const std::int32_t own = static_cast<std::int32_t>(items.size());
            if (def->Import()->KeepItems())
            {
                WriteLine("  keepItems is on: these are added to the recipe's own " + Runtime::ToString(own)
                    + ", for " + Runtime::ToString(Runtime::UncheckedAdd(own, pickupCount)) + " in the room.");
            }
            else
            {
                WriteLine("  keepItems is off: none of these reach the room. It has the recipe's own "
                    + Runtime::ToString(own) + ".");
            }
        }
        if (pickups.empty())
        {
            WriteLine("  Nothing here is a pickup this game has an answer for.");
            return 0;
        }
        WriteLine({});
        // GroupBy in first-seen order, then a stable OrderByDescending(count).
        std::vector<std::vector<const Q3Import::Q3Pickup*>> groups;
        for (const Q3Import::Q3Pickup& pickup : pickups)
        {
            auto found = std::find_if(groups.begin(), groups.end(),
                [&pickup](const auto& group) { return group.front()->Classname == pickup.Classname; });
            if (found == groups.end())
            {
                groups.push_back({&pickup});
            }
            else
            {
                found->push_back(&pickup);
            }
        }
        std::stable_sort(groups.begin(), groups.end(),
            [](const auto& a, const auto& b) { return a.size() > b.size(); });
        const auto pad = [](std::string text, std::size_t width, bool right)
        {
            return right
                ? Runtime::StringPadLeft(std::move(text), width)
                : Runtime::StringPadRight(std::move(text), width);
        };
        for (const auto& group : groups)
        {
            const auto scripted = std::count_if(group.begin(), group.end(),
                [](const Q3Import::Q3Pickup* p) { return p->TargetName.has_value(); });
            const std::int32_t scriptedCount = static_cast<std::int32_t>(scripted);
            const std::string note = scripted == 0 ? std::string()
                : static_cast<std::size_t>(scripted) == group.size()
                    ? std::string("  handed out by the level's own scripts, not walked over")
                    : "  " + Runtime::ToString(scriptedCount) + " handed out by the level's own scripts, not walked over";
            std::string line = "  " + pad(Runtime::ToString(static_cast<std::int32_t>(group.size())), 4, true)
                + "  " + pad(group.front()->Classname, 24, false)
                + " " + pad(::MphRead::ToString(group.front()->Type), 14, false) + note;
            TrimManagedWhitespaceEnd(line);
            WriteLine(line);
        }
        WriteLine({});
        WriteLine("  \"items\": [");
        for (std::int32_t i = 0; i < pickupCount; i++)
        {
            const Q3Import::Q3Pickup& pickup = Runtime::ManagedListAt(pickups, i);
            const std::string comma = i < pickupCount - 1 ? "," : "";
            std::string line = "    { \"position\": [ " + Round(pickup.Position.X) + ", "
                + Round(pickup.Position.Y) + ", " + Round(pickup.Position.Z) + " ], \"type\": \""
                + ::MphRead::ToString(pickup.Type) + "\" }" + comma;
            if (pickup.TargetName.has_value())
            {
                line += "   // " + pickup.Classname + ", given by " + *pickup.TargetName;
            }
            WriteLine(line);
        }
        WriteLine("  ]");
        WriteLine({});
        WriteLine("  Paste that into the recipe and set \"keepItems\": false under \"import\",");
        WriteLine("  or the room gets one of each from the recipe and one from the level.");
        WriteLine("  A recipe may carry // comments, so those lines can go in as they are.");
        return 0;
    }

    // Two decimals, invariant, and never "-0".
    std::string MapReport::Round(float value)
    {
        float rounded = value;
        if (value > -1.0e8F && value < 1.0e8F)
        {
            rounded = ::MphRead::NativeRuntime::RoundToEven(value * 100.0F) / 100.0F;
        }
        return ::MphRead::NativeRuntime::ToStringInvariant(rounded == 0 ? 0.0F : rounded, "0.##");
    }
}
