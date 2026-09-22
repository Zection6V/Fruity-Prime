#pragma once

#include "../../NativeRuntime/System/Net.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <new>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace MphRead::Mods::Network
{
    enum class PacketType : std::uint8_t;
}

namespace MphRead::Mods::Network
{
    struct ReceivedPacket
    {
        const std::shared_ptr<System::Net::IPEndPoint> Sender{};
        const std::shared_ptr<std::vector<std::uint8_t>> Data{};
        const std::int32_t Length = 0;

        ReceivedPacket() noexcept = default;
        ReceivedPacket(std::shared_ptr<System::Net::IPEndPoint> sender,
            std::shared_ptr<std::vector<std::uint8_t>> data, std::int32_t length) noexcept;
        ReceivedPacket(const ReceivedPacket&) noexcept = default;
        ReceivedPacket(ReceivedPacket&& other) noexcept;
        ReceivedPacket& operator=(const ReceivedPacket& other) noexcept;
        ReceivedPacket& operator=(ReceivedPacket&& other) noexcept;
        ~ReceivedPacket() = default;

        [[nodiscard]] PacketType Type() const;
        [[nodiscard]] std::span<const std::uint8_t> Payload() const;
    };

    class NetTransport final
    {
    private:
        struct State;

    public:
        class DrainEnumerable final
        {
        public:
            class iterator final
            {
            public:
                using value_type = ReceivedPacket;
                using difference_type = std::ptrdiff_t;

                iterator() noexcept = default;
                [[nodiscard]] ReceivedPacket operator*() const;
                iterator& operator++();
                void operator++(int);

                [[nodiscard]] bool AtEnd() const noexcept;
                friend bool operator==(const iterator& left, std::default_sentinel_t) noexcept
                {
                    return left.AtEnd();
                }
                friend bool operator!=(const iterator& left, std::default_sentinel_t) noexcept
                {
                    return !left.AtEnd();
                }

            private:
                friend class DrainEnumerable;
                explicit iterator(std::shared_ptr<State> state);
                void Advance();

                std::shared_ptr<State> _state{};
                std::optional<ReceivedPacket> _current{};
            };

            [[nodiscard]] iterator begin() const;
            [[nodiscard]] std::default_sentinel_t end() const noexcept;

        private:
            friend class NetTransport;
            explicit DrainEnumerable(std::shared_ptr<State> state) noexcept;
            std::shared_ptr<State> _state;
        };

        explicit NetTransport(std::int32_t port);
        ~NetTransport() = default;

        NetTransport(const NetTransport&) = delete;
        NetTransport(NetTransport&&) = delete;
        NetTransport& operator=(const NetTransport&) = delete;
        NetTransport& operator=(NetTransport&&) = delete;

        void AnswerPingsImmediately() noexcept;

        [[nodiscard]] std::int32_t LocalPort() const noexcept;
        [[nodiscard]] std::int64_t PacketsDropped() const noexcept;

        static std::atomic<std::int64_t> TotalPacketsDropped;
        static std::atomic<std::int64_t> TotalPacketsSent;

        [[nodiscard]] DrainEnumerable Drain();

        void EnqueueForPlayback(const std::shared_ptr<std::vector<std::uint8_t>>& data,
            std::int32_t length);

        void Send(const std::shared_ptr<System::Net::IPEndPoint>& target,
            PacketType type, std::span<const std::uint8_t> payload,
            std::int64_t extraHoldTicks = 0);

        void Dispose();

    private:
        static constexpr std::int32_t MaxQueuedPackets = 2048;
        static constexpr std::int32_t SocketBufferBytes = 1 << 20;

        static const std::shared_ptr<System::Net::IPEndPoint> _playbackSender;
        std::shared_ptr<State> _state;
    };
}
