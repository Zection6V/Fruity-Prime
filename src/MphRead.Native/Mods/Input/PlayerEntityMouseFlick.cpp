#include "PlayerEntityMouseFlick.hpp"

#include "MouseFlick.hpp"
#include "../SpectatorMode.hpp"
#include "../../Entities/CamSeq/CameraSequence.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Scene.hpp"

namespace MphRead::Entities
{
    void PlayerEntity::ModCheckMouseFlick()
    {
        if (!IsMainPlayer() || _isBot)
        {
            return;
        }
        Scene& scene = ::MphRead::NativeRuntime::RequireReference(_scene);
        Formats::CameraSequence* sequence = Formats::CameraSequence::Current();
        if (!_controls.MouseAim() || _controls.Boost().IsDown()
            || TestFlag(_flags1, PlayerFlags1::NoAimInput)
            || TestFlag(_flags1, PlayerFlags1::WeaponMenuOpen)
            || Mods::SpectatorMode::IsSpectating()
            || scene.FrameAdvance() || scene.FrameAdvanceLastFrame()
            || (sequence != nullptr && sequence->BlockInput()))
        {
            Mods::Input::MouseFlick::Reset();
            return;
        }
        float dirX = 0;
        float dirY = 0;
        if (Mods::Input::MouseFlick::Check(_input.MouseDeltaX(), _input.MouseDeltaY(), scene.FrameCount(), dirX, dirY))
        {
            _swipeBoostRequested = true;
            _swipeBoostX = dirX;
            _swipeBoostY = dirY;
        }
    }
}
