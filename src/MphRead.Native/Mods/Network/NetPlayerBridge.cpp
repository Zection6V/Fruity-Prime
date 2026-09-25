#include "NetPlayerBridge.hpp"

#include "../../GameState.hpp"
#include "../EndScreen.hpp"
#include "NetDamage.hpp"
#include "NetHitClaims.hpp"
#include "NetHitPrediction.hpp"
#include "NetHooks.hpp"
#include "NetLog.hpp"
#include "NetPlayerLifecycle.hpp"
#include "NetRoomChange.hpp"
#include "NetSession.hpp"
#include "NetShotDiagnostics.hpp"
#include "NetSmoothing.hpp"
#include "NetUnlagged.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using ::MphRead::NativeRuntime::HasFlag;
using ::OpenTK::Mathematics::Length;
using ::OpenTK::Mathematics::LengthSquared;

namespace MphRead::Mods::Network
{
    namespace
    {
        [[nodiscard]] bool VectorEquals(OpenTK::Mathematics::Vector3 left, OpenTK::Mathematics::Vector3 right) noexcept
        {
            return left.X == right.X && left.Y == right.Y && left.Z == right.Z;
        }

        [[nodiscard]] constexpr std::size_t Index(std::int32_t value) noexcept
        {
            return static_cast<std::size_t>(value);
        }
    }

    std::string NetPlayerBridge::FormSaidByAuthority()
    {
        std::string text;
        text.reserve(Entities::PlayerEntity::SlotCapacity);
        for (std::int32_t i = 0; i < Entities::PlayerEntity::MaxPlayers() && i < static_cast<std::int32_t>(_formSaid.size()); ++i)
        {
            text.push_back(_formSaid[Index(i)] == 0 ? '-' : _formSaid[Index(i)] == 2 ? 'A' : 'b');
        }
        return text;
    }

    OpenTK::Mathematics::Vector3 NetPlayerBridge::InFormFor(
        Entities::PlayerEntity& player, OpenTK::Mathematics::Vector3 position, bool measuredInAlt)
    {
        return InForm(player, position, measuredInAlt);
    }

    OpenTK::Mathematics::Vector3 NetPlayerBridge::InForm(
        Entities::PlayerEntity& player, OpenTK::Mathematics::Vector3 position, bool measuredInAlt)
    {
        if (measuredInAlt == player.IsAltForm())
        {
            return position;
        }
        const std::int32_t hunter = static_cast<std::int32_t>(player.Hunter());
        if (hunter < 0 || hunter >= 8)
        {
            return position;
        }
        const auto& volumes = Entities::PlayerEntity::PlayerVolumes[Index(hunter)];
        const OpenTK::Mathematics::Vector3 delta = volumes[0].SpherePosition - volumes[2].SpherePosition;
        return measuredInAlt ? position - delta : position + delta;
    }

    bool NetPlayerBridge::Sane(OpenTK::Mathematics::Vector3 value) noexcept
    {
        return std::isfinite(value.X) && std::isfinite(value.Y) && std::isfinite(value.Z)
            && std::fabs(value.X) < PositionLimit && std::fabs(value.Y) < PositionLimit
            && std::fabs(value.Z) < PositionLimit;
    }

