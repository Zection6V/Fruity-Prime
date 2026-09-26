#include "GamepadManager.hpp"

#include "GamepadAnalog.hpp"
#include "GamepadCalibration.hpp"
#include "GamepadGlyphs.hpp"
#include "GamepadOptionState.hpp"
#include "GamepadProfiles.hpp"
#include "GamepadRuntimeConfig.hpp"
#include "InputSourceTracker.hpp"
#include "../../NativeRuntime/System/Enum.hpp"

#include <algorithm>
#include <cmath>

namespace MphRead::Mods::Input
{
    namespace
    {
        constexpr ::MphRead::NativeRuntime::EnumNameEntry FamilyNames[] = {
            {0, "Unknown"}, {1, "Xbox"}, {2, "PlayStation"}, {3, "Nintendo"}, {4, "Generic"}};
        constexpr ::MphRead::NativeRuntime::EnumNameEntry CapabilityNames[] = {
            {0, "None"}, {1, "Rumble"}, {2, "Gyro"}, {4, "Touchpad"}, {8, "AnalogTriggers"},
            {16, "AnalogLeftStick"}, {32, "AnalogRightStick"}};
    }

    std::string ToString(GamepadFamily value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(value, FamilyNames, std::size(FamilyNames), false);
    }

    bool TryParse(std::string_view text, GamepadFamily& value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumTryParse(text, false, FamilyNames, std::size(FamilyNames), value);
    }

    std::string ToString(GamepadCapabilities value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(value, CapabilityNames, std::size(CapabilityNames), true);
    }

    GamepadDevice::GamepadDevice() : Runtime(std::make_shared<GamepadRuntimeConfig>())
    {
    }

    GamepadDeviceSnapshot GamepadDevice::Snapshot() const
    {
        GamepadDeviceSnapshot snapshot{};
        snapshot.DeviceId = DeviceId;
        snapshot.Name = Name;
        snapshot.ProfileKey = ProfileKey;
        snapshot.Family = Family;
        snapshot.Capabilities = Capabilities;
        snapshot.IsMapped = IsMapped;
        snapshot.Mapping = Mapping;
        snapshot.State = State;
        snapshot.RawState = RawState;
        snapshot.Revision = Revision;
        return snapshot;
    }

    std::recursive_mutex GamepadManager::Gate{};
    std::vector<std::shared_ptr<GamepadDevice>> GamepadManager::Known{};
    GamepadDevice* GamepadManager::_active = nullptr;
    std::optional<std::string> GamepadManager::_selected{};
    bool GamepadManager::_used = false;
    std::int64_t GamepadManager::_revision = 0;
    GamepadSnapshot GamepadManager::_snapshot{};
    std::optional<std::string> GamepadManager::_lastInputDevice{};
    ::MphRead::NativeRuntime::Event<const GamepadDeviceSnapshot&> GamepadManager::DeviceAdded{};
    ::MphRead::NativeRuntime::Event<const GamepadDeviceSnapshot&> GamepadManager::DeviceRemoved{};
    ::MphRead::NativeRuntime::Event<> GamepadManager::ActiveChanged{};

    GamepadSnapshot GamepadManager::Snapshot()
    {
        const std::lock_guard<std::recursive_mutex> guard(Gate);
        return _snapshot;
    }

    GamepadState GamepadManager::ActiveState()
    {
        return Snapshot().State;
    }

    std::optional<GamepadDeviceSnapshot> GamepadManager::ActiveDevice()
    {
        const std::lock_guard<std::recursive_mutex> guard(Gate);
        return _active == nullptr ? std::nullopt : std::optional<GamepadDeviceSnapshot>(_active->Snapshot());
    }

    std::optional<std::string> GamepadManager::SelectedDeviceId()
    {
        const std::lock_guard<std::recursive_mutex> guard(Gate);
        return _selected;
    }

    std::vector<GamepadDeviceSnapshot> GamepadManager::Devices()
    {
        const std::lock_guard<std::recursive_mutex> guard(Gate);
        std::vector<GamepadDeviceSnapshot> devices;
        for (const std::shared_ptr<GamepadDevice>& device : Known)
        {
            devices.push_back(device->Snapshot());
        }
        return devices;
    }

    GamepadDevice* GamepadManager::Find(const std::string& id)
    {
        for (const std::shared_ptr<GamepadDevice>& device : Known)
        {
            if (device->DeviceId == id)
            {
                return device.get();
            }
        }
        return nullptr;
    }

