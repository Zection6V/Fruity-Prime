#include "MapVote.hpp"

#include "../../Formats/Types.hpp"
#include "NetProtocol.hpp"
#include "NetSession.hpp"
#include "../../NativeRuntime/System/Encoding.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#include <locale.h>
#include <wctype.h>
#endif

using ::MphRead::NativeRuntime::AppendUtf8;
using ::MphRead::NativeRuntime::DecodeUtf8Scalar;
using ::MphRead::NativeRuntime::StringIsNullOrWhiteSpace;
using ::MphRead::NativeRuntime::Utf8Scalar;

namespace
{
    void ReplaceHit(
        MphRead::Mods::EndScreen::Hit& target,
        const MphRead::Mods::EndScreen::Hit& source) noexcept
    {
        target.~Hit();
        ::new (static_cast<void*>(std::addressof(target)))
            MphRead::Mods::EndScreen::Hit(source);
    }


#if !defined(_WIN32)
    [[nodiscard]] void* FindVersionedIcuSymbol(
        void* library, const char* base) noexcept
    {
        if (library == nullptr)
        {
            return nullptr;
        }
        if (void* symbol = dlsym(library, base); symbol != nullptr)
        {
            return symbol;
        }
        char name[96]{};
        for (int version = 99; version >= 50; --version)
        {
            const int count = std::snprintf(
                name, sizeof(name), "%s_%d", base, version);
            if (count <= 0 || static_cast<std::size_t>(count) >= sizeof(name))
            {
                continue;
            }
            if (void* symbol = dlsym(library, name); symbol != nullptr)
            {
                return symbol;
            }
        }
        return nullptr;
    }

    [[nodiscard]] std::uint32_t IcuUpper(std::uint32_t scalar) noexcept
    {
        using CaseFunction = std::int32_t (*)(std::int32_t);
        static const CaseFunction function = []() noexcept
        {
            void* library = dlopen("libicuuc.so", RTLD_LAZY | RTLD_LOCAL);
#if defined(__APPLE__)
            if (library == nullptr)
            {
                library = dlopen(
                    "/usr/lib/libicucore.A.dylib",
                    RTLD_LAZY | RTLD_LOCAL);
            }
#endif
            return reinterpret_cast<CaseFunction>(
                FindVersionedIcuSymbol(library, "u_toupper"));
        }();

        if (function == nullptr || scalar > 0x10FFFFU)
        {
            return scalar;
        }
        const std::int32_t mapped = function(static_cast<std::int32_t>(scalar));
        return mapped < 0 ? scalar : static_cast<std::uint32_t>(mapped);
    }
#endif

