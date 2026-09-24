#include "NetHitPrediction.hpp"

#include "../../Entities/BeamProjectileEntity.hpp"
#include "../../Entities/BombEntity.hpp"
#include "../../Entities/Players/HalfturretEntity.hpp"
#include "../../GameState.hpp"
#include "NetDamage.hpp"
#include "NetHooks.hpp"
#include "NetSession.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <langinfo.h>
#include <locale.h>
#endif

namespace
{
    [[nodiscard]] std::int32_t UncheckedAddInt32(
        std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t result = static_cast<std::uint32_t>(left)
            + static_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(result);
    }

    [[nodiscard]] std::int32_t UncheckedSubtractInt32(
        std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t result = static_cast<std::uint32_t>(left)
            - static_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(result);
    }

    [[nodiscard]] std::int64_t UncheckedAddInt64(
        std::int64_t left, std::int64_t right) noexcept
    {
        const std::uint64_t result = static_cast<std::uint64_t>(left)
            + static_cast<std::uint64_t>(right);
        return std::bit_cast<std::int64_t>(result);
    }

    void UncheckedIncrement(std::int64_t& value) noexcept
    {
        value = UncheckedAddInt64(value, 1);
    }

    [[nodiscard]] bool HasFlag(
        MphRead::Entities::DamageFlags value,
        MphRead::Entities::DamageFlags flag) noexcept
    {
        return (value & flag) != MphRead::Entities::DamageFlags::None;
    }

    [[nodiscard]] std::string CurrentCultureDecimalSeparator()
    {
#if defined(_WIN32)
        wchar_t buffer[16]{};
        const int length = GetLocaleInfoEx(
            LOCALE_NAME_USER_DEFAULT, LOCALE_SDECIMAL, buffer,
            static_cast<int>(sizeof(buffer) / sizeof(buffer[0])));
        if (length <= 1)
        {
            return ".";
        }
        const int utf8Length = WideCharToMultiByte(
            CP_UTF8, 0, buffer, length - 1, nullptr, 0, nullptr, nullptr);
        if (utf8Length <= 0)
        {
            return ".";
        }
        std::string result(static_cast<std::size_t>(utf8Length), '\0');
        WideCharToMultiByte(
            CP_UTF8, 0, buffer, length - 1, result.data(), utf8Length, nullptr, nullptr);
        return result;
#else
#if defined(__ANDROID__)
        const lconv* locale = ::localeconv();
        const char* separator = locale != nullptr ? locale->decimal_point : nullptr;
        return separator != nullptr && separator[0] != '\0' ? separator : ".";
#else
        locale_t locale = newlocale(LC_NUMERIC_MASK, "", nullptr);
        if (locale == static_cast<locale_t>(0))
        {
            return ".";
        }
        const char* separator = nl_langinfo_l(RADIXCHAR, locale);
        std::string result = separator != nullptr && separator[0] != '\0'
            ? separator
            : ".";
        freelocale(locale);
        return result;
#endif
#endif
    }

    [[nodiscard]] std::string CurrentCultureNegativeSign()
    {
#if defined(_WIN32)
        wchar_t buffer[16]{};
        const int length = GetLocaleInfoEx(
            LOCALE_NAME_USER_DEFAULT, LOCALE_SNEGATIVESIGN, buffer,
            static_cast<int>(sizeof(buffer) / sizeof(buffer[0])));
        if (length <= 1)
        {
            return "-";
        }
        const int utf8Length = WideCharToMultiByte(
            CP_UTF8, 0, buffer, length - 1, nullptr, 0, nullptr, nullptr);
        if (utf8Length <= 0)
        {
            return "-";
        }
        std::string result(static_cast<std::size_t>(utf8Length), '\0');
        WideCharToMultiByte(
            CP_UTF8, 0, buffer, length - 1, result.data(), utf8Length, nullptr, nullptr);
        return result;
#else
#if defined(__ANDROID__)
        const lconv* locale = ::localeconv();
        const char* sign = locale != nullptr ? locale->negative_sign : nullptr;
        return sign != nullptr && sign[0] != '\0' ? sign : "-";
#else
        locale_t locale = newlocale(LC_MONETARY_MASK, "", nullptr);
        if (locale == static_cast<locale_t>(0))
        {
            return "-";
        }
#if defined(NEGATIVE_SIGN)
        const char* sign = nl_langinfo_l(NEGATIVE_SIGN, locale);
        std::string result = sign != nullptr && sign[0] != '\0' ? sign : "-";
#else
        std::string result = "-";
#endif
        freelocale(locale);
        return result;
#endif
#endif
    }

