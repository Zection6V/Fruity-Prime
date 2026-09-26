#include "GamepadInput.hpp"

#include "AimInputSourceTracker.hpp"
#include "GamepadActions.hpp"
#include "GamepadAnalog.hpp"
#include "GamepadOptions.hpp"
#include "GamepadRuntimeConfig.hpp"
#include "GamepadUiRouter.hpp"
#include "PadBindings.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Entities/Players/PlayerInput.hpp"

namespace MphRead::Mods::Input
{
    GamepadEdges& GamepadInput::Edges()
    {
        static GamepadEdges edges{};
        return edges;
    }

    GamepadActions& GamepadInput::Actions()
    {
        static GamepadActions actions{};
        return actions;
    }

    GamepadState GamepadInput::State()
    {
        return GamepadManager::ActiveState();
    }

    bool GamepadInput::WheelHeld()
    {
        return _context == GamepadContext::Gameplay && Actions().WheelOpen();
    }

    std::pair<float, float> GamepadInput::AimStick()
    {
        return GamepadOptions::Southpaw()
            ? GamepadAnalog::ApplyRadialDeadZone(_frame.LeftX, _frame.LeftY, GamepadOptions::LeftInner(), GamepadOptions::LeftOuter())
            : GamepadAnalog::ApplyRadialDeadZone(_frame.RightX, _frame.RightY, GamepadOptions::RightInner(), GamepadOptions::RightOuter());
    }

    bool GamepadInput::Active()
    {
        return State().Connected;
    }

    bool GamepadInput::InUse()
    {
        const GamepadState state = State();
        if (!state.Connected)
        {
            return false;
        }
        const auto left = GamepadAnalog::ApplyRadialDeadZone(state.LeftX, state.LeftY, GamepadOptions::LeftInner(), GamepadOptions::LeftOuter());
        const auto right = GamepadAnalog::ApplyRadialDeadZone(state.RightX, state.RightY, GamepadOptions::RightInner(), GamepadOptions::RightOuter());
        const std::pair<float, float> zero{0.0F, 0.0F};
        return state.Buttons != GamepadButtons::None || left != zero || right != zero;
    }

    void GamepadInput::BeginFrame()
    {
        const GamepadSnapshot snapshot = GamepadManager::Snapshot();
        _frameSnapshot = snapshot;
        GamepadRuntimeConfig::Frame() = snapshot.Runtime;
        _frame = snapshot.State;
        const GamepadContext context = GamepadContexts::Current();
        _pressed = Edges().Update(snapshot);
        const std::int64_t contextRevision = GamepadContexts::Revision();
        if (_context != context || _revision != snapshot.Revision || _contextRevision != contextRevision
            || _bindingsRevision != PadBindings::Revision())
        {
            Actions().Reset();
            _bindingsRevision = PadBindings::Revision();
            _blocked = _frame.Buttons;
            _pressed = GamepadButtons::None;
        }
        _context = context;
        _revision = snapshot.Revision;
        _contextRevision = contextRevision;
        _blocked &= _frame.Buttons;
        _frame.Buttons &= ~_blocked;
        _aimDeltaX = _aimDeltaY = 0;
        const std::int32_t main = Entities::PlayerEntity::MainPlayerIndex();
        const auto& players = Entities::PlayerEntity::Players();
        if (context != GamepadContext::Gameplay || !GamepadContexts::Focused() || !_frame.Connected
            || (main >= 0 && main < static_cast<std::int32_t>(players.size())
                && players[static_cast<std::size_t>(main)] != nullptr
                && players[static_cast<std::size_t>(main)]->Health() == 0))
        {
            AimInputSourceTracker::Reset();
        }
        if (!GamepadContexts::Focused())
        {
            _frame = {};
            _pressed = GamepadButtons::None;
            return;
        }
        if (!_frame.Connected)
        {
            Actions().Reset();
            return;
        }
        Actions().Update(_frame.Buttons);
        if (context != GamepadContext::Gameplay || WheelHeld())
        {
            return;
        }
        const auto [x, y] = AimStick();
        _aimDeltaX = -GamepadAnalog::ApplyResponseCurve(x, GamepadOptions::Curve()) * TurnRate * GamepadOptions::LookX()
            * (GamepadOptions::InvertX() ? -1 : 1);
        _aimDeltaY = GamepadAnalog::ApplyResponseCurve(y, GamepadOptions::Curve()) * TurnRate * GamepadOptions::LookY()
            * (GamepadOptions::InvertY() ? -1 : 1);
    }

