#include "NetTestScript.hpp"

#include "NetSession.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Scene.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace MphRead::Mods::Network::Detail
{
    // Runtime/BCL operations whose exact managed semantics are not a C++
    // standard-library contract. These declaration seams carry only the
    // referenced C# operation and intentionally add no fallback behavior.
    [[nodiscard]] std::optional<std::string> NetTestScriptEnvironmentGetVariable(
        std::string_view name);
    [[nodiscard]] bool NetTestScriptDoubleTryParseInvariant(
        std::string_view value, double& result);
    [[nodiscard]] std::int32_t NetTestScriptDoubleToInt32Unchecked(double value);

    // PlayerControls/Keybind are owned by a later Native slice. Keep each
    // consumed member as one operation rather than defining a competing type.
    [[nodiscard]] Entities::PlayerControls& NetTestScriptControls(
        Entities::PlayerEntity& player);
    [[nodiscard]] std::int32_t NetTestScriptAllLength(
        const Entities::PlayerControls& controls);
    [[nodiscard]] Entities::Keybind& NetTestScriptAllAt(
        Entities::PlayerControls& controls, std::int32_t index);

    [[nodiscard]] Entities::Keybind& NetTestScriptShoot(Entities::PlayerControls& controls);
    [[nodiscard]] Entities::Keybind& NetTestScriptMorph(Entities::PlayerControls& controls);
    [[nodiscard]] Entities::Keybind& NetTestScriptAltAttack(Entities::PlayerControls& controls);
    [[nodiscard]] Entities::Keybind& NetTestScriptMoveUp(Entities::PlayerControls& controls);
    [[nodiscard]] Entities::Keybind& NetTestScriptMoveRight(Entities::PlayerControls& controls);
    [[nodiscard]] Entities::Keybind& NetTestScriptMoveDown(Entities::PlayerControls& controls);
    [[nodiscard]] Entities::Keybind& NetTestScriptMoveLeft(Entities::PlayerControls& controls);
    [[nodiscard]] Entities::Keybind& NetTestScriptZoom(Entities::PlayerControls& controls);
    [[nodiscard]] Entities::Keybind& NetTestScriptJump(Entities::PlayerControls& controls);
    [[nodiscard]] Entities::Keybind& NetTestScriptNextWeapon(Entities::PlayerControls& controls);
    [[nodiscard]] Entities::Keybind& NetTestScriptBoost(Entities::PlayerControls& controls);

    [[nodiscard]] bool NetTestScriptKeybindIsDown(const Entities::Keybind& bind);
    [[nodiscard]] bool NetTestScriptKeybindIsReleased(const Entities::Keybind& bind);
    void NetTestScriptSetKeybindIsDown(Entities::Keybind& bind, bool value);
    void NetTestScriptSetKeybindIsPressed(Entities::Keybind& bind, bool value);
    void NetTestScriptSetKeybindIsReleased(Entities::Keybind& bind, bool value);

    // These PlayerEntity partial members are likewise owned outside this slice.
    [[nodiscard]] bool NetTestScriptPlayerZoomed(Entities::PlayerEntity& player);
    [[nodiscard]] bool NetTestScriptPlayerModCanZoom(Entities::PlayerEntity& player);
    void NetTestScriptPlayerModArmZoomWeapon(Entities::PlayerEntity& player);
    void NetTestScriptPlayerModArmAffinityWeapon(Entities::PlayerEntity& player);
    void NetTestScriptPlayerModNoteInput(Entities::PlayerEntity& player);
    [[nodiscard]] OpenTK::Mathematics::Vector3 NetTestScriptPlayerModAimTarget(
        Entities::PlayerEntity& player);
    [[nodiscard]] std::pair<float, float> NetTestScriptPlayerModAimDeltaTowards(
        Entities::PlayerEntity& player,
        OpenTK::Mathematics::Vector3 target);
    [[nodiscard]] bool NetTestScriptPlayerModChargeReady(Entities::PlayerEntity& player);
}