    void NetPlayerBridge::RecordPresses(Entities::PlayerEntity& player)
    {
        if (!player.ModIsInPlay())
        {
            _pressHistory.fill(0);
            _hasLatch = false;
            return;
        }
        Entities::PlayerControls& c = player.Controls();
        IntentButtons pressed = IntentButtons::None;
        if (c.MoveLeft().IsPressed()) pressed |= IntentButtons::MoveLeft;
        if (c.MoveRight().IsPressed()) pressed |= IntentButtons::MoveRight;
        if (c.MoveUp().IsPressed()) pressed |= IntentButtons::MoveUp;
        if (c.MoveDown().IsPressed()) pressed |= IntentButtons::MoveDown;
        if (c.Shoot().IsPressed()) pressed |= IntentButtons::Shoot;
        if (c.Zoom().IsPressed()) pressed |= IntentButtons::Zoom;
        if (c.Jump().IsPressed()) pressed |= IntentButtons::Jump;
        if (c.Morph().IsPressed()) pressed |= IntentButtons::Morph;
        if (c.Boost().IsPressed()) pressed |= IntentButtons::Boost;
        if (c.AltAttack().IsPressed()) pressed |= IntentButtons::AltAttack;
        if (c.ScanVisor().IsPressed()) pressed |= IntentButtons::ScanVisor;
        if (c.NextWeapon().IsPressed()) pressed |= IntentButtons::NextWeapon;
        if (c.PrevWeapon().IsPressed()) pressed |= IntentButtons::PrevWeapon;
        if (c.RolltLeft().IsPressed()) pressed |= IntentButtons::RollLeft;
        if (c.RollRight().IsPressed()) pressed |= IntentButtons::RollRight;
        if (c.RollUp().IsPressed()) pressed |= IntentButtons::RollUp;
        if (c.RollDown().IsPressed()) pressed |= IntentButtons::RollDown;
        for (std::size_t i = _pressHistory.size() - 1; i > 0; i--)
        {
            _pressHistory[i] = _pressHistory[i - 1];
        }
        _pressHistory[0] = static_cast<std::uint32_t>(pressed);
        if (c.Shoot().IsReleased() || c.Boost().IsReleased() || c.AltAttack().IsPressed())
        {
            _latchedCharge = player.ModChargeLevel();
            _latchedBoostDamage = player.ModBoostDamage();
            _hasLatch = true;
        }
    }

    IntentPacket NetPlayerBridge::CaptureIntent(Entities::PlayerEntity& player)
    {
        Entities::PlayerControls& c = player.Controls();
        IntentButtons buttons = IntentButtons::None;
        if (c.MoveLeft().IsDown()) buttons |= IntentButtons::MoveLeft;
        if (c.MoveRight().IsDown()) buttons |= IntentButtons::MoveRight;
        if (c.MoveUp().IsDown()) buttons |= IntentButtons::MoveUp;
        if (c.MoveDown().IsDown()) buttons |= IntentButtons::MoveDown;
        if (c.Shoot().IsDown()) buttons |= IntentButtons::Shoot;
        if (c.Zoom().IsDown()) buttons |= IntentButtons::Zoom;
        if (c.Jump().IsDown()) buttons |= IntentButtons::Jump;
        if (c.Morph().IsDown()) buttons |= IntentButtons::Morph;
        if (c.Boost().IsDown()) buttons |= IntentButtons::Boost;
        if (c.AltAttack().IsDown()) buttons |= IntentButtons::AltAttack;
        if (c.ScanVisor().IsDown()) buttons |= IntentButtons::ScanVisor;
        if (c.NextWeapon().IsDown()) buttons |= IntentButtons::NextWeapon;
        if (c.PrevWeapon().IsDown()) buttons |= IntentButtons::PrevWeapon;
        if (c.RolltLeft().IsDown()) buttons |= IntentButtons::RollLeft;
        if (c.RollRight().IsDown()) buttons |= IntentButtons::RollRight;
        if (c.RollUp().IsDown()) buttons |= IntentButtons::RollUp;
        if (c.RollDown().IsDown()) buttons |= IntentButtons::RollDown;
        if (player.EquipInfo()->Zoomed) buttons |= IntentButtons::ZoomedState;
        if (player.IsAltForm()) buttons |= IntentButtons::AltFormState;
        if (HasFlag(player.LoadFlags(), Entities::LoadFlags::Spawned) && player.Health() > 0)
        {
            buttons |= IntentButtons::InPlayState;
        }
        if (HasFlag(player.Flags2(), Entities::PlayerFlags2::Spectating))
        {
            buttons |= IntentButtons::SpectatingState;
        }
        if (Mods::EndScreen::Ready())
        {
            buttons |= IntentButtons::ReadyState;
        }
        IntentPacket intent{};
        intent.Buttons = buttons;
        intent.Aim = player.ModGunVector();
        intent.Position = player.Position;
        intent.WeaponSelect = static_cast<std::uint8_t>(player.CurrentWeapon());
        intent.AmmoUa = static_cast<std::uint16_t>(std::clamp(player.ModAmmo().first, 0, 0xFFFF));
        intent.AmmoMissiles = static_cast<std::uint16_t>(std::clamp(player.ModAmmo().second, 0, 0xFFFF));
        intent.Presses = std::make_shared<std::vector<std::uint32_t>>(_pressHistory.begin(), _pressHistory.end());
        intent.ChargeLevel = static_cast<std::uint8_t>(std::clamp(_hasLatch ? _latchedCharge : player.ModChargeLevel(), 0, 255));
        intent.BoostDamage = static_cast<std::uint8_t>(std::clamp(_hasLatch ? _latchedBoostDamage : player.ModBoostDamage(), 0, 255));
        intent.ShotFlags = static_cast<std::uint8_t>((player.DoubleDamage() ? IntentPacket::FlagDoubleDamage : 0)
            | (player.IsPrimeHunter() ? IntentPacket::FlagPrimeHunter : 0));
        intent.HasState = true;
        intent.AckFrame = NetHooks::SnapshotOwnsPuppets() && NetSession::AppliedSnapshotFrame() != 0
            ? NetSession::AppliedSnapshotFrame()
            : NetSession::LastSnapshotFrame();
        std::uint32_t readFrame = 0;
        std::uint8_t readSub = 0;
        if (NetSmoothing::AckPoint(readFrame, readSub))
        {
            intent.AckFrame = readFrame;
            intent.AckSubFrame = readSub;
        }
        _hasLatch = false;
        if (NetLog::Enabled() && (HasFlag(intent.Buttons, IntentButtons::Shoot) || c.Shoot().IsReleased()))
        {
            NetShotDiagnostics::Trace("input", ShotKey::For(player.SlotIndex(), intent.AckFrame), player.CurrentWeapon(),
                "intentFrame=" + std::to_string(intent.Frame) + " intentLife=" + std::to_string(intent.LifeId)
                + " inPlay=" + (HasFlag(intent.Buttons, IntentButtons::InPlayState) ? "True" : "False")
                + " shoot=" + (c.Shoot().IsDown() ? "True" : "False")
                + " press=" + (c.Shoot().IsPressed() ? "True" : "False"));
        }
        return intent;
    }

