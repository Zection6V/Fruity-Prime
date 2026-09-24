#include "NetSession.hpp"

#include "../../NativeRuntime/System/Enum.hpp"

#include "../../GameState.hpp"
#include "../../Metadata/Metadata.hpp"
#include "../../Utility/Rng.hpp"
#include "../SpectatorMode.hpp"
#include "../Chat/ChatBox.hpp"
#include "DemoRecorder.hpp"
#include "NetDamage.hpp"
#include "NetHitPrediction.hpp"
#include "MapVote.hpp"
#include "NetLog.hpp"
#include "NetMatchEnd.hpp"
#include "NetMatchSync.hpp"
#include "NetPlayerBridge.hpp"
#include "NetPlayerSetup.hpp"
#include "NetRoomChange.hpp"
#include "NetSlotManager.hpp"
#include "NetUnlagged.hpp"
#include "PlayerColors.hpp"
#include "../../Formats/Types.hpp"
#include "../../NativeRuntime/System/Encoding.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <langinfo.h>
#include <locale.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using ::MphRead::NativeRuntime::DecodeUtf8Scalar;
using ::MphRead::NativeRuntime::HasFlag;
using ::MphRead::NativeRuntime::IncrementInPlace;
using ::MphRead::NativeRuntime::MathMax;
using ::MphRead::NativeRuntime::StringIsNullOrWhiteSpace;
using ::MphRead::NativeRuntime::Utf8Scalar;
using ::MphRead::NativeRuntime::WideToUtf8;

namespace
{
    using MphRead::Mods::Network::NetRole;

    struct NumberSymbols
    {
        std::string Decimal = ".";
        std::string Negative = "-";
        std::string NaN = "NaN";
        std::string PositiveInfinity = "\xE2\x88\x9E";
        std::string NegativeInfinity = "-\xE2\x88\x9E";
    };

    [[nodiscard]] bool GlobalizationInvariantRequested() noexcept
    {
        const char* value = std::getenv("DOTNET_SYSTEM_GLOBALIZATION_INVARIANT");
        if (value == nullptr)
        {
            return false;
        }
        const std::string_view text(value);
        if (text == "1")
        {
            return true;
        }
        return text.size() == 4
            && (text[0] == 't' || text[0] == 'T')
            && (text[1] == 'r' || text[1] == 'R')
            && (text[2] == 'u' || text[2] == 'U')
            && (text[3] == 'e' || text[3] == 'E');
    }

#if defined(_WIN32)
    [[nodiscard]] std::string LocaleString(LCTYPE type, std::string fallback)
    {
        wchar_t buffer[128]{};
        const int count = GetLocaleInfoEx(
            LOCALE_NAME_USER_DEFAULT, type, buffer, static_cast<int>(std::size(buffer)));
        if (count <= 1)
        {
            return fallback;
        }
        std::string result = WideToUtf8(buffer);
        return result.empty() ? fallback : result;
    }
#endif

    [[nodiscard]] NumberSymbols CurrentNumberSymbols()
    {
        NumberSymbols symbols{};
        if (GlobalizationInvariantRequested())
        {
            symbols.PositiveInfinity = "Infinity";
            symbols.NegativeInfinity = "-Infinity";
            return symbols;
        }
#if defined(_WIN32)
        symbols.Decimal = LocaleString(LOCALE_SDECIMAL, ".");
        symbols.Negative = LocaleString(LOCALE_SNEGATIVESIGN, "-");
#ifdef LOCALE_SNAN
        symbols.NaN = LocaleString(LOCALE_SNAN, "NaN");
#endif
#ifdef LOCALE_SPOSINFINITY
        symbols.PositiveInfinity = LocaleString(LOCALE_SPOSINFINITY, "\xE2\x88\x9E");
#endif
#ifdef LOCALE_SNEGINFINITY
        symbols.NegativeInfinity = LocaleString(
            LOCALE_SNEGINFINITY, symbols.Negative + "\xE2\x88\x9E");
#endif
#else
#if defined(__ANDROID__)
        if (const lconv* locale = ::localeconv(); locale != nullptr)
        {
            if (locale->decimal_point != nullptr && locale->decimal_point[0] != '\0')
            {
                symbols.Decimal = locale->decimal_point;
            }
            if (locale->negative_sign != nullptr && locale->negative_sign[0] != '\0')
            {
                symbols.Negative = locale->negative_sign;
            }
        }
#else
        locale_t numericLocale = newlocale(LC_NUMERIC_MASK, "", nullptr);
        if (numericLocale != static_cast<locale_t>(0))
        {
            const char* decimal = nl_langinfo_l(RADIXCHAR, numericLocale);
            if (decimal != nullptr && *decimal != '\0')
            {
                symbols.Decimal = decimal;
            }
            freelocale(numericLocale);
        }
        locale_t monetaryLocale = newlocale(LC_MONETARY_MASK, "", nullptr);
        if (monetaryLocale != static_cast<locale_t>(0))
        {
#if defined(NEGATIVE_SIGN)
            const char* negative = nl_langinfo_l(NEGATIVE_SIGN, monetaryLocale);
            if (negative != nullptr && *negative != '\0')
            {
                symbols.Negative = negative;
            }
#endif
            freelocale(monetaryLocale);
        }
#endif
        symbols.NegativeInfinity = symbols.Negative + symbols.PositiveInfinity;
#endif
        return symbols;
    }

    [[nodiscard]] std::string ApplyCurrentSymbols(
        std::string text, const NumberSymbols& symbols)
    {
        if (!text.empty() && text.front() == '-')
        {
            text.erase(text.begin());
            text.insert(0, symbols.Negative);
        }
        const std::size_t dot = text.find('.');
        if (dot != std::string::npos)
        {
            text.replace(dot, 1, symbols.Decimal);
        }
        return text;
    }

    [[nodiscard]] std::string Int32Text(std::int32_t value)
    {
        std::array<char, 32> buffer{};
        const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        if (converted.ec != std::errc{})
        {
            throw std::runtime_error("Int32 formatting failed.");
        }
        return ApplyCurrentSymbols(
            std::string(buffer.data(), static_cast<std::size_t>(converted.ptr - buffer.data())),
            CurrentNumberSymbols());
    }

    void IncrementDecimal(std::string& digits)
    {
        std::size_t index = digits.size();
        while (index > 0 && digits[index - 1] == '9')
        {
            digits[index - 1] = '0';
            --index;
        }
        if (index == 0)
        {
            digits.insert(digits.begin(), '1');
        }
        else
        {
            digits[index - 1]++;
        }
    }

