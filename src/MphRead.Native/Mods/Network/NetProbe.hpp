#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace MphRead::Mods::Network
{
    class NetProbe final
    {
    public:
        NetProbe() = delete;

        [[nodiscard]] static std::pair<bool, std::string> Probe(
            const std::string& address,
            std::int32_t port,
            std::int32_t timeoutMs = 3000);
    };
}