    bool NetPlayerBridge::RespawnRequested(std::int32_t slot)
    {
        return NetSession::Active() && slot != NetSession::LocalSlot()
            && slot >= 0 && slot < static_cast<std::int32_t>(_respawnRequested.size()) && _respawnRequested[Index(slot)];
    }

    void NetPlayerBridge::ApplyIntent(Entities::PlayerEntity& player, const IntentPacket& intent)
    {
        if (intent.LifeId == 0 || !NetPlayerLifecycle::Matches(player.SlotIndex(), intent.SlotGeneration, intent.LifeId))
        {
            return;
        }
        if (!Sane(intent.Aim))
        {
            _rejectedUpdates++;
            NetLog::Event("slot " + std::to_string(player.SlotIndex()) + " intent rejected: aim=" + intent.Aim.ToString());
            return;
        }
        Entities::PlayerControls& c = player.Controls();
        const std::int32_t aimSlot = player.SlotIndex();
        if (aimSlot >= 0 && aimSlot < static_cast<std::int32_t>(_aimHeld.size()) && _aimHeld[Index(aimSlot)]
            && (intent.AckFrame >= SpawnFrame[Index(aimSlot)]
                || NetSession::NetFrame() - SpawnFrame[Index(aimSlot)] > AimHoldCeiling))
        {
            _aimHeld[Index(aimSlot)] = false;
        }
        std::int32_t shootAge = 0;
        const IntentButtons missed = MissedPresses(player.SlotIndex(), intent, shootAge);
        if (player.SlotIndex() >= 0 && player.SlotIndex() < static_cast<std::int32_t>(ShootPressAge.size()))
        {
            ShootPressAge[Index(player.SlotIndex())] = shootAge;
        }
        NativeRuntime::ManagedAt(_respawnRequested, player.SlotIndex()) = !HasFlag(intent.Buttons, IntentButtons::InPlayState)
            && HasFlag(intent.Buttons, IntentButtons::Shoot);
        if (!HasFlag(intent.Buttons, IntentButtons::InPlayState))
        {
            c.ClearAll();
            NativeRuntime::ManagedAt(ShootPressAge, player.SlotIndex()) = 0;
            player.ModSetSpectating(HasFlag(intent.Buttons, IntentButtons::SpectatingState));
            return;
        }
        Set(c.MoveLeft(), HasFlag(intent.Buttons, IntentButtons::MoveLeft), HasFlag(missed, IntentButtons::MoveLeft));
        Set(c.MoveRight(), HasFlag(intent.Buttons, IntentButtons::MoveRight), HasFlag(missed, IntentButtons::MoveRight));
        Set(c.MoveUp(), HasFlag(intent.Buttons, IntentButtons::MoveUp), HasFlag(missed, IntentButtons::MoveUp));
        Set(c.MoveDown(), HasFlag(intent.Buttons, IntentButtons::MoveDown), HasFlag(missed, IntentButtons::MoveDown));
        Set(c.Shoot(), HasFlag(intent.Buttons, IntentButtons::Shoot), HasFlag(missed, IntentButtons::Shoot));
        Set(c.Zoom(), HasFlag(intent.Buttons, IntentButtons::Zoom), HasFlag(missed, IntentButtons::Zoom));
        Set(c.Jump(), HasFlag(intent.Buttons, IntentButtons::Jump), HasFlag(missed, IntentButtons::Jump));
        Set(c.Morph(), HasFlag(intent.Buttons, IntentButtons::Morph), HasFlag(missed, IntentButtons::Morph));
        if (c.Morph().IsPressed())
        {
            NetLog::Event("slot " + std::to_string(player.SlotIndex()) + " morph press received, now " + player.ModFormState());
        }
        Set(c.Boost(), HasFlag(intent.Buttons, IntentButtons::Boost), HasFlag(missed, IntentButtons::Boost));
        Set(c.AltAttack(), HasFlag(intent.Buttons, IntentButtons::AltAttack), HasFlag(missed, IntentButtons::AltAttack));
        Set(c.ScanVisor(), HasFlag(intent.Buttons, IntentButtons::ScanVisor), HasFlag(missed, IntentButtons::ScanVisor));
        Set(c.NextWeapon(), HasFlag(intent.Buttons, IntentButtons::NextWeapon), HasFlag(missed, IntentButtons::NextWeapon));
        Set(c.PrevWeapon(), HasFlag(intent.Buttons, IntentButtons::PrevWeapon), HasFlag(missed, IntentButtons::PrevWeapon));
        Set(c.RolltLeft(), HasFlag(intent.Buttons, IntentButtons::RollLeft), HasFlag(missed, IntentButtons::RollLeft));
        Set(c.RollRight(), HasFlag(intent.Buttons, IntentButtons::RollRight), HasFlag(missed, IntentButtons::RollRight));
        Set(c.RollUp(), HasFlag(intent.Buttons, IntentButtons::RollUp), HasFlag(missed, IntentButtons::RollUp));
        Set(c.RollDown(), HasFlag(intent.Buttons, IntentButtons::RollDown), HasFlag(missed, IntentButtons::RollDown));
        if (intent.WeaponSelect != 0xFF)
        {
            player.ModSetWeapon(static_cast<BeamType>(intent.WeaponSelect));
        }
        player.ModSetAmmo(intent.AmmoUa, intent.AmmoMissiles);
        if ((intent.Buttons & PressedButtons) != IntentButtons::None)
        {
            player.ModNoteInput();
        }
        player.ModSetZoom(HasFlag(intent.Buttons, IntentButtons::ZoomedState));
        player.ModSetSpectating(HasFlag(intent.Buttons, IntentButtons::SpectatingState));
        if (NetSession::IsAuthority())
        {
            ApplyForm(player, HasFlag(intent.Buttons, IntentButtons::AltFormState));
        }
        if (intent.HasState && (NetSession::IsAuthority() || NetSession::IsHost()))
        {
            player.ModSetShotState(intent.ChargeLevel, intent.BoostDamage,
                (intent.ShotFlags & IntentPacket::FlagDoubleDamage) != 0);
        }
    }