    template <typename TFloat>
    [[nodiscard]] std::string NumberTextCore(
        TFloat value, std::int32_t minDecimals, std::int32_t maxDecimals,
        std::int32_t precision)
    {
        const NumberSymbols symbols = CurrentNumberSymbols();
        if (std::isnan(value))
        {
            return symbols.NaN;
        }
        if (std::isinf(value))
        {
            return std::signbit(value)
                ? symbols.NegativeInfinity
                : symbols.PositiveInfinity;
        }

        const bool negative = std::signbit(value);
        const TFloat magnitude = std::fabs(value);
        std::string scaled = "0";

        if (magnitude != static_cast<TFloat>(0))
        {
            std::array<char, 96> buffer{};
            const auto converted = std::to_chars(
                buffer.data(), buffer.data() + buffer.size(), magnitude,
                std::chars_format::scientific, precision - 1);
            if (converted.ec != std::errc{})
            {
                throw std::runtime_error("Floating-point formatting failed.");
            }

            const std::string_view scientific(
                buffer.data(), static_cast<std::size_t>(converted.ptr - buffer.data()));
            std::size_t exponentMarker = scientific.find('e');
            if (exponentMarker == std::string_view::npos)
            {
                exponentMarker = scientific.find('E');
            }
            if (exponentMarker == std::string_view::npos)
            {
                throw std::runtime_error("Floating-point formatting failed.");
            }

            std::string significant;
            significant.reserve(static_cast<std::size_t>(precision));
            for (std::size_t index = 0; index < exponentMarker; ++index)
            {
                const char unit = scientific[index];
                if (unit == '.')
                {
                    continue;
                }
                if (unit < '0' || unit > '9')
                {
                    throw std::runtime_error("Floating-point formatting failed.");
                }
                significant.push_back(unit);
            }

            const char* exponentFirst = scientific.data() + exponentMarker + 1;
            const char* const exponentLast = scientific.data() + scientific.size();
            bool exponentNegative = false;
            if (exponentFirst != exponentLast
                && (*exponentFirst == '+' || *exponentFirst == '-'))
            {
                exponentNegative = *exponentFirst == '-';
                ++exponentFirst;
            }
            std::int32_t exponent = 0;
            const auto parsed = std::from_chars(exponentFirst, exponentLast, exponent);
            if (parsed.ec != std::errc{} || parsed.ptr != exponentLast)
            {
                throw std::runtime_error("Floating-point formatting failed.");
            }
            if (exponentNegative)
            {
                exponent = -exponent;
            }

            const std::int32_t shift = exponent - (precision - 1) + maxDecimals;
            if (shift >= 0)
            {
                scaled = significant;
                scaled.append(static_cast<std::size_t>(shift), '0');
            }
            else
            {
                const std::int32_t discarded = -shift;
                const std::int32_t kept
                    = static_cast<std::int32_t>(significant.size()) - discarded;
                bool roundUp = false;
                if (kept > 0)
                {
                    scaled.assign(significant, 0, static_cast<std::size_t>(kept));
                    if (static_cast<std::size_t>(kept) < significant.size())
                    {
                        roundUp = significant[static_cast<std::size_t>(kept)] >= '5';
                    }
                }
                else if (kept == 0)
                {
                    scaled = "0";
                    roundUp = !significant.empty() && significant.front() >= '5';
                }
                else
                {
                    scaled = "0";
                }
                if (roundUp)
                {
                    IncrementDecimal(scaled);
                }
            }

            const std::size_t firstNonZero = scaled.find_first_not_of('0');
            if (firstNonZero == std::string::npos)
            {
                scaled = "0";
            }
            else if (firstNonZero != 0)
            {
                scaled.erase(0, firstNonZero);
            }
        }

        std::string result = scaled;
        if (maxDecimals > 0)
        {
            const std::size_t decimals = static_cast<std::size_t>(maxDecimals);
            if (result.size() <= decimals)
            {
                result.insert(0, decimals + 1 - result.size(), '0');
            }
            result.insert(result.end() - static_cast<std::ptrdiff_t>(decimals), '.');
            if (maxDecimals > minDecimals)
            {
                const std::size_t decimalPoint = result.find('.');
                while (result.size() > decimalPoint + 1U
                        + static_cast<std::size_t>(minDecimals)
                    && result.back() == '0')
                {
                    result.pop_back();
                }
                if (minDecimals == 0 && result.back() == '.')
                {
                    result.pop_back();
                }
            }
        }

        if (negative && scaled != "0")
        {
            result.insert(0, symbols.Negative);
        }
        const std::size_t decimalPoint = result.find('.');
        if (decimalPoint != std::string::npos)
        {
            result.replace(decimalPoint, 1, symbols.Decimal);
        }
        return result;
    }

    [[nodiscard]] std::string OneDecimal(double value)
    {
        return NumberTextCore(value, 1, 1, 15);
    }

    [[nodiscard]] std::string ZeroDecimals(float value)
    {
        return NumberTextCore(value, 0, 0, 7);
    }

    [[nodiscard]] std::string GameModeName(std::uint8_t mode)
    {
        switch (mode)
        {
        case 0: return "None";
        case 2: return "SinglePlayer";
        case 3: return "Battle";
        case 4: return "BattleTeams";
        case 5: return "Survival";
        case 6: return "SurvivalTeams";
        case 7: return "Capture";
        case 8: return "Bounty";
        case 9: return "BountyTeams";
        case 10: return "Nodes";
        case 11: return "NodesTeams";
        case 12: return "Defender";
        case 13: return "DefenderTeams";
        case 14: return "PrimeHunter";
        case 15: return "Unknown15";
        default: return Int32Text(static_cast<std::int32_t>(mode));
        }
    }

    [[nodiscard]] std::string RoleName(NetRole role)
    {
        switch (role)
        {
        case NetRole::Offline: return "Offline";
        case NetRole::Host: return "Host";
        case NetRole::Client: return "Client";
        case NetRole::Server: return "Server";
        }
        return Int32Text(static_cast<std::int32_t>(role));
    }