namespace
{
    template <typename T>
    [[nodiscard]] T& RequireReference(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    [[nodiscard]] std::int32_t AddInt32Unchecked(
        std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] std::int32_t SubtractInt32Unchecked(
        std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) - static_cast<std::uint32_t>(right));
    }

    void IncrementInt32Unchecked(std::int32_t& value) noexcept
    {
        value = AddInt32Unchecked(value, 1);
    }

    void DecrementInt32Unchecked(std::int32_t& value) noexcept
    {
        value = SubtractInt32Unchecked(value, 1);
    }

    [[nodiscard]] float Length(OpenTK::Mathematics::Vector3 value)
    {
        return std::sqrt(value.X * value.X + value.Y * value.Y + value.Z * value.Z);
    }

    [[nodiscard]] bool TestFlag(
        MphRead::Entities::LoadFlags value,
        MphRead::Entities::LoadFlags flag) noexcept
    {
        return (value & flag) == flag;
    }

    [[nodiscard]] std::size_t CheckedIndex(std::int32_t index, std::size_t length)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= length)
        {
            throw MphRead::SceneDetail::IndexOutOfRangeException();
        }
        return static_cast<std::size_t>(index);
    }
}

namespace MphRead::Mods::Network
{
    double NetTestScript::_phaseSeconds = NetTestScript::ReadPhaseSeconds();
    OpenTK::Mathematics::Vector3 NetTestScript::_lastPosition
        = OpenTK::Mathematics::Vector3::Zero;

    double NetTestScript::PhaseSeconds() noexcept
    {
        return _phaseSeconds;
    }

    void NetTestScript::SetPhaseSeconds(double value) noexcept
    {
        _phaseSeconds = value;
    }

    std::int32_t NetTestScript::PhaseCount() noexcept
    {
        return static_cast<std::int32_t>(_order.size());
    }

    bool NetTestScript::Enabled() noexcept
    {
        return _enabled;
    }

    void NetTestScript::SetEnabled(bool value) noexcept
    {
        _enabled = value;
    }

    float NetTestScript::AimDeltaX() noexcept
    {
        return _aimDeltaX;
    }

    float NetTestScript::AimDeltaY() noexcept
    {
        return _aimDeltaY;
    }

    std::int32_t NetTestScript::FramesOnTarget() noexcept
    {
        return _framesOnTarget;
    }

    double NetTestScript::ReadPhaseSeconds()
    {
        const std::optional<std::string> value
            = Detail::NetTestScriptEnvironmentGetVariable("MPHREAD_PHASE_SECONDS");
        double parsed = 0.0;
        if (value.has_value()
            && Detail::NetTestScriptDoubleTryParseInvariant(*value, parsed)
            && parsed > 0.0)
        {
            return parsed;
        }
        return 5.0;
    }

    TestPhase NetTestScript::Phase()
    {
        const std::optional<MatchStatePacket> serverMatch = NetSession::ServerMatch();
        const double elapsed = serverMatch.has_value()
            ? static_cast<double>(serverMatch->TimeElapsed)
            : static_cast<double>(_frame) / 60.0;
        const std::int32_t quotient
            = Detail::NetTestScriptDoubleToInt32Unchecked(elapsed / PhaseSeconds());
        const std::int32_t index
            = quotient % static_cast<std::int32_t>(_order.size());
        return _order[CheckedIndex(index, _order.size())];
    }

    void NetTestScript::Reset()
    {
        _frame = 0;
        _stuckFrames = 0;
        _lastPosition = OpenTK::Mathematics::Vector3::Zero;
        _aimDeltaX = 0.0F;
        _aimDeltaY = 0.0F;
        _framesOnTarget = 0;
    }

    void NetTestScript::ApplyOffline(
        std::shared_ptr<Entities::PlayerEntity> player,
        std::int32_t slot,
        std::int32_t frame)
    {
        _offlineSlot = slot;
        _frame = frame;
        Drive(player);
        _offlineSlot = -1;
    }

    void NetTestScript::HoldFire(
        std::shared_ptr<Entities::PlayerEntity> player,
        bool down)
    {
        Entities::PlayerEntity& playerValue = RequireReference(player);
        Entities::PlayerControls& controls = Detail::NetTestScriptControls(playerValue);
        Clear(controls);
        Entities::Keybind& shoot = Detail::NetTestScriptShoot(controls);
        Hold(shoot, down);
        Finish(playerValue, controls);
    }

    void NetTestScript::LayBombs(
        std::shared_ptr<Entities::PlayerEntity> player,
        std::int32_t frame)
    {
        Entities::PlayerEntity& playerValue = RequireReference(player);
        Entities::PlayerControls& controls = Detail::NetTestScriptControls(playerValue);
        Clear(controls);
        if (playerValue.Health() == 0)
        {
            Entities::Keybind& shoot = Detail::NetTestScriptShoot(controls);
            Hold(shoot, true);
        }
        else if (!playerValue.IsAltForm() && Settled(playerValue))
        {
            Entities::Keybind& morph = Detail::NetTestScriptMorph(controls);
            Hold(morph, true);
        }
        else if (playerValue.IsAltForm())
        {
            Entities::Keybind& altAttack = Detail::NetTestScriptAltAttack(controls);
            const bool down = frame % 20 < 3;
            Hold(altAttack, down);
        }
        Finish(playerValue, controls);
    }

