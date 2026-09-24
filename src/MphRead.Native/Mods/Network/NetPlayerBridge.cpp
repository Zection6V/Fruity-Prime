#include "NetPlayerBridge.hpp"

#include "../../GameState.hpp"
#include "../EndScreen.hpp"
#include "NetDamage.hpp"
#include "NetHitPrediction.hpp"
#include "NetLog.hpp"
#include "NetRoomChange.hpp"
#include "NetSession.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Formats/Types.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <langinfo.h>
#include <locale.h>
#endif

using ::MphRead::HasFlag;
using ::MphRead::NativeRuntime::MathMax;
using ::OpenTK::Mathematics::Length;
using ::OpenTK::Mathematics::LengthSquared;

namespace
{
    enum class Control : std::int32_t
    {
        MoveLeft = 0,
        MoveRight,
        MoveUp,
        MoveDown,
        Shoot,
        Zoom,
        Jump,
        Morph,
        Boost,
        AltAttack,
        ScanVisor,
        NextWeapon,
        PrevWeapon,
        RollLeft,
        RollRight,
        RollUp,
        RollDown
    };

    [[nodiscard]] bool VectorEquals(
        OpenTK::Mathematics::Vector3 left,
        OpenTK::Mathematics::Vector3 right) noexcept
    {
        return left.X == right.X && left.Y == right.Y && left.Z == right.Z;
    }

    [[nodiscard]] std::int32_t UncheckedIncrement(std::int32_t value) noexcept
    {
        const std::uint32_t bits = static_cast<std::uint32_t>(value) + 1U;
        return std::bit_cast<std::int32_t>(bits);
    }

    [[nodiscard]] std::int64_t UncheckedIncrement(std::int64_t value) noexcept
    {
        const std::uint64_t bits = static_cast<std::uint64_t>(value) + 1ULL;
        return std::bit_cast<std::int64_t>(bits);
    }

    [[nodiscard]] std::int32_t UncheckedMultiply(
        std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t bits = static_cast<std::uint32_t>(left)
            * static_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(bits);
    }

    [[nodiscard]] std::uint16_t ClampUInt16(std::int32_t value) noexcept
    {
        if (value < 0)
        {
            return 0;
        }
        if (value > static_cast<std::int32_t>(std::numeric_limits<std::uint16_t>::max()))
        {
            return std::numeric_limits<std::uint16_t>::max();
        }
        return static_cast<std::uint16_t>(value);
    }

#if defined(_WIN32)
    [[nodiscard]] std::string LocaleInfoUtf8(LCTYPE type, std::string fallback)
    {
        wchar_t buffer[32]{};
        const int length = GetLocaleInfoEx(
            LOCALE_NAME_USER_DEFAULT, type, buffer,
            static_cast<int>(std::size(buffer)));
        if (length <= 1)
        {
            return fallback;
        }
        const int bytes = WideCharToMultiByte(
            CP_UTF8, 0, buffer, length - 1, nullptr, 0, nullptr, nullptr);
        if (bytes <= 0)
        {
            return fallback;
        }
        std::string result(static_cast<std::size_t>(bytes), '\0');
        WideCharToMultiByte(
            CP_UTF8, 0, buffer, length - 1, result.data(), bytes, nullptr, nullptr);
        return result;
    }
#endif

    [[nodiscard]] std::string DecimalSeparator()
    {
#if defined(_WIN32)
        return LocaleInfoUtf8(LOCALE_SDECIMAL, ".");
#else
#if defined(__ANDROID__)
        const lconv* locale = ::localeconv();
        const char* value = locale != nullptr ? locale->decimal_point : nullptr;
        return value != nullptr && value[0] != '\0' ? value : ".";
#else
        locale_t locale = newlocale(LC_NUMERIC_MASK, "", nullptr);
        if (locale == static_cast<locale_t>(0))
        {
            return ".";
        }
        const char* value = nl_langinfo_l(RADIXCHAR, locale);
        std::string result = value != nullptr && value[0] != '\0' ? value : ".";
        freelocale(locale);
        return result;
#endif
#endif
    }