    [[nodiscard]] std::uint32_t InvariantUpperScalar(std::uint32_t scalar) noexcept
    {
        if (scalar >= 'a' && scalar <= 'z')
        {
            return scalar - ('a' - 'A');
        }

        // .NET invariant casing keeps dotless i unchanged.
        if (scalar == 0x0131U)
        {
            return scalar;
        }

#if defined(_WIN32)
        wchar_t source[2]{};
        int sourceLength = 0;
        if (scalar <= 0xFFFFU)
        {
            source[0] = static_cast<wchar_t>(scalar);
            sourceLength = 1;
        }
        else if (scalar <= 0x10FFFFU)
        {
            const std::uint32_t value = scalar - 0x10000U;
            source[0] = static_cast<wchar_t>(0xD800U + (value >> 10));
            source[1] = static_cast<wchar_t>(0xDC00U + (value & 0x3FFU));
            sourceLength = 2;
        }
        if (sourceLength != 0)
        {
            wchar_t target[2]{};
            const int mapped = LCMapStringEx(
                LOCALE_NAME_INVARIANT,
                LCMAP_UPPERCASE,
                source,
                sourceLength,
                target,
                2,
                nullptr,
                nullptr,
                0);
            if (mapped == 1)
            {
                return static_cast<std::uint32_t>(target[0]);
            }
            if (mapped == 2
                && target[0] >= 0xD800 && target[0] <= 0xDBFF
                && target[1] >= 0xDC00 && target[1] <= 0xDFFF)
            {
                return 0x10000U
                    + ((static_cast<std::uint32_t>(target[0]) - 0xD800U) << 10)
                    + (static_cast<std::uint32_t>(target[1]) - 0xDC00U);
            }
        }
#else
        const std::uint32_t icu = IcuUpper(scalar);
        if (icu != scalar)
        {
            return icu;
        }

        static locale_t locale = []() noexcept
        {
            locale_t value = newlocale(LC_CTYPE_MASK, "C.UTF-8", nullptr);
            if (value == nullptr)
            {
                value = newlocale(LC_CTYPE_MASK, "en_US.UTF-8", nullptr);
            }
            return value;
        }();
        if (locale != nullptr
            && scalar <= static_cast<std::uint32_t>(WCHAR_MAX))
        {
            const wint_t mapped = towupper_l(static_cast<wint_t>(scalar), locale);
            if (mapped != WEOF)
            {
                return static_cast<std::uint32_t>(mapped);
            }
        }
#endif

        if (scalar >= 0x00E0U && scalar <= 0x00F6U)
        {
            return scalar - 0x20U;
        }
        if (scalar >= 0x00F8U && scalar <= 0x00FEU)
        {
            return scalar - 0x20U;
        }
        if (scalar == 0x00FFU)
        {
            return 0x0178U;
        }
        if (scalar >= 0x03B1U && scalar <= 0x03C1U)
        {
            return scalar - 0x20U;
        }
        if (scalar >= 0x03C3U && scalar <= 0x03CBU)
        {
            return scalar - 0x20U;
        }
        if (scalar >= 0x0430U && scalar <= 0x044FU)
        {
            return scalar - 0x20U;
        }
        return scalar;
    }

    [[nodiscard]] std::string ToUpperInvariant(const std::string& value)
    {
        std::string result;
        result.reserve(value.size());
        for (std::size_t index = 0; index < value.size();)
        {
            const Utf8Scalar unit = DecodeUtf8Scalar(value, index);
            index += unit.Length;
            AppendUtf8(result, InvariantUpperScalar(unit.Value));
        }
        return result;
    }

    [[nodiscard]] const std::string& RequireString(
        const std::optional<std::string>& value)
    {
        if (!value.has_value())
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    [[nodiscard]] const std::string& OrEmpty(
        const std::optional<std::string>& value) noexcept
    {
        static const std::string empty;
        return value.has_value() ? *value : empty;
    }
}

namespace MphRead::Mods::Network
{
    bool MapVote::_active = false;
    std::optional<std::string> MapVote::_roomKey = std::string();
    std::optional<std::string> MapVote::_proposer = std::string();
    std::int32_t MapVote::_yes = 0;
    std::int32_t MapVote::_no = 0;
    std::int32_t MapVote::_eligible = 0;
    std::int32_t MapVote::_needed = 0;
    std::int32_t MapVote::_seconds = 0;
    bool MapVote::_answered = false;
    bool MapVote::_supported = false;
    bool MapVote::_disabled = false;
    MphRead::Mods::EndScreen::Hit MapVote::_hitAccept{};
    MphRead::Mods::EndScreen::Hit MapVote::_hitDeny{};

    std::string MapVote::WhyNotProposing()
    {
        if (!NetSession::Active())
        {
            return "You are not in an online match.";
        }
        if (!_supported)
        {
            return "This server has not answered about voting; it may be an older build.";
        }
        if (_disabled)
        {
            return "Voting is switched off on this server.";
        }
        if (_active)
        {
            return "A vote is already running: " + OrEmpty(_roomKey) + ".";
        }
        if (_seconds > 0)
        {
            return "Another vote may be called in " + std::to_string(_seconds) + " s.";
        }
        return "";
    }

