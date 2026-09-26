#pragma once

#include "GamepadDeviceSnapshot.hpp"
#include "GamepadState.hpp"
#include "../../NativeRuntime/System/Event.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::Mods::Input
{
    class GamepadRuntimeConfig;

    enum class GamepadFamily : std::int32_t { Unknown, Xbox, PlayStation, Nintendo, Generic };

    // GamepadFamily.ToString(), Enum.TryParse and Enum.IsDefined.
    [[nodiscard]] std::string ToString(GamepadFamily value);
    [[nodiscard]] bool TryParse(std::string_view text, GamepadFamily& value);
    [[nodiscard]] constexpr bool IsDefined(GamepadFamily value) noexcept
    {
        return static_cast<std::int32_t>(value) >= 0 && static_cast<std::int32_t>(value) <= 4;
    }

    // [Flags]
    enum class GamepadCapabilities : std::int32_t
    {
        None = 0, Rumble = 1, Gyro = 2, Touchpad = 4, AnalogTriggers = 8, AnalogLeftStick = 16, AnalogRightStick = 32
    };

    [[nodiscard]] constexpr GamepadCapabilities operator|(GamepadCapabilities left, GamepadCapabilities right) noexcept
    {
        return static_cast<GamepadCapabilities>(static_cast<std::int32_t>(left) | static_cast<std::int32_t>(right));
    }

    [[nodiscard]] constexpr GamepadCapabilities operator&(GamepadCapabilities left, GamepadCapabilities right) noexcept
    {
        return static_cast<GamepadCapabilities>(static_cast<std::int32_t>(left) & static_cast<std::int32_t>(right));
    }

    // GamepadCapabilities.ToString().
    [[nodiscard]] std::string ToString(GamepadCapabilities value);

    class GamepadDevice final
    {
    public:
        std::string DeviceId{};
        std::string Name{};
        std::string ProfileKey{};
        GamepadFamily Family = GamepadFamily::Unknown;
        GamepadCapabilities Capabilities = GamepadCapabilities::None;
        bool IsMapped = false;
        std::string Mapping{};
        GamepadState State{};
        GamepadState RawState{};
        bool LeftTriggerHeld = false;
        bool RightTriggerHeld = false;
        std::shared_ptr<GamepadRuntimeConfig> Runtime;
        std::int64_t Revision = 0;

        GamepadDevice();

        [[nodiscard]] GamepadDeviceSnapshot Snapshot() const;
    };

    struct GamepadSnapshot
    {
        std::optional<std::string> DeviceId{};
        GamepadState State{};
        std::int64_t Revision = 0;
        std::shared_ptr<GamepadRuntimeConfig> Runtime{};
    };

    class GamepadManager final
    {
    public:
        GamepadManager() = delete;

        [[nodiscard]] static GamepadSnapshot Snapshot();
        [[nodiscard]] static GamepadState ActiveState();
        [[nodiscard]] static std::optional<GamepadDeviceSnapshot> ActiveDevice();
        [[nodiscard]] static std::optional<std::string> SelectedDeviceId();
        [[nodiscard]] static const std::optional<std::string>& LastInputDevice() noexcept { return _lastInputDevice; }
        [[nodiscard]] static std::vector<GamepadDeviceSnapshot> Devices();

        static ::MphRead::NativeRuntime::Event<const GamepadDeviceSnapshot&> DeviceAdded;
        static ::MphRead::NativeRuntime::Event<const GamepadDeviceSnapshot&> DeviceRemoved;
        static ::MphRead::NativeRuntime::Event<> ActiveChanged;

        static void UpdateDevice(const std::string& id, GamepadState state, bool mapped,
            GamepadFamily family = GamepadFamily::Unknown,
            GamepadCapabilities capabilities = GamepadCapabilities::None,
            const std::optional<std::string>& mapping = std::nullopt);
        static void ReplaceRuntime(std::shared_ptr<GamepadRuntimeConfig> runtime);
        static void RefreshProfiles();
        static void SelectDevice(const std::optional<std::string>& id);
        static void ClearDevice(const std::string& id);
        static void RemoveDevice(const std::string& id);
        static void ClearAll();

    private:
        [[nodiscard]] static GamepadDevice* Find(const std::string& id);
        static void Activate(GamepadDevice* device);
        static void Publish();
        [[nodiscard]] static bool StickActivity(float x, float y, float oldX, float oldY, float threshold);

        static std::recursive_mutex Gate;
        static std::vector<std::shared_ptr<GamepadDevice>> Known;
        static GamepadDevice* _active;
        static std::optional<std::string> _selected;
        static bool _used;
        static std::int64_t _revision;
        static GamepadSnapshot _snapshot;
        static std::optional<std::string> _lastInputDevice;
    };
}