    void GamepadManager::Activate(GamepadDevice* device)
    {
        if (_active == device)
        {
            return;
        }
        _active = device;
        _revision++;
        Publish();
    }

    void GamepadManager::Publish()
    {
        GamepadRuntimeConfig::Current(_active != nullptr ? _active->Runtime : GamepadRuntimeConfig::Fallback());
        GamepadProfiles::NoteActive(GamepadRuntimeConfig::Current()->ProfileName());
        _snapshot = GamepadSnapshot{
            _active != nullptr ? std::optional<std::string>(_active->DeviceId) : std::nullopt,
            _active != nullptr ? _active->State : GamepadState{},
            _revision,
            _active != nullptr ? _active->Runtime : GamepadRuntimeConfig::Fallback()};
    }

    void GamepadManager::UpdateDevice(const std::string& id, GamepadState state, bool mapped,
        GamepadFamily family, GamepadCapabilities capabilities, const std::optional<std::string>& mapping)
    {
        std::optional<GamepadDeviceSnapshot> notification;
        bool activeChanged = false;
        {
            const std::lock_guard<std::recursive_mutex> guard(Gate);
            const std::int64_t oldRevision = _revision;
            GamepadDevice* device = Find(id);
            const bool added = device == nullptr;
            if (device == nullptr)
            {
                auto created = std::make_shared<GamepadDevice>();
                created->DeviceId = id;
                created->ProfileKey = GamepadProfiles::DeviceKey(id);
                created->Runtime = GamepadProfiles::Resolve(created->ProfileKey);
                Known.push_back(created);
                device = created.get();
            }
            const GamepadState previous = device->State;
            const std::shared_ptr<GamepadOptionState> options = device->Runtime->Options();
            state.LeftX = GamepadAnalog::Finite(state.LeftX);
            state.LeftY = GamepadAnalog::Finite(state.LeftY);
            state.RightX = GamepadAnalog::Finite(state.RightX);
            state.RightY = GamepadAnalog::Finite(state.RightY);
            state.LeftTrigger = GamepadAnalog::Finite(state.LeftTrigger, 0, 1);
            state.RightTrigger = GamepadAnalog::Finite(state.RightTrigger, 0, 1);
            state.Connected = true;
            device->RawState = state;
            std::tie(state.LeftX, state.LeftY) = options->LeftCalibration.Normalize(state.LeftX, state.LeftY);
            std::tie(state.RightX, state.RightY) = options->RightCalibration.Normalize(state.RightX, state.RightY);
            state.LeftTrigger = GamepadCalibration::Trigger(state.LeftTrigger, options->LeftTriggerMin, options->LeftTriggerMax);
            state.RightTrigger = GamepadCalibration::Trigger(state.RightTrigger, options->RightTriggerMin, options->RightTriggerMax);
            device->LeftTriggerHeld = GamepadAnalog::Trigger(state.LeftTrigger, device->LeftTriggerHeld, options->TriggerThreshold);
            device->RightTriggerHeld = GamepadAnalog::Trigger(state.RightTrigger, device->RightTriggerHeld, options->TriggerThreshold);
            if (device->LeftTriggerHeld)
            {
                state.Buttons |= GamepadButtons::LeftTrigger;
            }
            if (device->RightTriggerHeld)
            {
                state.Buttons |= GamepadButtons::RightTrigger;
            }
            state.Connected = true;
            device->State = state;
            const std::string name = state.Name.value_or("gamepad");
            if (added || device->Name != name || family != GamepadFamily::Unknown)
            {
                device->Family = family == GamepadFamily::Unknown ? GamepadGlyphs::Detect(name) : family;
            }
            device->Name = name;
            device->IsMapped = mapped;
            device->Mapping = mapping.has_value() ? *mapping : mapped ? "Platform mapping" : "Unmapped fallback";
            device->Capabilities = capabilities;
            const bool activity = Any(state.Buttons & ~previous.Buttons)
                || StickActivity(state.LeftX, state.LeftY, previous.LeftX, previous.LeftY, options->ActivityThreshold)
                || StickActivity(state.RightX, state.RightY, previous.RightX, previous.RightY, options->ActivityThreshold);
            if (_selected == id || (!_selected.has_value() && (_active == nullptr
                || (!_used && mapped && !_active->IsMapped))))
            {
                Activate(device);
            }
            if (activity)
            {
                _lastInputDevice = id;
                if (!_selected.has_value() || _selected == id)
                {
                    _used = true;
                    Activate(device);
                    InputSourceTracker::Note(InputSource::Gamepad);
                }
            }
            Publish();
            device->Revision++;
            if (added)
            {
                notification = device->Snapshot();
            }
            activeChanged = oldRevision != _revision;
        }
        if (activeChanged)
        {
            ActiveChanged.Invoke();
        }
        if (notification.has_value())
        {
            DeviceAdded.Invoke(*notification);
        }
    }

