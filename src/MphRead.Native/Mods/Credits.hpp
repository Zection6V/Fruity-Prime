#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace MphRead
{
    namespace Mods
    {
        class Credits final
        {
        public:
            class Entry final
            {
            public:
                Entry() = default;
                Entry(std::optional<std::string> who, std::optional<std::string> what,
                    std::optional<std::string> where);

                [[nodiscard]] const std::optional<std::string>& Who() const noexcept;
                [[nodiscard]] const std::optional<std::string>& What() const noexcept;
                [[nodiscard]] const std::optional<std::string>& Where() const noexcept;

                friend bool operator==(const Entry&, const Entry&) = default;

            private:
                std::optional<std::string> _who;
                std::optional<std::string> _what;
                std::optional<std::string> _where;
            };

            inline static constexpr std::string_view Author = "Livetek";
            inline static constexpr std::string_view ForkWork
                = "this fork: multiplayer and the dedicated server, "
                  "the launcher, custom maps, the Android head and the pro HUD";
            inline static constexpr std::string_view SupportUrl = "https://ko-fi.com/livetek";

            [[nodiscard]] static std::string Summary();
            [[nodiscard]] static std::string Compact();
            [[nodiscard]] static std::string Names();
            [[nodiscard]] static const std::array<Entry, 12>& Entries();
            static void Print();

            Credits() = delete;
            Credits(const Credits&) = delete;
            Credits& operator=(const Credits&) = delete;
        };
    }
}
