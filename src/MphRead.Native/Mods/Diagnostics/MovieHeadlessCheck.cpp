#include "MovieHeadlessCheck.hpp"

#include "Entities/PlayerSpawnEntity.hpp"
#include "Entities/Players/PlayerEntity.hpp"
#include "HomingCheckScene.hpp"
#include "Mods/Headless.hpp"
#include "NativeRuntime/System/Console.hpp"
#include "NativeRuntime/System/ExceptionText.hpp"
#include "Scene.hpp"
#include "Sound/Music.hpp"

#include <cstdint>
#include <exception>
#include <memory>
#include <string>

namespace MphRead::Mods::Diagnostics
{
    namespace
    {
        void DrainMusicLoad() noexcept
        {
            MusicPlayer::StopLoading(true);
            try
            {
                MusicPlayer::WaitForLoad();
                MusicPlayer::Remove(true);
            }
            catch (...)
            {
            }
        }
    }

    std::int32_t MovieHeadlessCheck::Run()
    {
        constexpr std::int32_t SimulationFrames = 240;
        try
        {
            std::uint64_t advanced = 0;
            std::uint32_t completed = 0;
            bool moviePlaying = false;
            bool updatePending = false;
            bool slot0InPlay = false;
            std::int32_t playerSpawnCount = 0;
            std::int32_t activePlayerSpawns = 0;
            {
                HomingCheckScene fixture("UNIT4_LAND", true, false, true);
                Scene& scene = fixture.Get();
                const std::uint64_t before = scene.FrameCount();
                for (std::int32_t frame = 0; frame < SimulationFrames; ++frame)
                {
                    fixture.Step();
                }
                advanced = scene.FrameCount() - before;
                completed = scene.MovieSuppressedCompletionCount();
                moviePlaying = scene.MoviePlaying();
                updatePending = scene.MovieUpdatePending();
                slot0InPlay = fixture.Shooter().ModIsInPlay();
                auto playerSpawns = scene.GetPlayerSpawnEntities().GetEnumerator();
                while (playerSpawns.MoveNext())
                {
                    ++playerSpawnCount;
                    if (playerSpawns.Current()->IsActive())
                    {
                        ++activePlayerSpawns;
                    }
                }
            }
            DrainMusicLoad();
            const bool passed = Headless::Active()
                && completed == 1
                && !moviePlaying
                && !updatePending
                && advanced == static_cast<std::uint64_t>(SimulationFrames)
                && slot0InPlay
                && activePlayerSpawns > 0;
            NativeRuntime::ConsoleWriteLine(std::string("MOVIE HEADLESS ")
                + (passed ? "PASS" : "FAIL")
                + " room=UNIT4_LAND suppressed_completions=" + std::to_string(completed)
                + " frames_advanced=" + std::to_string(advanced)
                + " movie_playing=" + std::to_string(moviePlaying)
                + " update_pending=" + std::to_string(updatePending)
                + " slot0_in_play=" + std::to_string(slot0InPlay)
                + " player_spawn_count=" + std::to_string(playerSpawnCount)
                + " active_player_spawns=" + std::to_string(activePlayerSpawns));
            return passed ? 0 : 1;
        }
        catch (...)
        {
            DrainMusicLoad();
            NativeRuntime::ConsoleWriteLine(
                "MOVIE HEADLESS FAIL " + NativeRuntime::ExceptionToString(std::current_exception()));
            return 1;
        }
    }
}
