#pragma once

#include <string>
#include <string_view>
#include <vector>

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
                Entry(std::string who, std::string what, std::string where);

                [[nodiscard]] const std::string& Who() const noexcept;
                [[nodiscard]] const std::string& What() const noexcept;
                [[nodiscard]] const std::string& Where() const noexcept;

                friend bool operator==(const Entry&, const Entry&) = default;

            private:
                std::string _who;
                std::string _what;
                std::string _where;
            };

            inline static constexpr std::string_view Author = "Livetek";
            inline static constexpr std::string_view ForkWork
                = "this fork: multiplayer and the dedicated server, "
                  "the launcher, custom maps, the Android head and the pro HUD";
            inline static constexpr std::string_view SupportUrl = "https://ko-fi.com/livetek";

            [[nodiscard]] static std::string Summary();
            [[nodiscard]] static std::string Compact();
            [[nodiscard]] static std::string Names();
            [[nodiscard]] static const std::vector<Entry>& Entries();
            static void Print();

            Credits() = delete;
            Credits(const Credits&) = delete;
            Credits& operator=(const Credits&) = delete;
        };
    }
}
