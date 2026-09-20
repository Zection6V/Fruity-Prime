#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace MphRead
{
    enum class Hunter : std::uint8_t;
}

namespace MphRead::Mods
{
    enum class WindowStartMode : std::int32_t;
}

namespace MphRead::Mods::Launcher
{
    class LauncherPrefs final
    {
    public:
        LauncherPrefs() = delete;
        inline static constexpr std::string_view DefaultServer = "89.160.162.50";

        [[nodiscard]] static std::string Directory();
        static void Directory(std::string value);

        [[nodiscard]] static std::string ServerAddress();
        static void ServerAddress(std::string value);

        [[nodiscard]] static std::int32_t ServerPort() noexcept;
        static void ServerPort(std::int32_t value) noexcept;

        [[nodiscard]] static std::string MasterHost();
        static void MasterHost(std::string value);

        [[nodiscard]] static std::int32_t MasterPort() noexcept;
        static void MasterPort(std::int32_t value) noexcept;

        [[nodiscard]] static std::int32_t LastRole() noexcept;
        static void LastRole(std::int32_t value) noexcept;

        [[nodiscard]] static std::string PlayerName();
        static void PlayerName(std::string value);

        [[nodiscard]] static MphRead::Hunter LastHunter() noexcept;
        static void LastHunter(MphRead::Hunter value) noexcept;

        [[nodiscard]] static std::int32_t LastColor() noexcept;
        static void LastColor(std::int32_t value) noexcept;

        [[nodiscard]] static std::int32_t Bots() noexcept;
        static void Bots(std::int32_t value) noexcept;

        [[nodiscard]] static std::int32_t BotLevel() noexcept;
        static void BotLevel(std::int32_t value) noexcept;

        [[nodiscard]] static std::int32_t HostPort() noexcept;
        static void HostPort(std::int32_t value) noexcept;

        [[nodiscard]] static bool ListHostedGame() noexcept;
        static void ListHostedGame(bool value) noexcept;

        [[nodiscard]] static bool HostOnMaster() noexcept;
        static void HostOnMaster(bool value) noexcept;

        [[nodiscard]] static std::int32_t LastKind() noexcept;
        static void LastKind(std::int32_t value) noexcept;

        [[nodiscard]] static bool AutoUpdate() noexcept;
        static void AutoUpdate(bool value) noexcept;

        [[nodiscard]] static MphRead::Mods::WindowStartMode WindowMode() noexcept;
        static void WindowMode(MphRead::Mods::WindowStartMode value) noexcept;

        [[nodiscard]] static bool DebugLogs() noexcept;
        static void DebugLogs(bool value) noexcept;

        static void Load();
        static void Save();

    private:
        [[nodiscard]] static std::string Path();

        static std::string _directory;
        static std::string _serverAddress;
        static std::int32_t _serverPort;
        static std::string _masterHost;
        static std::int32_t _masterPort;
        static std::int32_t _lastRole;
        static std::string _playerName;
        static MphRead::Hunter _lastHunter;
        static std::int32_t _lastColor;
        static std::int32_t _bots;
        static std::int32_t _botLevel;
        static std::int32_t _hostPort;
        static bool _listHostedGame;
        static bool _hostOnMaster;
        static std::int32_t _lastKind;
        static bool _autoUpdate;
        static MphRead::Mods::WindowStartMode _windowMode;
        static bool _debugLogs;
    };
}
