#include "ThumbnailMode.hpp"

#include "../Sound/Music.hpp"
#include "../Sound/Sfx.hpp"

namespace MphRead::Mods
{
    bool ThumbnailMode::_active = false;
    float ThumbnailMode::_sfxVolume = 0.35f;
    float ThumbnailMode::_musicVolume = 1.0f;

    bool ThumbnailMode::Active() noexcept
    {
        return _active;
    }

    void ThumbnailMode::Enter()
    {
        if (Active())
        {
            return;
        }
        _sfxVolume = Sound::Sfx::Volume();
        _musicVolume = Music::UserVolume();
        _active = true;
        Sound::Sfx::Volume(0.0f);
        Music::UserVolume(0.0f);
    }

    void ThumbnailMode::Exit()
    {
        if (!Active())
        {
            return;
        }
        _active = false;
        Sound::Sfx::Volume(_sfxVolume);
        Music::SetUserVolume(_musicVolume);
    }
}
