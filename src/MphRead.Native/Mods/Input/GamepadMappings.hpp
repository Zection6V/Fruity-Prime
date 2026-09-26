#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::Mods::Input
{
    enum class GamepadCapabilities : std::int32_t;
    struct GamepadLayout;

    class GamepadMappings final
    {
    public:
        GamepadMappings() = delete;

        inline static constexpr std::string_view FileName = "gamecontrollerdb.txt";
        inline static bool ReloadRequested = false;

        [[nodiscard]] static GamepadCapabilities Capabilities(const std::string& guid, GamepadCapabilities fallback);
        [[nodiscard]] static GamepadCapabilities ParseCapabilities(const std::string& line);
        static void SaveOverride(const std::string& mapping);
        [[nodiscard]] static std::string ReplaceOverride(const std::string& existing, const std::string& mapping);
        static void ResetOverrides();
        [[nodiscard]] static const std::string& Summary() noexcept { return _summary; }
        static void EnsureLoaded();
        [[nodiscard]] static std::vector<std::string> Paths(const std::optional<std::string>& resourceDirectory = std::nullopt,
            const std::optional<std::string>& settingsDirectory = std::nullopt);
        [[nodiscard]] static std::string Suggest(std::int32_t slot);
        [[nodiscard]] static std::optional<std::string> CompatibleMacXboxMapping(const std::string& guid,
            const std::string& name, std::int32_t axes, std::int32_t buttons, std::int32_t hats, bool macOS);
        [[nodiscard]] static bool TryMapMacXbox(std::int32_t slot);
        [[nodiscard]] static std::string Platform();

    private:
        [[nodiscard]] static std::optional<std::string> TryRead(const std::string& path);
        [[nodiscard]] static bool Apply(const std::string& text);
        [[nodiscard]] static std::int32_t Count(const std::string& text);
        [[nodiscard]] static std::string DescribeLayout(const std::string& guid, const std::string& name,
            const GamepadLayout& layout, const std::string& platform);

        // StringComparer.OrdinalIgnoreCase: keyed on the upper-cased guid.
        inline static std::map<std::string, GamepadCapabilities> MappingCapabilities{};
        inline static bool _loaded = false;
        inline static std::string _summary = "no extra mappings loaded";
    };
}
