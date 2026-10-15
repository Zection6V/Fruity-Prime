#include "MapCommands.hpp"

#include "CommandLine.hpp"
#include "OptionValues.hpp"

#include "../../Formats/Enums.hpp"
#include "../../Formats/Formats.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/ExceptionText.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../NativeRuntime/System/Number.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/OpenTK/Mathematics.hpp"
#include "../Diagnostics/GpuLifetimeCheck.hpp"
#include "../MapGen/AltFormProbe.hpp"
#include "../MapGen/BotTour.hpp"
#include "../MapGen/MapBundle.hpp"
#include "../MapGen/CustomRooms.hpp"
#include "../MapGen/MapDefinition.hpp"
#include "../MapGen/MapCatalog.hpp"
#include "../MapGen/MapCheck.hpp"
#include "../MapGen/MapReport.hpp"
#include "../MapGen/MphTextures.hpp"
#include "../MapGen/MoverProbe.hpp"
#include "../MapGen/PadProbe.hpp"
#include "../MapGen/Q3Bsp.hpp"
#include "../Network/Testing/Maps/MapAudit.hpp"
#include "../ThumbnailBatch.hpp"
#include "../ThumbnailCapture.hpp"
#include "../ThumbnailGenerator.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

using ::MphRead::NativeRuntime::Int32TryParseCurrentCulture;
using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase;

namespace
{
    using MphRead::Mods::Entry::HasFlag;
    using MphRead::Mods::Entry::ValueAfter;

    void WriteLine(std::string_view value)
    {
        std::cout << value << '\n';
    }

    void SetExitCode(std::int32_t value) noexcept
    {
        MphRead::NativeRuntime::SetEnvironmentExitCode(value);
    }

    void GenerateThumbnails(const std::vector<std::string>& args, int width, int height)
    {
        using MphRead::Mods::ThumbnailBatch;
        using MphRead::Mods::ThumbnailGenerator;

        const bool force = HasFlag(args, "force");
        std::vector<std::string> rooms = force
            ? ThumbnailGenerator::MultiplayerRooms()
            : ThumbnailGenerator::MissingThumbnails();
        if (rooms.empty())
        {
            WriteLine("[thumbnails] all previews already present in "
                + ThumbnailGenerator::CacheDirectory());
            WriteLine("[thumbnails] pass -force to re-render them");
            return;
        }
        int jobs = ThumbnailBatch::DefaultParallelism();
        const std::optional<std::string> jobsValue = ValueAfter(args, "jobs");
        std::int32_t parsedJobs = 0;
        if (jobsValue.has_value() && Int32TryParseCurrentCulture(*jobsValue, parsedJobs))
        {
            jobs = parsedJobs;
        }
        WriteLine("[thumbnails] rendering " + std::to_string(rooms.size())
            + " preview(s) at " + std::to_string(width) + "x" + std::to_string(height)
            + ", " + std::to_string(jobs) + " at a time");
        WriteLine("[thumbnails] output: " + ThumbnailGenerator::CacheDirectory());
        const int written = ThumbnailBatch::Run(rooms, jobs, width, height);
        WriteLine("[thumbnails] done -- " + std::to_string(written) + "/"
            + std::to_string(rooms.size()) + " written");
    }
}

