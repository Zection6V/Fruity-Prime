#include "HitRig.hpp"

#include "NetSession.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Metadata/Weapons.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../NativeRuntime/System/Number.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace MphRead::Mods::Network
{
    namespace Runtime = ::MphRead::NativeRuntime;
    using Entities::PlayerControls;
    using Entities::PlayerEntity;

    std::string ToString(HitRig::RigMode value)
    {
        switch (value)
        {
        case HitRig::RigMode::Off: return "Off";
        case HitRig::RigMode::Jump: return "Jump";
        case HitRig::RigMode::Sniper: return "Sniper";
        case HitRig::RigMode::Duel: return "Duel";
        case HitRig::RigMode::Volley: return "Volley";
        }
        return std::to_string(static_cast<std::int32_t>(value));
    }

    bool HitRig::Configure(const std::optional<std::string>& value)
    {
        if (!value.has_value())
        {
            return false;
        }
        const std::string key = Runtime::ToLowerInvariant(Runtime::StringTrim(*value));
        const auto volley = [](::MphRead::BeamType weapon)
        {
            _mode = RigMode::Volley;
            _volleyWeapon = weapon;
            return true;
        };
        if (key == "jump" || key == "jumppad")
        {
            _mode = RigMode::Jump;
            return true;
        }
        if (key == "sniper" || key == "long")
        {
            _mode = RigMode::Sniper;
            return true;
        }
        if (key == "duel" || key == "trade")
        {
            _mode = RigMode::Duel;
            return true;
        }
        if (key == "missile") return volley(::MphRead::BeamType::Missile);
        if (key == "magmaul") return volley(::MphRead::BeamType::Magmaul);
        if (key == "judicator") return volley(::MphRead::BeamType::Judicator);
        if (key == "battlehammer") return volley(::MphRead::BeamType::Battlehammer);
        if (key == "shockcoil") return volley(::MphRead::BeamType::ShockCoil);
        if (key == "voltdriver") return volley(::MphRead::BeamType::VoltDriver);
        if (key == "powerbeam") return volley(::MphRead::BeamType::PowerBeam);
        return false;
    }

    void HitRig::Reset()
    {
        _frame = 0;
        _stuckFrames = 0;
        _lastPosition = OpenTK::Mathematics::Vector3::Zero;
        _aimDeltaX = 0;
        _aimDeltaY = 0;
        _triggers = 0;
        _framesOnTarget = 0;
        _framesAirborne = 0;
        _rangeSum = 0;
        _rangeSamples = 0;
        _worstVerticalSpeed = 0;
        _verticalSpeedSum = 0;
        _verticalSpeedSamples = 0;
    }

    bool HitRig::IsSniper()
    {
        return _mode == RigMode::Duel || std::max(NetSession::LocalSlot(), 0) % 2 == 0;
    }

    void HitRig::Drive(PlayerEntity& player)
    {
        _frame++;
        PlayerControls& c = player.Controls();
        ClearControls(c);
        if (player.Health() == 0)
        {
            c.Shoot().SetIsDown(true);
            _aimDeltaX = 0;
            _aimDeltaY = -TurnRate;
            FinishControls(player, c);
            return;
        }
        PlayerEntity* other = Opponent(player);
        if (IsSniper())
        {
            DriveSniper(player, c, other);
        }
        else
        {
            DriveRunner(player, c, other);
        }
        FinishControls(player, c);
    }

    void HitRig::DriveRunner(PlayerEntity& player, PlayerControls& c, PlayerEntity* other)
    {
        static_cast<void>(AimAt(player, other, 0));
        const bool airborne = !::MphRead::TestFlag(player.Flags1(), Entities::PlayerFlags1::Standing);
        if (airborne)
        {
            _framesAirborne++;
        }
        const float rise = std::abs(player.Speed().Y);
        _verticalSpeedSum += rise;
        _verticalSpeedSamples++;
        if (rise > _worstVerticalSpeed)
        {
            _worstVerticalSpeed = rise;
        }
        c.Jump().SetIsDown(_mode == RigMode::Jump ? _frame % 24 < 3 : _frame % 90 < 3);
        Square(c, _mode == RigMode::Jump ? 50 : 80);
    }

    void HitRig::DriveSniper(PlayerEntity& player, PlayerControls& c, PlayerEntity* other)
    {
        if (_mode == RigMode::Volley)
        {
            if (player.CurrentWeapon() != _volleyWeapon)
            {
                player.ModArmWeapon(_volleyWeapon);
            }
        }
        else if (player.CurrentWeapon() != ::MphRead::BeamType::Imperialist)
        {
            player.ModArmZoomWeapon();
        }
        player.ModSetAmmo(std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::max());
        if (_mode != RigMode::Volley && player.ModCanZoom()
            && !player.EquipInfo()->Zoomed && _frame % 8 == 0)
        {
            c.Zoom().SetIsDown(true);
        }
        const bool onTarget = AimAt(player, other, HeadAimHeight);
        if (onTarget)
        {
            _framesOnTarget++;
        }
        if (other == nullptr)
        {
            Square(c, 60);
            return;
        }
        const OpenTK::Mathematics::Vector3 otherPosition = other->Position;
        const OpenTK::Mathematics::Vector3 position = player.Position;
        const float range = OpenTK::Mathematics::Length(otherPosition - position);
        _rangeSum += range;
        _rangeSamples++;
        HoldRange(player, c, range, _mode == RigMode::Sniper ? LongRange
            : _mode == RigMode::Volley && _volleyWeapon != ::MphRead::BeamType::ShockCoil ? VolleyRange : CloseRange);
        const std::int32_t tap = _mode == RigMode::Volley
            ? Runtime::RequireReference((*::MphRead::Weapons::Current)[static_cast<std::size_t>(_volleyWeapon)]).ShotCooldown * 2 + 3
            : 63;
        const std::int32_t clock = _mode == RigMode::Duel && NetSession::LastSnapshotFrame() != 0
            ? static_cast<std::int32_t>(NetSession::LastSnapshotFrame())
            : _frame;
        c.Shoot().SetIsDown(onTarget && clock % tap < 3);
        if (c.Shoot().IsDown() && clock % tap == 0)
        {
            _triggers++;
        }
    }

    void HitRig::HoldRange(PlayerEntity& player, PlayerControls& c, float range, float want)
    {
        const OpenTK::Mathematics::Vector3 position = player.Position;
        const float moved = OpenTK::Mathematics::Length(position - _lastPosition);
        _lastPosition = position;
        _stuckFrames = moved < 0.02F ? _stuckFrames + 1 : 0;
        const bool stuck = _stuckFrames > 20;
        if (stuck && _stuckFrames > 90)
        {
            _stuckFrames = 0;
            _stuckDirection = !_stuckDirection;
        }
        if (stuck)
        {
            c.MoveLeft().SetIsDown(_stuckDirection);
            c.MoveRight().SetIsDown(!_stuckDirection);
            c.Jump().SetIsDown(_stuckFrames % 30 < 3);
            return;
        }
        constexpr float slack = 2.5F;
        c.MoveUp().SetIsDown(range > want + slack);
        c.MoveDown().SetIsDown(range < want - slack);
        if (!c.MoveUp().IsDown() && !c.MoveDown().IsDown())
        {
            c.MoveLeft().SetIsDown(_frame / 70 % 2 == 0);
            c.MoveRight().SetIsDown(_frame / 70 % 2 == 1);
        }
    }

    bool HitRig::AimAt(PlayerEntity& player, PlayerEntity* target, float headHeight)
    {
        _aimDeltaX = 0;
        _aimDeltaY = 0;
        if (target == nullptr)
        {
            return false;
        }
        const OpenTK::Mathematics::Vector3 targetPosition = target->Position;
        const OpenTK::Mathematics::Vector3 at = headHeight > 0
            ? OpenTK::Mathematics::Vector3(targetPosition.X, targetPosition.Y + headHeight, targetPosition.Z)
            : target->ModAimTarget();
        const auto [turnX, turnY] = player.ModAimDeltaTowards(at);
        if (!std::isfinite(turnX) || !std::isfinite(turnY))
        {
            return false;
        }
        _aimDeltaX = std::clamp(turnX, -TurnRate, TurnRate);
        _aimDeltaY = std::clamp(turnY, -TurnRate, TurnRate);
        return std::abs(turnX) < FiringCone && std::abs(turnY) < FiringCone;
    }

    PlayerEntity* HitRig::Opponent(PlayerEntity& self)
    {
        PlayerEntity* best = nullptr;
        float bestDistance = std::numeric_limits<float>::max();
        for (const std::shared_ptr<PlayerEntity>& otherPtr : PlayerEntity::Players())
        {
            PlayerEntity& other = Runtime::RequireReference(otherPtr);
            if (&other == &self || !::MphRead::TestFlag(other.LoadFlags(), Entities::LoadFlags::Active)
                || !::MphRead::TestFlag(other.LoadFlags(), Entities::LoadFlags::Spawned) || other.Health() == 0)
            {
                continue;
            }
            const OpenTK::Mathematics::Vector3 a = other.Position;
            const OpenTK::Mathematics::Vector3 b = self.Position;
            const float distance = OpenTK::Mathematics::LengthSquared(a - b);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = &other;
            }
        }
        return best;
    }

    void HitRig::Square(PlayerControls& c, std::int32_t framesPerSide)
    {
        const std::int32_t side = _frame / framesPerSide % 4;
        c.MoveUp().SetIsDown(side == 0);
        c.MoveRight().SetIsDown(side == 1);
        c.MoveDown().SetIsDown(side == 2);
        c.MoveLeft().SetIsDown(side == 3);
    }

    void HitRig::ClearControls(PlayerControls& c)
    {
        if (_wasDown.size() < c.All().size())
        {
            _wasDown.assign(c.All().size(), false);
        }
        for (std::size_t i = 0; i < c.All().size(); i++)
        {
            Entities::Keybind& bind = *c.All()[static_cast<std::int32_t>(i)];
            _wasDown[i] = bind.IsDown();
            bind.SetIsDown(false);
            bind.SetIsPressed(false);
            bind.SetIsReleased(false);
        }
    }

    void HitRig::FinishControls(PlayerEntity& player, PlayerControls& c)
    {
        bool any = false;
        for (std::size_t i = 0; i < c.All().size() && i < _wasDown.size(); i++)
        {
            Entities::Keybind& bind = *c.All()[static_cast<std::int32_t>(i)];
            bind.SetIsPressed(bind.IsDown() && !_wasDown[i]);
            bind.SetIsReleased(!bind.IsDown() && _wasDown[i]);
            any |= bind.IsDown() || bind.IsReleased();
        }
        if (any)
        {
            player.ModNoteInput();
        }
        player.ModApplyScriptAim(_aimDeltaX, _aimDeltaY);
    }

    std::string HitRig::Describe()
    {
        if (!Active())
        {
            return "hit rig: off";
        }
        const std::string role = IsSniper() ? "sniper" : "runner";
        if (IsSniper())
        {
            const double range = _rangeSamples > 0 ? _rangeSum / static_cast<double>(_rangeSamples) : 0;
            return "hit rig: " + ToString(_mode) + " as " + role + ", " + std::to_string(_triggers) + " triggers, "
                + std::to_string(_framesOnTarget) + " frames on target, mean range " + Runtime::ToString(range, "F1") + " units";
        }
        const double rise = _verticalSpeedSamples > 0 ? _verticalSpeedSum / static_cast<double>(_verticalSpeedSamples) : 0;
        return "hit rig: " + ToString(_mode) + " as " + role + ", " + std::to_string(_framesAirborne) + " frames airborne, "
            + "mean |vertical speed| " + Runtime::ToString(rise, "F3") + " units/frame, worst "
            + Runtime::ToString(_worstVerticalSpeed, "F3");
    }
}