    void NetTestScript::Rest(
        std::shared_ptr<Entities::PlayerEntity> player,
        bool wantBiped)
    {
        Entities::PlayerEntity& playerValue = RequireReference(player);
        Entities::PlayerControls& controls = Detail::NetTestScriptControls(playerValue);
        Clear(controls);
        if (playerValue.Health() == 0)
        {
            Entities::Keybind& shoot = Detail::NetTestScriptShoot(controls);
            Hold(shoot, true);
        }
        else if (wantBiped && playerValue.IsAltForm() && Settled(playerValue))
        {
            Entities::Keybind& morph = Detail::NetTestScriptMorph(controls);
            Hold(morph, true);
        }
        Finish(playerValue, controls);
    }

    void NetTestScript::WalkForward(std::shared_ptr<Entities::PlayerEntity> player)
    {
        Entities::PlayerEntity& playerValue = RequireReference(player);
        Entities::PlayerControls& controls = Detail::NetTestScriptControls(playerValue);
        Clear(controls);
        if (playerValue.Health() == 0)
        {
            Entities::Keybind& shoot = Detail::NetTestScriptShoot(controls);
            Hold(shoot, true);
        }
        else
        {
            Entities::Keybind& moveUp = Detail::NetTestScriptMoveUp(controls);
            Hold(moveUp, true);
        }
        Finish(playerValue, controls);
    }

    void NetTestScript::Apply(std::shared_ptr<Entities::PlayerEntity> player)
    {
        if (!_enabled)
        {
            return;
        }
        IncrementInt32Unchecked(_frame);
        Drive(player);
    }

    void NetTestScript::Drive(
        const std::shared_ptr<Entities::PlayerEntity>& player)
    {
        Entities::PlayerEntity& playerValue = RequireReference(player);
        Entities::PlayerControls& controls = Detail::NetTestScriptControls(playerValue);
        Clear(controls);
        std::shared_ptr<Entities::PlayerEntity> target = FindTarget(player);
        const bool onTarget = AimAt(playerValue, target);
        const TestPhase phase = Phase();

        if (phase != TestPhase::Zoom
            && Detail::NetTestScriptPlayerZoomed(playerValue)
            && _frame % 8 == 0)
        {
            Entities::Keybind& zoom = Detail::NetTestScriptZoom(controls);
            Hold(zoom, true);
        }

        switch (phase)
        {
        case TestPhase::Idle:
            break;
        case TestPhase::Walk:
            Square(controls);
            break;
        case TestPhase::Jump:
        {
            Square(controls);
            Entities::Keybind& jump = Detail::NetTestScriptJump(controls);
            const bool down = _frame % 45 < 3;
            Hold(jump, down);
            break;
        }
        case TestPhase::Turn:
            _aimDeltaX = 4.0F;
            _aimDeltaY = std::sin(static_cast<float>(_frame) / 40.0F) * 2.0F;
            break;
        case TestPhase::Shoot:
        {
            Entities::Keybind& shoot = Detail::NetTestScriptShoot(controls);
            const bool down = _frame % 30 < 20;
            Hold(shoot, down);
            break;
        }
        case TestPhase::SwitchWeapons:
        {
            Entities::Keybind& nextWeapon = Detail::NetTestScriptNextWeapon(controls);
            const bool nextDown = _frame % 30 == 0;
            Hold(nextWeapon, nextDown);

            Entities::Keybind& shoot = Detail::NetTestScriptShoot(controls);
            const bool shootDown = _frame % 30 > 10 && _frame % 30 < 25;
            Hold(shoot, shootDown);
            break;
        }
        case TestPhase::Charge:
        {
            Entities::Keybind& shoot = Detail::NetTestScriptShoot(controls);
            Hold(shoot, true);
            break;
        }
        case TestPhase::MorphA:
            MorphOrShoot(playerValue, controls, Even());
            break;
        case TestPhase::AltAttackA:
            AltAttackOrShoot(controls, Even(), onTarget);
            break;
        case TestPhase::MorphB:
            MorphOrShoot(playerValue, controls, !Even());
            break;
        case TestPhase::AltAttackB:
            AltAttackOrShoot(controls, !Even(), onTarget);
            break;
        case TestPhase::Unmorph:
        {
            Entities::Keybind& morph = Detail::NetTestScriptMorph(controls);
            bool pressMorph = false;
            if (Settled(playerValue) && playerValue.IsAltForm())
            {
                pressMorph = _frame % 40 == 0;
            }
            Hold(morph, pressMorph);
            Square(controls);
            break;
        }
        case TestPhase::Zoom:
        {
            if (!Detail::NetTestScriptPlayerModCanZoom(playerValue))
            {
                Detail::NetTestScriptPlayerModArmZoomWeapon(playerValue);
            }

            Entities::Keybind& zoom = Detail::NetTestScriptZoom(controls);
            bool pressZoom = false;
            if (Detail::NetTestScriptPlayerModCanZoom(playerValue)
                && !Detail::NetTestScriptPlayerZoomed(playerValue))
            {
                pressZoom = _frame % 8 == 0;
            }
            Hold(zoom, pressZoom);

            Entities::Keybind& shoot = Detail::NetTestScriptShoot(controls);
            bool shootDown = false;
            if (Detail::NetTestScriptPlayerModCanZoom(playerValue))
            {
                shootDown = _frame % 40 < 8;
            }
            Hold(shoot, shootDown);
            break;
        }
        case TestPhase::Afflict:
            Detail::NetTestScriptPlayerModArmAffinityWeapon(playerValue);
            Duel(playerValue, controls, target, onTarget, true);
            break;
        case TestPhase::Duel:
            Duel(playerValue, controls, target, onTarget);
            break;
        }

        Finish(playerValue, controls);
    }

