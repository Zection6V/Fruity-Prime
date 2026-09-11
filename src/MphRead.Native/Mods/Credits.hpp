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
                using NullableString = std::optional<std::string>;

                Entry() = default;
                Entry(NullableString who, NullableString what, NullableString where);

                [[nodiscard]] const NullableString& Who() const noexcept;
                [[nodiscard]] const NullableString& What() const noexcept;
                [[nodiscard]] const NullableString& Where() const noexcept;

                friend bool operator==(const Entry&, const Entry&) = default;

            private:
                NullableString _who;
                NullableString _what;
                NullableString _where;
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
