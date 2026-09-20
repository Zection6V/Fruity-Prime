#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::Mods::Input
{
    class GamepadMappings final
    {
    public:
        GamepadMappings() = delete;
        GamepadMappings(const GamepadMappings&) = delete;
        GamepadMappings& operator=(const GamepadMappings&) = delete;

        inline static constexpr std::string_view FileName = "gamecontrollerdb.txt";

        [[nodiscard]] static std::string Summary();
        static void EnsureLoaded();
        [[nodiscard]] static std::string Suggest(std::int32_t slot);
        [[nodiscard]] static std::string Platform();

    private:
        [[nodiscard]] static std::vector<std::string> Paths();
        [[nodiscard]] static std::optional<std::string> TryRead(const std::string& path);
        [[nodiscard]] static bool Apply(const std::string& text);
        [[nodiscard]] static std::int32_t Count(std::string_view text);

        static std::atomic_bool _loaded;
        static std::string _summary;
    };
}