    bool NetTestScript::Settled(Entities::PlayerEntity& player)
    {
        return !player.IsMorphing() && !player.IsUnmorphing();
    }

    bool NetTestScript::Even() noexcept
    {
        const std::int32_t slot = _offlineSlot >= 0
            ? _offlineSlot
            : std::max(NetSession::LocalSlot(), 0);
        return slot % 2 == 0;
    }

    void NetTestScript::MorphOrShoot(
        Entities::PlayerEntity& player,
        Entities::PlayerControls& controls,
        bool morphing)
    {
        if (morphing)
        {
            Entities::Keybind& morph = Detail::NetTestScriptMorph(controls);
            bool pressMorph = false;
            if (Settled(player) && !player.IsAltForm())
            {
                pressMorph = _frame % 40 == 0;
            }
            Hold(morph, pressMorph);
            Square(controls);

            Entities::Keybind& boost = Detail::NetTestScriptBoost(controls);
            const bool boostDown = _frame % 60 < 20;
            Hold(boost, boostDown);
            return;
        }

        Entities::Keybind& morph = Detail::NetTestScriptMorph(controls);
        bool pressMorph = false;
        if (Settled(player) && player.IsAltForm())
        {
            pressMorph = _frame % 40 == 0;
        }
        Hold(morph, pressMorph);

        Entities::Keybind& shoot = Detail::NetTestScriptShoot(controls);
        bool shootDown = false;
        if (!player.IsAltForm())
        {
            shootDown = _frame % 30 < 24;
        }
        Hold(shoot, shootDown);
    }

    void NetTestScript::AltAttackOrShoot(
        Entities::PlayerControls& controls,
        bool attacking,
        bool onTarget)
    {
        if (attacking)
        {
            Square(controls);
            Entities::Keybind& altAttack = Detail::NetTestScriptAltAttack(controls);
            const bool attackDown = _frame % 45 < 6;
            Hold(altAttack, attackDown);
            return;
        }

        Entities::Keybind& shoot = Detail::NetTestScriptShoot(controls);
        const bool shootDown = onTarget && _frame % 30 < 24;
        Hold(shoot, shootDown);
    }

    void NetTestScript::Clear(Entities::PlayerControls& controls)
    {
        const std::int32_t initialLength = Detail::NetTestScriptAllLength(controls);
        if (_wasDown.size() < static_cast<std::size_t>(initialLength))
        {
            _wasDown.assign(static_cast<std::size_t>(initialLength), false);
        }

        for (std::int32_t i = 0; i < Detail::NetTestScriptAllLength(controls);
            i = AddInt32Unchecked(i, 1))
        {
            Entities::Keybind& bind = Detail::NetTestScriptAllAt(controls, i);
            _wasDown[CheckedIndex(i, _wasDown.size())]
                = Detail::NetTestScriptKeybindIsDown(bind);
            Detail::NetTestScriptSetKeybindIsDown(bind, false);
            Detail::NetTestScriptSetKeybindIsPressed(bind, false);
            Detail::NetTestScriptSetKeybindIsReleased(bind, false);
        }
    }