    bool GamepadInput::TakeMenuPress()
    {
        if (_context != GamepadContext::Gameplay && _context != GamepadContext::Results)
        {
            return false;
        }
        return Actions().Take(PadAction::Menu)
            || (_context == GamepadContext::Results && TakePress(GamepadButtons::B));
    }

    bool GamepadInput::TakeChatPress()
    {
        if (_context != GamepadContext::Gameplay)
        {
            return false;
        }
        return Actions().Take(PadAction::Chat);
    }

    bool GamepadInput::TakePress(GamepadButtons buttons)
    {
        if (!Any(_pressed & buttons))
        {
            return false;
        }
        _pressed &= ~buttons;
        return true;
    }

    void GamepadInput::Apply(Entities::PlayerEntity* player)
    {
        if (!GamepadContexts::Focused() || _context != GamepadContext::Gameplay || player == nullptr || !Active()
            || player->IsBot() || (player->LoadFlags() & Entities::LoadFlags::Active) == Entities::LoadFlags::None)
        {
            return;
        }
        if (player->Health() == 0 || player->IsAltForm())
        {
            Actions().CloseWheel();
        }
        Entities::PlayerControls& controls = player->Controls();
        const auto move = GamepadOptions::Southpaw()
            ? GamepadAnalog::ApplyRadialDeadZone(_frame.RightX, _frame.RightY, GamepadOptions::RightInner(), GamepadOptions::RightOuter())
            : GamepadAnalog::ApplyRadialDeadZone(_frame.LeftX, _frame.LeftY, GamepadOptions::LeftInner(), GamepadOptions::LeftOuter());
        const auto [moveX, moveY] = GamepadAnalog::QuantizeMovement(move.first, move.second);
        Hold(controls.MoveUp(), moveY > WalkThreshold);
        Hold(controls.RollUp(), moveY > WalkThreshold);
        Hold(controls.MoveDown(), moveY < -WalkThreshold);
        Hold(controls.RollDown(), moveY < -WalkThreshold);
        Hold(controls.MoveLeft(), moveX < -WalkThreshold);
        Hold(controls.RolltLeft(), moveX < -WalkThreshold);
        Hold(controls.MoveRight(), moveX > WalkThreshold);
        Hold(controls.RollRight(), moveX > WalkThreshold);
        ApplyBindings(controls);
        if (Actions().WasPressed(PadAction::LastWeapon) && player->PreviousWeapon() != player->CurrentWeapon())
        {
            if (Entities::Keybind* last = GamepadActions::WeaponBind(controls, player->PreviousWeapon()))
            {
                Hold(*last, true, true);
            }
        }
        if (InUse())
        {
            player->ModNoteInput();
        }
    }

    void GamepadInput::ApplyBindings(Entities::PlayerControls& controls)
    {
        GamepadActions& actions = Actions();
        const auto bind = [&actions](Entities::Keybind& keybind, PadAction action)
        {
            Hold(keybind, actions.Down(action), actions.WasPressed(action));
        };
        bind(controls.Shoot(), PadAction::Shoot);
        bind(controls.AltAttack(), PadAction::Shoot);
        bind(controls.Jump(), PadAction::Jump);
        bind(controls.Boost(), PadAction::Jump);
        bind(controls.Zoom(), PadAction::Zoom);
        bind(controls.Morph(), PadAction::Morph);
        bind(controls.Scan(), PadAction::Scan);
        bind(controls.ScanVisor(), PadAction::ScanVisor);
        Hold(controls.WeaponMenu(), WheelHeld(), actions.WasPressed(PadAction::WeaponWheel));
        bind(controls.Pause(), PadAction::Scoreboard);
        bind(controls.NextWeapon(), PadAction::NextWeapon);
        bind(controls.PrevWeapon(), PadAction::PrevWeapon);
        bind(controls.Missile(), PadAction::Missile);
        bind(controls.PowerBeam(), PadAction::PowerBeam);
        bind(controls.VoltDriver(), PadAction::VoltDriver);
        bind(controls.Battlehammer(), PadAction::Battlehammer);
        bind(controls.Imperialist(), PadAction::Imperialist);
        bind(controls.Judicator(), PadAction::Judicator);
        bind(controls.Magmaul(), PadAction::Magmaul);
        bind(controls.ShockCoil(), PadAction::ShockCoil);
        bind(controls.OmegaCannon(), PadAction::OmegaCannon);
        bind(controls.AffinitySlot(), PadAction::AffinitySlot);
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
            bind.SetIsReleased(false);
        }
        if (pressed)
        {
            bind.SetIsPressed(true);
        }
    }
}