    [[nodiscard]] std::string UInt64ToDigits(std::uint64_t value)
    {
        std::array<char, 32> buffer{};
        const auto converted = std::to_chars(
            buffer.data(), buffer.data() + buffer.size(), value);
        if (converted.ec != std::errc{})
        {
            throw std::runtime_error("Integer formatting failed.");
        }
        return std::string(buffer.data(), converted.ptr);
    }

    [[nodiscard]] std::string Int64ToCurrentCulture(std::int64_t value)
    {
        const std::uint64_t bits = static_cast<std::uint64_t>(value);
        if (value >= 0)
        {
            return UInt64ToDigits(bits);
        }
        const std::uint64_t magnitude = 0ULL - bits;
        return CurrentCultureNegativeSign() + UInt64ToDigits(magnitude);
    }

    [[nodiscard]] std::string FormatDoubleF1(double value)
    {
        const bool negative = std::signbit(value);
        const double magnitude = std::fabs(value);

        std::array<char, 128> buffer{};
        const auto converted = std::to_chars(
            buffer.data(), buffer.data() + buffer.size(), magnitude,
            std::chars_format::fixed, 1);
        if (converted.ec != std::errc{})
        {
            throw std::runtime_error("Double formatting failed.");
        }

        std::string result(buffer.data(), converted.ptr);
        const std::size_t point = result.find('.');
        if (point != std::string::npos)
        {
            result.replace(point, 1, CurrentCultureDecimalSeparator());
        }
        if (negative)
        {
            result.insert(0, CurrentCultureNegativeSign());
        }
        return result;
    }
}

namespace MphRead::Mods::Network
{
    bool NetHitPrediction::Predicting() noexcept
    {
        return NetSession::Active()
            && !NetSession::IsHost()
            && !NetSession::IsAuthority()
            && !NetDamage::Replaying();
    }

    std::int32_t NetHitPrediction::HoldFrames() noexcept
    {
        const std::int32_t slot = NetHooks::LocalSlot();
        const std::int32_t ping = slot >= 0
                && static_cast<std::size_t>(slot) < NetSession::SlotPing.size()
            ? NetSession::SlotPing[static_cast<std::size_t>(slot)]
            : 0;
        const std::int32_t frames
            = static_cast<std::int32_t>(static_cast<float>(ping) * 0.06F) + 12;
        return std::clamp(frames, 15, 90);
    }

    float NetHitPrediction::MarkerAlpha() noexcept
    {
        if (!_markerEnabled || _markerTimer <= 0)
        {
            return 0.0F;
        }
        constexpr std::int32_t fade = 6;
        return _markerTimer >= fade
            ? 1.0F
            : static_cast<float>(_markerTimer) / static_cast<float>(fade);
    }

    void NetHitPrediction::Reset()
    {
        for (auto& row : _pendingFrame)
        {
            row.fill(0);
        }
        for (auto& row : _pendingDamage)
        {
            row.fill(0);
        }
        for (auto& row : _pendingLethal)
        {
            row.fill(false);
        }
        _pendingCount.fill(0);
        _pendingHead.fill(0);
        _healFrame.fill(0);
        _healAmount.fill(0);
        _healCount = 0;
        _healHead = 0;
        _predicted = 0;
        _confirmed = 0;
        _selfPredicted = 0;
        _selfConfirmed = 0;
        _denied = 0;
        _unpredicted = 0;
        _lethalHeld = 0;
        _deathsPredicted = 0;
        _selfDeathsPredicted = 0;
        _deathsUndone = 0;
        _shownHealth.fill(0);
        _predictedFrame.fill(0);
        _drainPredicted = 0;
        _markerTimer = 0;
    }