    void NetPlayerBridge::NoteSpawn(std::int32_t slot)
    {
        if (slot < 0 || slot >= static_cast<std::int32_t>(_pressSeen.size()))
        {
            return;
        }
        const auto s = Index(slot);
        _pressSeen[s] = false;
        ShootPressAge[s] = 0;
        SpawnFrame[s] = NetSession::NetFrame();
        _aimHeld[s] = true;
    }

    bool NetPlayerBridge::AimTrusted(std::int32_t slot)
    {
        return slot < 0 || slot >= static_cast<std::int32_t>(_aimHeld.size()) || !_aimHeld[Index(slot)];
    }

    IntentButtons NetPlayerBridge::MissedPresses(std::int32_t slot, const IntentPacket& intent, std::int32_t& shootAge)
    {
        shootAge = 0;
        if (slot < 0 || slot >= static_cast<std::int32_t>(_lastPressFrame.size()) || intent.Presses == nullptr)
        {
            return IntentButtons::None;
        }
        const auto s = Index(slot);
        if (!_pressSeen[s])
        {
            _pressSeen[s] = true;
            _lastPressFrame[s] = intent.Frame;
            return IntentButtons::None;
        }
        IntentButtons missed = IntentButtons::None;
        const std::vector<std::uint32_t>& presses = *intent.Presses;
        for (std::int32_t i = static_cast<std::int32_t>(presses.size()) - 1; i >= 0; i--)
        {
            if (intent.Frame < static_cast<std::uint32_t>(i))
            {
                continue;
            }
            const std::uint32_t frame = intent.Frame - static_cast<std::uint32_t>(i);
            if (frame <= _lastPressFrame[s])
            {
                continue;
            }
            const auto press = static_cast<IntentButtons>(presses[Index(i)]);
            missed |= press;
            if (shootAge == 0 && HasFlag(press, IntentButtons::Shoot))
            {
                shootAge = i;
            }
        }
        _lastPressFrame[s] = std::max(_lastPressFrame[s], intent.Frame);
        return missed;
    }