    [[nodiscard]] std::vector<std::uint8_t> AsciiBytes(std::string_view text)
    {
        std::vector<std::uint8_t> result;
        result.reserve(text.size());
        for (std::size_t index = 0; index < text.size();)
        {
            const Utf8Scalar unit = DecodeUtf8Scalar(text, index);
            if (!unit.Valid())
            {
                result.push_back(static_cast<std::uint8_t>('?'));
                index += unit.Length;
                continue;
            }
            result.push_back(unit.Value <= 0x7FU
                ? static_cast<std::uint8_t>(unit.Value)
                : static_cast<std::uint8_t>('?'));
            index += unit.Length;
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
        return value.value();
    }

    template <typename T>
    [[nodiscard]] const std::vector<T>& RequireVector(
        const std::shared_ptr<std::vector<T>>& value)
    {
        if (value == nullptr)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    [[nodiscard]] bool HunterDefined(std::uint8_t value) noexcept
    {
        return value <= static_cast<std::uint8_t>(MphRead::Hunter::Random);
    }

    void FillCryptographicRandom(std::span<std::uint8_t> bytes)
    {
#if defined(_WIN32)
        HMODULE bcrypt = LoadLibraryW(L"bcrypt.dll");
        if (bcrypt == nullptr)
        {
            throw std::runtime_error("RandomNumberGenerator.Fill failed.");
        }
        using BCryptGenRandomFn = LONG (WINAPI*)(void*, unsigned char*, unsigned long, unsigned long);
        auto random = reinterpret_cast<BCryptGenRandomFn>(
            GetProcAddress(bcrypt, "BCryptGenRandom"));
        if (random == nullptr)
        {
            FreeLibrary(bcrypt);
            throw std::runtime_error("RandomNumberGenerator.Fill failed.");
        }
        constexpr unsigned long UseSystemPreferredRng = 0x00000002UL;
        const LONG status = random(nullptr, bytes.data(),
            static_cast<unsigned long>(bytes.size()), UseSystemPreferredRng);
        FreeLibrary(bcrypt);
        if (status < 0)
        {
            throw std::runtime_error("RandomNumberGenerator.Fill failed.");
        }
#else
        const int fd = open("/dev/urandom", O_RDONLY);
        if (fd < 0)
        {
            throw std::runtime_error("RandomNumberGenerator.Fill failed.");
        }
        std::size_t offset = 0;
        while (offset < bytes.size())
        {
            const ssize_t count = read(fd, bytes.data() + offset, bytes.size() - offset);
            if (count <= 0)
            {
                close(fd);
                throw std::runtime_error("RandomNumberGenerator.Fill failed.");
            }
            offset += static_cast<std::size_t>(count);
        }
        static_cast<void>(close(fd));
#endif
    }

    [[nodiscard]] std::uint32_t ReadUInt32LittleEndian(
        std::span<const std::uint8_t> data) noexcept
    {
        return static_cast<std::uint32_t>(data[0])
            | (static_cast<std::uint32_t>(data[1]) << 8U)
            | (static_cast<std::uint32_t>(data[2]) << 16U)
            | (static_cast<std::uint32_t>(data[3]) << 24U);
    }

    void WriteUInt32LittleEndian(
        std::span<std::uint8_t> data, std::uint32_t value) noexcept
    {
        data[0] = static_cast<std::uint8_t>(value);
        data[1] = static_cast<std::uint8_t>(value >> 8U);
        data[2] = static_cast<std::uint8_t>(value >> 16U);
        data[3] = static_cast<std::uint8_t>(value >> 24U);
    }

    void ConsoleWriteLine(const std::string& text)
    {
        std::cout << text << '\n';
    }
}

namespace MphRead::Mods::Network
{
    std::unique_ptr<NetTransport> NetSession::_transport{};
    std::vector<std::shared_ptr<RemotePeer>> NetSession::_peers{};
    std::shared_ptr<System::Net::IPEndPoint> NetSession::_hostEndPoint{};
    std::array<std::uint8_t, NetConfig::MaxPacketSize> NetSession::_scratch{};

    NetRole NetSession::_role = NetRole::Offline;
    std::int32_t NetSession::_localSlot = 0;
    std::uint32_t NetSession::_netFrame = 0;
    std::optional<std::string> NetSession::_lastError{};

    std::int64_t NetSession::_snapshotsReceived = 0;
    std::int64_t NetSession::_snapshotsSent = 0;
    std::int64_t NetSession::_statesApplied = 0;
    std::int64_t NetSession::_intentsReceived = 0;

    SnapshotSink NetSession::_snapshotSink{};
    std::function<void()> NetSession::_serverMatchEnded{};

    Hunter NetSession::_localHunter = Hunter::Samus;
    std::int32_t NetSession::_localColor = 0;
    const std::uint32_t NetSession::ClientId = NetSession::NewClientId();

    bool NetSession::_connectionLost = false;
    double NetSession::_lastServerPacket = 0.0;
    std::int32_t NetSession::_reAnnouncements = 0;
    double NetSession::_longestServerSilence = 0.0;
    std::int32_t NetSession::_authorityStandDowns = 0;
    bool NetSession::_reAnnounced = false;
    std::int64_t NetSession::_authorityFrames = 0;
    bool NetSession::_refused = false;
    RefusedPacket NetSession::_refusedReason{};

    std::array<std::uint32_t, Entities::PlayerEntity::SlotCapacity>
        NetSession::_lastSlotIntentFrame{};
    std::int64_t NetSession::_intentsOutOfOrder = 0;

    std::optional<MatchStatePacket> NetSession::_serverMatch{};
    bool NetSession::_isAuthority = false;
    bool NetSession::_authorityNeedsStateApply = false;
    std::string NetSession::_playerName = "Player";

    std::uint32_t NetSession::_lastSnapshotFrame = 0;
    std::int32_t NetSession::_lateSnapshotRun = 0;
    std::int32_t NetSession::_snapshotStreamResets = 0;
    std::int64_t NetSession::_snapshotsOutOfOrder = 0;

    std::uint32_t NetSession::RemoteIntentAge(std::int32_t slot) noexcept
    {
        if (slot < 0
            || slot >= static_cast<std::int32_t>(RemoteIntentArrived.size())
            || RemoteIntentArrived[static_cast<std::size_t>(slot)] == 0)
        {
            return std::numeric_limits<std::uint32_t>::max();
        }
        const std::uint32_t arrived = RemoteIntentArrived[static_cast<std::size_t>(slot)];
        return _netFrame >= arrived ? _netFrame - arrived : 0U;
    }

    void NetSession::NoteStatesApplied() noexcept
    {
        IncrementInPlace(_statesApplied);
    }

    void NetSession::StartServerAuthority(
        SnapshotSink sink, std::function<void()> matchEnded)
    {
        Stop();
        _role = NetRole::Server;
        _snapshotSink = std::move(sink);
        _serverMatchEnded = std::move(matchEnded);
        _isAuthority = true;
        _localSlot = -1;
        _netFrame = 0;
        _lastError.reset();
        NetUnlagged::Reset();
        NetHitPrediction::Reset();
    }

    void NetSession::StartHost(std::int32_t port)
    {
        Stop();
        try
        {
            _transport = std::make_unique<NetTransport>(port);
            _role = NetRole::Host;
            _localSlot = 0;
            _netFrame = 0;
            _lastError.reset();
            ConsoleWriteLine("[net] hosting on UDP " + Int32Text(_transport->LocalPort()));
        }
        catch (const std::exception& ex)
        {
            _lastError = ex.what();
            ConsoleWriteLine("[net] host failed: " + std::string(ex.what()));
            _role = NetRole::Offline;
        }
    }

    void NetSession::StartClient(const std::string& address, std::int32_t port)
    {
        Stop();
        try
        {
            _transport = std::make_unique<NetTransport>(0);
            _transport->AnswerPingsImmediately();
            _hostEndPoint = std::make_shared<System::Net::IPEndPoint>(
                ResolveIPv4(address), port);
            _role = NetRole::Client;
            _localSlot = -1;
            _netFrame = 0;
            _lastError.reset();
            NetLog::Open(_playerName);
            NetLog::Event("joining " + address + ":" + Int32Text(port)
                + " as \"" + _playerName + "\"");
            SendHello();
            SendIdentify();
            ConsoleWriteLine("[net] joining " + address + ":" + Int32Text(port)
                + " as \"" + _playerName + "\"");
        }
        catch (const std::exception& ex)
        {
            _lastError = ex.what();
            ConsoleWriteLine("[net] join failed: " + std::string(ex.what()));
            _role = NetRole::Offline;
        }
    }

    void NetSession::StartPlayback()
    {
        Stop();
        _transport = std::make_unique<NetTransport>(0);
        _role = NetRole::Client;
        _localSlot = -1;
        _netFrame = 0;
        _lastError.reset();
    }

    void NetSession::RewindPlayback()
    {
        NetUnlagged::Reset();
        NetHitPrediction::Reset();
        _lastSnapshotFrame = 0;
        _lastSlotIntentFrame.fill(0);
        RemoteStateValid.fill(false);
        RemoteIntentValid.fill(false);
        _snapshotsReceived = 0;
        _snapshotsSent = 0;
        _snapshotsOutOfOrder = 0;
        _statesApplied = 0;
        _intentsReceived = 0;
        _intentsOutOfOrder = 0;
        NetPlayerBridge::Reset();
        NetDamage::Reset();
    }

    void NetSession::InjectPlaybackPacket(
        const std::shared_ptr<std::vector<std::uint8_t>>& data, std::int32_t length)
    {
        if (_transport != nullptr)
        {
            _transport->EnqueueForPlayback(data, length);
        }
    }

    std::array<std::uint8_t, 4> NetSession::ResolveIPv4(const std::string& address)
    {
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        addrinfo* result = nullptr;
        const int error = getaddrinfo(address.c_str(), nullptr, &hints, &result);
        if (error != 0)
        {
#if defined(_WIN32)
            throw std::runtime_error(gai_strerrorA(error));
#else
            throw std::runtime_error(gai_strerror(error));
#endif
        }
        for (addrinfo* current = result; current != nullptr; current = current->ai_next)
        {
            if (current->ai_family != AF_INET
                || current->ai_addrlen < static_cast<decltype(current->ai_addrlen)>(
                    sizeof(sockaddr_in)))
            {
                continue;
            }
            const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(current->ai_addr);
            std::array<std::uint8_t, 4> bytes{};
            std::memcpy(bytes.data(), &ipv4->sin_addr.s_addr, bytes.size());
            freeaddrinfo(result);
            return bytes;
        }
        freeaddrinfo(result);
        throw std::runtime_error(address + " has no IPv4 address");
    }

    void NetSession::Stop()
    {
        NetPlayerSetup::Reset();
        SpectatorMode::Reset();
        DemoRecorder::Stop();
        NetMatchSync::Reset();
        NetSlotManager::Reset();
        NetDamage::Reset();
        NetRoomChange::Reset();
        NetMatchEnd::Reset();
        NetPlayerBridge::Reset();
        Mods::Chat::ChatBox::Clear();
        _isAuthority = false;
        _authorityNeedsStateApply = false;
        _snapshotSink = {};
        _serverMatchEnded = {};
        if (_transport != nullptr)
        {
            if (_role == NetRole::Client && _hostEndPoint != nullptr)
            {
                _transport->Send(_hostEndPoint, PacketType::Bye, {});
            }
            _transport->Dispose();
            _transport.reset();
        }
        _peers.clear();
        _hostEndPoint.reset();
        _role = NetRole::Offline;
        MapVote::Reset();
        _connectionLost = false;
        _localSlot = 0;
        RemoteStateValid.fill(false);
        RemoteIntentValid.fill(false);
        RemoteIntentArrived.fill(0);
        SlotPing.fill(0);
        _lastSlotIntentFrame.fill(0);
        _lastServerPacket = 0.0;
        _reAnnouncements = 0;
        _longestServerSilence = 0.0;
        _authorityStandDowns = 0;
        _authorityFrames = 0;
        _refused = false;
        _snapshotStreamResets = 0;
        _lateSnapshotRun = 0;
        _reAnnounced = false;
        SlotOccupied.fill(false);
        _snapshotsReceived = 0;
        _snapshotsSent = 0;
        _snapshotsOutOfOrder = 0;
        _intentsOutOfOrder = 0;
        _lastSnapshotFrame = 0;
        _statesApplied = 0;
        _intentsReceived = 0;
        _serverMatch.reset();
        NetUnlagged::Reset();
        NetHitPrediction::Reset();
    }

    void NetSession::SendIdentify()
    {
        if (_transport == nullptr || _hostEndPoint == nullptr)
        {
            return;
        }
        const std::vector<std::uint8_t> name = AsciiBytes(_playerName);
        const std::size_t count = std::min(
            name.size(), static_cast<std::size_t>(RosterPacket::MaxNameBytes));
        _scratch[0] = static_cast<std::uint8_t>(_localHunter);
        _scratch[1] = static_cast<std::uint8_t>(
            PlayerColors::Clamp(_localColor));
        std::copy_n(name.begin(), count, _scratch.begin() + 2);
        _transport->Send(_hostEndPoint, PacketType::Identify,
            std::span<const std::uint8_t>(_scratch).first(count + 2));
    }

    std::uint32_t NetSession::NewClientId()
    {
        std::array<std::uint8_t, 4> bytes{};
        FillCryptographicRandom(bytes);
        const std::uint32_t id = ReadUInt32LittleEndian(bytes);
        return id == 0 ? 1U : id;
    }

    void NetSession::SendHello()
    {
        if (_transport == nullptr || _hostEndPoint == nullptr)
        {
            return;
        }
        _scratch[0] = NetConfig::ProtocolVersion;
        _scratch[1] = _localSlot >= 0 && _localSlot < 0xFF
            ? static_cast<std::uint8_t>(_localSlot)
            : 0xFFU;
        WriteUInt32LittleEndian(std::span<std::uint8_t>(_scratch).subspan(2, 4), ClientId);
        _transport->Send(_hostEndPoint, PacketType::Hello,
            std::span<const std::uint8_t>(_scratch).first(6));
    }

    void NetSession::RebindSocket()
    {
        if (_role != NetRole::Client || _transport == nullptr)
        {
            return;
        }
        const std::int32_t wasPort = _transport->LocalPort();
        _transport->Dispose();
        _transport = std::make_unique<NetTransport>(0);
        ConsoleWriteLine("[net] rebound the socket: " + Int32Text(wasPort)
            + " -> " + Int32Text(_transport->LocalPort()));
        SendHello();
        SendIdentify();
    }

    void NetSession::Update(double time)
    {
        if (_role == NetRole::Server)
        {
            _netFrame++;
            IncrementInPlace(_authorityFrames);
            return;
        }
        if (_transport == nullptr)
        {
            return;
        }

        _netFrame++;
        if (_isAuthority)
        {
            IncrementInPlace(_authorityFrames);
        }

        for (ReceivedPacket packet : _transport->Drain())
        {
            DemoRecorder::Record(packet);
            Handle(packet, time);
        }

        if (_role == NetRole::Host)
        {
            DropTimedOutPeers(time);
        }
        else if (_role == NetRole::Client && _localSlot < 0 && _netFrame % 60U == 0U)
        {
            SendHello();
        }
        else if (_role == NetRole::Client && _netFrame % 60U == 0U
            && time - _lastServerPacket > SilenceBeforeRejoin)
        {
            IncrementInPlace(_reAnnouncements);
            _reAnnounced = true;
            if (!_connectionLost)
            {
                _connectionLost = true;
                Mods::Chat::ChatBox::System("Connection lost, retrying...");
            }
            ConsoleWriteLine("[net] no word from the server; re-announcing (#"
                + Int32Text(_reAnnouncements) + ", silent for "
                + OneDecimal(time - _lastServerPacket) + " s)");
            NetLog::Event("server silent, re-announcing");
            SendHello();
            SendIdentify();
        }
        else if (_role == NetRole::Client && _netFrame % 120U == 0U
            && _localSlot >= 0 && _localSlot < static_cast<std::int32_t>(GameState::Nicknames().size()))
        {
            const std::optional<std::string> nickname
                = GameState::Nicknames()[_localSlot];
            const bool different = !nickname.has_value() || nickname.value() != _playerName;
            if (different)
            {
                SendIdentify();
            }
        }
    }

    void NetSession::Handle(ReceivedPacket packet, double time)
    {
        if (_role == NetRole::Client)
        {
            if (_lastServerPacket > 0.0 && time > _lastServerPacket)
            {
                _longestServerSilence = MathMax(
                    _longestServerSilence, time - _lastServerPacket);
            }
            _lastServerPacket = time;
            if (_connectionLost)
            {
                _connectionLost = false;
                Mods::Chat::ChatBox::System("Reconnected.");
            }
        }

        const PacketType type = packet.Type();
        switch (type)
        {
        case PacketType::Hello:
            if (_role == NetRole::Host)
            {
                HandleHello(packet, time);
            }
            break;
        case PacketType::Welcome:
            if (_role == NetRole::Client)
            {
                if (_reAnnounced)
                {
                    _reAnnounced = false;
                    if (_isAuthority)
                    {
                        _isAuthority = false;
                        IncrementInPlace(_authorityStandDowns);
                        ConsoleWriteLine("[net] re-admitted; standing down as the "
                            "simulation authority until the server says otherwise");
                        NetLog::Event("re-admitted, authority relinquished");
                    }
                }
                const std::span<const std::uint8_t> payload = packet.Payload();
                if (payload.size() >= 1)
                {
                    const std::int32_t assigned = payload[0];
                    if (_localSlot >= 0 && assigned != _localSlot)
                    {
                        ConsoleWriteLine("[net] came back as slot " + Int32Text(assigned)
                            + ", was slot " + Int32Text(_localSlot)
                            + "; releasing the old one");
                        NetLog::Event("reconnected into slot " + Int32Text(assigned)
                            + ", was " + Int32Text(_localSlot));
                        NetSlotManager::ReleaseSlot(_localSlot);
                    }
                    _localSlot = assigned;
                    ConsoleWriteLine("[net] joined as slot " + Int32Text(_localSlot));
                    NetLog::Event("server assigned slot " + Int32Text(_localSlot));
                }
            }
            break;
        case PacketType::Intent:
            if (_role == NetRole::Host)
            {
                HandleIntent(packet, time);
            }
            break;
        case PacketType::SlotIntent:
            if (_role == NetRole::Client)
            {
                HandleSlotIntent(packet);
            }
            break;
        case PacketType::Snapshot:
            if (_role == NetRole::Client)
            {
                HandleSnapshot(packet);
            }
            break;
        case PacketType::Authority:
            if (_role == NetRole::Client && !_isAuthority)
            {
                _isAuthority = true;
                _authorityNeedsStateApply = true;
                ConsoleWriteLine("[net] this client is now the simulation authority");
                NetLog::Event("became the simulation authority");
            }
            break;
        case PacketType::Refused:
            if (_role == NetRole::Client)
            {
                const std::span<const std::uint8_t> payload = packet.Payload();
                if (payload.size() >= 1 && _localSlot < 0)
                {
                    _refusedReason = RefusedPacket::Read(payload);
                    _refused = true;
                }
            }
            break;
        case PacketType::Roster:
            if (_role == NetRole::Client)
            {
                HandleRoster(packet);
            }
            break;
        case PacketType::Ping:
            if (_role == NetRole::Client && _hostEndPoint != nullptr && _transport != nullptr)
            {
                _transport->Send(_hostEndPoint, PacketType::Pong, packet.Payload());
            }
            break;
        case PacketType::MatchState:
        case PacketType::MapChange:
            if (_role == NetRole::Client)
            {
                HandleMatchState(packet, type == PacketType::MapChange);
            }
            break;
        case PacketType::Chat:
            HandleChat(packet, time);
            break;
        case PacketType::VoteState:
            if (_role == NetRole::Client)
            {
                const std::span<const std::uint8_t> payload = packet.Payload();
                if (payload.size() >= VoteStatePacket::Size)
                {
                    MapVote::Apply(VoteStatePacket::Read(payload));
                }
            }
            break;
        case PacketType::Bye:
            HandleBye(packet);
            break;
        default:
            break;
        }
    }

    void NetSession::HandleChat(ReceivedPacket packet, double time)
    {
        const std::span<const std::uint8_t> payload = packet.Payload();
        if (payload.size() < ChatPacket::Size)
        {
            return;
        }
        ChatPacket chat = ChatPacket::Read(payload);
        if (RequireString(chat.Text).empty())
        {
            return;
        }
        if (_role == NetRole::Host)
        {
            std::shared_ptr<RemotePeer> peer = FindPeer(packet.Sender);
            if (peer == nullptr || peer->SlotIndex < 0)
            {
                return;
            }
            peer->LastSeenTime = time;
            chat.Slot = static_cast<std::uint8_t>(peer->SlotIndex);
            if (peer->SlotIndex < static_cast<std::int32_t>(GameState::Nicknames().size()))
            {
                const std::optional<std::string> nickname
                    = GameState::Nicknames()[peer->SlotIndex];
                if (nickname.has_value() && !nickname->empty())
                {
                    chat.Name = nickname;
                }
            }
            chat.Kind = ChatPacket::KindSay;
            chat.Write(_scratch);
            for (std::size_t i = 0; i < _peers.size(); ++i)
            {
                if (_peers[i] != peer && _transport != nullptr)
                {
                    _transport->Send(_peers[i]->EndPoint, PacketType::Chat,
                        std::span<const std::uint8_t>(_scratch).first(ChatPacket::Size));
                }
            }
        }
        Mods::Chat::ChatBox::Receive(chat);
    }

    void NetSession::SendChat(const std::string& text)
    {
        if (_transport == nullptr || StringIsNullOrWhiteSpace(text))
        {
            return;
        }
        ChatPacket chat{};
        chat.Slot = static_cast<std::uint8_t>(std::max(_localSlot, 0));
        chat.Kind = ChatPacket::KindSay;
        chat.Name = _playerName;
        chat.Text = text;
        chat.Write(_scratch);
        if (_role == NetRole::Host)
        {
            for (std::size_t i = 0; i < _peers.size(); ++i)
            {
                _transport->Send(_peers[i]->EndPoint, PacketType::Chat,
                    std::span<const std::uint8_t>(_scratch).first(ChatPacket::Size));
            }
            return;
        }
        if (_hostEndPoint != nullptr)
        {
            _transport->Send(_hostEndPoint, PacketType::Chat,
                std::span<const std::uint8_t>(_scratch).first(ChatPacket::Size));
        }
    }

    void NetSession::SendVote(std::uint8_t kind, const std::string& roomKey)
    {
        if (_transport == nullptr || _hostEndPoint == nullptr || _role != NetRole::Client)
        {
            return;
        }
        VotePacket vote{};
        vote.Kind = kind;
        vote.RoomKey = roomKey;
        vote.Write(_scratch);
        _transport->Send(_hostEndPoint, PacketType::Vote,
            std::span<const std::uint8_t>(_scratch).first(VotePacket::Size));
    }

    void NetSession::HandleHello(ReceivedPacket packet, double time)
    {
        const std::span<const std::uint8_t> payload = packet.Payload();
        if (payload.size() < 1 || payload[0] != NetConfig::ProtocolVersion)
        {
            return;
        }
        const std::uint32_t clientId = payload.size() >= 6
            ? ReadUInt32LittleEndian(payload.subspan(2, 4))
            : 0U;
        std::shared_ptr<RemotePeer> peer = FindPeer(packet.Sender);
        if (peer == nullptr && clientId != 0)
        {
            for (std::size_t i = 0; i < _peers.size(); ++i)
            {
                if (_peers[i]->ClientId == clientId)
                {
                    peer = _peers[i];
                    ConsoleWriteLine("[net] slot " + Int32Text(peer->SlotIndex)
                        + " came back on " + packet.Sender->ToString()
                        + " (was " + peer->EndPoint->ToString() + ")");
                    peer->EndPoint = packet.Sender;
                    break;
                }
            }
        }
        if (peer == nullptr)
        {
            const std::int32_t slot = NextFreeSlot();
            if (slot < 0)
            {
                return;
            }
            peer = std::make_shared<RemotePeer>();
            peer->EndPoint = packet.Sender;
            peer->SlotIndex = slot;
            _peers.push_back(peer);
            ConsoleWriteLine("[net] peer " + packet.Sender->ToString()
                + " -> slot " + Int32Text(slot));
        }
        peer->ClientId = clientId;
        peer->LastSeenTime = time;
        _scratch[0] = static_cast<std::uint8_t>(peer->SlotIndex);
        if (_transport == nullptr)
        {
            throw System::NullReferenceException();
        }
        _transport->Send(peer->EndPoint, PacketType::Welcome,
            std::span<const std::uint8_t>(_scratch).first(1));
    }

    void NetSession::HandleIntent(ReceivedPacket packet, double time)
    {
        const std::span<const std::uint8_t> payload = packet.Payload();
        if (payload.size() < IntentPacket::Size)
        {
            return;
        }
        std::shared_ptr<RemotePeer> peer = FindPeer(packet.Sender);
        if (peer == nullptr || peer->SlotIndex < 0)
        {
            return;
        }
        const IntentPacket intent = IntentPacket::Read(payload);
        if (peer->LastIntentFrame != 0 && intent.Frame <= peer->LastIntentFrame
            && peer->LastIntentFrame - intent.Frame < IntentResetGap)
        {
            return;
        }
        peer->LastIntentFrame = intent.Frame;
        peer->LatestIntent = intent;
        peer->LastSeenTime = time;
        RemoteIntents.at(static_cast<std::size_t>(peer->SlotIndex)) = intent;
        RemoteIntentValid.at(static_cast<std::size_t>(peer->SlotIndex)) = true;
    }

    void NetSession::HandleSlotIntent(ReceivedPacket packet)
    {
        const std::span<const std::uint8_t> payload = packet.Payload();
        if (payload.size() < 1 + IntentPacket::Size)
        {
            return;
        }
        const std::int32_t slot = payload[0];
        if (slot < 0 || slot >= static_cast<std::int32_t>(RemoteIntents.size())
            || slot == _localSlot)
        {
            return;
        }
        AcceptSlotIntent(slot, IntentPacket::Read(payload.subspan(1)));
    }

    void NetSession::AcceptSlotIntent(std::int32_t slot, IntentPacket intent)
    {
        if (slot < 0 || slot >= static_cast<std::int32_t>(RemoteIntents.size())
            || slot == _localSlot)
        {
            return;
        }
        const std::size_t index = static_cast<std::size_t>(slot);
        if (_lastSlotIntentFrame[index] != 0 && intent.Frame <= _lastSlotIntentFrame[index]
            && _lastSlotIntentFrame[index] - intent.Frame < IntentResetGap)
        {
            IncrementInPlace(_intentsOutOfOrder);
            return;
        }
        _lastSlotIntentFrame[index] = intent.Frame;
        RemoteIntents[index] = intent;
        RemoteIntentValid[index] = true;
        RemoteIntentArrived[index] = std::max(_netFrame, 1U);
        IncrementInPlace(_intentsReceived);
    }

    void NetSession::ForgetSlot(std::int32_t slot)
    {
        if (slot < 0 || slot >= Entities::PlayerEntity::SlotCapacity)
        {
            return;
        }
        const std::size_t index = static_cast<std::size_t>(slot);
        _lastSlotIntentFrame[index] = 0;
        RemoteIntentValid[index] = false;
        RemoteIntents[index] = IntentPacket{};
        RemoteStateValid[index] = false;
        RemoteStates[index] = PlayerState{};
        for (std::size_t i = 0; i < _peers.size(); ++i)
        {
            if (_peers[i]->SlotIndex == slot)
            {
                _peers[i]->LastIntentFrame = 0;
            }
        }
    }

    bool NetSession::ConsumeAuthorityStateSync() noexcept
    {
        if (!_authorityNeedsStateApply)
        {
            return false;
        }
        _authorityNeedsStateApply = false;
        return true;
    }

    std::int32_t NetSession::ServerPlayerCount() noexcept
    {
        return _serverMatch.has_value()
            ? static_cast<std::int32_t>(_serverMatch->PlayerCount)
            : 0;
    }

    void NetSession::HandleRoster(ReceivedPacket packet)
    {
        const std::span<const std::uint8_t> payload = packet.Payload();
        if (payload.size() < RosterPacket::Size)
        {
            return;
        }
        ApplyRoster(RosterPacket::Read(payload));
    }

    void NetSession::ApplyRoster(RosterPacket roster)
    {
        SlotOccupied.fill(false);
        for (std::int32_t i = 0; i < roster.Count; ++i)
        {
            const std::size_t row = static_cast<std::size_t>(i);
            const std::int32_t slot = RequireVector(roster.Slots).at(row);
            if (slot < 0 || slot >= static_cast<std::int32_t>(SlotOccupied.size()))
            {
                continue;
            }
            const std::size_t index = static_cast<std::size_t>(slot);
            SlotOccupied[index] = true;
            GameState::Nicknames()[slot]
                = RequireVector(roster.Names).at(row).value_or(std::string());
            if (HunterDefined(RequireVector(roster.Hunters).at(row)))
            {
                SlotHunter[index] = static_cast<Hunter>(
                    RequireVector(roster.Hunters).at(row));
            }
            PlayerColors::Choice[slot] = (PlayerColors::Clamp(
                    RequireVector(roster.Colors).at(row)));
            SlotPing[index] = RequireVector(roster.Pings).at(row);
        }
    }

    void NetSession::HandleMatchState(ReceivedPacket packet, bool rotated)
    {
        const std::span<const std::uint8_t> payload = packet.Payload();
        if (payload.size() < MatchStatePacket::Size)
        {
            return;
        }
        ApplyMatchState(MatchStatePacket::Read(payload), rotated);
    }

    void NetSession::ApplyMatchState(MatchStatePacket state, bool rotated)
    {
        const std::optional<std::string> previous = _serverMatch.has_value()
            ? _serverMatch->RoomKey
            : std::nullopt;
        _serverMatch = state;
        if (rotated || !previous.has_value() || previous != state.RoomKey)
        {
            const std::string room = state.RoomKey.value_or(std::string{});
            ConsoleWriteLine("[net] server map: " + room + " ("
                + GameModeName(state.Mode) + ", " + ZeroDecimals(state.TimeRemaining)
                + " s left)");
            const std::vector<MapChangedHandler> handlers = MapChanged;
            for (const MapChangedHandler& handler : handlers)
            {
                handler(state);
            }
        }
    }

    void NetSession::HandleSnapshot(ReceivedPacket packet)
    {
        const std::span<const std::uint8_t> payload = packet.Payload();
        if (payload.size() < SnapshotHeader::Size)
        {
            return;
        }
        const SnapshotHeader header = SnapshotHeader::Read(payload);
        if (_lastSnapshotFrame != 0 && header.Frame <= _lastSnapshotFrame
            && _lastSnapshotFrame - header.Frame < SnapshotResetGap)
        {
            IncrementInPlace(_snapshotsOutOfOrder);
            IncrementInPlace(_lateSnapshotRun);
            if (_lateSnapshotRun < LateSnapshotsBeforeReset)
            {
                return;
            }
            NetLog::Event("snapshot stream re-based: " + Int32Text(_lateSnapshotRun)
                + " in a row older than " + std::to_string(_lastSnapshotFrame)
                + " (now " + std::to_string(header.Frame) + ")");
            IncrementInPlace(_snapshotStreamResets);
        }
        _lateSnapshotRun = 0;
        _lastSnapshotFrame = header.Frame;
        IncrementInPlace(_snapshotsReceived);
        Rng::SetRng1(header.Rng1);
        Rng::SetRng2(header.Rng2);

        std::size_t offset = SnapshotHeader::Size;
        RemoteStateValid.fill(false);
        for (std::int32_t i = 0; i < header.PlayerCount; ++i)
        {
            if (offset + PlayerState::Size > payload.size())
            {
                break;
            }
            const PlayerState state = PlayerState::Read(payload.subspan(offset));
            offset += PlayerState::Size;
            if (state.SlotIndex < RemoteStates.size())
            {
                const std::size_t slot = state.SlotIndex;
                RemoteStates[slot] = state;
                RemoteStateValid[slot] = true;
            }
        }
    }

    void NetSession::HandleBye(ReceivedPacket packet)
    {
        if (_role == NetRole::Host)
        {
            std::shared_ptr<RemotePeer> peer = FindPeer(packet.Sender);
            if (peer != nullptr)
            {
                ConsoleWriteLine("[net] peer " + peer->EndPoint->ToString()
                    + " left (slot " + Int32Text(peer->SlotIndex) + ")");
                RemoteIntentValid.at(static_cast<std::size_t>(peer->SlotIndex)) = false;
                const auto found = std::find(_peers.begin(), _peers.end(), peer);
                if (found != _peers.end())
                {
                    _peers.erase(found);
                }
            }
        }
        else
        {
            ConsoleWriteLine("[net] host closed the session");
            Stop();
        }
    }

    void NetSession::DropTimedOutPeers(double time)
    {
        for (std::int32_t i = static_cast<std::int32_t>(_peers.size()) - 1; i >= 0; --i)
        {
            const std::shared_ptr<RemotePeer> peer = _peers[static_cast<std::size_t>(i)];
            if (time - peer->LastSeenTime > NetConfig::TimeoutSeconds)
            {
                ConsoleWriteLine("[net] peer " + peer->EndPoint->ToString()
                    + " timed out (slot " + Int32Text(peer->SlotIndex) + ")");
                RemoteIntentValid.at(static_cast<std::size_t>(peer->SlotIndex)) = false;
                _peers.erase(_peers.begin() + i);
            }
        }
    }

    std::shared_ptr<RemotePeer> NetSession::FindPeer(
        const std::shared_ptr<System::Net::IPEndPoint>& endPoint)
    {
        for (const std::shared_ptr<RemotePeer>& peer : _peers)
        {
            if (peer->EndPoint != nullptr && endPoint != nullptr
                && peer->EndPoint->Equals(*endPoint))
            {
                return peer;
            }
        }
        return nullptr;
    }

    std::int32_t NetSession::NextFreeSlot()
    {
        for (std::int32_t slot = 1; slot < Entities::PlayerEntity::MaxPlayers(); ++slot)
        {
            bool used = false;
            for (const std::shared_ptr<RemotePeer>& peer : _peers)
            {
                if (peer->SlotIndex == slot)
                {
                    used = true;
                    break;
                }
            }
            if (!used)
            {
                return slot;
            }
        }
        return -1;
    }

    void NetSession::SendIntent(IntentPacket intent)
    {
        if (_transport == nullptr || _role != NetRole::Client || _hostEndPoint == nullptr)
        {
            return;
        }
        intent.Frame = _netFrame;
        intent.Write(_scratch);
        _transport->Send(_hostEndPoint, PacketType::Intent,
            std::span<const std::uint8_t>(_scratch).first(IntentPacket::Size));
        if (_localSlot >= 0)
        {
            DemoRecorder::RecordOwnIntent(_localSlot,
                std::span<const std::uint8_t>(_scratch).first(IntentPacket::Size));
        }
    }

    void NetSession::SendMatchEnd()
    {
        if (_role == NetRole::Server)
        {
            if (_serverMatchEnded)
            {
                _serverMatchEnded();
            }
            return;
        }
        if (_transport == nullptr || _role != NetRole::Client || _hostEndPoint == nullptr)
        {
            return;
        }
        _transport->Send(_hostEndPoint, PacketType::MatchEnd, {});
    }

    void NetSession::BroadcastSnapshot()
    {
        const bool asServer = _role == NetRole::Server && static_cast<bool>(_snapshotSink);
        if (_transport == nullptr && !asServer)
        {
            return;
        }
        const bool asHost = _role == NetRole::Host && !_peers.empty();
        const bool asAuthority = _role == NetRole::Client
            && _isAuthority && _hostEndPoint != nullptr;
        if (!asHost && !asAuthority && !asServer)
        {
            return;
        }

        std::int32_t count = 0;
        std::size_t offset = SnapshotHeader::Size;
        const auto& players = Entities::PlayerEntity::Players();
        for (std::size_t i = 0; i < players.size(); ++i)
        {
            if (players[i] == nullptr)
            {
                throw System::NullReferenceException();
            }
            Entities::PlayerEntity& player = *players[i];
            if (!HasFlag(player.LoadFlags(), Entities::LoadFlags::Active))
            {
                continue;
            }
            if (offset + PlayerState::Size > NetConfig::MaxPacketSize - 1)
            {
                break;
            }
            if (!std::isfinite(player.Position.X)
                || !std::isfinite(player.Position.Y)
                || !std::isfinite(player.Position.Z))
            {
                NetLog::Event("slot " + Int32Text(static_cast<std::int32_t>(i))
                    + " not published: position is "
                    + static_cast<OpenTK::Mathematics::Vector3>(player.Position).ToString());
                continue;
            }

            PlayerState state{};
            state.SlotIndex = static_cast<std::uint8_t>(i);
            state.Flags = static_cast<std::uint8_t>(
                PlayerState::FlagActive
                | (player.IsAltForm() ? PlayerState::FlagAltForm : 0)
                | (player.ModIsInPlay() ? PlayerState::FlagSpawned : 0)
                | (player.EquipInfo()->Zoomed ? PlayerState::FlagZoomed : 0)
                | (HasFlag(player.Flags2(), Entities::PlayerFlags2::Spectating)
                    ? PlayerState::FlagSpectating : 0)
                | (player.ModFrozen() ? PlayerState::FlagFrozen : 0)
                | (player.ModDisrupted() ? PlayerState::FlagDisrupted : 0)
                | (player.ModBurning() ? PlayerState::FlagBurning : 0));
            state.Position = player.Position;
            state.Speed = player.Speed();
            state.Facing = player.FacingVector();
            state.Health = static_cast<std::uint16_t>(
                std::clamp(player.Health(), 0,
                    static_cast<std::int32_t>(std::numeric_limits<std::uint16_t>::max())));
            state.CurrentWeapon = static_cast<std::uint8_t>(player.CurrentWeapon());
            state.Team = static_cast<std::uint8_t>(player.Team());
            state.Points = static_cast<std::int16_t>(
                std::clamp(GameState::Points()[static_cast<std::int32_t>(i)],
                    static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::min()),
                    static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::max())));
            state.Kills = static_cast<std::uint16_t>(
                std::clamp(GameState::Kills()[static_cast<std::int32_t>(i)], 0,
                    static_cast<std::int32_t>(std::numeric_limits<std::uint16_t>::max())));
            state.Deaths = static_cast<std::uint16_t>(
                std::clamp(GameState::Deaths()[static_cast<std::int32_t>(i)], 0,
                    static_cast<std::int32_t>(std::numeric_limits<std::uint16_t>::max())));
            NetDamage::Write(static_cast<std::int32_t>(i), state);
            state.Write(std::span<std::uint8_t>(_scratch).subspan(offset));
            offset += PlayerState::Size;
            ++count;
        }

        SnapshotHeader header{};
        header.Frame = _netFrame;
        header.Rng1 = Rng::Rng1();
        header.Rng2 = Rng::Rng2();
        header.PlayerCount = static_cast<std::uint8_t>(count);
        header.Write(_scratch);
        IncrementInPlace(_snapshotsSent);
        NetUnlagged::Record(header.Frame);
        DemoRecorder::RecordOwnSnapshot(
            std::span<const std::uint8_t>(_scratch).first(offset));
        if (asServer)
        {
            _snapshotSink(std::span<const std::uint8_t>(_scratch).first(offset));
            return;
        }

        NetTransport& transport = *_transport;
        if (asAuthority)
        {
            transport.Send(_hostEndPoint, PacketType::Snapshot,
                std::span<const std::uint8_t>(_scratch).first(offset));
            return;
        }
        for (const std::shared_ptr<RemotePeer>& peer : _peers)
        {
            transport.Send(peer->EndPoint, PacketType::Snapshot,
                std::span<const std::uint8_t>(_scratch).first(offset));
        }
    }
}