    bool NetHitPrediction::Predicts(
        Entities::PlayerEntity& victim,
        Entities::EntityBase* source,
        Entities::DamageFlags flags)
    {
        if (!_enabled || victim.Health() <= 0)
        {
            return false;
        }

        const std::int32_t local = NetHooks::LocalSlot();
        if (local < 0)
        {
            return false;
        }

        if (source == nullptr)
        {
            return victim.SlotIndex() == local
                && HasFlag(flags, Entities::DamageFlags::Death);
        }

        Entities::PlayerEntity* owner = OwnerOf(source);
        return owner != nullptr && owner->SlotIndex() == local;
    }

    Entities::PlayerEntity* NetHitPrediction::OwnerOf(Entities::EntityBase* source)
    {
        if (auto* beam = dynamic_cast<Entities::BeamProjectileEntity*>(source))
        {
            if (std::shared_ptr<Entities::PlayerEntity> player
                = std::dynamic_pointer_cast<Entities::PlayerEntity>(beam->Owner()))
            {
                return player.get();
            }
            if (std::shared_ptr<Entities::HalfturretEntity> turret
                = std::dynamic_pointer_cast<Entities::HalfturretEntity>(beam->Owner()))
            {
                return turret->Owner().get();
            }
            return nullptr;
        }

        if (auto* bomb = dynamic_cast<Entities::BombEntity*>(source))
        {
            return bomb->Owner();
        }

        if (auto* attacker = dynamic_cast<Entities::PlayerEntity*>(source))
        {
            return attacker;
        }

        return nullptr;
    }

    void NetHitPrediction::NoteHit(
        Entities::PlayerEntity& victim,
        Entities::PlayerEntity* attacker,
        Entities::DamageFlags flags,
        std::uint32_t& damage)
    {
        const std::int32_t local = NetHooks::LocalSlot();
        if (local < 0)
        {
            return;
        }

        bool self = false;
        if (attacker == nullptr)
        {
            if (victim.SlotIndex() != local)
            {
                return;
            }
            self = true;
        }
        else
        {
            if (attacker->SlotIndex() != local)
            {
                return;
            }
            self = attacker == &victim;
        }

        if (Predicting() && _enabled)
        {
            bool lethal = victim.Health() > 0
                && (damage >= static_cast<std::uint32_t>(victim.Health())
                    || HasFlag(flags, Entities::DamageFlags::Death));

            if (lethal && !self && !_deathEnabled)
            {
                damage = static_cast<std::uint32_t>(
                    std::max<std::int32_t>(0, victim.Health() - 1));
                UncheckedIncrement(_lethalHeld);
                lethal = false;
            }

            const std::int32_t victimSlot = victim.SlotIndex();
            const std::uint32_t frame = NetSession::NetFrame();
            const std::int32_t appliedDamage
                = std::bit_cast<std::int32_t>(damage);
            Push(victimSlot, frame, appliedDamage, lethal);

            if (self)
            {
                UncheckedIncrement(_selfPredicted);
            }
            else
            {
                UncheckedIncrement(_predicted);
            }

            if (lethal)
            {
                if (self)
                {
                    UncheckedIncrement(_selfDeathsPredicted);
                }
                else
                {
                    UncheckedIncrement(_deathsPredicted);
                }
            }
        }

        if (!GameState::SinglePlayer() && !self)
        {
            _markerTimer = MarkerFrames;
        }
    }