    void NetPlayerBridge::Set(Entities::Keybind& bind, bool down, bool pressed)
    {
        const bool wasDown = bind.IsDown();
        bind.SetIsDown(down || pressed);
        bind.SetIsPressed(pressed);
        bind.SetIsReleased(!down && wasDown && !pressed);
    }

    void NetPlayerBridge::BeginRemoteLife(Entities::PlayerEntity& player, const PlayerState& state)
    {
        const std::int32_t slot = player.SlotIndex();
        ForgetSlot(slot);
        NativeRuntime::ManagedAt(_appliedLifeId, slot) = state.LifeId;
        _lifeApplied[Index(slot)] = true;
        NetHitPrediction::NoteRespawn(slot);
        NetDamage::BeginLife(slot, state);
        NetHitClaims::ForgetSlot(slot);
        NetUnlagged::ResetSlot(slot);
        player.ModResetNetworkHistory();
        player.Controls().ClearAll();
        player.ModSetFrozen(false);
        player.ModSetBurning(false);
        player.ModSetDisrupted(false);
        if (state.LifeId != 0)
        {
            NetPlayerLifecycle::ApplyingSpawn(true);
            try
            {
                player.ModNetSpawn(state.Position, state.Facing);
            }
            catch (...)
            {
                NetPlayerLifecycle::ApplyingSpawn(false);
                throw;
            }
            NetPlayerLifecycle::ApplyingSpawn(false);
            Move(player, state.Position);
            player.ModSetSpawnFacing(state.Facing);
            if (state.Health == 0)
            {
                player.ModNetDie();
            }
        }
        player.SetHealth(state.Health);
    }

