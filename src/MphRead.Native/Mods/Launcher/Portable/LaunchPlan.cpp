#include "LaunchPlan.hpp"

#include <random>
#include <utility>

namespace
{
    constexpr MphRead::Hunter RandomHunter = static_cast<MphRead::Hunter>(8);

    MphRead::Hunter RollPlayableHunter()
    {
        static std::random_device device;
        static std::mt19937 engine(device());
        static std::uniform_int_distribution<int> distribution(0, MphRead::Mods::Launcher::Hunters::Playable - 1);
        return static_cast<MphRead::Hunter>(distribution(engine));
    }
}

namespace MphRead::Mods::Launcher
{
    MphRead::Hunter Hunters::_rolled = RandomHunter;

    MphRead::Hunter Hunters::Resolve(MphRead::Hunter hunter)
    {
        if (hunter != RandomHunter)
        {
            return hunter;
        }
        if (_rolled == RandomHunter)
        {
            _rolled = RollPlayableHunter();
        }
        return _rolled;
    }

    void Hunters::Reroll() noexcept
    {
        _rolled = RandomHunter;
    }

    LaunchPlan::LaunchPlan(Init init)
        : _kind(init.Kind),
          _hunter(Hunters::Resolve(init.Hunter)),
          _roomKey(std::move(init.RoomKey)),
          _mode(init.Mode),
          _bots(init.Bots),
          _botLevel(init.BotLevel),
          _port(init.Port),
          _playerName(std::move(init.PlayerName)),
          _saveSlot(init.SaveSlot),
          _newGame(init.NewGame),
          _demoPath(std::move(init.DemoPath))
    {
    }

    LaunchKind LaunchPlan::Kind() const noexcept
    {
        return _kind;
    }

    MphRead::Hunter LaunchPlan::Hunter() const noexcept
    {
        return _hunter;
    }

    const std::optional<std::string>& LaunchPlan::RoomKey() const noexcept
    {
        return _roomKey;
    }

    MphRead::GameMode LaunchPlan::Mode() const noexcept
    {
        return _mode;
    }

    std::int32_t LaunchPlan::Bots() const noexcept
    {
        return _bots;
    }

    std::int32_t LaunchPlan::BotLevel() const noexcept
    {
        return _botLevel;
    }

    std::int32_t LaunchPlan::Port() const noexcept
    {
        return _port;
    }

    const std::optional<std::string>& LaunchPlan::PlayerName() const noexcept
    {
        return _playerName;
    }

    std::uint8_t LaunchPlan::SaveSlot() const noexcept
    {
        return _saveSlot;
    }

    bool LaunchPlan::NewGame() const noexcept
    {
        return _newGame;
    }

    const std::optional<std::string>& LaunchPlan::DemoPath() const noexcept
    {
        return _demoPath;
    }
}