namespace MphRead::Mods::Network::Detail
{
    struct NetLaunchMatchState
    {
        std::optional<std::string> RoomKey;
        std::int32_t Mode;
        double TimeRemaining;
    };

    struct NetMatchSyncMatchStatePacket
    {
        float TimeRemaining = 0.0f;
        float TimeElapsed = 0.0f;
        std::uint8_t Flags = 0;
        std::uint16_t PointGoal = 0;
        std::optional<std::string> RoomKey{};

        [[nodiscard]] bool FriendlyFire() const
        {
            return (Flags & (1u << 2)) != 0;
        }

        [[nodiscard]] bool ShadowFreeze() const
        {
            return (Flags & (1u << 3)) == 0;
        }
    };

    void NetConnectCommandNetSessionStop()
    {
        NetSession::Stop();
    }

    void NetLaunchSetSessionPlayerName(const std::string& value)
    {
        NetSession::SetPlayerName(value);
    }

    void NetLaunchSetSessionLocalHunter(Hunter value)
    {
        NetSession::SetLocalHunter(value);
    }

    void NetLaunchSetSessionLocalColor(std::int32_t value)
    {
        NetSession::SetLocalColor(value);
    }

    void NetLaunchStartClient(const std::string& address, std::int32_t port)
    {
        NetSession::StartClient(address, port);
    }