    void NetPlayerBridge::ApplyState(Entities::PlayerEntity& player, const PlayerState& state, bool isLocal)
    {
        const std::int32_t slot = player.SlotIndex();
        if (slot != state.SlotIndex || !NetPlayerLifecycle::Matches(slot, state.SlotGeneration, state.LifeId))
        {
            return;
        }
        if (!Sane(state.Position) || !Sane(state.Speed) || !Sane(state.Facing))
        {
            _rejectedUpdates++;
            return;
        }
        const auto s = Index(slot);
        const bool fresh = !_lifeApplied[s] || _appliedLifeId[s] != state.LifeId;
        if (fresh)
        {
            BeginRemoteLife(player, state);
        }
        const bool spawned = (state.Flags & PlayerState::FlagSpawned) != 0 && state.Health > 0;
        _formSaid[s] = static_cast<std::uint8_t>((state.Flags & PlayerState::FlagAltForm) != 0 ? 2 : 1);
        if (!NetRoomChange::Settling())
        {
            GameState::Points()[slot] = state.Points;
            GameState::Kills()[slot] = state.Kills;
            GameState::Deaths()[slot] = state.Deaths;
        }
        NetDamage::Replay(player, state);
        if (!spawned)
        {
            if (state.Health == 0 && player.Health() > 0)
            {
                player.ModNetDie();
            }
            player.SetHealth(state.Health);
            if (state.Health == 0)
            {
                NetHitPrediction::NoteDeath(slot);
            }
            player.ModSetSpectating((state.Flags & PlayerState::FlagSpectating) != 0);
            return;
        }
        if (!fresh && player.Health() <= 0)
        {
            return;
        }
        if (!isLocal)
        {
            Move(player, InForm(player, state.Position, (state.Flags & PlayerState::FlagAltForm) != 0));
            player.SetSpeed(state.Speed);
            player.SetHealth(NetHitPrediction::HealthFor(slot, state.Health));
            player.ModSetFacing(state.Facing);
            player.ModSetWeapon(static_cast<BeamType>(state.CurrentWeapon));
            player.EquipInfo()->Zoomed = (state.Flags & PlayerState::FlagZoomed) != 0;
            ApplyForm(player, (state.Flags & PlayerState::FlagAltForm) != 0);
            player.ModSetSpectating((state.Flags & PlayerState::FlagSpectating) != 0);
        }
        else
        {
            if (!fresh && NetRoomChange::GameplayReady() && Diverged(player, state, slot))
            {
                Move(player, state.Position);
                player.SetSpeed(state.Speed);
                _divergedFrames[s] = 0;
            }
            player.SetHealth(NetHitPrediction::LocalHealthFor(player, state.Health));
        }
        player.ModSetFrozen((state.Flags & PlayerState::FlagFrozen) != 0);
        ApplyAfflictions(player, state);
    }

    void NetPlayerBridge::ApplyAfflictions(Entities::PlayerEntity& player, PlayerState state)
    {
        player.ModSetDisrupted((state.Flags & PlayerState::FlagDisrupted) != 0);
        player.ModSetBurning((state.Flags & PlayerState::FlagBurning) != 0);
    }

    void NetPlayerBridge::ApplyForm(Entities::PlayerEntity& player, bool altForm)
    {
        const std::int32_t slot = player.SlotIndex();
        if (slot < 0 || slot >= static_cast<std::int32_t>(_formReconciliation.size()))
        {
            return;
        }
        const FormCorrection correction = ReconcileForm(slot, NetSession::NetFrame(),
            altForm, player.IsAltForm(), player.IsMorphing(), player.IsUnmorphing(),
            NetSession::SlotPing[Index(slot)]);
        if (correction == FormCorrection::Start)
        {
            player.ModStartFormSwitch();
        }
        else if (correction == FormCorrection::Force)
        {
            player.ModForceForm(altForm);
        }
    }

    FormCorrection NetPlayerBridge::ReconcileForm(std::int32_t slot, std::uint32_t frame, bool desiredAlt,
        bool actualAlt, bool morphing, bool unmorphing, std::int32_t ping)
    {
        return slot < 0 || slot >= static_cast<std::int32_t>(_formReconciliation.size()) ? FormCorrection::None
            : _formReconciliation[Index(slot)].Step(frame, desiredAlt, actualAlt, morphing, unmorphing, ping);
    }