namespace MphRead::Mods::Entry
{
    bool TryRunMapBundle(const std::vector<std::string>& args)
    {
        if (HasFlag(args, "mapbundle"))
        {
            const std::optional<std::string> which = ValueAfter(args, "mapbundle");
            const std::optional<std::string> outPath = ValueAfter(args, "out");
            int cooked = 0;
            int failed = 0;
            for (const auto& definition : MapGen::CustomRooms::Definitions())
            {
                if (which.has_value() && !StringEqualsOrdinalIgnoreCase(*which, RequireReference(definition).Name())
                    && !StringEqualsOrdinalIgnoreCase(*which, "all"))
                {
                    continue;
                }
                if (!RequireReference(definition).SourcePath().has_value() || RequireReference(definition).BundlePath().has_value()
                    || RequireReference(definition).Import() == nullptr)
                {
                    continue;
                }
                try
                {
                    (void)MapGen::MapBundle::Cook(
                        definition.get(), *RequireReference(definition).SourcePath(), outPath);
                    cooked = ::MphRead::NativeRuntime::UncheckedAdd(cooked, 1);
                }
                catch (...)
                {
                    const std::exception_ptr error = std::current_exception();
                    ::MphRead::NativeRuntime::ConsoleWriteLine(
                        RequireReference(definition).Name() + ": "
                        + ::MphRead::NativeRuntime::ExceptionMessage(error));
                    failed = ::MphRead::NativeRuntime::UncheckedAdd(failed, 1);
                }
            }
            if (cooked == 0 && failed == 0)
            {
                ::MphRead::NativeRuntime::ConsoleWriteLine(
                    "No map to bundle. A bundle is cooked from a recipe and the level it converts; put both in "
                    + MapGen::CustomRooms::MapDirectory() + ".");
            }
            SetExitCode(failed == 0 ? 0 : 1);
            return true;
        }
        return false;
    }

