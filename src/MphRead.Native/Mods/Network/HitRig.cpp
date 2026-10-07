#include "HitRig.hpp"

#include "NetSession.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Metadata/Weapons.hpp"
#include "../../Entities/JumpPadEntity.hpp"
#include "../../Scene.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../NativeRuntime/System/Number.hpp"

#include <algorithm>
#include <bit>
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
        case HitRig::RigMode::Dialanche: return "Dialanche";
        case HitRig::RigMode::All: return "All";
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
        if (key == "imperialist") return volley(::MphRead::BeamType::Imperialist);
        if (key == "omega" || key == "omegacannon") return volley(::MphRead::BeamType::OmegaCannon);
        if (key == "dialanche") { _mode = RigMode::Dialanche; return true; }
        // Every weapon in turn against a target that never leaves the jump
        // pads: the runner walks back onto the nearest pad each time it lands.
        if (key == "all" || key == "pads") { _mode = RigMode::All; return true; }
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
        _padLaunches = 0;
        _wasAirborne = false;
    }

    bool HitRig::IsSniper()
    {
        return _mode == RigMode::Duel || std::max(NetSession::LocalSlot(), 0) % 2 == 0;
    }

    void HitRig::Drive(PlayerEntity& player)
    {
        Runtime::IncrementInPlace(_frame);
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
        if (_mode == RigMode::Dialanche)
        {
            DriveDialanche(player, c, other);
        }
        else if (IsSniper())
        {
            DriveSniper(player, c, other);
        }
        else if (_mode == RigMode::All)
        {
            DrivePadRider(player, c, other);
        }
        else
        {
            DriveRunner(player, c, other);
        }
        FinishControls(player, c);
    }

    void HitRig::DriveDialanche(PlayerEntity& player, PlayerControls& c, PlayerEntity* other)
    {
        _aimDeltaX = _aimDeltaY = 0;
        // A real Spire intent stream approaches a human target that settles
        // after a short walk (also exercises the client's movement report).
        // No teleport, forced form, damage injection, or authority hook.
        if (player.Hunter() != ::MphRead::Hunter::Spire)
        {
            c.MoveUp().SetIsDown(_frame < 60);
            return;
        }
        if (other == nullptr) return;
        const bool onTarget = AimAt(player, other, 0);
        const float range = OpenTK::Mathematics::Length(static_cast<OpenTK::Mathematics::Vector3>(player.Position)
            - static_cast<OpenTK::Mathematics::Vector3>(other->Position));
        if (!player.IsAltForm())
        {
            c.MoveUp().SetIsDown(onTarget && range > 1.1F);
            c.Morph().SetIsDown(range < 1.5F && !player.IsMorphing() && !player.IsUnmorphing() && _frame % 20 == 0);
            return;
        }
        c.RollUp().SetIsDown(onTarget && range > 1.1F);
        if (onTarget && range < 2.5F && !player.IsMorphing() && _frame % 48 == 0)
        {
            c.AltAttack().SetIsDown(true);
            ++_triggers;
        }
        _framesOnTarget += onTarget ? 1 : 0;
        _rangeSum += range;
        ++_rangeSamples;
    }

    void HitRig::DriveRunner(PlayerEntity& player, PlayerControls& c, PlayerEntity* other)
    {
        static_cast<void>(AimAt(player, other, 0));
        const bool airborne = !::MphRead::TestFlag(player.Flags1(), Entities::PlayerFlags1::Standing);
        if (airborne)
        {
            Runtime::IncrementInPlace(_framesAirborne);
        }
        const float rise = std::abs(player.Speed().Y);
        _verticalSpeedSum += rise;
        Runtime::IncrementInPlace(_verticalSpeedSamples);
        if (rise > _worstVerticalSpeed)
        {
            _worstVerticalSpeed = rise;
        }
        c.Jump().SetIsDown(_mode == RigMode::Jump ? _frame % 24 < 3 : _frame % 90 < 3);
        Square(c, _mode == RigMode::Jump ? 50 : 80);
    }

    ::MphRead::BeamType HitRig::CycleWeapon() noexcept
    {
        static constexpr ::MphRead::BeamType Cycle[] = {
            ::MphRead::BeamType::PowerBeam, ::MphRead::BeamType::VoltDriver,
            ::MphRead::BeamType::Missile, ::MphRead::BeamType::Battlehammer,
            ::MphRead::BeamType::Imperialist, ::MphRead::BeamType::Judicator,
            ::MphRead::BeamType::Magmaul, ::MphRead::BeamType::ShockCoil,
            ::MphRead::BeamType::OmegaCannon,
        };
        return Cycle[static_cast<std::size_t>(_frame / CycleFrames) % std::size(Cycle)];
    }

    namespace
    {
        OpenTK::Mathematics::Vector3 PadCenter(const ::MphRead::CollisionVolume& volume)
        {
            using OpenTK::Mathematics::Scale;
            using OpenTK::Mathematics::Vector3;
            switch (volume.Type)
            {
            case ::MphRead::VolumeType::Box:
                return volume.BoxPosition + Scale(Scale(volume.BoxVector1, volume.BoxDot1)
                    + Scale(volume.BoxVector2, volume.BoxDot2) + Scale(volume.BoxVector3, volume.BoxDot3), 0.5F);
            case ::MphRead::VolumeType::Cylinder:
                return volume.CylinderPosition + Scale(volume.CylinderVector, volume.CylinderDot / 2.0F);
            case ::MphRead::VolumeType::Sphere:
                return volume.SpherePosition;
            default:
                return Vector3::Zero;
            }
        }
    }

    void HitRig::DrivePadRider(PlayerEntity& player, PlayerControls& c, PlayerEntity* other)
    {
        using OpenTK::Mathematics::Vector3;
        const bool airborne = !::MphRead::TestFlag(player.Flags1(), Entities::PlayerFlags1::Standing);
        const float rise = std::abs(player.Speed().Y);
        _verticalSpeedSum += rise;
        Runtime::IncrementInPlace(_verticalSpeedSamples);
        _worstVerticalSpeed = std::max(_worstVerticalSpeed, rise);
        if (airborne)
        {
            Runtime::IncrementInPlace(_framesAirborne);
            // A rise this steep is a pad, not a jump: count the launches so a
            // run can tell a rider that kept riding from one stuck in a corner.
            if (!_wasAirborne && player.Speed().Y > 0.5F)
            {
                Runtime::IncrementInPlace(_padLaunches);
            }
            _wasAirborne = true;
            // In the air the pad does the steering, so the rider shoots back
            // with the same weapon of the cycle: that is what puts two players
            // killing each other inside one round trip, the case the kill
            // arbitration exists for.
            const ::MphRead::BeamType weapon = CycleWeapon();
            if (player.CurrentWeapon() != weapon)
            {
                player.ModArmWeapon(weapon);
            }
            player.ModSetAmmo(std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::max());
            const bool onTarget = AimAt(player, other, HeadAimHeight);
            const std::int32_t tap = Runtime::RequireReference(
                (*::MphRead::Weapons::Current)[static_cast<std::size_t>(weapon)]).ShotCooldown * 2 + 3;
            c.Shoot().SetIsDown(onTarget && _frame % tap < 3);
            if (c.Shoot().IsDown() && _frame % tap == 0)
            {
                Runtime::IncrementInPlace(_triggers);
            }
            return;
        }
        _wasAirborne = false;
        Scene* scene = _scene;
        if (scene == nullptr)
        {
            Square(c, 50);
            return;
        }
        const Vector3 position = player.Position;
        Vector3 best = Vector3::Zero;
        float bestDistance = std::numeric_limits<float>::max();
        auto entities = scene->Entities().GetEnumerator();
        while (entities.MoveNext())
        {
            auto* pad = dynamic_cast<Entities::JumpPadEntity*>(entities.Current().get());
            if (pad == nullptr || !pad->Active)
            {
                continue;
            }
            const Vector3 center = PadCenter(pad->ModVolume());
            const float dx = center.X - position.X;
            const float dz = center.Z - position.Z;
            const float distance = dx * dx + dz * dz;
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = center;
            }
        }
        if (bestDistance == std::numeric_limits<float>::max())
        {
            // No pads in this room: fall back to the jump runner.
            c.Jump().SetIsDown(_frame % 24 < 3);
            Square(c, 50);
            return;
        }
        // Face the pad at eye height and walk onto it. The pad does the rest.
        const auto [turnX, turnY] = player.ModAimDeltaTowards(
            Vector3(best.X, position.Y + 0.6F, best.Z));
        if (!std::isfinite(turnX) || !std::isfinite(turnY))
        {
            return;
        }
        _aimDeltaX = std::clamp(turnX, -TurnRate, TurnRate);
        _aimDeltaY = std::clamp(turnY, -TurnRate, TurnRate);
        c.MoveUp().SetIsDown(std::abs(turnX) < 45.0F);
    }

    void HitRig::DriveSniper(PlayerEntity& player, PlayerControls& c, PlayerEntity* other)
    {
        if (_mode == RigMode::Volley || _mode == RigMode::All)
        {
            const ::MphRead::BeamType weapon = _mode == RigMode::All ? CycleWeapon() : _volleyWeapon;
            if (player.CurrentWeapon() != weapon)
            {
                player.ModArmWeapon(weapon);
            }
        }
        else if (player.CurrentWeapon() != ::MphRead::BeamType::Imperialist)
        {
            player.ModArmZoomWeapon();
        }
        player.ModSetAmmo(std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::max());
        const bool volley = _mode == RigMode::Volley || _mode == RigMode::All;
        const ::MphRead::BeamType weapon = _mode == RigMode::All ? CycleWeapon() : _volleyWeapon;
        if (!volley && player.ModCanZoom()
            && !player.EquipInfo()->Zoomed && _frame % 8 == 0)
        {
            c.Zoom().SetIsDown(true);
        }
        const bool onTarget = AimAt(player, other, HeadAimHeight);
        if (onTarget)
        {
            Runtime::IncrementInPlace(_framesOnTarget);
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
        Runtime::IncrementInPlace(_rangeSamples);
        HoldRange(player, c, range, _mode == RigMode::Sniper ? LongRange
            : volley && weapon != ::MphRead::BeamType::ShockCoil ? VolleyRange : CloseRange);
        const std::int32_t tap = volley
            ? Runtime::RequireReference((*::MphRead::Weapons::Current)[static_cast<std::size_t>(weapon)]).ShotCooldown * 2 + 3
            : 63;
        const std::int32_t clock = _mode == RigMode::Duel && NetSession::LastSnapshotFrame() != 0
            ? std::bit_cast<std::int32_t>(NetSession::LastSnapshotFrame())
            : _frame;
        c.Shoot().SetIsDown(onTarget && clock % tap < 3);
        if (c.Shoot().IsDown() && clock % tap == 0)
        {
            Runtime::IncrementInPlace(_triggers);
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
        if (_mode == RigMode::Dialanche)
        {
            return "hit rig: Dialanche, " + std::to_string(_triggers) + " attack press edges, "
                + std::to_string(_framesOnTarget) + " frames on target";
        }
        const std::string role = IsSniper() ? "sniper" : _mode == RigMode::All ? "pad rider" : "runner";
        if (IsSniper())
        {
            const double range = _rangeSamples > 0 ? _rangeSum / static_cast<double>(_rangeSamples) : 0;
            return "hit rig: " + ToString(_mode) + " as " + role + ", " + std::to_string(_triggers) + " triggers, "
                + std::to_string(_framesOnTarget) + " frames on target, mean range " + Runtime::ToString(range, "F1") + " units";
        }
        const double rise = _verticalSpeedSamples > 0 ? _verticalSpeedSum / static_cast<double>(_verticalSpeedSamples) : 0;
        return "hit rig: " + ToString(_mode) + " as " + role + ", " + std::to_string(_framesAirborne) + " frames airborne, "
            + "mean |vertical speed| " + Runtime::ToString(rise, "F3") + " units/frame, worst "
            + Runtime::ToString(_worstVerticalSpeed, "F3")
            + (_mode == RigMode::All ? ", " + std::to_string(_padLaunches) + " pad launches, "
                + std::to_string(_triggers) + " triggers in the air" : std::string());
    }
}