    bool NetLaunchSessionActive()
    {
        return NetSession::Active();
    }

    void NetLaunchSessionUpdate(double elapsedSeconds)
    {
        NetSession::Update(elapsedSeconds);
    }

    bool NetLaunchSessionRefused()
    {
        return NetSession::Refused();
    }

    std::string NetLaunchDescribeRefusedReason(const std::string& where)
    {
        return NetSession::RefusedReason().Describe(std::optional<std::string>(where));
    }

    std::int32_t NetLaunchSessionLocalSlot()
    {
        return NetSession::LocalSlot();
    }

    std::optional<NetLaunchMatchState> NetLaunchSessionServerMatch()
    {
        const std::optional<MatchStatePacket> state = NetSession::ServerMatch();
        if (!state.has_value())
        {
            return std::nullopt;
        }
        return NetLaunchMatchState{
            state->RoomKey,
            static_cast<std::int32_t>(state->Mode),
            static_cast<double>(state->TimeRemaining)
        };
    }

    void NetLaunchSessionSendIdentify()
    {
        NetSession::SendIdentify();
    }

    bool NetMatchEndNetSessionActive()
    {
        return NetSession::Active();
    }

    bool NetMatchEndNetSessionIsAuthority()
    {
        return NetSession::IsAuthority();
    }