    bool TryRunMapCommands(const std::vector<std::string>& args)
    {
        if (HasFlag(args, "mapgen"))
        {
            const std::optional<std::string> only = ValueAfter(args, "mapgen");
            const bool force = HasFlag(args, "force");
            (void)force;
            int count = 0;
            int failed = 0;
            for (const auto& definition : MapGen::CustomRooms::Definitions())
            {
                if (only.has_value() && !StringEqualsOrdinalIgnoreCase(*only, RequireReference(definition).Name())
                    && !StringEqualsOrdinalIgnoreCase(*only, "all"))
                {
                    continue;
                }
                try
                {
                    MapGen::CustomRooms::Build(definition.get(), true /* verbose */);
                    count = ::MphRead::NativeRuntime::UncheckedAdd(count, 1);
                }
                catch (...)
                {
                    const std::exception_ptr error = std::current_exception();
                    ::MphRead::NativeRuntime::ConsoleWriteLine(
                        RequireReference(definition).Name() + ": "
                        + ::MphRead::NativeRuntime::ExceptionMessage(error));
                    failed = ::MphRead::NativeRuntime::UncheckedAdd(failed, 1);
                }
            }
            if (count == 0 && failed == 0)
            {
                ::MphRead::NativeRuntime::ConsoleWriteLine("No maps to generate. Put a Quake 3 .pk3 or a map JSON in "
                    + MapGen::CustomRooms::MapDirectory() + ".");
            }
            SetExitCode(failed == 0 ? 0 : 1);
            return true;
        }

        const std::optional<std::string> q3Maps = ValueAfter(args, "q3maps");
        if (q3Maps.has_value())
        {
            try
            {
                for (const std::string& map : MapGen::Q3Bsp::ListMaps(*q3Maps))
                {
                    WriteLine(map);
                }
                SetExitCode(0);
            }
            catch (const std::exception& ex)
            {
                WriteLine("Could not read " + *q3Maps + ": " + ex.what());
                SetExitCode(1);
            }
            return true;
        }

        const std::optional<std::string> q3Shaders = ValueAfter(args, "q3shaders");
        if (q3Shaders.has_value())
        {
            SetExitCode(MapGen::MapReport::ListShaders(*q3Shaders, ValueAfter(args, "map")));
            return true;
        }

        const std::optional<std::string> mapCheck = ValueAfter(args, "mapcheck");
        if (mapCheck.has_value())
        {
            SetExitCode(MapGen::MapCheck::Run(*mapCheck));
            return true;
        }

        const std::optional<std::string> altProbe = ValueAfter(args, "altprobe");
        if (altProbe.has_value())
        {
            const std::vector<std::string> at = NativeRuntime::StringSplit(
                ValueAfter(args, "at").value_or(""), ',');
            float atX = 0.0F;
            float atY = 0.0F;
            float atZ = 0.0F;
            if (at.size() != 3
                || !TryParseFloatInvariant(std::optional<std::string>(at[0]), atX, false)
                || !TryParseFloatInvariant(std::optional<std::string>(at[1]), atY, false)
                || !TryParseFloatInvariant(std::optional<std::string>(at[2]), atZ, false))
            {
                WriteLine("-altprobe needs -at X,Y,Z");
                SetExitCode(1);
                return true;
            }
            const std::optional<std::string> traceDelay = ValueAfter(args, "delay");
            std::int32_t parsedDelay = -1;
            MapGen::AltFormProbe::TraceDelay(
                traceDelay.has_value() && Int32TryParseCurrentCulture(*traceDelay, parsedDelay)
                    && parsedDelay >= 0
                    ? std::optional<std::int32_t>(parsedDelay)
                    : std::nullopt);
            SetExitCode(MapGen::AltFormProbe::Run(*altProbe,
                OpenTK::Mathematics::Vector3(atX, atY, atZ), ParseHunter(args)));
            return true;
        }

        // -portalviews ["ROOM"|all]: render the views through a translated
        // level's portals here and now, and build the room again with them.
        if (HasFlag(args, "portalviews"))
        {
            const std::optional<std::string> only = ValueAfter(args, "portalviews");
            std::int32_t failed = 0;
            for (const auto& definition : MapGen::CustomRooms::Definitions())
            {
                if (definition->Portals().empty()
                    || (only.has_value() && !StringEqualsOrdinalIgnoreCase(*only, "all")
                        && !StringEqualsOrdinalIgnoreCase(*only, definition->Name())))
                {
                    continue;
                }
                const bool ok = ThumbnailCapture::CaptureRoom(definition->Name() + "#portals",
                    ThumbnailGenerator::PortalViewSize, ThumbnailGenerator::PortalViewSize);
                WriteLine(definition->Name() + ": " + std::to_string(definition->Portals().size()) + " portal views "
                    + (ok ? "rendered" : "NOT rendered"));
                if (ok)
                {
                    MapGen::CustomRooms::Build(definition.get(), true, true);
                }
                else
                {
                    failed++;
                }
            }
            SetExitCode(failed == 0 ? 0 : 1);
            return true;
        }

        // -shotat "ROOM" -at X,Y,Z -look X,Y,Z -out FILE.png [-size W,H]: the
        // room from one camera, for comparing with the level it came from.
        if (const std::optional<std::string> shotRoom = ValueAfter(args, "shotat"); shotRoom.has_value())
        {
            const auto vector = [&](const char* name, OpenTK::Mathematics::Vector3& value) -> bool
            {
                const std::vector<std::string> parts = NativeRuntime::StringSplit(ValueAfter(args, name).value_or(""), ',');
                return parts.size() == 3
                    && TryParseFloatInvariant(std::optional<std::string>(parts[0]), value.X, false)
                    && TryParseFloatInvariant(std::optional<std::string>(parts[1]), value.Y, false)
                    && TryParseFloatInvariant(std::optional<std::string>(parts[2]), value.Z, false);
            };
            ThumbnailCapture::View view;
            const std::optional<std::string> out = ValueAfter(args, "out");
            if (!vector("at", view.Position) || !vector("look", view.Target) || !out.has_value())
            {
                WriteLine("-shotat needs -at X,Y,Z -look X,Y,Z -out FILE.png");
                SetExitCode(1);
                return true;
            }
            view.Path = *out;
            std::int32_t width = 960;
            std::int32_t height = 540;
            if (const std::optional<std::string> size = ValueAfter(args, "size"))
            {
                const std::vector<std::string> parts = NativeRuntime::StringSplit(*size, ',');
                if (parts.size() == 2)
                {
                    (void)Int32TryParseCurrentCulture(parts[0], width);
                    (void)Int32TryParseCurrentCulture(parts[1], height);
                }
            }
            SetExitCode(ThumbnailCapture::CaptureViews(*shotRoom, {view}, width, height) ? 0 : 1);
            return true;
        }

        if (const std::optional<std::string> texDump = ValueAfter(args, "mphtexdump"); texDump.has_value())
        {
            SetExitCode(MapGen::MphTextures::Dump(*texDump));
            return true;
        }

        // -mapsync: fetches the server's maps the map folder lacks, now.
        if (HasFlag(args, "mapsync"))
        {
            const std::int32_t arrived = MapGen::MapCatalog::Sync(
                [](const std::string& pack, std::int32_t done, std::int32_t total)
                {
                    WriteLine("[maps] " + pack + " (" + std::to_string(done + 1) + "/" + std::to_string(total) + ")");
                });
            WriteLine("[maps] " + std::to_string(arrived) + " downloaded");
            SetExitCode(0);
            return true;
        }

        if (const std::optional<std::string> botTour = ValueAfter(args, "bottour"); botTour.has_value())
        {
            std::int32_t players = 6;
            if (const std::optional<std::string> value = ValueAfter(args, "players"))
            {
                (void)Int32TryParseCurrentCulture(*value, players);
            }
            double seconds = 120.0;
            (void)TryParseDoubleInvariant(ValueAfter(args, "seconds"), seconds);
            SetExitCode(MapGen::BotTour::Run(*botTour, players, seconds));
            return true;
        }

        if (const std::optional<std::string> moverProbe = ValueAfter(args, "moverprobe"); moverProbe.has_value())
        {
            SetExitCode(MapGen::MoverProbe::Run(*moverProbe));
            return true;
        }

        if (const std::optional<std::string> padProbe = ValueAfter(args, "padprobe"); padProbe.has_value())
        {
            SetExitCode(MapGen::PadProbe::Run(*padProbe, ParseHunter(args), HasFlag(args, "alt")));
            return true;
        }

        const std::optional<std::string> mapItems = ValueAfter(args, "mapitems");
        if (mapItems.has_value())
        {
            std::optional<float> itemScale;
            float parsedItemScale = 0.0F;
            if (TryParseFloatInvariant(ValueAfter(args, "scale"), parsedItemScale, false)
                && parsedItemScale > 0.0F)
            {
                itemScale = parsedItemScale;
            }
            SetExitCode(MapGen::MapReport::ListItems(
                *mapItems, ValueAfter(args, "map"), itemScale));
            return true;
        }

        const std::optional<std::string> mapMaterials = ValueAfter(args, "mapmaterials");
        if (mapMaterials.has_value())
        {
            SetExitCode(MapGen::MapReport::ListMaterials(*mapMaterials));
            return true;
        }
        return false;
    }