    bool NetPlayerBridge::Diverged(Entities::PlayerEntity& player, const PlayerState& state, std::int32_t slot)
    {
        if (slot < 0 || slot >= static_cast<std::int32_t>(_divergedFrames.size()))
        {
            return false;
        }
        OpenTK::Mathematics::Vector3 then = player.Position;
        const std::int32_t lagFrames = slot < static_cast<std::int32_t>(NetSession::SlotPing.size())
            ? std::clamp(NetSession::SlotPing[Index(slot)] * 60 / 1000, 0, 100)
            : 0;
        OpenTK::Mathematics::Vector3 past{};
        if (lagFrames > 0 && NetSession::NetFrame() > static_cast<std::uint32_t>(lagFrames)
            && player.ModGetNetworkPosition(NetSession::NetFrame() - static_cast<std::uint32_t>(lagFrames), past))
        {
            then = past;
        }
        if (LengthSquared(state.Position - then) <= DesyncDistance * DesyncDistance)
        {
            _divergedFrames[Index(slot)] = 0;
            return false;
        }
        _divergedFrames[Index(slot)]++;
        return _divergedFrames[Index(slot)] >= DivergedFramesBeforeCorrecting;
    }

    void NetPlayerBridge::NoteRoomChanged()
    {
        _formReconciliation.fill(FormReconciliation{});
        _lifeApplied.fill(false);
        _reportSeen.fill(false);
        _divergedFrames.fill(0);
    }

    void NetPlayerBridge::Reset()
    {
        _formReconciliation.fill(FormReconciliation{});
        _appliedLifeId.fill(0);
        _lifeApplied.fill(false);
        _snaps = 0;
        _worstSnap = 0.0F;
        NodeLookupsUnresolved = 0;
        PlacementsRefused = 0;
        SpawnFacingsTurned = 0;
        WorstSpawnFacing = 0.0F;
        StaleDeathsIgnored = 0;
        _formSaid.fill(0);
        _lastPressFrame.fill(0);
        _pressSeen.fill(false);
        _aimHeld.fill(false);
        SpawnFrame.fill(0);
        ShootPressAge.fill(0);
        _pressHistory.fill(0);
        _hasLatch = false;
        _divergedFrames.fill(0);
        _lastReportPosition.fill(OpenTK::Mathematics::Vector3::Zero);
        _lastReportFrame.fill(0);
        _reportSeen.fill(false);
    }

    void NetPlayerBridge::ForgetSlot(std::int32_t slot)
    {
        if (slot < 0 || slot >= Entities::PlayerEntity::SlotCapacity)
        {
            return;
        }
        const auto s = Index(slot);
        _formReconciliation[s].Reset();
        _lifeApplied[s] = false;
        _appliedLifeId[s] = 0;
        _lastPressFrame[s] = 0;
        _pressSeen[s] = false;
        _aimHeld[s] = false;
        SpawnFrame[s] = 0;
        ShootPressAge[s] = 0;
        _respawnRequested[s] = false;
        if (slot == NetSession::LocalSlot())
        {
            _pressHistory.fill(0);
            _hasLatch = false;
            _latchedCharge = _latchedBoostDamage = 0;
        }
        _divergedFrames[s] = 0;
        _lastReportPosition[s] = OpenTK::Mathematics::Vector3::Zero;
        _lastReportFrame[s] = 0;
        _reportSeen[s] = false;
    }

    void NetPlayerBridge::ApplyReportedPosition(Entities::PlayerEntity& player, const IntentPacket& intent)
    {
        if (!Sane(intent.Position))
        {
            _rejectedUpdates++;
            return;
        }
        if (FrozenInPlace(player))
        {
            return;
        }
        if (VectorEquals(intent.Position, OpenTK::Mathematics::Vector3::Zero))
        {
            return; // the owner has not spawned yet
        }
        if (StaleSinceSpawn(player, intent))
        {
            return;
        }
        const OpenTK::Mathematics::Vector3 reported = InForm(player, intent.Position,
            HasFlag(intent.Buttons, IntentButtons::AltFormState));
        NoteReportedVelocity(player, reported, intent.Frame);
        const OpenTK::Mathematics::Vector3 delta = reported - static_cast<OpenTK::Mathematics::Vector3>(player.Position);
        const float distance = Length(delta);
        if (distance > SnapDistance)
        {
            _snaps++;
            _worstSnap = std::max(_worstSnap, distance);
            Move(player, reported);
            return;
        }
        Move(player, reported);
    }