    bool NetMatchEndNetSessionIsHost()
    {
        return NetSession::IsHost();
    }

    std::optional<MatchStatePacket> NetMatchEndNetSessionServerMatch()
    {
        return NetSession::ServerMatch();
    }

    std::uint32_t NetMatchEndNetSessionNetFrame()
    {
        return NetSession::NetFrame();
    }

    void NetMatchEndNetSessionSendMatchEnd()
    {
        NetSession::SendMatchEnd();
    }

    bool NetMatchSyncNetSessionActive()
    {
        return NetSession::Active();
    }

    bool NetMatchSyncNetSessionServerMatchHasValue()
    {
        return NetSession::ServerMatch().has_value();
    }

    NetMatchSyncMatchStatePacket NetMatchSyncNetSessionServerMatchValue()
    {
        const MatchStatePacket state = NetSession::ServerMatch().value();
        return NetMatchSyncMatchStatePacket{
            state.TimeRemaining,
            state.TimeElapsed,
            state.Flags,
            state.PointGoal,
            state.RoomKey
        };
    }

    std::uint32_t NetPlayerBridgeLastSnapshotFrame()
    {
        return NetSession::LastSnapshotFrame();
    }

    bool NetPlayerBridgeIsAuthority()
    {
        return NetSession::IsAuthority();
    }