    bool TryRunMapAudits(const std::vector<std::string>& args)
    {
        if (HasFlag(args, "gpulifetime"))
        {
            std::string room = "TEST ARENA";
            const std::optional<std::string> named = ValueAfter(args, "gpulifetime");
            if (named.has_value() && !named->empty() && named->front() != '-')
            {
                room = *named;
            }
            std::int32_t cycles = 5;
            std::int32_t frames = 90;
            std::int32_t parsed = 0;
            if (const auto value = ValueAfter(args, "cycles"); value.has_value()
                && Int32TryParseCurrentCulture(*value, parsed) && parsed > 0)
            {
                cycles = parsed;
            }
            if (const auto value = ValueAfter(args, "frames"); value.has_value()
                && Int32TryParseCurrentCulture(*value, parsed) && parsed > 0)
            {
                frames = parsed;
            }
            SetExitCode(Diagnostics::GpuLifetimeCheck::Run(room, cycles, frames));
            return true;
        }

        if (HasFlag(args, "mapteststorymoviecycle"))
        {
            SetExitCode(Network::MapAudit::RunStoryMovieCycle());
            return true;
        }

        if (HasFlag(args, "mapteststorymovietransition"))
        {
            SetExitCode(Network::MapAudit::RunStoryMovieTransitionCycle());
            return true;
        }

        const std::optional<std::string> mapTest = ValueAfter(args, "maptest");
        if (mapTest.has_value())
        {
            int players = 8;
            const std::optional<std::string> playersValue = ValueAfter(args, "players");
            std::int32_t parsedPlayers = 0;
            if (playersValue.has_value() && Int32TryParseCurrentCulture(*playersValue, parsedPlayers))
            {
                players = parsedPlayers;
            }
            double seconds = 10;
            double parsedSeconds = 0;
            if (TryParseDoubleInvariant(ValueAfter(args, "seconds"), parsedSeconds))
            {
                seconds = parsedSeconds;
            }
            GameMode mode = GameMode::Battle;
            (void)TryParseGameMode(ValueAfter(args, "mode"), mode);
            Network::MapAudit::ShowWindow(HasFlag(args, "hudshots"));
            Network::MapAudit::TeamProbe(HasFlag(args, "teamprobe"));
            if (ValueAfter(args, "hunter").has_value())
            {
                Network::MapAudit::MainHunter(ParseHunter(args));
            }
            else
            {
                Network::MapAudit::MainHunter(std::nullopt);
            }
            const std::optional<std::string> drawRateValue = ValueAfter(args, "drawrate");
            std::int32_t drawRate = 0;
            if (drawRateValue.has_value() && Int32TryParseCurrentCulture(*drawRateValue, drawRate)
                && drawRate > 0)
            {
                Network::MapAudit::DrawRate(drawRate);
            }
            const std::optional<std::string> sizeValue = ValueAfter(args, "size");
            if (sizeValue.has_value())
            {
                const std::string lowered = ::MphRead::NativeRuntime::ToLowerInvariant(*sizeValue);
                std::vector<std::string_view> split;
                std::size_t start = 0;
                for (std::size_t i = 0; i <= lowered.size(); ++i)
                {
                    if (i == lowered.size() || lowered[i] == 'x')
                    {
                        split.emplace_back(lowered.data() + start, i - start);
                        start = i + 1;
                    }
                }
                std::int32_t auditWidth = 0;
                std::int32_t auditHeight = 0;
                if (split.size() == 2 && Int32TryParseCurrentCulture(split[0], auditWidth)
                    && Int32TryParseCurrentCulture(split[1], auditHeight)
                    && auditWidth > 0 && auditHeight > 0)
                {
                    Network::MapAudit::WindowSize(OpenTK::Mathematics::Vector2i(auditWidth, auditHeight));
                }
            }
            const bool movieChainProbe = HasFlag(args, "maptestmoviechain");
            const bool movieSkipProbe = HasFlag(args, "maptestmovieskip");
            const bool movieCloseProbe = HasFlag(args, "maptestmovieclose");
            const bool movieAudioCallProbe = HasFlag(args, "maptestmoviealcall");
            const bool cleanupExceptionProbe = HasFlag(args, "maptestcleanupexception");
            const bool partialInitializationProbe = HasFlag(args, "maptestpartialinit");
            const bool playMovies = HasFlag(args, "maptestmovies") || movieChainProbe || movieSkipProbe
                || movieCloseProbe || movieAudioCallProbe;
            SetExitCode(Network::MapAudit::Run(*mapTest, players, seconds, mode,
                HasFlag(args, "bots"), ValueAfter(args, "shots"),
                HasFlag(args, "renderprobe"), HasFlag(args, "allnodes"),
                HasFlag(args, "itemshots"), HasFlag(args, "itempickupcheck"),
                playMovies, movieChainProbe, movieSkipProbe, movieCloseProbe,
                cleanupExceptionProbe, partialInitializationProbe, movieAudioCallProbe));
            return true;
        }
        return false;
    }

    bool TryRunThumbnails(const std::vector<std::string>& args, int width, int height)
    {
        const std::vector<std::string> share = ValuesAfter(args, "thumbnail");
        if (!share.empty())
        {
            const int captured = ThumbnailCapture::CaptureRooms(share, width, height);
            WriteLine("[thumbnails] captured " + std::to_string(captured) + "/"
                + std::to_string(share.size()));
            return true;
        }

        if (HasFlag(args, "thumbnails"))
        {
            GenerateThumbnails(args, width, height);
            return true;
        }
        return false;
    }
}
