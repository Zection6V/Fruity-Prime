#include "GamepadChecks.hpp"

#include "AimAssist/AimAssistChecks.hpp"
#include "ControllerRuntimeChecks.hpp"
#include "GamepadAnalog.hpp"
#include "GamepadEnhancementChecks.hpp"
#include "GamepadGlyphs.hpp"
#include "GamepadHaptics.hpp"
#include "GamepadInput.hpp"
#include "GamepadManager.hpp"
#include "GamepadMappings.hpp"
#include "GamepadOptions.hpp"
#include "GamepadPlatformChecks.hpp"
#include "GamepadProbe.hpp"
#include "GamepadUiRouter.hpp"
#include "InputSourceTracker.hpp"
#include "PadBindings.hpp"
#include "WeaponSelectionDirection.hpp"
#include "../InputSettings.hpp"
#include "../Launcher/Portable/LauncherPrefs.hpp"
#include "../../Entities/Players/PlayerInput.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/Exceptions.hpp"
#include "../../NativeRuntime/System/Guid.hpp"
#include "../../NativeRuntime/System/IO.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace MphRead::Mods::Input
{
    namespace Runtime = ::MphRead::NativeRuntime;

    namespace
    {
        constexpr float Pi = std::numbers::pi_v<float>;

        class FakeHaptics final : public IGamepadHaptics
        {
        public:
            float Low = 0;
            bool Stopped = false;

            void Rumble(float lowFrequency, float highFrequency, std::chrono::milliseconds duration) override
            {
                static_cast<void>(highFrequency);
                static_cast<void>(duration);
                Low = lowFrequency;
                Stopped = false;
            }

            void Stop() override
            {
                Stopped = true;
            }
        };

        [[nodiscard]] GamepadSnapshot Snap(const char* id, const GamepadState& state, std::int64_t revision)
        {
            return GamepadSnapshot{std::string(id), state, revision, nullptr};
        }

        void RemoveAll()
        {
            for (const GamepadDeviceSnapshot& device : GamepadManager::Devices())
            {
                GamepadManager::RemoveDevice(device.DeviceId);
            }
        }
    }

    void GamepadChecks::Check(bool condition, std::string_view message)
    {
        if (!condition)
        {
            throw System::InvalidOperationException(message);
        }
        _checks++;
    }

    void GamepadChecks::Near(float actual, float expected, std::string_view message)
    {
        Check(std::abs(actual - expected) < .0001F, message);
    }

    GamepadState GamepadChecks::State(GamepadButtons buttons, float x, float trigger)
    {
        GamepadState state{};
        state.Connected = true;
        state.Name = "test";
        state.Buttons = buttons;
        state.LeftX = x;
        state.RightTrigger = trigger;
        return state;
    }

    std::int32_t GamepadChecks::Run(const std::optional<std::string>& shots)
    {
        static_cast<void>(shots);
        std::int32_t result = 0;
        try
        {
            GamepadPlatformChecks::Run();
            const std::string install = Runtime::PathCombine(std::vector<std::string>{Runtime::PathGetTempPath(), "mapping fixture",
                "Fruity Prime.app", "Contents", "MacOS"});
            const std::string resources = Runtime::PathCombine(Runtime::PathGetDirectoryName(install).value(), "Resources");
            const std::string settings = Runtime::PathCombine(Runtime::PathGetTempPath(), "mapping user settings");
            const std::string fileName(GamepadMappings::FileName);
            std::vector<std::string> mappingPaths = GamepadMappings::Paths(resources, settings);
            Check(mappingPaths.size() == 2
                && mappingPaths[0] == Runtime::PathCombine(std::vector<std::string>{Runtime::PathGetDirectoryName(install).value(), "Resources", fileName})
                && mappingPaths[1] == Runtime::PathCombine(settings, fileName), "macOS mappings load resources then user overrides");
            mappingPaths = GamepadMappings::Paths(install, install);
            Check(mappingPaths.size() == 1 && mappingPaths[0] == Runtime::PathCombine(install, fileName),
                "portable mapping paths remain beside executable without duplicate loads");
            mappingPaths = GamepadMappings::Paths(settings, install);
            Check(mappingPaths.size() == 2 && mappingPaths[0] == Runtime::PathCombine(settings, fileName),
                "unbundled macOS mapping path remains portable");
            GamepadPlatformChecks::Run();
            GamepadEnhancementChecks::Run();
            AimAssist::AimAssistChecks::Run();
            ControllerRuntimeChecks::Run();
            GamepadOptions::Reset();
            const std::pair<float, float> zero{0.0F, 0.0F};
            Check(GamepadAnalog::ApplyRadialDeadZone(.1F, .1F, .2F) == zero, "radial inner deadzone");
            const auto diagonal = GamepadAnalog::ApplyRadialDeadZone(1, 1, .2F);
            Near(diagonal.first, std::sqrt(.5F), "diagonal normalized");
            Near(diagonal.first * diagonal.first + diagonal.second * diagonal.second, 1, "maximum magnitude");
            Near(GamepadAnalog::ApplyRadialDeadZone(.8F, 0, .2F, .2F).first, 1, "outer deadzone");
            Check(GamepadAnalog::ApplyRadialDeadZone(std::nanf(""), 0, .2F) == zero, "invalid axis neutral");
            Near(GamepadAnalog::ApplyResponseCurve(.5F, GamepadCurve::Classic), .25F, "classic curve");
            Near(GamepadAnalog::ApplyResponseCurve(-.5F, GamepadCurve::Linear), -.5F, "linear sign");
            Check(GamepadAnalog::QuantizeMovement(.4F, .4F) == std::pair<std::int32_t, std::int32_t>(1, 1), "diagonal movement threshold");
            Check(GamepadAnalog::QuantizeMovement(.49F, 0) == std::pair<std::int32_t, std::int32_t>(0, 0), "neutral movement threshold");
            for (std::int32_t i = 0; i < 8; i++)
            {
                const float angle = static_cast<float>(i) * Pi / 4;
                const auto direction = GamepadAnalog::QuantizeMovement(.6F * std::cos(angle), .6F * std::sin(angle));
                Check(direction != std::pair<std::int32_t, std::int32_t>(0, 0), "all directions engage equally");
            }
            Check(!GamepadAnalog::Trigger(0, false), "resting trigger");
            Check(GamepadAnalog::Trigger(.61F, false), "trigger press");
            Check(GamepadAnalog::Trigger(.57F, true), "trigger hysteresis hold");
            Check(!GamepadAnalog::Trigger(.44F, true), "trigger release");
            Check(!GamepadAnalog::Trigger(0, true, .05F), "low threshold still releases");
            Entities::Keybind bind(OpenTK::Windowing::GraphicsLibraryFramework::Keys::Space);
            bind.SetIsReleased(true);
            GamepadInput::Hold(bind, true, false);
            Check(bind.IsDown() && !bind.IsReleased(), "held controller cannot release through idle keyboard");
            bind.SetIsDown(true);
            bind.SetIsPressed(true);
            GamepadInput::Hold(bind, false, false);
            Check(bind.IsDown() && bind.IsPressed(), "controller never clears keyboard input");
            for (std::int32_t i = 0; i < 6; i++)
            {
                const float angle = (static_cast<float>(i) + .5F) * Pi / 3;
                const auto direction = WeaponSelectionDirection::FromStick(std::sin(angle), std::cos(angle));
                Check(WeaponSelectionDirection::Resolve(direction.first, direction.second) == i, "controller wheel sector " + std::to_string(i));
                const float arc = (static_cast<float>(i) + .5F) * Pi / 12;
                Check(WeaponSelectionDirection::Resolve(std::sin(arc), std::cos(arc)) == i, "pointer wheel sector " + std::to_string(i));
            }
            Check(WeaponSelectionDirection::Resolve(0, 0) == -1, "resting wheel keeps weapon");
            GamepadEdges edge{};
            Check(edge.Update(Snap("one", State(), 1)) == GamepadButtons::None, "first controller neutral");
            Check(edge.Update(Snap("one", State(GamepadButtons::A), 1)) == GamepadButtons::A, "button press");
            Check(edge.Update(Snap("one", State(GamepadButtons::A), 1)) == GamepadButtons::None, "button hold");
            Check(edge.Update(Snap("one", State(), 1)) == GamepadButtons::None, "button release");
            Check(edge.Update(Snap("one", State(GamepadButtons::A), 2)) == GamepadButtons::None, "reconnect held");
            Check(edge.Update(Snap("two", State(GamepadButtons::B), 3)) == GamepadButtons::None, "switch held");
            RemoveAll();
            GamepadManager::SelectDevice(std::nullopt);
            GamepadManager::UpdateDevice("raw", State(), false);
            Check(GamepadManager::Snapshot().DeviceId == "raw", "first controller");
            GamepadManager::UpdateDevice("mapped", State(), true);
            Check(GamepadManager::Snapshot().DeviceId == "mapped", "mapped discovery preference");
            GamepadManager::UpdateDevice("raw", State(GamepadButtons::A), false);
            Check(GamepadManager::Snapshot().DeviceId == "raw", "button switches controller");
            GamepadManager::UpdateDevice("mapped", State(GamepadButtons::None, .1F), true);
            Check(GamepadManager::Snapshot().DeviceId == "raw", "drift cannot switch");
            GamepadManager::UpdateDevice("mapped", State(GamepadButtons::B), true);
            Check(GamepadManager::ActiveState().Buttons == GamepadButtons::B, "two controllers never merge");
            GamepadManager::SelectDevice("raw");
            GamepadManager::UpdateDevice("mapped", State(GamepadButtons::X), true);
            Check(GamepadManager::Snapshot().DeviceId == "raw", "explicit selection wins");
            GamepadManager::RemoveDevice("raw");
            Check(!GamepadManager::ActiveState().Connected && GamepadManager::ActiveState().Buttons == GamepadButtons::None,
                "disconnect clears immediately");
            GamepadManager::UpdateDevice("mapped", State(GamepadButtons::None, .8F), true);
            Check(GamepadManager::Snapshot().DeviceId == "mapped", "fallback after removal");
            GamepadManager::UpdateDevice("mapped", State(GamepadButtons::None, 0, .61F), true);
            Check(GamepadManager::ActiveState().Down(GamepadButtons::RightTrigger), "manager trigger press");
            GamepadManager::UpdateDevice("mapped", State(GamepadButtons::None, 0, .57F), true);
            Check(GamepadManager::ActiveState().Down(GamepadButtons::RightTrigger), "manager trigger hold");
            GamepadManager::UpdateDevice("mapped", State(GamepadButtons::None, 0, .44F), true);
            Check(!GamepadManager::ActiveState().Down(GamepadButtons::RightTrigger), "manager trigger release");
            GamepadContexts::Current(GamepadContext::Gameplay);
            GamepadManager::UpdateDevice("mapped", State(), true);
            GamepadInput::BeginFrame();
            GamepadManager::UpdateDevice("mapped", State(GamepadButtons::A), true);
            GamepadInput::BeginFrame();
            Check(GamepadInput::TakePress(GamepadButtons::A), "gameplay press before context transition");
            GamepadContexts::MenuVisible(true);
            GamepadContexts::MenuVisible(false);
            GamepadManager::UpdateDevice("mapped", State(GamepadButtons::B), true);
            GamepadInput::BeginFrame();
            Check(!GamepadInput::TakePress(GamepadButtons::B), "menu closed between simulation steps cannot leak held accept/back");
            GamepadManager::UpdateDevice("mapped", State(), true);
            GamepadInput::BeginFrame();
            GamepadManager::UpdateDevice("mapped", State(GamepadButtons::B), true);
            GamepadInput::BeginFrame();
            Check(GamepadInput::TakePress(GamepadButtons::B), "gameplay resumes after release and repress");
            CheckFocusLifecycle();
            CheckMenuLifecycle();
            for (const GamepadButtons key : {GamepadButtons::DpadUp, GamepadButtons::RightTrigger})
            {
                GamepadEventState events{};
                events.Key(key, true);
                events.Motion = State(GamepadButtons::None, .8F);
                Check(events.Snapshot().Down(key), "motion preserves held key " + ToString(key));
                events.Motion = State(key);
                events.Key(key, false);
                Check(events.Snapshot().Down(key), "motion contribution survives key release " + ToString(key));
                events.Motion = State();
                Check(events.Snapshot().Buttons == GamepadButtons::None, "motion release " + ToString(key));
                events.Key(key, true);
                events.Clear();
                Check(events.Snapshot().Buttons == GamepadButtons::None, "event lifecycle clear");
            }
            std::vector<UiAction> actions;
            GamepadUiRouter router{};
            static_cast<void>(router.Action.Add([&actions](UiAction action) { actions.push_back(action); }));
            router.Update(Snap("one", State(), 1), GamepadContext::Menu, 0);
            router.Update(Snap("one", State(GamepadButtons::DpadDown), 1), GamepadContext::Menu, 10);
            router.Update(Snap("one", State(GamepadButtons::DpadDown), 1), GamepadContext::Menu, 309);
            Check(actions.size() == 1, "repeat initial delay");
            router.Update(Snap("one", State(GamepadButtons::DpadDown), 1), GamepadContext::Menu, 310);
            router.Update(Snap("one", State(GamepadButtons::DpadDown), 1), GamepadContext::Menu, 400);
            Check(actions.size() == 3, "repeat cadence");
            router.Update(Snap("one", State(GamepadButtons::DpadUp), 1), GamepadContext::Menu, 401);
            Check(actions.back() == UiAction::Up, "direction change immediate");
            router.Update(Snap("one", State(GamepadButtons::A), 1), GamepadContext::Menu, 402);
            router.Update(Snap("one", State(GamepadButtons::A), 1), GamepadContext::Menu, 3000);
            Check(std::count(actions.begin(), actions.end(), UiAction::Accept) == 1, "accept never repeats");
            router.Update(Snap("one", State(GamepadButtons::B), 1), GamepadContext::BindingCapture, 3010);
            Check(std::find(actions.begin(), actions.end(), UiAction::Back) == actions.end(), "capture does not navigate");
            GamepadOptions::Load({"gamepad_left_inner_deadzone=.3", "gamepad_deadzone=.25", "gamepad_look=2", "gamepad_invert_y=true"});
            Near(GamepadOptions::LeftInner(), .3F, "explicit setting wins independent of order");
            Near(GamepadOptions::RightInner(), .25F, "legacy deadzone migration");
            Near(GamepadOptions::LookX(), 2, "legacy horizontal sensitivity");
            Near(GamepadOptions::LookY(), 2, "legacy vertical sensitivity");
            Check(GamepadOptions::InvertY(), "legacy inversion");
            PadBindings::Reset();
            Check(PadBindings::Conflicts(PadAction::Morph, GamepadButtons::A).size() == 1, "binding conflict visible");
            PadBindings::Assign(PadAction::Morph, 0, GamepadButtons::A, "Swap");
            Check(PadBindings::Get(PadAction::Jump) == GamepadButtons::B, "swap old button");
            PadBindings::Assign(PadAction::Scan, 0, GamepadButtons::A, "Keep Both");
            Check(PadBindings::Get(PadAction::Morph) == GamepadButtons::A, "intentional shared binding");
            PadBindings::Assign(PadAction::Zoom, 0, GamepadButtons::A, "Replace");
            Check(PadBindings::Get(PadAction::Morph) == GamepadButtons::None && PadBindings::Get(PadAction::Scan) == GamepadButtons::None,
                "replace clears all conflicts");
            PadBindings::Reset();
            PadBindings::SetSlot(PadAction::NextWeapon, 0, GamepadButtons::None);
            Check(PadBindings::Slot(PadAction::NextWeapon, 0) == GamepadButtons::None
                && PadBindings::Slot(PadAction::NextWeapon, 1) == GamepadButtons::DpadRight, "clearing primary preserves secondary position");
            Check(!PadBindings::TryLoad("pad_99999", "A"), "invalid action rejected");
            Check(!PadBindings::TryLoad("pad_Jump", "-1"), "invalid buttons rejected");
            Check(GamepadGlyphs::Resolve(GamepadButtons::A, GamepadFamily::PlayStation) == "Cross", "PlayStation label");
            Check(GamepadGlyphs::Resolve(GamepadButtons::A, GamepadFamily::Nintendo) == "B", "Nintendo position");
            const std::int64_t clock = Runtime::EnvironmentTickCount64() + 1000;
            InputSourceTracker::Note(InputSource::KeyboardMouse, clock - 200);
            InputSourceTracker::Note(InputSource::Gamepad, clock);
            InputSourceTracker::Note(InputSource::Touch, clock + 1);
            Check(InputSourceTracker::Current() == InputSource::Gamepad, "input source cooldown");
            InputSourceTracker::Note(InputSource::Touch, clock + 200);
            Check(InputSourceTracker::Current() == InputSource::Touch, "meaningful touch takeover");
            Check(GamepadGlyphs::Detect("Wireless Controller", std::string("030000004c0500000000000000000000")) == GamepadFamily::PlayStation,
                "GUID identifies generic PlayStation name");
            Check(GamepadGlyphs::Detect("Controller", std::nullopt, 0x057e) == GamepadFamily::Nintendo, "Android vendor family");
            auto rumble = std::make_shared<FakeHaptics>();
            GamepadHaptics::Register("mapped", rumble);
            GamepadManager::SelectDevice("mapped");
            GamepadOptions::Vibration(true);
            GamepadOptions::VibrationStrength(.5F);
            InputSourceTracker::Note(InputSource::Gamepad, clock + 400);
            GamepadHaptics::Play(GamepadFeedback::Damage);
            Near(rumble->Low, .16F, "haptic intensity scales");
            GamepadHaptics::Stop();
            GamepadContexts::Focused(false);
            GamepadHaptics::Play(GamepadFeedback::Damage);
            Check(rumble->Stopped, "background gameplay cannot restart vibration");
            GamepadContexts::Focused(true);
            GamepadOptions::Vibration(false);
            GamepadHaptics::Play(GamepadFeedback::Death);
            Near(rumble->Low, .16F, "disabled vibration does not start an effect");
            GamepadManager::RemoveDevice("mapped");
            Check(rumble->Stopped, "disconnect stops haptics immediately");
            GamepadHaptics::Unregister("mapped");
            PadBindings::Set(PadAction::Shoot, GamepadButtons::Y);
            Check(GamepadProbe::Actions(GamepadButtons::Y).find("Fire / alt attack") != std::string::npos, "probe uses remapped actions");
            CheckPersistence();
            // Launcher.Gui.GamepadUiChecks.Run(shots) belongs to the shell's
            // screens and runs with them.
            Runtime::ConsoleWriteLine("[gamepadcheck] PASS: " + std::to_string(_checks) + " deterministic checks");
            result = 0;
        }
        catch (const std::exception& ex)
        {
            Runtime::ConsoleWriteLine(std::string("[gamepadcheck] FAIL: ") + ex.what());
            result = 1;
        }
        RemoveAll();
        GamepadManager::SelectDevice(std::nullopt);
        GamepadOptions::Reset();
        PadBindings::Reset();
        return result;
    }

    void GamepadChecks::CheckFocusLifecycle()
    {
        PadBindings::Reset();
        GamepadContexts::Current(GamepadContext::Gameplay);
        GamepadContexts::Focused(true);
        const auto frame = [](GamepadButtons buttons)
        {
            GamepadManager::UpdateDevice("mapped", State(buttons), true);
            GamepadInput::BeginFrame();
        };
        frame(GamepadButtons::None);
        frame(GamepadButtons::RightThumb);
        Check(GamepadInput::WheelHeld(), "wheel can open before losing focus");
        GamepadContexts::Focused(false);
        GamepadManager::ClearAll();
        frame(GamepadButtons::None);
        frame(GamepadButtons::RightThumb);
        frame(GamepadButtons::RightThumb);
        Check(!GamepadInput::WheelHeld(), "background wheel is suppressed");
        GamepadContexts::Focused(true);
        frame(GamepadButtons::RightThumb);
        Check(!GamepadInput::WheelHeld(), "focus regain blocks buttons held during background polling");
        frame(GamepadButtons::None);
        frame(GamepadButtons::RightThumb);
        Check(GamepadInput::WheelHeld(), "focus regain allows release and repress");
        frame(GamepadButtons::None);
        GamepadContexts::Focused(false);
        GamepadContexts::Focused(true);
        frame(GamepadButtons::RightThumb);
        Check(!GamepadInput::WheelHeld() && !GamepadInput::TakePress(GamepadButtons::RightThumb),
            "focus change between simulation steps blocks held input");
        frame(GamepadButtons::None);
    }

    void GamepadChecks::CheckMenuLifecycle()
    {
        std::vector<UiAction> actions;
        GamepadUiRouter router{};
        static_cast<void>(router.Action.Add([&actions](UiAction action) { actions.push_back(action); }));
        GamepadContexts::MenuVisible(true);
        router.Update(Snap("one", State(), 1), GamepadContext::Menu, 0);
        GamepadContexts::MenuVisible(false);
        GamepadContexts::MenuVisible(true);
        router.Update(Snap("one", State(GamepadButtons::Start), 1), GamepadContext::Menu, 100);
        Check(actions.empty(), "reopened menu ignores the Start press that opened it");
        router.Update(Snap("one", State(), 1), GamepadContext::Menu, 110);
        router.Update(Snap("one", State(GamepadButtons::Start), 1), GamepadContext::Menu, 120);
        Check(actions.size() == 1 && actions[0] == UiAction::Back, "reopened menu accepts a fresh Back press");
        GamepadContexts::Focused(false);
        GamepadContexts::Focused(true);
        router.Update(Snap("one", State(GamepadButtons::A), 1), GamepadContext::Menu, 130);
        Check(actions.size() == 1, "menu blocks held input after focus changes between UI ticks");
        router.Update(Snap("one", State(), 1), GamepadContext::Menu, 140);
        router.Update(Snap("one", State(GamepadButtons::A), 1), GamepadContext::Menu, 150);
        Check(actions.size() == 2 && actions[1] == UiAction::Accept, "menu accepts a fresh press after regaining focus");
        GamepadContexts::MenuVisible(false);
    }

    void GamepadChecks::CheckPersistence()
    {
        const std::string previous = Launcher::LauncherPrefs::Directory();
        const std::string directory = Runtime::PathCombine(Runtime::PathGetTempPath(),
            "fruity-input-check-" + Runtime::Guid::NewGuid().ToString("N"));
        Runtime::DirectoryCreateDirectory(directory);
        const auto cleanup = [&]()
        {
            Launcher::LauncherPrefs::Directory(previous);
            Runtime::FileDelete(Runtime::PathCombine(directory, "controls.txt"));
            Runtime::DirectoryDelete(directory);
        };
        try
        {
            Launcher::LauncherPrefs::Directory(directory);
            const std::string path = Runtime::PathCombine(directory, "controls.txt");
            Runtime::FileWriteAllLines(path, {"future_control=keep-me", "gamepad_deadzone=.27", "gamepad_look=1.8", "pad_Jump=A"});
            InputSettings::Load();
            Near(GamepadOptions::RightInner(), .27F, "file migration");
            PadBindings::SetSlot(PadAction::NextWeapon, 0, GamepadButtons::None);
            InputSettings::Save();
            Check(Runtime::FileReadAllText(path).find("future_control=keep-me") != std::string::npos, "unknown settings preserved");
            PadBindings::Reset();
            InputSettings::Load();
            Check(PadBindings::Slot(PadAction::NextWeapon, 0) == GamepadButtons::None
                && PadBindings::Slot(PadAction::NextWeapon, 1) == GamepadButtons::DpadRight, "primary/secondary slots round-trip");
        }
        catch (...)
        {
            cleanup();
            throw;
        }
        cleanup();
    }
}