    std::int32_t NetPlayerBridgeSlotPingLength()
    {
        return static_cast<std::int32_t>(NetSession::SlotPing.size());
    }

    std::int32_t NetPlayerBridgeSlotPing(std::int32_t slot)
    {
        return NetSession::SlotPing.at(static_cast<std::size_t>(slot));
    }

    std::uint32_t NetPlayerBridgeNetFrame()
    {
        return NetSession::NetFrame();
    }

    bool NetPlayerSetupSessionActive()
    {
        return NetSession::Active();
    }

    std::int32_t NetPlayerSetupLocalSlot()
    {
        return NetSession::LocalSlot();
    }

    bool NetPlayerSetupIsServer()
    {
        return NetSession::IsServer();
    }

    bool NetSlotManagerSessionActive()
    {
        return NetSession::Active();
    }

    std::int32_t NetSlotManagerSessionLocalSlot()
    {
        return NetSession::LocalSlot();
    }

    bool NetSlotManagerSessionIsServer()
    {
        return NetSession::IsServer();
    }

    std::int32_t NetSlotManagerSlotOccupiedLength()
    {
        return static_cast<std::int32_t>(NetSession::SlotOccupied.size());
    }

    bool NetSlotManagerSlotOccupied(std::int32_t slot)
    {
        return NetSession::SlotOccupied.at(static_cast<std::size_t>(slot));
    }

