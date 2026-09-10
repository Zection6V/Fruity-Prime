#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace MphRead
{
    enum class Hunter : std::uint8_t;
    enum class GameMode : std::uint8_t;
}

namespace MphRead::Mods::Launcher
{
    class Hunters final
    {
    public:
        static constexpr int Playable = 7;

        Hunters() = delete;

        [[nodiscard]] static MphRead::Hunter Resolve(MphRead::Hunter hunter);
        static void Reroll() noexcept;

    private:
        static MphRead::Hunter _rolled;
    };

    enum class LaunchKind : std::int32_t
    {
        None = 0,
        Online = 1,
        Offline = 2,
        Host = 3,
        Adventure = 4,
        Demo = 5
    };

    class LaunchPlan final
    {
    public:
        struct Init
        {
            LaunchKind Kind = LaunchKind::None;
            MphRead::Hunter Hunter = static_cast<MphRead::Hunter>(0);
            std::optional<std::string> RoomKey{};
            MphRead::GameMode Mode = static_cast<MphRead::GameMode>(0);
            std::int32_t Bots = 0;
            std::int32_t BotLevel = 0;
            std::int32_t Port = 0;
            std::optional<std::string> PlayerName{};
            std::uint8_t SaveSlot = 0;
            bool NewGame = false;
            std::optional<std::string> DemoPath{};
        };

        LaunchPlan() = default;
        explicit LaunchPlan(Init init);

        [[nodiscard]] LaunchKind Kind() const noexcept;
        [[nodiscard]] MphRead::Hunter Hunter() const noexcept;
        [[nodiscard]] const std::optional<std::string>& RoomKey() const noexcept;
        [[nodiscard]] MphRead::GameMode Mode() const noexcept;
        [[nodiscard]] std::int32_t Bots() const noexcept;
        [[nodiscard]] std::int32_t BotLevel() const noexcept;
        [[nodiscard]] std::int32_t Port() const noexcept;
        [[nodiscard]] const std::optional<std::string>& PlayerName() const noexcept;
        [[nodiscard]] std::uint8_t SaveSlot() const noexcept;
        [[nodiscard]] bool NewGame() const noexcept;
        [[nodiscard]] const std::optional<std::string>& DemoPath() const noexcept;

    private:
        LaunchKind _kind = LaunchKind::None;
        MphRead::Hunter _hunter = static_cast<MphRead::Hunter>(0);
        std::optional<std::string> _roomKey{};
        MphRead::GameMode _mode = static_cast<MphRead::GameMode>(0);
        std::int32_t _bots = 0;
        std::int32_t _botLevel = 0;
        std::int32_t _port = 0;
        std::optional<std::string> _playerName{};
        std::uint8_t _saveSlot = 0;
        bool _newGame = false;
        std::optional<std::string> _demoPath{};
    };
}