    void NetTestScript::Finish(
        Entities::PlayerEntity& player,
        Entities::PlayerControls& controls)
    {
        bool any = false;
        for (std::int32_t i = 0;
            i < Detail::NetTestScriptAllLength(controls)
                && static_cast<std::size_t>(i) < _wasDown.size();
            i = AddInt32Unchecked(i, 1))
        {
            Entities::Keybind& bind = Detail::NetTestScriptAllAt(controls, i);
            const bool isDown = Detail::NetTestScriptKeybindIsDown(bind);
            Detail::NetTestScriptSetKeybindIsPressed(
                bind, isDown && !_wasDown[CheckedIndex(i, _wasDown.size())]);

            const bool isDownForRelease = Detail::NetTestScriptKeybindIsDown(bind);
            Detail::NetTestScriptSetKeybindIsReleased(
                bind, !isDownForRelease && _wasDown[CheckedIndex(i, _wasDown.size())]);

            any |= Detail::NetTestScriptKeybindIsDown(bind)
                || Detail::NetTestScriptKeybindIsReleased(bind);
        }
        if (any)
        {
            Detail::NetTestScriptPlayerModNoteInput(player);
        }
    }

    bool NetTestScript::AimAt(
        Entities::PlayerEntity& player,
        const std::shared_ptr<Entities::PlayerEntity>& target)
    {
        _aimDeltaX = 0.0F;
        _aimDeltaY = 0.0F;
        if (!target)
        {
            return false;
        }

        Entities::PlayerEntity& targetValue = RequireReference(target);
        const OpenTK::Mathematics::Vector3 aimTarget
            = Detail::NetTestScriptPlayerModAimTarget(targetValue);
        const std::pair<float, float> turn
            = Detail::NetTestScriptPlayerModAimDeltaTowards(player, aimTarget);
        const float turnX = turn.first;
        const float turnY = turn.second;

        if (!std::isfinite(turnX) || !std::isfinite(turnY))
        {
            return false;
        }

        _aimDeltaX = std::clamp(turnX, -TurnRate, TurnRate);
        _aimDeltaY = std::clamp(turnY, -TurnRate, TurnRate);
        const bool onTarget
            = std::fabs(turnX) < FiringCone && std::fabs(turnY) < FiringCone;
        if (onTarget)
        {
            IncrementInt32Unchecked(_framesOnTarget);
        }
        return onTarget;
    }

    void NetTestScript::Square(Entities::PlayerControls& controls)
    {
        const std::int32_t phase = _frame / 60 % 4;

        Entities::Keybind& moveUp = Detail::NetTestScriptMoveUp(controls);
        const bool up = phase == 0;
        Hold(moveUp, up);

        Entities::Keybind& moveRight = Detail::NetTestScriptMoveRight(controls);
        const bool right = phase == 1;
        Hold(moveRight, right);

        Entities::Keybind& moveDown = Detail::NetTestScriptMoveDown(controls);
        const bool down = phase == 2;
        Hold(moveDown, down);

        Entities::Keybind& moveLeft = Detail::NetTestScriptMoveLeft(controls);
        const bool left = phase == 3;
        Hold(moveLeft, left);
    }