    bool NetHitPrediction::Confirm(std::int32_t slot, std::int32_t landed)
    {
        if (slot < 0 || slot >= Slots)
        {
            return false;
        }

        const std::size_t index = static_cast<std::size_t>(slot);
        if (_pendingCount[index] == 0)
        {
            UncheckedIncrement(_unpredicted);
            return false;
        }

        const bool self = slot == NetHooks::LocalSlot();
        const std::int32_t take = std::clamp(
            landed, 1, _pendingCount[index]);

        for (std::int32_t i = 0; i < take; i++)
        {
            _pendingHead[index]
                = (_pendingHead[index] + 1) % PendingCapacity;
            _pendingCount[index]--;

            if (self)
            {
                UncheckedIncrement(_selfConfirmed);
            }
            else
            {
                UncheckedIncrement(_confirmed);
            }
        }

        return true;
    }

    void NetHitPrediction::ForgetSlot(std::int32_t slot)
    {
        if (slot < 0 || slot >= Slots)
        {
            return;
        }

        const std::size_t index = static_cast<std::size_t>(slot);
        _pendingFrame[index].fill(0);
        _pendingDamage[index].fill(0);
        _pendingLethal[index].fill(false);
        _pendingCount[index] = 0;
        _pendingHead[index] = 0;
        _shownHealth[index] = 0;
        _predictedFrame[index] = 0;
    }

    void NetHitPrediction::NoteRespawn(std::int32_t slot)
    {
        if (slot < 0 || slot >= Slots)
        {
            return;
        }

        const std::size_t index = static_cast<std::size_t>(slot);
        for (std::int32_t i = 0; i < _pendingCount[index]; i++)
        {
            const std::int32_t at
                = (_pendingHead[index] + i) % PendingCapacity;
            if (_pendingLethal[index][static_cast<std::size_t>(at)])
            {
                UncheckedIncrement(_deathsUndone);
                break;
            }
        }

        ForgetSlot(slot);
    }

    void NetHitPrediction::NoteDeath(std::int32_t slot)
    {
        if (slot < 0 || slot >= Slots)
        {
            return;
        }

        _pendingLethal[static_cast<std::size_t>(slot)].fill(false);
    }

    void NetHitPrediction::ForgetPending()
    {
        for (std::int32_t slot = 0; slot < Slots; slot++)
        {
            ForgetSlot(slot);
        }
        _healCount = 0;
        _healHead = 0;
    }

    void NetHitPrediction::NoteDrain(
        Entities::PlayerEntity& healer, std::int32_t amount)
    {
        if (!_enabled || !Predicting() || amount <= 0)
        {
            return;
        }

        const std::int32_t local = NetHooks::LocalSlot();
        if (local < 0 || healer.SlotIndex() != local)
        {
            return;
        }

        if (_healCount == HealCapacity)
        {
            _healHead = (_healHead + 1) % HealCapacity;
            _healCount--;
        }

        const std::int32_t tail
            = (_healHead + _healCount) % HealCapacity;
        _healFrame[static_cast<std::size_t>(tail)] = NetSession::NetFrame();
        _healAmount[static_cast<std::size_t>(tail)] = amount;
        _healCount++;
        _drainPredicted = UncheckedAddInt64(
            _drainPredicted, static_cast<std::int64_t>(amount));
    }

    std::int32_t NetHitPrediction::Debit(std::int32_t slot)
    {
        if (!_enabled || slot < 0 || slot >= Slots
            || _pendingCount[static_cast<std::size_t>(slot)] == 0)
        {
            return 0;
        }

        const std::size_t index = static_cast<std::size_t>(slot);
        const std::uint32_t now = NetSession::NetFrame();
        const std::int32_t hold = HoldFrames();
        std::int32_t debit = 0;

        for (std::int32_t i = 0; i < _pendingCount[index]; i++)
        {
            const std::int32_t at
                = (_pendingHead[index] + i) % PendingCapacity;
            const std::size_t atIndex = static_cast<std::size_t>(at);
            if (now - _pendingFrame[index][atIndex]
                < static_cast<std::uint32_t>(hold))
            {
                debit = UncheckedAddInt32(
                    debit, _pendingDamage[index][atIndex]);
            }
        }

        return debit;
    }

