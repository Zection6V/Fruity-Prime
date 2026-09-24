#include "GamepadInput.hpp"

#include "PadBindings.hpp"
#include "../InputSettings.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Entities/Players/PlayerInput.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <bit>
#include <cmath>
#include <cstdint>

using ::MphRead::NativeRuntime::MathMin;

namespace
{
    float DotNetAbs(float value) noexcept
    {
        const std::uint32_t bits = std::bit_cast<std::uint32_t>(value) & 0x7FFFFFFFU;
        return std::bit_cast<float>(bits);
    }

}

namespace MphRead::Mods::Input
{
    GamepadState GamepadInput::State{};
    GamepadButtons GamepadInput::_previous = GamepadButtons::None;
    GamepadButtons GamepadInput::_pressed = GamepadButtons::None;
    float GamepadInput::_aimDeltaX = 0.0F;
    float GamepadInput::_aimDeltaY = 0.0F;

    bool GamepadInput::Active() noexcept
    {
        return State.Connected;
    }

    bool GamepadInput::InUse()
    {
        if (!Active())
        {
            return false;
        }
        if (State.Buttons != GamepadButtons::None)
        {
            return true;
        }
        const auto [leftX, leftY] = ApplyDeadZone(State.LeftX, State.LeftY);
        const auto [rightX, rightY] = ApplyDeadZone(State.RightX, State.RightY);
        return leftX != 0.0F || leftY != 0.0F || rightX != 0.0F || rightY != 0.0F
            || State.LeftTrigger > TriggerThreshold
            || State.RightTrigger > TriggerThreshold;
    }

    float GamepadInput::AimDeltaX() noexcept
    {
        return _aimDeltaX;
    }

    float GamepadInput::AimDeltaY() noexcept
    {
        return _aimDeltaY;
    }

    void GamepadInput::BeginFrame()
    {
        if (!Active())
        {
            _pressed = GamepadButtons::None;
            _previous = GamepadButtons::None;
            _aimDeltaX = 0.0F;
            _aimDeltaY = 0.0F;
            return;
        }
        _pressed = static_cast<GamepadButtons>(
            static_cast<std::int32_t>(State.Buttons)
            & ~static_cast<std::int32_t>(_previous)
        );
        _previous = State.Buttons;
        const auto [x, y] = ApplyDeadZone(State.RightX, State.RightY);
        const float sensitivity = InputSettings::GamepadLookSensitivity();
        _aimDeltaX = -x * DotNetAbs(x) * TurnRate * sensitivity;
        _aimDeltaY = y * DotNetAbs(y) * TurnRate * sensitivity
            * (InputSettings::GamepadInvertY() ? -1 : 1);
    }

    std::pair<float, float> GamepadInput::ApplyDeadZone(float x, float y)
    {
        const float dead = InputSettings::GamepadDeadZone();
        const float length = std::sqrt(x * x + y * y);
        if (length <= dead)
        {
            return {0.0F, 0.0F};
        }
        if (length == 0.0F)
        {
            return {0.0F, 0.0F};
        }
        const float scaled = MathMin((length - dead) / (1.0F - dead), 1.0F);
        return {x / length * scaled, y / length * scaled};
    }

    bool GamepadInput::TakeMenuPress()
    {
        const GamepadButtons menu = PadBindings::Get(PadAction::Menu);
        if (menu == GamepadButtons::None
            || (static_cast<std::int32_t>(_pressed) & static_cast<std::int32_t>(menu)) == 0)
        {
            return false;
        }
        _pressed = static_cast<GamepadButtons>(
            static_cast<std::int32_t>(_pressed) & ~static_cast<std::int32_t>(menu)
        );
        return true;
    }

    bool GamepadInput::TakeChatPress()
    {
        const GamepadButtons chat = PadBindings::Get(PadAction::Chat);
        if (chat == GamepadButtons::None
            || (static_cast<std::int32_t>(_pressed) & static_cast<std::int32_t>(chat)) == 0)
        {
            return false;
        }
        _pressed = static_cast<GamepadButtons>(
            static_cast<std::int32_t>(_pressed) & ~static_cast<std::int32_t>(chat)
        );
        return true;
    }

    bool GamepadInput::TakePress(GamepadButtons buttons)
    {
        if ((static_cast<std::int32_t>(_pressed) & static_cast<std::int32_t>(buttons)) == 0)
        {
            return false;
        }
        _pressed = static_cast<GamepadButtons>(
            static_cast<std::int32_t>(_pressed) & ~static_cast<std::int32_t>(buttons)
        );
        return true;
    }

    void GamepadInput::Apply(Entities::PlayerEntity* player)
    {
        if (player == nullptr || !Active() || player->IsBot()
            || (player->LoadFlags() & Entities::LoadFlags::Active) == Entities::LoadFlags::None)
        {
            return;
        }
        auto& controls = player->Controls();
        const auto [moveX, moveY] = ApplyDeadZone(State.LeftX, State.LeftY);
        Hold(controls.MoveUp(), moveY > WalkThreshold);
        Hold(controls.RollUp(), moveY > WalkThreshold);
        Hold(controls.MoveDown(), moveY < -WalkThreshold);
        Hold(controls.RollDown(), moveY < -WalkThreshold);
        Hold(controls.MoveLeft(), moveX < -WalkThreshold);
        Hold(controls.RolltLeft(), moveX < -WalkThreshold);
        Hold(controls.MoveRight(), moveX > WalkThreshold);
        Hold(controls.RollRight(), moveX > WalkThreshold);

        const GamepadButtons shoot = PadBindings::Get(PadAction::Shoot);
        Hold(controls.Shoot(), shoot);
        Hold(controls.AltAttack(), shoot);
        Hold(controls.Zoom(), PadBindings::Get(PadAction::Zoom));
        const GamepadButtons jump = PadBindings::Get(PadAction::Jump);
        Hold(controls.Jump(), jump);
        Hold(controls.Boost(), jump);
        Hold(controls.Morph(), PadBindings::Get(PadAction::Morph));
        Hold(controls.Scan(), PadBindings::Get(PadAction::Scan));
        Hold(controls.ScanVisor(), PadBindings::Get(PadAction::ScanVisor));
        Hold(controls.Pause(), PadBindings::Get(PadAction::Scoreboard));

        Hold(controls.NextWeapon(), PadBindings::Get(PadAction::NextWeapon));
        Hold(controls.PrevWeapon(), PadBindings::Get(PadAction::PrevWeapon));
        Hold(controls.Missile(), PadBindings::Get(PadAction::Missile));
        Hold(controls.PowerBeam(), PadBindings::Get(PadAction::PowerBeam));

        if (InUse())
        {
            player->ModNoteInput();
        }
    }

    void GamepadInput::Hold(Entities::Keybind& bind, GamepadButtons buttons)
    {
        const bool down = (static_cast<std::int32_t>(State.Buttons)
            & static_cast<std::int32_t>(buttons)) != 0;
        const bool pressed = (static_cast<std::int32_t>(_pressed)
            & static_cast<std::int32_t>(buttons)) != 0;
        Hold(bind, down, pressed);
    }

    void GamepadInput::Hold(Entities::Keybind& bind, bool down)
    {
        Hold(bind, down, false);
    }

    void GamepadInput::Hold(Entities::Keybind& bind, bool down, bool pressed)
    {
        if (down)
        {
            bind.SetIsDown(true);
        }
        if (pressed)
        {
            bind.SetIsPressed(true);
        }
    }
}
