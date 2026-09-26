#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace MphRead::Mods::Input
{
    class GamepadRuntimeConfig;
    struct GamepadDeviceSnapshot;

    struct GamepadProfile
    {
        std::int32_t Version = 0;
        std::string Name{};
        // null in the C# where a file carried none.
        std::shared_ptr<std::vector<std::string>> Settings{};
    };

    struct GamepadProfileLibrary
    {
        std::vector<GamepadProfile> Profiles{};
        std::map<std::string, std::string> Assignments{};
    };

    class GamepadProfiles final
    {
    public:
        GamepadProfiles() = delete;

        [[nodiscard]] static std::int64_t Revision() noexcept { return _revision; }
        [[nodiscard]] static const std::string& Status() noexcept { return _status; }
        [[nodiscard]] static const std::string& ActiveName() noexcept { return _activeName; }
        static void NoteActive(const std::string& name) { _activeName = name; }
        [[nodiscard]] static const std::vector<GamepadProfile>& Profiles() noexcept { return _library.Profiles; }

        static void Initialize();
        [[nodiscard]] static GamepadProfile Capture(const std::string& name);
        static void Apply(const GamepadProfile& profile);
        static void Save(const std::string& name);
        static void Load(const std::string& name);
        static void Export(const std::string& name, const std::string& path);
        [[nodiscard]] static std::string Import(const std::string& path);
        static void Assign(const std::string& name, const GamepadDeviceSnapshot& device);
        static void Unassign(const GamepadDeviceSnapshot& device);
        [[nodiscard]] static std::string DeviceKey(const std::string& id);
        [[nodiscard]] static std::shared_ptr<GamepadRuntimeConfig> Resolve(const std::string& key);
        static void WriteAtomic(const std::string& path, const std::string& text);

    private:
        [[nodiscard]] static std::string LibraryPath();
        [[nodiscard]] static std::shared_ptr<GamepadRuntimeConfig> BuildRuntime(const GamepadProfile& profile);
        static void Validate(const GamepadProfile& profile);
        [[nodiscard]] static const GamepadProfile& Find(const std::string& name);
        static void Store(const GamepadProfile& profile);
        static void Commit(GamepadProfileLibrary library);

        inline static GamepadProfileLibrary _library{};
        inline static std::string _directory{};
        inline static std::int64_t _revision = 0;
        inline static std::string _status{};
        inline static std::string _activeName = "Custom settings";
    };
}