    std::int32_t NetHitPrediction::HealthFor(
        std::int32_t slot, std::int32_t authorityHealth)
    {
        if (!_enabled || slot < 0 || slot >= Slots)
        {
            return authorityHealth;
        }

        const std::size_t index = static_cast<std::size_t>(slot);
        const std::int32_t debit = Debit(slot);
        std::int32_t health = debit > 0 && authorityHealth > 1
            ? std::max<std::int32_t>(
                1, UncheckedSubtractInt32(authorityHealth, debit))
            : authorityHealth;

        if (authorityHealth > 0 && _shownHealth[index] > 0)
        {
            const std::uint32_t now = NetSession::NetFrame();
            const std::uint32_t age = now - _predictedFrame[index];
            const std::int32_t hold = HoldFrames();
            if (age < static_cast<std::uint32_t>(hold))
            {
                health = std::min(health, _shownHealth[index]);
                health = std::max<std::int32_t>(1, health);
            }
        }

        _shownHealth[index] = health;
        return health;
    }

    std::int32_t NetHitPrediction::LocalHealthFor(
        Entities::PlayerEntity& player, std::int32_t authorityHealth)
    {
        if (!_enabled || authorityHealth <= 0)
        {
            return authorityHealth;
        }

        const std::uint32_t now = NetSession::NetFrame();
        const std::int32_t hold = HoldFrames();
        std::int32_t credit = 0;

        for (std::int32_t i = 0; i < _healCount; i++)
        {
            const std::int32_t at = (_healHead + i) % HealCapacity;
            const std::size_t index = static_cast<std::size_t>(at);
            if (now - _healFrame[index] < static_cast<std::uint32_t>(hold))
            {
                credit = UncheckedAddInt32(credit, _healAmount[index]);
            }
        }

        credit = UncheckedSubtractInt32(
            credit, Debit(NetHooks::LocalSlot()));

        if (player.Health() <= 0 && HeldDead(NetHooks::LocalSlot()))
        {
            return 0;
        }

        if (credit == 0)
        {
            return authorityHealth;
        }

        const std::int32_t max = player.HealthMax() > 0
            ? player.HealthMax()
            : authorityHealth;
        return std::clamp(
            UncheckedAddInt32(authorityHealth, credit), 1, max);
    }

    bool NetHitPrediction::HeldDead(std::int32_t slot)
    {
        if (!_enabled || slot < 0 || slot >= Slots)
        {
            return false;
        }

        if (!_deathEnabled && slot != NetHooks::LocalSlot())
        {
            return false;
        }

        const std::size_t index = static_cast<std::size_t>(slot);
        const std::uint32_t now = NetSession::NetFrame();
        const std::int32_t hold = HoldFrames();

        for (std::int32_t i = 0; i < _pendingCount[index]; i++)
        {
            const std::int32_t at
                = (_pendingHead[index] + i) % PendingCapacity;
            const std::size_t atIndex = static_cast<std::size_t>(at);
            if (_pendingLethal[index][atIndex]
                && now - _pendingFrame[index][atIndex]
                    < static_cast<std::uint32_t>(hold))
            {
                return true;
            }
        }

        return false;
    }

    void NetHitPrediction::Tick()
    {
        if (_markerTimer > 0)
        {
            _markerTimer--;
        }

        if (!NetSession::Active())
        {
            return;
        }

        const std::uint32_t now = NetSession::NetFrame();

        for (std::int32_t slot = 0; slot < Slots; slot++)
        {
            const std::size_t index = static_cast<std::size_t>(slot);
            while (_pendingCount[index] > 0)
            {
                const std::uint32_t frame
                    = _pendingFrame[index][static_cast<std::size_t>(
                        _pendingHead[index])];

                if (now - frame < static_cast<std::uint32_t>(PendingFrames))
                {
                    break;
                }

                _pendingHead[index]
                    = (_pendingHead[index] + 1) % PendingCapacity;
                _pendingCount[index]--;
                UncheckedIncrement(_denied);
            }
        }

        while (_healCount > 0
            && now - _healFrame[static_cast<std::size_t>(_healHead)]
                >= static_cast<std::uint32_t>(PendingFrames))
        {
            _healHead = (_healHead + 1) % HealCapacity;
            _healCount--;
        }
    }