    void NetTestScript::Duel(
        Entities::PlayerEntity& player,
        Entities::PlayerControls& controls,
        const std::shared_ptr<Entities::PlayerEntity>& target,
        bool onTarget,
        bool charged)
    {
        if (!target)
        {
            Square(controls);
            Entities::Keybind& shoot = Detail::NetTestScriptShoot(controls);
            const bool shootDown = _frame % 60 < 20;
            Hold(shoot, shootDown);
            return;
        }

        Entities::PlayerEntity& targetValue = RequireReference(target);
        const OpenTK::Mathematics::Vector3 targetPosition = targetValue.Position;
        const OpenTK::Mathematics::Vector3 playerPositionForDistance = player.Position;
        const float distance = Length(targetPosition - playerPositionForDistance);

        const OpenTK::Mathematics::Vector3 playerPositionForMoved = player.Position;
        const float moved = Length(playerPositionForMoved - _lastPosition);
        _lastPosition = static_cast<OpenTK::Mathematics::Vector3>(player.Position);
        _stuckFrames = moved < 0.02F
            ? AddInt32Unchecked(_stuckFrames, 1)
            : 0;
        const bool stuck = _stuckFrames > 20;
        if (stuck && _stuckFrames > 90)
        {
            _stuckFrames = 0;
            _stuckDirection = !_stuckDirection;
        }

        Entities::Keybind& moveUp = Detail::NetTestScriptMoveUp(controls);
        const bool moveUpDown = !stuck && distance > PreferredRange;
        Hold(moveUp, moveUpDown);

        Entities::Keybind& moveDown = Detail::NetTestScriptMoveDown(controls);
        const bool moveDownDown = !stuck && distance < PreferredRange / 2.0F;
        Hold(moveDown, moveDownDown);

        Entities::Keybind& moveLeft = Detail::NetTestScriptMoveLeft(controls);
        const bool moveLeftDown = stuck
            ? _stuckDirection
            : distance <= PreferredRange && _frame / 90 % 2 == 0;
        Hold(moveLeft, moveLeftDown);

        Entities::Keybind& moveRight = Detail::NetTestScriptMoveRight(controls);
        const bool moveRightDown = stuck
            ? !_stuckDirection
            : distance <= PreferredRange && _frame / 90 % 2 == 1;
        Hold(moveRight, moveRightDown);

        Entities::Keybind& jump = Detail::NetTestScriptJump(controls);
        const bool jumpDown
            = stuck ? _stuckFrames % 30 < 3 : _frame % 150 < 3;
        Hold(jump, jumpDown);

        if (!charged)
        {
            Entities::Keybind& shoot = Detail::NetTestScriptShoot(controls);
            const bool shootDown = onTarget && _frame % 30 < 24;
            Hold(shoot, shootDown);
            return;
        }

        if (_releaseFrames > 0)
        {
            DecrementInt32Unchecked(_releaseFrames);
            return;
        }
        if (onTarget && Detail::NetTestScriptPlayerModChargeReady(player))
        {
            _releaseFrames = 4;
            return;
        }

        Entities::Keybind& shoot = Detail::NetTestScriptShoot(controls);
        Hold(shoot, true);
    }

    std::shared_ptr<Entities::PlayerEntity> NetTestScript::FindTarget(
        const std::shared_ptr<Entities::PlayerEntity>& self)
    {
        if (NetSession::Active()
            && RequireReference(self).SlotIndex() >= 0)
        {
            const std::int32_t count = static_cast<std::int32_t>(
                Entities::PlayerEntity::Players().size());
            for (std::int32_t step = 1; step < count;
                step = AddInt32Unchecked(step, 1))
            {
                const std::int32_t targetSlot
                    = AddInt32Unchecked(RequireReference(self).SlotIndex(), step) % count;
                const auto& players = Entities::PlayerEntity::Players();
                const std::shared_ptr<Entities::PlayerEntity> target
                    = players[CheckedIndex(targetSlot, players.size())];
                if (target != self)
                {
                    Entities::PlayerEntity& targetValue = RequireReference(target);
                    if (TestFlag(targetValue.LoadFlags(), Entities::LoadFlags::Active)
                        && TestFlag(targetValue.LoadFlags(), Entities::LoadFlags::Spawned)
                        && targetValue.Health() > 0)
                    {
                        return target;
                    }
                }
            }
        }

        std::shared_ptr<Entities::PlayerEntity> best{};
        float bestDistance = std::numeric_limits<float>::max();
        for (std::int32_t i = 0;
            i < static_cast<std::int32_t>(Entities::PlayerEntity::Players().size());
            i = AddInt32Unchecked(i, 1))
        {
            const auto& players = Entities::PlayerEntity::Players();
            const std::shared_ptr<Entities::PlayerEntity> other
                = players[CheckedIndex(i, players.size())];
            if (other == self)
            {
                continue;
            }

            Entities::PlayerEntity& otherValue = RequireReference(other);
            if (!TestFlag(otherValue.LoadFlags(), Entities::LoadFlags::Active)
                || !TestFlag(otherValue.LoadFlags(), Entities::LoadFlags::Spawned)
                || otherValue.Health() == 0)
            {
                continue;
            }

            const OpenTK::Mathematics::Vector3 otherPosition = otherValue.Position;
            const OpenTK::Mathematics::Vector3 selfPosition = RequireReference(self).Position;
            const float distance = Length(otherPosition - selfPosition);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = other;
            }
        }
        return best;
    }

    void NetTestScript::Hold(Entities::Keybind& bind, bool down)
    {
        Detail::NetTestScriptSetKeybindIsDown(bind, down);
    }
}