    void NetPlayerBridge::RestoreSnapshotPosition(Entities::PlayerEntity& player, const PlayerState& state)
    {
        if (FrozenInPlace(player))
        {
            return;
        }
        OpenTK::Mathematics::Vector3 smoothed{};
        bool smoothedAlt = false;
        if (NetSmoothing::Sample(player.SlotIndex(), smoothed, smoothedAlt)
            && Sane(smoothed) && !VectorEquals(smoothed, OpenTK::Mathematics::Vector3::Zero))
        {
            Move(player, InForm(player, smoothed, smoothedAlt));
            return;
        }
        if (!Sane(state.Position) || VectorEquals(state.Position, OpenTK::Mathematics::Vector3::Zero))
        {
            return;
        }
        Move(player, InForm(player, state.Position, (state.Flags & PlayerState::FlagAltForm) != 0));
    }

    void NetPlayerBridge::RestoreReportedPosition(Entities::PlayerEntity& player, const IntentPacket& intent)
    {
        if (!Sane(intent.Position) || VectorEquals(intent.Position, OpenTK::Mathematics::Vector3::Zero)
            || StaleSinceSpawn(player, intent) || FrozenInPlace(player))
        {
            return;
        }
        Move(player, InForm(player, intent.Position, HasFlag(intent.Buttons, IntentButtons::AltFormState)));
    }

    bool NetPlayerBridge::StaleSinceSpawn(Entities::PlayerEntity& player, const IntentPacket& intent)
    {
        return !NetPlayerLifecycle::Matches(player.SlotIndex(), intent.SlotGeneration, intent.LifeId)
            || !HasFlag(intent.Buttons, IntentButtons::InPlayState);
    }

    void NetPlayerBridge::NoteReportedVelocity(Entities::PlayerEntity& player,
        OpenTK::Mathematics::Vector3 reported, std::uint32_t frame)
    {
        const std::int32_t slot = player.SlotIndex();
        if (slot < 0 || slot >= static_cast<std::int32_t>(_lastReportFrame.size()))
        {
            return;
        }
        const auto s = Index(slot);
        if (_reportSeen[s] && frame > _lastReportFrame[s])
        {
            const std::uint32_t elapsed = std::min(frame - _lastReportFrame[s], 8U);
            const OpenTK::Mathematics::Vector3 travelled = reported - _lastReportPosition[s];
            const float step = Length(travelled);
            if (!Sane(travelled) || step > SnapDistance)
            {
                player.SetSpeed(OpenTK::Mathematics::Vector3::Zero);
            }
            else
            {
                OpenTK::Mathematics::Vector3 speed = OpenTK::Mathematics::Divide(travelled, static_cast<float>(elapsed));
                const float magnitude = Length(speed);
                if (magnitude > MaxReportedSpeed)
                {
                    speed = OpenTK::Mathematics::Multiply(speed, MaxReportedSpeed / magnitude);
                }
                player.SetSpeed(speed);
            }
        }
        if (!_reportSeen[s] || frame > _lastReportFrame[s])
        {
            _reportSeen[s] = true;
            _lastReportFrame[s] = frame;
            _lastReportPosition[s] = reported;
        }
    }

    bool NetPlayerBridge::FrozenInPlace(Entities::PlayerEntity& player)
    {
        return player.ModFrozen();
    }

    void NetPlayerBridge::Move(Entities::PlayerEntity& player, OpenTK::Mathematics::Vector3 position)
    {
        const OpenTK::Mathematics::Vector3 previous = player.Position;
        player.Position = position;
        player.SetPrevPosition(position);
        player.ModRefreshNodeRef(previous);
        player.ModRefreshVolume();
    }
}
