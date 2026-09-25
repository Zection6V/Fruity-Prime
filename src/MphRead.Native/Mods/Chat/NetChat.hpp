#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace MphRead::Mods::Network
{
    struct ChatPacket;
}

namespace MphRead::Mods::Chat
{
    class NetChat final
    {
    public:
        NetChat() = delete;

        [[nodiscard]] static const std::vector<std::string>& History() noexcept { return _history; }
        [[nodiscard]] static const std::vector<std::pair<std::string, bool>>& Entries() noexcept { return _entries; }
        [[nodiscard]] static std::int32_t Revision() noexcept { return _revision; }

        static void Clear();
        static void Receive(const Network::ChatPacket& packet);
        static void Remember(const Network::ChatPacket& packet);
        static void Send(const std::string& text);

    private:
        static std::vector<std::string> _history;
        static std::vector<std::pair<std::string, bool>> _entries;
        static std::int32_t _revision;
    };
}