    [[nodiscard]] std::string NegativeSign()
    {
#if defined(_WIN32)
        return LocaleInfoUtf8(LOCALE_SNEGATIVESIGN, "-");
#else
#if defined(__ANDROID__)
        const lconv* locale = ::localeconv();
        const char* value = locale != nullptr ? locale->negative_sign : nullptr;
        return value != nullptr && value[0] != '\0' ? value : "-";
#else
        locale_t locale = newlocale(LC_MONETARY_MASK, "", nullptr);
        if (locale == static_cast<locale_t>(0))
        {
            return "-";
        }
#if defined(NEGATIVE_SIGN)
        const char* value = nl_langinfo_l(NEGATIVE_SIGN, locale);
        std::string result = value != nullptr && value[0] != '\0' ? value : "-";
#else
        std::string result = "-";
#endif
        freelocale(locale);
        return result;
#endif
#endif
    }

    [[nodiscard]] std::string NaNSymbol()
    {
#if defined(_WIN32) && defined(LOCALE_SNAN)
        return LocaleInfoUtf8(LOCALE_SNAN, "NaN");
#else
        return "NaN";
#endif
    }

    [[nodiscard]] std::string PositiveInfinitySymbol()
    {
#if defined(_WIN32) && defined(LOCALE_SPOSINFINITY)
        return LocaleInfoUtf8(LOCALE_SPOSINFINITY, "\xE2\x88\x9E");
#else
        return "\xE2\x88\x9E";
#endif
    }

    [[nodiscard]] std::string NegativeInfinitySymbol()
    {
#if defined(_WIN32) && defined(LOCALE_SNEGINFINITY)
        return LocaleInfoUtf8(LOCALE_SNEGINFINITY, NegativeSign() + "\xE2\x88\x9E");
#else
        return NegativeSign() + "\xE2\x88\x9E";
#endif
    }

    [[nodiscard]] std::string Int32Text(std::int32_t value)
    {
        std::array<char, 32> buffer{};
        const auto converted = std::to_chars(
            buffer.data(), buffer.data() + buffer.size(), value);
        if (converted.ec != std::errc{})
        {
            throw std::runtime_error("Int32 formatting failed.");
        }
        std::string result(buffer.data(), converted.ptr);
        if (!result.empty() && result[0] == '-')
        {
            result.erase(result.begin());
            result.insert(0, NegativeSign());
        }
        return result;
    }

    [[nodiscard]] std::string SingleText(float value)
    {
        if (std::isnan(value))
        {
            return NaNSymbol();
        }
        if (std::isinf(value))
        {
            return std::signbit(value) ? NegativeInfinitySymbol() : PositiveInfinitySymbol();
        }
        std::array<char, 64> buffer{};
        const auto converted = std::to_chars(
            buffer.data(), buffer.data() + buffer.size(), value,
            std::chars_format::general);
        if (converted.ec != std::errc{})
        {
            throw std::runtime_error("Single formatting failed.");
        }
        std::string result(buffer.data(), converted.ptr);
        if (!result.empty() && result[0] == '-')
        {
            result.erase(result.begin());
            result.insert(0, NegativeSign());
        }
        const std::size_t point = result.find('.');
        if (point != std::string::npos)
        {
            result.replace(point, 1, DecimalSeparator());
        }
        return result;
    }

    [[nodiscard]] std::string VectorText(OpenTK::Mathematics::Vector3 value)
    {
        return "(" + SingleText(value.X) + ", " + SingleText(value.Y)
            + ", " + SingleText(value.Z) + ")";
    }

    [[nodiscard]] bool IsPressedOn(const MphRead::Entities::Keybind& bind)
    {
        return bind.IsPressed();
    }

    [[nodiscard]] bool IsDownOn(const MphRead::Entities::Keybind& bind)
    {
        return bind.IsDown();
    }

    [[nodiscard]] MphRead::Entities::Keybind& ControlFor(
        MphRead::Entities::PlayerControls& controls, Control control)
    {
        switch (control)
        {
        case Control::MoveLeft: return controls.MoveLeft();
        case Control::MoveRight: return controls.MoveRight();
        case Control::MoveUp: return controls.MoveUp();
        case Control::MoveDown: return controls.MoveDown();
        case Control::Shoot: return controls.Shoot();
        case Control::Zoom: return controls.Zoom();
        case Control::Jump: return controls.Jump();
        case Control::Morph: return controls.Morph();
        case Control::Boost: return controls.Boost();
        case Control::AltAttack: return controls.AltAttack();
        case Control::ScanVisor: return controls.ScanVisor();
        case Control::NextWeapon: return controls.NextWeapon();
        case Control::PrevWeapon: return controls.PrevWeapon();
        case Control::RollLeft: return controls.RolltLeft();
        case Control::RollRight: return controls.RollRight();
        case Control::RollUp: return controls.RollUp();
        case Control::RollDown: return controls.RollDown();
        }
        return controls.MoveLeft();
    }