    Hunter NetSlotManagerSlotHunter(std::int32_t slot)
    {
        return NetSession::SlotHunter.at(static_cast<std::size_t>(slot));
    }

    void NetSlotManagerSessionForgetSlot(std::int32_t slot)
    {
        NetSession::ForgetSlot(slot);
    }

    bool NetRoomChangeSessionActive()
    {
        return NetSession::Active();
    }

    std::uint32_t NetRoomChangeSessionNetFrame()
    {
        return NetSession::NetFrame();
    }

    std::optional<MatchStatePacket> NetRoomChangeSessionServerMatch()
    {
        return NetSession::ServerMatch();
    }

    std::int32_t NetRoomChangeSessionLocalSlot()
    {
        return NetSession::LocalSlot();
    }

    Hunter NetRoomChangeSessionSlotHunter(std::int32_t slot)
    {
        return NetSession::SlotHunter.at(static_cast<std::size_t>(slot));
    }

    std::int32_t NetRoomChangeSessionSlotOccupiedLength()
    {
        return static_cast<std::int32_t>(NetSession::SlotOccupied.size());
    }

    bool NetRoomChangeSessionSlotOccupied(std::int32_t slot)
    {
        return NetSession::SlotOccupied.at(static_cast<std::size_t>(slot));
    }

    std::string NetLogRoleText()
    {
        return RoleName(NetSession::Role());
    }

    std::int32_t NetLogLocalSlot()
    {
        return NetSession::LocalSlot();
    }

    bool NetLogIsAuthority()
    {
        return NetSession::IsAuthority();
    }

    std::optional<MatchStatePacket> NetLogServerMatch()
    {
        return NetSession::ServerMatch();
    }

    std::int32_t NetLogSlotOccupiedLength()
    {
        return static_cast<std::int32_t>(NetSession::SlotOccupied.size());
    }

    bool NetLogSlotOccupied(std::int32_t slot)
    {
        return NetSession::SlotOccupied.at(static_cast<std::size_t>(slot));
    }

    bool NetLogRemoteStateValid(std::int32_t slot)
    {
        return NetSession::RemoteStateValid.at(static_cast<std::size_t>(slot));
    }

    bool NetLogRemoteIntentValid(std::int32_t slot)
    {
        return NetSession::RemoteIntentValid.at(static_cast<std::size_t>(slot));
    }

    void DedicatedServerApplyMatchState(const MatchStatePacket& state, bool rotated)
    {
        NetSession::ApplyMatchState(state, rotated);
    }

    void DedicatedServerApplyRoster(const RosterPacket& roster)
    {
        NetSession::ApplyRoster(roster);
    }

    void DedicatedServerAcceptSlotIntent(std::int32_t slotIndex, const IntentPacket& intent)
    {
        NetSession::AcceptSlotIntent(slotIndex, intent);
    }
}

namespace MphRead::Mods::Network
{
    namespace
    {
        // NetSession.cs NetRole : int
        constexpr ::MphRead::NativeRuntime::EnumNameEntry NetRoleNames[] = {
            {0x0ULL, "Offline"},
            {0x1ULL, "Host"},
            {0x2ULL, "Client"},
            {0x3ULL, "Server"},
        };
    }

    std::string ToString(NetRole value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(
            value, NetRoleNames, std::size(NetRoleNames), false);
    }
}
