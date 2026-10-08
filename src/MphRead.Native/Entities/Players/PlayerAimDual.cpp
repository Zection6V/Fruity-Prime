#include "PlayerEntity.hpp"
#include "../../Scene.hpp"
#include <cmath>
#include "../../Mods/Gameplay/NativeGameplayClock.hpp"

namespace MphRead::Entities
{
    namespace Input = Mods::Input;
    namespace Native = Mods::Input::NativeAim;
    void PlayerEntity::ApplyDualAim(bool alt)
    {
        auto controls = _controls.NativeControl();
        // Only the strict ROM cadence waits for the native tick (scene time,
        // so a Touch reset cannot move its phase). Otherwise every 60 Hz step
        // responds with half-step rates: the same curve in real time.
        const bool rom = _aimFrame.Native && controls.RomCadence;
        if (rom && !Mods::Gameplay::NativeGameplayClock::IsNativeTick(_scene->FrameCount())) return;
        controls.AutoPitchTimer = _timeSinceInput / 2;
        controls.AutoPitchLimit = static_cast<std::uint32_t>(_values.SwayStartTime);
        std::uint16_t held = 0;
        if (_controls.AimLeft().IsDown()) held |= 1;
        if (_controls.AimRight().IsDown()) held |= 2;
        if (_controls.AimUp().IsDown()) held |= 4;
        if (_controls.AimDown().IsDown()) held |= 8;
        if (_controls.MoveLeft().IsDown()) held |= 0x100;
        if (_controls.MoveRight().IsDown()) held |= 0x200;
        if (_controls.MoveUp().IsDown()) held |= 0x400;
        if (_controls.MoveDown().IsDown()) held |= 0x800;
        _nativeInputSlot.Produce(held, true, controls.Flag84E);
        _nativeInputSlot.Bytes[0x34] = static_cast<std::uint8_t>((_input.Touch().Down ? 1 : 0) | (_input.Touch().Continued ? 8 : 0));
        _nativeInputShadow = _nativeInputSlot;
        const float scale = rom ? 1.0F : 0.5F;
        _buttonAimX = _nativeDual.X * scale;
        _buttonAimY = _nativeDual.Y * scale;
        _aimFrame.Yaw = _buttonAimX; _aimFrame.Pitch = _buttonAimY;
        // Native Human old E4/E8 always precede the producer, including zero.
        UpdateAimX(_buttonAimX); UpdateAimY(_buttonAimY);
        const auto form = alt ? Native::Form::AltStrafe : Native::Form::Biped;
        if (_nativeDual.Produce(_nativeInputShadow, controls, form, !rom) && _aimFrame.Native)
        {
            // Two 60 Hz steps return as far as one native tick: 1-(1-k)^(1/2).
            const float k = rom ? controls.AutoPitchScale : 1.0F - std::sqrt(1.0F - controls.AutoPitchScale);
            UpdateAimY(-_aimY * k);
        }
        _input.HasInput |= held != 0;
    }
}