    [[nodiscard]] bool IsPressed(
        MphRead::Entities::PlayerControls& controls, Control control)
    {
        return IsPressedOn(ControlFor(controls, control));
    }

    [[nodiscard]] bool IsDown(
        MphRead::Entities::PlayerControls& controls, Control control)
    {
        return IsDownOn(ControlFor(controls, control));
    }
}

namespace MphRead::Mods::Network
{
    std::string NetPlayerBridge::FormSaidByAuthority()
    {
        std::string text;
        text.reserve(Entities::PlayerEntity::SlotCapacity);
        for (std::int32_t i = 0;
            i < Entities::PlayerEntity::MaxPlayers()
                && i < static_cast<std::int32_t>(_formSaid.size());
            ++i)
        {
            text.push_back(_formSaid[static_cast<std::size_t>(i)] == 0
                ? '-'
                : _formSaid[static_cast<std::size_t>(i)] == 2 ? 'A' : 'b');
        }
        return text;
    }

    OpenTK::Mathematics::Vector3 NetPlayerBridge::InFormFor(
        Entities::PlayerEntity& player,
        OpenTK::Mathematics::Vector3 position,
        bool measuredInAlt)
    {
        return InForm(player, position, measuredInAlt);
    }

    OpenTK::Mathematics::Vector3 NetPlayerBridge::InForm(
        Entities::PlayerEntity& player,
        OpenTK::Mathematics::Vector3 position,
        bool measuredInAlt)
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
        const auto& volumes
            = Entities::PlayerEntity::PlayerVolumes[static_cast<std::size_t>(hunter)];
        const OpenTK::Mathematics::Vector3 delta
            = volumes[0].SpherePosition - volumes[2].SpherePosition;
        return measuredInAlt ? position - delta : position + delta;
    }

    bool NetPlayerBridge::Sane(OpenTK::Mathematics::Vector3 value) noexcept
    {
        return std::isfinite(value.X) && std::isfinite(value.Y) && std::isfinite(value.Z)
            && std::fabs(value.X) < PositionLimit
            && std::fabs(value.Y) < PositionLimit
            && std::fabs(value.Z) < PositionLimit;
    }

    void NetPlayerBridge::RecordPresses(Entities::PlayerEntity& player)
    {
        Entities::PlayerControls& controls = player.Controls();
        IntentButtons pressed = IntentButtons::None;
        if (IsPressed(controls, Control::MoveLeft)) pressed |= IntentButtons::MoveLeft;
        if (IsPressed(controls, Control::MoveRight)) pressed |= IntentButtons::MoveRight;
        if (IsPressed(controls, Control::MoveUp)) pressed |= IntentButtons::MoveUp;
        if (IsPressed(controls, Control::MoveDown)) pressed |= IntentButtons::MoveDown;
        if (IsPressed(controls, Control::Shoot)) pressed |= IntentButtons::Shoot;
        if (IsPressed(controls, Control::Zoom)) pressed |= IntentButtons::Zoom;
        if (IsPressed(controls, Control::Jump)) pressed |= IntentButtons::Jump;
        if (IsPressed(controls, Control::Morph)) pressed |= IntentButtons::Morph;
        if (IsPressed(controls, Control::Boost)) pressed |= IntentButtons::Boost;
        if (IsPressed(controls, Control::AltAttack)) pressed |= IntentButtons::AltAttack;
        if (IsPressed(controls, Control::ScanVisor)) pressed |= IntentButtons::ScanVisor;
        if (IsPressed(controls, Control::NextWeapon)) pressed |= IntentButtons::NextWeapon;
        if (IsPressed(controls, Control::PrevWeapon)) pressed |= IntentButtons::PrevWeapon;
        if (IsPressed(controls, Control::RollLeft)) pressed |= IntentButtons::RollLeft;
        if (IsPressed(controls, Control::RollRight)) pressed |= IntentButtons::RollRight;
        if (IsPressed(controls, Control::RollUp)) pressed |= IntentButtons::RollUp;
        if (IsPressed(controls, Control::RollDown)) pressed |= IntentButtons::RollDown;
        for (std::int32_t i = static_cast<std::int32_t>(_pressHistory.size()) - 1;
            i > 0; --i)
        {
            _pressHistory[static_cast<std::size_t>(i)]
                = _pressHistory[static_cast<std::size_t>(i - 1)];
        }
        _pressHistory[0] = static_cast<std::uint32_t>(pressed);
    }

    IntentPacket NetPlayerBridge::CaptureIntent(Entities::PlayerEntity& player)
    {
        Entities::PlayerControls& controls = player.Controls();
        IntentButtons buttons = IntentButtons::None;
        if (IsDown(controls, Control::MoveLeft)) buttons |= IntentButtons::MoveLeft;
        if (IsDown(controls, Control::MoveRight)) buttons |= IntentButtons::MoveRight;
        if (IsDown(controls, Control::MoveUp)) buttons |= IntentButtons::MoveUp;
        if (IsDown(controls, Control::MoveDown)) buttons |= IntentButtons::MoveDown;
        if (IsDown(controls, Control::Shoot)) buttons |= IntentButtons::Shoot;
        if (IsDown(controls, Control::Zoom)) buttons |= IntentButtons::Zoom;
        if (IsDown(controls, Control::Jump)) buttons |= IntentButtons::Jump;
        if (IsDown(controls, Control::Morph)) buttons |= IntentButtons::Morph;
        if (IsDown(controls, Control::Boost)) buttons |= IntentButtons::Boost;
        if (IsDown(controls, Control::AltAttack)) buttons |= IntentButtons::AltAttack;
        if (IsDown(controls, Control::ScanVisor)) buttons |= IntentButtons::ScanVisor;
        if (IsDown(controls, Control::NextWeapon)) buttons |= IntentButtons::NextWeapon;
        if (IsDown(controls, Control::PrevWeapon)) buttons |= IntentButtons::PrevWeapon;
        if (IsDown(controls, Control::RollLeft)) buttons |= IntentButtons::RollLeft;
        if (IsDown(controls, Control::RollRight)) buttons |= IntentButtons::RollRight;
        if (IsDown(controls, Control::RollUp)) buttons |= IntentButtons::RollUp;
        if (IsDown(controls, Control::RollDown)) buttons |= IntentButtons::RollDown;
        if (player.EquipInfo()->Zoomed)
        {
            buttons |= IntentButtons::ZoomedState;
        }
        if (player.IsAltForm())
        {
            buttons |= IntentButtons::AltFormState;
        }
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

        IntentPacket result{};
        result.Buttons = buttons;
        result.Aim = player.ModGunVector();
        result.Position = player.Position;
        result.WeaponSelect = static_cast<std::uint8_t>(player.CurrentWeapon());
        result.AmmoUa = ClampUInt16(player.ModAmmo().first);
        result.AmmoMissiles = ClampUInt16(player.ModAmmo().second);
        result.Presses = std::make_shared<std::vector<std::uint32_t>>(
            _pressHistory.begin(), _pressHistory.end());
        result.AckFrame = NetSession::LastSnapshotFrame();
        return result;
    }

    void NetPlayerBridge::ApplyIntent(
        Entities::PlayerEntity& player, const IntentPacket& intent)
    {
        if (!Sane(intent.Aim))
        {
            _rejectedUpdates = UncheckedIncrement(_rejectedUpdates);
            NetLog::Event("slot " + Int32Text(player.SlotIndex())
                + " intent rejected: aim=" + VectorText(intent.Aim));
            return;
        }

        Entities::PlayerControls& controls = player.Controls();
        const IntentButtons missed = MissedPresses(player.SlotIndex(), intent);
        Set(ControlFor(controls, Control::MoveLeft), HasFlag(intent.Buttons, IntentButtons::MoveLeft), HasFlag(missed, IntentButtons::MoveLeft));
        Set(ControlFor(controls, Control::MoveRight), HasFlag(intent.Buttons, IntentButtons::MoveRight), HasFlag(missed, IntentButtons::MoveRight));
        Set(ControlFor(controls, Control::MoveUp), HasFlag(intent.Buttons, IntentButtons::MoveUp), HasFlag(missed, IntentButtons::MoveUp));
        Set(ControlFor(controls, Control::MoveDown), HasFlag(intent.Buttons, IntentButtons::MoveDown), HasFlag(missed, IntentButtons::MoveDown));
        Set(ControlFor(controls, Control::Shoot), HasFlag(intent.Buttons, IntentButtons::Shoot), HasFlag(missed, IntentButtons::Shoot));
        Set(ControlFor(controls, Control::Zoom), HasFlag(intent.Buttons, IntentButtons::Zoom), HasFlag(missed, IntentButtons::Zoom));
        Set(ControlFor(controls, Control::Jump), HasFlag(intent.Buttons, IntentButtons::Jump), HasFlag(missed, IntentButtons::Jump));
        Set(ControlFor(controls, Control::Morph), HasFlag(intent.Buttons, IntentButtons::Morph), HasFlag(missed, IntentButtons::Morph));
        if (IsPressedOn(ControlFor(controls, Control::Morph)))
        {
            NetLog::Event("slot " + Int32Text(player.SlotIndex())
                + " morph press received, now " + player.ModFormState());
        }
        Set(ControlFor(controls, Control::Boost), HasFlag(intent.Buttons, IntentButtons::Boost), HasFlag(missed, IntentButtons::Boost));
        Set(ControlFor(controls, Control::AltAttack), HasFlag(intent.Buttons, IntentButtons::AltAttack), HasFlag(missed, IntentButtons::AltAttack));
        Set(ControlFor(controls, Control::ScanVisor), HasFlag(intent.Buttons, IntentButtons::ScanVisor), HasFlag(missed, IntentButtons::ScanVisor));
        Set(ControlFor(controls, Control::NextWeapon), HasFlag(intent.Buttons, IntentButtons::NextWeapon), HasFlag(missed, IntentButtons::NextWeapon));
        Set(ControlFor(controls, Control::PrevWeapon), HasFlag(intent.Buttons, IntentButtons::PrevWeapon), HasFlag(missed, IntentButtons::PrevWeapon));
        Set(ControlFor(controls, Control::RollLeft), HasFlag(intent.Buttons, IntentButtons::RollLeft), HasFlag(missed, IntentButtons::RollLeft));
        Set(ControlFor(controls, Control::RollRight), HasFlag(intent.Buttons, IntentButtons::RollRight), HasFlag(missed, IntentButtons::RollRight));
        Set(ControlFor(controls, Control::RollUp), HasFlag(intent.Buttons, IntentButtons::RollUp), HasFlag(missed, IntentButtons::RollUp));
        Set(ControlFor(controls, Control::RollDown), HasFlag(intent.Buttons, IntentButtons::RollDown), HasFlag(missed, IntentButtons::RollDown));

        if (intent.WeaponSelect != 0xFFU)
        {
            player.ModSetWeapon(static_cast<MphRead::BeamType>(intent.WeaponSelect));
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
    }

    IntentButtons NetPlayerBridge::MissedPresses(
        std::int32_t slot, const IntentPacket& intent)
    {
        if (slot < 0 || slot >= static_cast<std::int32_t>(_lastPressFrame.size())
            || intent.Presses == nullptr)
        {
            return IntentButtons::None;
        }
        const std::size_t index = static_cast<std::size_t>(slot);
        if (!_pressSeen[index])
        {
            _pressSeen[index] = true;
            _lastPressFrame[index] = intent.Frame;
            return IntentButtons::None;
        }
        IntentButtons missed = IntentButtons::None;
        for (std::int32_t i = static_cast<std::int32_t>(intent.Presses->size()) - 1;
            i >= 0; --i)
        {
            if (intent.Frame < static_cast<std::uint32_t>(i))
            {
                continue;
            }
            const std::uint32_t frame = intent.Frame - static_cast<std::uint32_t>(i);
            if (frame <= _lastPressFrame[index])
            {
                continue;
            }
            missed |= static_cast<IntentButtons>(
                intent.Presses->at(static_cast<std::size_t>(i)));
        }
        _lastPressFrame[index] = std::max(_lastPressFrame[index], intent.Frame);
        return missed;
    }

    void NetPlayerBridge::Set(Entities::Keybind& bind, bool down, bool pressed)
    {
        const bool wasDown = IsDownOn(bind);
        bind.SetIsDown(down || pressed);
        bind.SetIsPressed(pressed);
        bind.SetIsReleased(!down && wasDown && !pressed);
    }

    void NetPlayerBridge::ApplyState(
        Entities::PlayerEntity& player, const PlayerState& state, bool isLocal)
    {
        if (!Sane(state.Position) || !Sane(state.Speed) || !Sane(state.Facing))
        {
            _rejectedUpdates = UncheckedIncrement(_rejectedUpdates);
            NetLog::Event("slot " + Int32Text(player.SlotIndex())
                + " snapshot rejected: pos=" + VectorText(state.Position)
                + " speed=" + VectorText(state.Speed)
                + " facing=" + VectorText(state.Facing));
            return;
        }
        const bool spawned = (state.Flags & PlayerState::FlagSpawned) != 0;
        const bool wasInPlay
            = HasFlag(player.LoadFlags(), Entities::LoadFlags::Spawned) && player.Health() > 0;
        const std::int32_t slot = player.SlotIndex();
        if (slot >= 0 && slot < static_cast<std::int32_t>(_formSaid.size()))
        {
            _formSaid[static_cast<std::size_t>(slot)]
                = static_cast<std::uint8_t>((state.Flags & PlayerState::FlagAltForm) != 0 ? 2 : 1);
        }
        const bool justPlaced = spawned && slot >= 0
            && slot < static_cast<std::int32_t>(_authoritySpawned.size())
            && !_authoritySpawned[static_cast<std::size_t>(slot)];
        if (slot >= 0 && slot < static_cast<std::int32_t>(_authoritySpawned.size()))
        {
            _authoritySpawned[static_cast<std::size_t>(slot)] = spawned;
        }
        if (slot >= 0 && slot < static_cast<std::int32_t>(GameState::Points().size())
            && !NetRoomChange::Settling())
        {
            GameState::Points()[slot] = state.Points;
            GameState::Kills()[slot] = state.Kills;
            GameState::Deaths()[slot] = state.Deaths;
        }
        NetDamage::Replay(player, state);
        if (!spawned)
        {
            if (wasInPlay && state.Health == 0 && player.Health() > 0)
            {
                player.ModNetDie();
            }
            player.SetHealth(state.Health);
            if (state.Health == 0)
            {
                NetHitPrediction::NoteDeath(slot);
            }
            return;
        }
        if (!wasInPlay)
        {
            if (NetHitPrediction::HeldDead(slot))
            {
                return;
            }
            NetHitPrediction::NoteRespawn(slot);
            player.ModNetSpawn(state.Position, state.Facing);
        }
        if (isLocal)
        {
            if (NetRoomChange::Settling())
            {
            }
            else if (justPlaced)
            {
                if (player.ModPlacementBelongsHere(state.Position))
                {
                    Move(player, state.Position);
                }
                else
                {
                    PlacementsRefused = UncheckedIncrement(PlacementsRefused);
                    NetLog::Event("slot " + Int32Text(player.SlotIndex())
                        + " kept its own spawn: the authority placed it at "
                        + VectorText(state.Position)
                        + ", which is not near any spawn point in this room");
                    _authoritySpawned[static_cast<std::size_t>(slot)] = false;
                }
                player.SetSpeed(OpenTK::Mathematics::Vector3::Zero);
            }
            else if (Diverged(player, state, slot))
            {
                NetLog::Event("slot " + Int32Text(player.SlotIndex())
                    + " pulled back to the authority from "
                    + VectorText(player.Position) + " to " + VectorText(state.Position));
                Move(player, state.Position);
                player.SetSpeed(state.Speed);
                _divergedFrames[static_cast<std::size_t>(slot)] = 0;
            }
            player.SetHealth(NetHitPrediction::LocalHealthFor(player, state.Health));
            player.ModSetFrozen((state.Flags & PlayerState::FlagFrozen) != 0);
            ApplyAfflictions(player, state);
            return;
        }

        Move(player, InForm(player, state.Position,
            (state.Flags & PlayerState::FlagAltForm) != 0));
        player.SetSpeed(state.Speed);
        player.SetHealth(NetHitPrediction::HealthFor(slot, state.Health));
        player.ModSetFacing(state.Facing);
        player.ModSetWeapon(static_cast<MphRead::BeamType>(state.CurrentWeapon));
        player.EquipInfo()->Zoomed = (state.Flags & PlayerState::FlagZoomed) != 0;
        ApplyForm(player, (state.Flags & PlayerState::FlagAltForm) != 0);
        player.ModSetSpectating((state.Flags & PlayerState::FlagSpectating) != 0);
        player.ModSetFrozen((state.Flags & PlayerState::FlagFrozen) != 0);
        ApplyAfflictions(player, state);
    }

    void NetPlayerBridge::ApplyAfflictions(
        Entities::PlayerEntity& player, PlayerState state)
    {
        player.ModSetDisrupted((state.Flags & PlayerState::FlagDisrupted) != 0);
        player.ModSetBurning((state.Flags & PlayerState::FlagBurning) != 0);
    }

    void NetPlayerBridge::ApplyForm(Entities::PlayerEntity& player, bool altForm)
    {
        const std::int32_t slot = player.SlotIndex();
        if (slot < 0 || slot >= static_cast<std::int32_t>(_formMismatch.size()))
        {
            return;
        }
        const std::size_t index = static_cast<std::size_t>(slot);
        if (player.IsAltForm() == altForm)
        {
            _formMismatch[index] = 0;
            _formAttempts[index] = 0;
            return;
        }
        _formMismatch[index] = UncheckedIncrement(_formMismatch[index]);
        if (_formMismatch[index] <= FormGraceFrames)
        {
            return;
        }
        _formMismatch[index] = 0;
        if (_formAttempts[index] == 0)
        {
            _formAttempts[index] = 1;
            player.ModStartFormSwitch();
            return;
        }
        _formAttempts[index] = 0;
        player.ModForceForm(altForm);
    }

    bool NetPlayerBridge::Diverged(
        Entities::PlayerEntity& player, const PlayerState& state, std::int32_t slot)
    {
        if (slot < 0 || slot >= static_cast<std::int32_t>(_divergedFrames.size()))
        {
            return false;
        }
        const std::size_t index = static_cast<std::size_t>(slot);
        OpenTK::Mathematics::Vector3 then = player.Position;
        std::int32_t lagFrames = 0;
        if (slot < static_cast<std::int32_t>(NetSession::SlotPing.size()))
        {
            const std::int32_t ping = NetSession::SlotPing[slot];
            const std::int32_t frames = UncheckedMultiply(ping, 60) / 1000;
            lagFrames = std::clamp(frames, 0, 100);
        }
        OpenTK::Mathematics::Vector3 past{};
        if (lagFrames > 0
            && NetSession::NetFrame() > static_cast<std::uint32_t>(lagFrames)
            && player.ModGetNetworkPosition(NetSession::NetFrame()
                    - static_cast<std::uint32_t>(lagFrames), past))
        {
            then = past;
        }
        if (LengthSquared(state.Position - then) <= DesyncDistance * DesyncDistance)
        {
            _divergedFrames[index] = 0;
            return false;
        }
        _divergedFrames[index] = UncheckedIncrement(_divergedFrames[index]);
        return _divergedFrames[index] >= DivergedFramesBeforeCorrecting;
    }

    void NetPlayerBridge::NoteRoomChanged()
    {
        _authoritySpawned.fill(false);
        _reportSeen.fill(false);
        _divergedFrames.fill(0);
        _spawnIntentFrame.fill(0);
        _wasInPlay.fill(false);
        _staleFrames.fill(0);
    }

    void NetPlayerBridge::Reset()
    {
        _formMismatch.fill(0);
        _snaps = 0;
        _worstSnap = 0.0F;
        NodeLookupsUnresolved = 0;
        PlacementsRefused = 0;
        _formSaid.fill(0);
        _formAttempts.fill(0);
        _lastPressFrame.fill(0);
        _pressSeen.fill(false);
        _pressHistory.fill(0);
        _authoritySpawned.fill(false);
        _divergedFrames.fill(0);
        _spawnIntentFrame.fill(0);
        _wasInPlay.fill(false);
        _staleFrames.fill(0);
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
        const std::size_t index = static_cast<std::size_t>(slot);
        _formMismatch[index] = 0;
        _formAttempts[index] = 0;
        _lastPressFrame[index] = 0;
        _pressSeen[index] = false;
        _pressHistory[index] = 0;
        _authoritySpawned[index] = false;
        _divergedFrames[index] = 0;
        _spawnIntentFrame[index] = 0;
        _wasInPlay[index] = false;
        _staleFrames[index] = 0;
        _lastReportPosition[index] = OpenTK::Mathematics::Vector3::Zero;
        _lastReportFrame[index] = 0;
        _reportSeen[index] = false;
    }

    void NetPlayerBridge::ApplyReportedPosition(
        Entities::PlayerEntity& player, const IntentPacket& intent)
    {
        if (!Sane(intent.Position))
        {
            _rejectedUpdates = UncheckedIncrement(_rejectedUpdates);
            return;
        }
        if (FrozenInPlace(player))
        {
            return;
        }
        if (VectorEquals(intent.Position, OpenTK::Mathematics::Vector3::Zero))
        {
            return;
        }
        if (StaleSinceSpawn(player, intent))
        {
            return;
        }
        const OpenTK::Mathematics::Vector3 reported = InForm(
            player, intent.Position, HasFlag(intent.Buttons, IntentButtons::AltFormState));
        NoteReportedVelocity(player, reported, intent.Frame);
        const OpenTK::Mathematics::Vector3 delta = reported - player.Position;
        const float distance = Length(delta);
        if (distance > SnapDistance)
        {
            _snaps = UncheckedIncrement(_snaps);
            _worstSnap = MathMax(_worstSnap, distance);
            Move(player, reported);
            return;
        }
        Move(player, reported);
    }

    void NetPlayerBridge::RestoreReportedPosition(
        Entities::PlayerEntity& player, const IntentPacket& intent)
    {
        if (!Sane(intent.Position)
            || VectorEquals(intent.Position, OpenTK::Mathematics::Vector3::Zero)
            || StaleSinceSpawn(player, intent)
            || FrozenInPlace(player))
        {
            return;
        }
        Move(player, InForm(
            player, intent.Position, HasFlag(intent.Buttons, IntentButtons::AltFormState)));
    }

    bool NetPlayerBridge::StaleSinceSpawn(
        Entities::PlayerEntity& player, const IntentPacket& intent)
    {
        const std::int32_t slot = player.SlotIndex();
        if (slot < 0 || slot >= static_cast<std::int32_t>(_spawnIntentFrame.size()))
        {
            return false;
        }
        const std::size_t index = static_cast<std::size_t>(slot);
        const bool inPlay
            = HasFlag(player.LoadFlags(), Entities::LoadFlags::Spawned) && player.Health() > 0;
        if (inPlay && !_wasInPlay[index])
        {
            _spawnIntentFrame[index] = intent.Frame;
            _staleFrames[index] = 0;
        }
        _wasInPlay[index] = inPlay;
        if (!inPlay)
        {
            _staleFrames[index] = 0;
            return false;
        }
        if (!HasFlag(intent.Buttons, IntentButtons::InPlayState))
        {
            return true;
        }
        if (intent.Frame > _spawnIntentFrame[index])
        {
            _staleFrames[index] = 0;
            return false;
        }
        _staleFrames[index] = UncheckedIncrement(_staleFrames[index]);
        if (_staleFrames[index] <= StaleAfterSpawnFrames)
        {
            return true;
        }
        NetLog::Event("slot " + Int32Text(slot)
            + " still reporting pre-spawn frames after "
            + Int32Text(_staleFrames[index]) + " of them; following it anyway");
        _spawnIntentFrame[index] = intent.Frame;
        _staleFrames[index] = 0;
        return false;
    }

    void NetPlayerBridge::NoteReportedVelocity(
        Entities::PlayerEntity& player,
        OpenTK::Mathematics::Vector3 reported,
        std::uint32_t frame)
    {
        const std::int32_t slot = player.SlotIndex();
        if (slot < 0 || slot >= static_cast<std::int32_t>(_lastReportFrame.size()))
        {
            return;
        }
        const std::size_t index = static_cast<std::size_t>(slot);
        if (_reportSeen[index] && frame > _lastReportFrame[index])
        {
            const std::uint32_t elapsed
                = std::min(frame - _lastReportFrame[index], 8U);
            const OpenTK::Mathematics::Vector3 travelled
                = reported - _lastReportPosition[index];
            const float step = Length(travelled);
            if (!Sane(travelled) || step > SnapDistance)
            {
                player.SetSpeed(OpenTK::Mathematics::Vector3::Zero);
            }
            else
            {
                const float elapsedFloat = static_cast<float>(elapsed);
                OpenTK::Mathematics::Vector3 speed(
                    travelled.X / elapsedFloat,
                    travelled.Y / elapsedFloat,
                    travelled.Z / elapsedFloat);
                const float magnitude = Length(speed);
                if (magnitude > MaxReportedSpeed)
                {
                    const float scale = MaxReportedSpeed / magnitude;
                    speed = OpenTK::Mathematics::Vector3(
                        speed.X * scale, speed.Y * scale, speed.Z * scale);
                }
                player.SetSpeed(speed);
            }
        }
        if (!_reportSeen[index] || frame > _lastReportFrame[index])
        {
            _reportSeen[index] = true;
            _lastReportFrame[index] = frame;
            _lastReportPosition[index] = reported;
        }
    }

    bool NetPlayerBridge::FrozenInPlace(Entities::PlayerEntity& player)
    {
        return player.ModFrozen();
    }

    void NetPlayerBridge::Move(
        Entities::PlayerEntity& player, OpenTK::Mathematics::Vector3 position)
    {
        const OpenTK::Mathematics::Vector3 previous = player.Position;
        player.Position = position;
        player.SetPrevPosition(position);
        player.ModRefreshNodeRef(previous);
        player.ModRefreshVolume();
    }
}
