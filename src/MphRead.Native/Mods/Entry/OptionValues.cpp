#include "OptionValues.hpp"

#include "CommandLine.hpp"

#include "../../Formats/Enums.hpp"
#include "../../Formats/Formats.hpp"
#include "../../NativeRuntime/System/Encoding.hpp"
#include "../../NativeRuntime/System/Number.hpp"
#include "../Network/Discovery/NetMaster.hpp"
#include "../Network/Protocol/Packets/PacketTypes.hpp"

#include <array>
#include <stdexcept>
#include <type_traits>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

using ::MphRead::NativeRuntime::Int32TryParseCurrentCulture;
using ::MphRead::NativeRuntime::WideToWtf8;

namespace
{
    using MphRead::BeamType;
    using MphRead::GameMode;
    using MphRead::Hunter;

    template <typename Float>
    [[nodiscard]] bool TryParseFloatingClassic(std::string_view text, Float& value, bool allowThousands)
    {
        using ::MphRead::NativeRuntime::NumberFormatInfo;
        using ::MphRead::NativeRuntime::NumberStyles;
        const NumberStyles styles = allowThousands
            ? NumberStyles::Float | NumberStyles::AllowThousands
            : NumberStyles::Float;
        if constexpr (std::is_same_v<Float, float>)
        {
            return ::MphRead::NativeRuntime::TryParseSingle(text, styles, NumberFormatInfo::InvariantInfo(), value);
        }
        else
        {
            return ::MphRead::NativeRuntime::TryParseDouble(text, styles, NumberFormatInfo::InvariantInfo(), value);
        }
    }
}

namespace MphRead::Mods::Entry
{
    bool TryParseDoubleInvariant(
        const std::optional<std::string>& text, double& value, bool allowThousands)
    {
        if (!text.has_value())
        {
            value = 0;
            return false;
        }
        return TryParseFloatingClassic(*text, value, allowThousands);
    }

    bool TryParseFloatInvariant(
        const std::optional<std::string>& text, float& value, bool allowThousands)
    {
        if (!text.has_value())
        {
            value = 0;
            return false;
        }
        return TryParseFloatingClassic(*text, value, allowThousands);
    }

    bool TryParseDoubleCurrent(std::string_view text, double& value)
    {
        return ::MphRead::NativeRuntime::DoubleTryParseCurrentCulture(text, value);
    }
    bool TryParseGameMode(const std::optional<std::string>& text, GameMode& value)
    {
        if (!text.has_value())
        {
            return false;
        }
        return ::MphRead::TryParse(*text, true, value);
    }

    bool TryParseHunter(const std::optional<std::string>& text, Hunter& value)
    {
        if (!text.has_value())
        {
            return false;
        }
        return ::MphRead::TryParse(*text, true, value);
    }

    bool TryParseBeam(const std::optional<std::string>& text, BeamType& value)
    {
        if (!text.has_value())
        {
            return false;
        }
        return ::MphRead::TryParse(*text, true, value);
    }

    std::string MachineName()
    {
#if defined(_WIN32)
        DWORD size = 256;
        std::vector<wchar_t> buffer(size);
        for (;;)
        {
            DWORD actual = size;
            if (GetComputerNameW(buffer.data(), &actual) != FALSE)
            {
                return WideToWtf8(std::wstring_view(buffer.data(), actual));
            }
            if (GetLastError() != ERROR_BUFFER_OVERFLOW)
            {
                throw std::runtime_error("GetComputerNameW failed");
            }
            size *= 2;
            buffer.resize(size);
        }
#else
        std::array<char, 256> buffer{};
        if (gethostname(buffer.data(), buffer.size()) != 0)
        {
            throw std::runtime_error("gethostname failed");
        }
        buffer.back() = '\0';
        return std::string(buffer.data());
#endif
    }

    int ParsePort(const std::vector<std::string>& args)
    {
        const std::optional<std::string> value = ValueAfter(args, "port");
        std::int32_t port = 0;
        if (value.has_value() && Int32TryParseCurrentCulture(*value, port))
        {
            return port;
        }
        return MphRead::Mods::Network::NetConfig::DefaultPort;
    }

    int ParseMasterPort(const std::vector<std::string>& args)
    {
        const std::optional<std::string> value = ValueAfter(args, "masterport");
        std::int32_t port = 0;
        if (value.has_value() && Int32TryParseCurrentCulture(*value, port))
        {
            return port;
        }
        return MphRead::Mods::Network::NetMasterConfig::DefaultPort;
    }

    std::string ParseName(const std::vector<std::string>& args)
    {
        const std::optional<std::string> value = ValueAfter(args, "name");
        return value.has_value() ? *value : MachineName();
    }

    Hunter ParseHunter(const std::vector<std::string>& args)
    {
        Hunter hunter = Hunter::Samus;
        (void)TryParseHunter(ValueAfter(args, "hunter"), hunter);
        return hunter;
    }

    int ParseRecolor(const std::vector<std::string>& args)
    {
        const std::optional<std::string> value = ValueAfter(args, "recolor");
        std::int32_t recolor = 0;
        if (value.has_value() && Int32TryParseCurrentCulture(*value, recolor))
        {
            return recolor;
        }
        return 0;
    }
}