    void NetHitPrediction::Push(
        std::int32_t slot, std::uint32_t frame,
        std::int32_t damage, bool lethal)
    {
        if (slot < 0 || slot >= Slots)
        {
            return;
        }

        const std::size_t index = static_cast<std::size_t>(slot);
        if (_pendingCount[index] == PendingCapacity)
        {
            _pendingHead[index]
                = (_pendingHead[index] + 1) % PendingCapacity;
            _pendingCount[index]--;
            UncheckedIncrement(_denied);
        }

        _predictedFrame[index] = frame;
        const std::int32_t tail
            = (_pendingHead[index] + _pendingCount[index]) % PendingCapacity;
        const std::size_t tailIndex = static_cast<std::size_t>(tail);
        _pendingFrame[index][tailIndex] = frame;
        _pendingDamage[index][tailIndex] = std::max<std::int32_t>(0, damage);
        _pendingLethal[index][tailIndex] = lethal;
        _pendingCount[index]++;
    }

    std::string NetHitPrediction::Describe()
    {
        if (!_enabled)
        {
            return "hit prediction: off";
        }

        if (_predicted == 0)
        {
            std::string own;
            if (_selfPredicted > 0)
            {
                own = ", ";
                own += Int64ToCurrentCulture(_selfPredicted);
                own += " self-hits predicted (";
                own += Int64ToCurrentCulture(_selfConfirmed);
                own += " confirmed, ";
                own += Int64ToCurrentCulture(_selfDeathsPredicted);
                own += " of them lethal)";
            }

            std::string result
                = "hit prediction: on, nothing predicted here (";
            result += Int64ToCurrentCulture(_unpredicted);
            result += " hits arrived from the authority)";
            result += own;
            return result;
        }

        const double agreed
            = static_cast<double>(_confirmed) * 100.0
            / static_cast<double>(_predicted);

        std::string deaths;
        if (_deathEnabled)
        {
            deaths = Int64ToCurrentCulture(_deathsPredicted);
            deaths += " kills predicted, ";
            deaths += Int64ToCurrentCulture(_deathsUndone);
            deaths += " undone";
        }
        else
        {
            deaths = Int64ToCurrentCulture(_lethalHeld);
            deaths += " kills left to the authority";
        }

        if (_selfDeathsPredicted > 0)
        {
            deaths += ", ";
            deaths += Int64ToCurrentCulture(_selfDeathsPredicted);
            deaths += " self-kills predicted";
        }

        std::string drain;
        if (_drainPredicted > 0)
        {
            drain = ", ";
            drain += Int64ToCurrentCulture(_drainPredicted);
            drain += " health drained ahead";
        }

        std::string self;
        if (_selfPredicted > 0)
        {
            self = ", ";
            self += Int64ToCurrentCulture(_selfPredicted);
            self += " self-hits predicted (";
            self += Int64ToCurrentCulture(_selfConfirmed);
            self += " confirmed)";
        }

        std::string result = "hit prediction: ";
        result += Int64ToCurrentCulture(_predicted);
        result += " predicted, ";
        result += Int64ToCurrentCulture(_confirmed);
        result += " confirmed (";
        result += FormatDoubleF1(agreed);
        result += "%), ";
        result += Int64ToCurrentCulture(_denied);
        result += " denied, ";
        result += Int64ToCurrentCulture(_unpredicted);
        result += " unpredicted, ";
        result += deaths;
        result += drain;
        result += self;
        return result;
    }
}