    void MapVote::NoteLayout(
        MphRead::Mods::EndScreen::Hit accept,
        MphRead::Mods::EndScreen::Hit deny) noexcept
    {
        ReplaceHit(_hitAccept, accept);
        ReplaceHit(_hitDeny, deny);
    }

    bool MapVote::HandleClick()
    {
        if (!_active || _answered)
        {
            return false;
        }
        const float x = MphRead::Mods::EndScreen::PointerX();
        const float y = MphRead::Mods::EndScreen::PointerY();
        if (_hitAccept.Contains(x, y))
        {
            Cast(true);
            return true;
        }
        if (_hitDeny.Contains(x, y))
        {
            Cast(false);
            return true;
        }
        return false;
    }

    std::shared_ptr<std::vector<float>> MapVote::TouchTargets()
    {
        if (!_active || _answered || _hitAccept.Right <= _hitAccept.Left)
        {
            static const std::shared_ptr<std::vector<float>> empty
                = std::make_shared<std::vector<float>>();
            return empty;
        }
        return std::make_shared<std::vector<float>>(std::initializer_list<float>{
            _hitAccept.Left, _hitAccept.Top, _hitAccept.Right, _hitAccept.Bottom,
            _hitDeny.Left, _hitDeny.Top, _hitDeny.Right, _hitDeny.Bottom
        });
    }

    void MapVote::Reset()
    {
        ReplaceHit(_hitAccept, MphRead::Mods::EndScreen::Hit{});
        ReplaceHit(_hitDeny, MphRead::Mods::EndScreen::Hit{});
        _active = false;
        _roomKey = std::string();
        _proposer = std::string();
        _seconds = 0;
        _needed = 0;
        _eligible = 0;
        _no = 0;
        _yes = 0;
        _answered = false;
        _supported = false;
        _disabled = false;
    }

    void MapVote::Apply(VoteStatePacket state)
    {
        _supported = true;
        _disabled = state.State != VoteStatePacket::StateRunning
            && state.Seconds == std::numeric_limits<std::uint16_t>::max();
        const bool wasActive = _active;
        const std::optional<std::string> wasRoom = _roomKey;
        _active = state.State == VoteStatePacket::StateRunning;
        _roomKey = state.RoomKey;
        _proposer = state.Proposer;
        _yes = state.Yes;
        _no = state.No;
        _eligible = state.Eligible;
        _needed = state.Needed;
        _seconds = _disabled ? 0 : state.Seconds;
        if (!_active || !wasActive || wasRoom != _roomKey)
        {
            if (!_active || wasRoom != _roomKey)
            {
                _answered = false;
            }
        }
    }

    void MapVote::Propose(const std::optional<std::string>& roomKey)
    {
        if (!NetSession::Active() || StringIsNullOrWhiteSpace(roomKey))
        {
            return;
        }
        NetSession::SendVote(VotePacket::KindPropose, *roomKey);
    }

    void MapVote::Cast(bool yes)
    {
        if (!_active || _answered || !NetSession::Active())
        {
            return;
        }
        _answered = true;
        NetSession::SendVote(
            yes ? VotePacket::KindYes : VotePacket::KindNo,
            "");
    }

    std::string MapVote::PromptLine()
    {
        if (!_active)
        {
            return "";
        }
        const std::string proposer = OrEmpty(_proposer);
        return proposer + " PROPOSES " + ToUpperInvariant(RequireString(_roomKey));
    }

    std::string MapVote::TallyLine()
    {
        if (!_active)
        {
            return "";
        }
        const std::string answer = _answered ? "" : "  F1 YES / F2 NO";
        return std::to_string(_yes) + "/" + std::to_string(_needed)
            + " OF " + std::to_string(_eligible) + "   "
            + std::to_string(_seconds) + "s" + answer;
    }
}

namespace MphRead::Mods::Network::Detail
{
    void NetSessionMapVoteReset()
    {
        MapVote::Reset();
    }

    void NetSessionMapVoteApply(const VoteStatePacket& state)
    {
        MapVote::Apply(state);
    }
}