    void GamepadManager::ReplaceRuntime(std::shared_ptr<GamepadRuntimeConfig> runtime)
    {
        const std::lock_guard<std::recursive_mutex> guard(Gate);
        if (_active != nullptr)
        {
            _active->Runtime = std::move(runtime);
        }
        else
        {
            GamepadRuntimeConfig::Fallback() = std::move(runtime);
        }
        _revision++;
        Publish();
    }

    void GamepadManager::RefreshProfiles()
    {
        const std::lock_guard<std::recursive_mutex> guard(Gate);
        for (const std::shared_ptr<GamepadDevice>& device : Known)
        {
            device->Runtime = GamepadProfiles::Resolve(device->ProfileKey);
        }
        _revision++;
        Publish();
    }

    bool GamepadManager::StickActivity(float x, float y, float oldX, float oldY, float threshold)
    {
        return x * x + y * y > threshold * threshold
            && (std::abs(x - oldX) > 0.08F || std::abs(y - oldY) > 0.08F);
    }

    void GamepadManager::SelectDevice(const std::optional<std::string>& id)
    {
        bool changed = false;
        {
            const std::lock_guard<std::recursive_mutex> guard(Gate);
            const std::int64_t previous = _revision;
            _selected = !id.has_value() || id->empty() ? std::nullopt : id;
            _used = false;
            if (_selected.has_value())
            {
                Activate(Find(*_selected));
            }
            else if (_active == nullptr)
            {
                GamepadDevice* mappedDevice = nullptr;
                for (const std::shared_ptr<GamepadDevice>& device : Known)
                {
                    if (device->IsMapped)
                    {
                        mappedDevice = device.get();
                        break;
                    }
                }
                Activate(mappedDevice != nullptr ? mappedDevice : Known.empty() ? nullptr : Known.front().get());
            }
            changed = previous != _revision;
        }
        if (changed)
        {
            ActiveChanged.Invoke();
        }
    }

    void GamepadManager::ClearDevice(const std::string& id)
    {
        const std::lock_guard<std::recursive_mutex> guard(Gate);
        GamepadDevice* device = Find(id);
        if (device == nullptr)
        {
            return;
        }
        GamepadState cleared{};
        cleared.Connected = true;
        cleared.Name = device->Name;
        device->State = cleared;
        device->RawState = device->State;
        device->LeftTriggerHeld = device->RightTriggerHeld = false;
        if (_active == device)
        {
            _revision++;
            Publish();
        }
    }

    void GamepadManager::RemoveDevice(const std::string& id)
    {
        GamepadDeviceSnapshot removed{};
        bool changed = false;
        std::shared_ptr<GamepadDevice> keep;
        {
            const std::lock_guard<std::recursive_mutex> guard(Gate);
            GamepadDevice* device = Find(id);
            if (device == nullptr)
            {
                return;
            }
            removed = device->Snapshot();
            changed = _active == device;
            device->State = {};
            const auto found = std::find_if(Known.begin(), Known.end(),
                [device](const std::shared_ptr<GamepadDevice>& known) { return known.get() == device; });
            keep = *found;
            Known.erase(found);
            if (_selected == id)
            {
                _selected = std::nullopt;
            }
            if (_active == device)
            {
                _used = false;
                Activate(nullptr);
            }
        }
        if (changed)
        {
            ActiveChanged.Invoke();
        }
        DeviceRemoved.Invoke(removed);
    }

    void GamepadManager::ClearAll()
    {
        const std::lock_guard<std::recursive_mutex> guard(Gate);
        for (const std::shared_ptr<GamepadDevice>& device : Known)
        {
            ClearDevice(device->DeviceId);
        }
    }
}
