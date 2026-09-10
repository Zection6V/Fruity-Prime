#pragma once

namespace MphRead::Mods
{
    class ThumbnailMode final
    {
    public:
        ThumbnailMode() = delete;
        ThumbnailMode(const ThumbnailMode&) = delete;
        ThumbnailMode& operator=(const ThumbnailMode&) = delete;

        [[nodiscard]] static bool Active() noexcept;
        static void Enter();
        static void Exit();

    private:
        static bool _active;
        static float _sfxVolume;
        static float _musicVolume;
    };
}
