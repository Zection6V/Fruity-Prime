#include "ControllerRuntimeChecks.hpp"

#include "AimInputSourceTracker.hpp"
#include "ControllerLayoutState.hpp"
#include "GamepadChecks.hpp"
#include "GamepadHaptics.hpp"
#include "GamepadInput.hpp"
#include "GamepadManager.hpp"
#include "GamepadMappings.hpp"
#include "GamepadOptions.hpp"
#include "GamepadProfiles.hpp"
#include "GamepadRuntimeConfig.hpp"
#include "GamepadUiRouter.hpp"
#include "HapticScheduler.hpp"
#include "InputPrompt.hpp"
#include "InputSourceTracker.hpp"
#include "PadBindings.hpp"
#include "../Launcher/Portable/LauncherPrefs.hpp"
#include "../../NativeRuntime/System/Exceptions.hpp"
#include "../../NativeRuntime/System/Guid.hpp"
#include "../../NativeRuntime/System/IO.hpp"

#include <chrono>
#include <future>
#include <limits>

namespace MphRead::Mods::Input
{
    namespace Runtime = ::MphRead::NativeRuntime;

    void ControllerRuntimeChecks::Run()
    {
        const auto check = [](bool ok, const std::string& message) { GamepadChecks::Check(ok, "controller runtime: " + message); };
        const auto removeAll = []()
        {
            for (const GamepadDeviceSnapshot& device : GamepadManager::Devices())
            {
                GamepadManager::RemoveDevice(device.DeviceId);
            }
        };
        const auto find = [](const std::string& id)
        {
            for (const GamepadDeviceSnapshot& device : GamepadManager::Devices())
            {
                if (device.DeviceId == id)
                {
                    return device;
                }
            }
            throw System::InvalidOperationException("Sequence contains no matching element");
        };
        removeAll();
        GamepadOptions::Reset();
        PadBindings::Reset();
        check((GamepadMappings::ParseCapabilities("guid,name,leftx:a0,lefty:a1,lefttrigger:b6,righttrigger:b7,")
            & GamepadCapabilities::AnalogTriggers) == GamepadCapabilities::None, "mapped digital triggers are not advertised as analog");
        GamepadManager::UpdateDevice("runtime-a", {}, true);
        GamepadManager::SelectDevice("runtime-a");
        GamepadOptions::LeftTriggerMin(.3F);
        GamepadOptions::LeftCalibration(StickCalibration{.1F, 0, -1, 1, -1, 1});
        GamepadManager::UpdateDevice("runtime-b", {}, true);
        GamepadManager::SelectDevice("runtime-b");
        GamepadOptions::LeftTriggerMin(.1F);
        GamepadState input{};
        input.LeftTrigger = .3F;
        input.LeftX = .1F;
        GamepadManager::UpdateDevice("runtime-a", input, true);
        GamepadManager::UpdateDevice("runtime-b", input, true);
        const GamepadDeviceSnapshot a = find("runtime-a");
        const GamepadDeviceSnapshot b = find("runtime-b");
        check(a.State.LeftTrigger == 0 && b.State.LeftTrigger > .2F && a.State.LeftX == 0 && b.State.LeftX == .1F,
            "simultaneous devices process their own calibration before selection");
        GamepadManager::SelectDevice("runtime-a");
        check(GamepadOptions::LeftTriggerMin() == .3F && GamepadManager::ActiveState().LeftX == 0, "switch publishes correct runtime immediately");
        GamepadInput::BeginFrame();
        std::async(std::launch::async, []() { GamepadManager::SelectDevice("runtime-b"); }).get();
        check(GamepadOptions::LeftTriggerMin() == .3F, "input frame retains its runtime during a concurrent device switch");
        GamepadInput::BeginFrame();
        check(GamepadOptions::LeftTriggerMin() == .1F, "next input frame adopts the newly selected runtime");
        GamepadManager::SelectDevice("runtime-a");
        GamepadState moved{};
        moved.LeftX = .6F;
        GamepadManager::UpdateDevice("runtime-a", moved, true);
        check(a.State.LeftX == 0, "published snapshot is immutable after device updates");
        bool reentered = false;
        const std::int64_t token = GamepadManager::ActiveChanged.Add([&reentered]()
        {
            auto task = std::async(std::launch::async, []() { return GamepadManager::Devices().size(); });
            reentered = task.wait_for(std::chrono::milliseconds(1000)) == std::future_status::ready;
        });
        GamepadManager::SelectDevice("runtime-b");
        GamepadManager::ActiveChanged.Remove(token);
        check(reentered, "events dispatch outside manager lock");
        GamepadManager::SelectDevice("runtime-a");
        InputSourceTracker::Note(InputSource::KeyboardMouse, std::numeric_limits<std::int64_t>::max() / 4);
        GamepadState pressed{};
        pressed.Buttons = GamepadButtons::A;
        GamepadManager::UpdateDevice("runtime-b", pressed, true);
        check(GamepadManager::Snapshot().DeviceId == "runtime-a" && InputSourceTracker::Current() == InputSource::KeyboardMouse,
            "inactive device cannot override explicit selection or prompt source");
        removeAll();
        GamepadOptions::Reset();
        PadBindings::Reset();
        const GamepadProfile saved = GamepadProfiles::Capture("before");
        for (const GamepadProfile& bad : {
            GamepadProfile{2, "future", std::make_shared<std::vector<std::string>>(std::vector<std::string>{"gamepad_look_x=1"})},
            GamepadProfile{1, "bad", std::make_shared<std::vector<std::string>>(std::vector<std::string>{"gamepad_look_x=NaN"})},
            GamepadProfile{1, "bad", std::make_shared<std::vector<std::string>>(std::vector<std::string>{"pad_Jump_primary=A, B"})}})
        {
            bool rejected = false;
            try
            {
                GamepadProfiles::Apply(bad);
            }
            catch (const System::IO::InvalidDataException&)
            {
                rejected = true;
            }
            check(rejected && *GamepadProfiles::Capture("after").Settings == *saved.Settings, "invalid profile has no partial application");
        }
        const std::string prior = Launcher::LauncherPrefs::Directory();
        const std::string temp = Runtime::PathCombine(Runtime::PathGetTempPath(),
            "controller-library-" + Runtime::Guid::NewGuid().ToString("N"));
        Runtime::DirectoryCreateDirectory(temp);
        const auto cleanup = [&]()
        {
            Launcher::LauncherPrefs::Directory(prior);
            GamepadProfiles::Initialize();
            Runtime::FileDelete(Runtime::PathCombine(temp, "controller-profiles.json"));
            Runtime::DirectoryDelete(temp);
        };
        try
        {
            const std::string path = Runtime::PathCombine(temp, "controller-profiles.json");
            Runtime::FileWriteAllText(path, "{ broken");
            Launcher::LauncherPrefs::Directory(temp);
            GamepadProfiles::Initialize();
            check(GamepadProfiles::Profiles().empty() && !GamepadProfiles::Status().empty() && Runtime::FileReadAllText(path) == "{ broken",
                "corrupt library preserved and reported without crashing");
        }
        catch (...)
        {
            cleanup();
            throw;
        }
        cleanup();
        HapticScheduler scheduler{};
        check(scheduler.Accept(GamepadFeedback::Fire, 0, 45) && scheduler.Accept(GamepadFeedback::Damage, 1, 100)
            && !scheduler.Accept(GamepadFeedback::Fire, 80, 45) && scheduler.Accept(GamepadFeedback::Fire, 102, 45),
            "damage preempts fire and retains its full duration");
        const std::string guid = "030000005e040000130b000099090000";
        const std::string first = guid + ",One,a:b0,platform:Windows,";
        const std::string second = guid + ",Two,a:b1,platform:Windows,";
        const std::string other = guid + ",Mac,a:b2,platform:Mac OS X,";
        const std::string merged = GamepadMappings::ReplaceOverride("# comment\n" + first + "\n" + other + "\n" + first, second);
        std::size_t seconds = 0;
        for (std::size_t at = merged.find(second); at != std::string::npos; at = merged.find(second, at + second.size()))
        {
            seconds++;
        }
        check(merged.find(first) == std::string::npos && merged.find(other) != std::string::npos
            && merged.find("# comment") != std::string::npos && seconds == 1,
            "mapping replacement deduplicates only matching GUID/platform");
        AimInputSourceTracker::Reset();
        InputSourceTracker::Reset();
        GamepadManager::UpdateDevice("ui-triggers", {}, true, GamepadFamily::Unknown, GamepadCapabilities::AnalogTriggers);
        GamepadManager::SelectDevice("ui-triggers");
        GamepadOptions::TriggerThreshold(.95F);
        GamepadUiRouter router{};
        std::int32_t pages = 0;
        static_cast<void>(router.Action.Add([&pages](UiAction action)
        {
            if (action == UiAction::PageDown)
            {
                pages++;
            }
        }));
        const auto trigger = [&router](float value, std::int64_t now)
        {
            GamepadState state{};
            state.RightTrigger = value;
            GamepadManager::UpdateDevice("ui-triggers", state, true, GamepadFamily::Unknown, GamepadCapabilities::AnalogTriggers);
            router.Update(GamepadManager::Snapshot(), GamepadContext::Menu, now);
        };
        trigger(0, 0);
        trigger(.5F, 1);
        trigger(.4F, 2);
        trigger(.5F, 3);
        check(pages == 1 && !GamepadManager::ActiveState().Down(GamepadButtons::RightTrigger),
            "menu threshold independent of gameplay and retains hysteresis");
        trigger(.2F, 4);
        trigger(.5F, 5);
        check(pages == 2, "menu trigger rearms below release threshold");
        GamepadManager::RemoveDevice("ui-triggers");
        const std::shared_ptr<ControllerLayoutState> layout = GamepadRuntimeConfig::Current()->Layout();
        layout->Apply("Bumper Jumper");
        GamepadOptions::LookX(2);
        GamepadOptions::ScopedX(.7F);
        GamepadOptions::LeftInner(.1F);
        check(layout->Name() == "Bumper Jumper", "sensitivity and calibration preserve control layout identity");
        PadBindings::SetSlot(PadAction::Jump, 0, GamepadButtons::Y, GamepadButtons::LeftBumper);
        check(InputPrompt::For(PadAction::Jump).Button == GamepadButtons::Y && InputPrompt::For(PadAction::Jump).Modifier == GamepadButtons::LeftBumper,
            "semantic prompts follow rebound combinations");
        GamepadOptions::Reset();
        PadBindings::Reset();
    }
}
