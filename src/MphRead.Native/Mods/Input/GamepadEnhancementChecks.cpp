#include "GamepadEnhancementChecks.hpp"

#include "GamepadActions.hpp"
#include "GamepadCalibration.hpp"
#include "GamepadChecks.hpp"
#include "GamepadInput.hpp"
#include "GamepadManager.hpp"
#include "GamepadMappingWizard.hpp"
#include "GamepadOptions.hpp"
#include "GamepadProbe.hpp"
#include "GamepadProfiles.hpp"
#include "GamepadUiRouter.hpp"
#include "PadBindings.hpp"
#include "WeaponSelectionDirection.hpp"
#include "../Launcher/Portable/LauncherPrefs.hpp"
#include "../../Entities/Players/PlayerInput.hpp"
#include "../../Formats/Enums.hpp"
#include "../../NativeRuntime/System/Exceptions.hpp"
#include "../../NativeRuntime/System/Guid.hpp"
#include "../../NativeRuntime/System/IO.hpp"
#include "../../NativeRuntime/System/Json.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <set>

namespace MphRead::Mods::Input
{
    namespace Runtime = ::MphRead::NativeRuntime;

    namespace
    {
        void Check(bool ok, const std::string& message)
        {
            GamepadChecks::Check(ok, message);
        }

        // typeof(PlayerControls).GetProperty(action.ToString()).
        [[nodiscard]] Entities::Keybind& Bind(Entities::PlayerControls& controls, PadAction action)
        {
            switch (action)
            {
            case PadAction::VoltDriver: return controls.VoltDriver();
            case PadAction::Battlehammer: return controls.Battlehammer();
            case PadAction::Imperialist: return controls.Imperialist();
            case PadAction::Judicator: return controls.Judicator();
            case PadAction::Magmaul: return controls.Magmaul();
            case PadAction::ShockCoil: return controls.ShockCoil();
            case PadAction::OmegaCannon: return controls.OmegaCannon();
            case PadAction::AffinitySlot: return controls.AffinitySlot();
            default: throw System::NullReferenceException();
            }
        }
    }

    void GamepadEnhancementChecks::Run()
    {
        GamepadOptions::Reset();
        PadBindings::Reset();
        const std::array<PadAction, 9> weapons{PadAction::VoltDriver, PadAction::Battlehammer, PadAction::Imperialist,
            PadAction::Judicator, PadAction::Magmaul, PadAction::ShockCoil, PadAction::OmegaCannon,
            PadAction::AffinitySlot, PadAction::LastWeapon};
        for (const PadAction action : weapons)
        {
            Check(PadBindings::Get(action) == GamepadButtons::None, ToString(action) + " defaults to unassigned");
        }
        GamepadActions actions{};
        PadBindings::SetSlot(PadAction::Imperialist, 0, GamepadButtons::A, GamepadButtons::LeftBumper);
        actions.Update(GamepadButtons::LeftBumper);
        Check(!actions.Down(PadAction::PrevWeapon), "modifier alone never triggers its ordinary action");
        actions.Update(GamepadButtons::LeftBumper | GamepadButtons::A);
        Check(actions.WasPressed(PadAction::Imperialist) && !actions.Down(PadAction::Jump), "modifier chord selects a weapon without jumping");
        actions.Update(GamepadButtons::LeftBumper | GamepadButtons::A);
        Check(!actions.WasPressed(PadAction::Imperialist), "held chord fires only one weapon selection");
        actions.Update(GamepadButtons::A);
        Check(!actions.Down(PadAction::Jump), "releasing modifier first does not leak the remaining button");
        actions.Update(GamepadButtons::None);
        actions.Update(GamepadButtons::A);
        Check(actions.WasPressed(PadAction::Jump), "unmodified binding returns after release");
        actions.Update(GamepadButtons::None);
        actions.Update(GamepadButtons::LeftBumper | GamepadButtons::A);
        actions.Update(GamepadButtons::LeftBumper);
        actions.Update(GamepadButtons::LeftBumper | GamepadButtons::A);
        Check(actions.WasPressed(PadAction::Imperialist), "modifier can remain held across repeated selections");
        PadBindings::SetSlot(PadAction::Jump, 1, GamepadButtons::A, GamepadButtons::RightBumper);
        actions.Reset();
        actions.Update(GamepadButtons::A);
        Check(actions.Down(PadAction::Jump), "plain and modified slots may share their action button");
        Check(PadBindings::Conflicts(PadAction::VoltDriver, GamepadButtons::A, GamepadButtons::LeftBumper)
            == std::vector<PadAction>{PadAction::Imperialist}, "conflicts compare the whole combination");
        PadBindings::SetSlot(PadAction::VoltDriver, 0, GamepadButtons::X, GamepadButtons::RightBumper);
        PadBindings::Assign(PadAction::VoltDriver, 0, GamepadButtons::A, "Swap", GamepadButtons::LeftBumper);
        Check(PadBindings::Slot(PadAction::Imperialist, 0) == GamepadButtons::X
            && PadBindings::Modifier(PadAction::Imperialist, 0) == GamepadButtons::RightBumper, "swap preserves both parts of a combination");
        const std::string probed = GamepadProbe::Actions(GamepadButtons::LeftBumper | GamepadButtons::A);
        Check(probed.find("Volt Driver") != std::string::npos && probed.find("Jump") == std::string::npos,
            "live probe resolves combinations like gameplay");
        PadBindings::Reset();
        GamepadOptions::WheelToggle(true);
        actions.Reset();
        actions.Update(GamepadButtons::RightThumb);
        actions.Update(GamepadButtons::None);
        Check(actions.WheelOpen(), "toggle wheel stays open after release");
        actions.Update(GamepadButtons::RightThumb);
        Check(!actions.WheelOpen(), "second press closes toggle wheel");
        actions.Update(GamepadButtons::None);
        actions.Update(GamepadButtons::RightThumb);
        actions.Reset();
        Check(!actions.WheelOpen(), "context reset closes toggle wheel");
        GamepadOptions::SetWheelSlot(0, 4);
        const std::array<std::int32_t, 6>& order = GamepadOptions::WheelOrder();
        Check(std::set<std::int32_t>(order.begin(), order.end()).size() == 6 && order[4] == 0,
            "wheel rearrangement swaps without duplicate weapons");
        Check(WeaponSelectionDirection::ControllerSlot(.5F, .8F) == 4, "wheel direction follows configured order");
        GamepadOptions::WheelThreshold(.7F);
        Check(WeaponSelectionDirection::ControllerSlot(.5F, 0) == -1, "wheel threshold rejects small stick movement");
        GamepadOptions::Load({"gamepad_wheel_order=0,0,2,3,4,5", "gamepad_scoped_x=NaN", "gamepad_lt_min=.8", "gamepad_lt_max=.1"});
        Check(GamepadOptions::WheelOrder() == std::array<std::int32_t, 6>{0, 1, 2, 3, 4, 5} && GamepadOptions::ScopedX() == 1,
            "invalid wheel order and non-finite scoped sensitivity use safe defaults");
        Check(GamepadOptions::LeftTriggerMax() >= GamepadOptions::LeftTriggerMin() + .099F, "trigger calibration always keeps a nonzero range");
        GamepadOptions::Reset();
        GamepadContexts::Current(GamepadContext::Gameplay);
        for (const PadAction action : weapons)
        {
            if (action == PadAction::LastWeapon)
            {
                continue;
            }
            PadBindings::Reset();
            PadBindings::SetSlot(action, 0, GamepadButtons::X);
            GamepadManager::UpdateDevice("enhancement-check", GamepadState{}, true);
            GamepadInput::BeginFrame();
            GamepadState pressed{};
            pressed.Buttons = GamepadButtons::X;
            GamepadManager::UpdateDevice("enhancement-check", pressed, true);
            GamepadInput::BeginFrame();
            Entities::PlayerControls controls = Entities::PlayerControls::GetDefault();
            GamepadInput::ApplyBindings(controls);
            Check(Bind(controls, action).IsPressed(), ToString(action) + " reaches its actual gameplay keybind");
        }
        Entities::PlayerControls defaults = Entities::PlayerControls::GetDefault();
        Check(GamepadActions::WeaponBind(defaults, BeamType::Imperialist) == &defaults.Imperialist()
            && GamepadActions::WeaponBind(defaults, BeamType::None) == nullptr,
            "last weapon resolves an existing weapon action and rejects invalid values");
        GamepadManager::RemoveDevice("enhancement-check");
        CheckCalibration();
        CheckMapping();
        CheckProfiles();
        GamepadOptions::Reset();
        PadBindings::Reset();
    }

    void GamepadEnhancementChecks::CheckCalibration()
    {
        constexpr float Pi = std::numbers::pi_v<float>;
        GamepadCalibration calibration{};
        for (std::int32_t i = 0; i < 20; i++)
        {
            GamepadState rest{};
            rest.LeftX = .03F;
            rest.RightY = .06F;
            rest.LeftTrigger = .1F;
            calibration.Sample(rest, true);
        }
        for (std::int32_t i = 0; i < 100; i++)
        {
            const float angle = static_cast<float>(i) * Pi * 2 / 100;
            GamepadState range{};
            range.LeftX = .9F * std::cos(angle);
            range.LeftY = .9F * std::sin(angle);
            range.RightX = std::cos(angle);
            range.RightY = std::sin(angle);
            range.LeftTrigger = .9F;
            range.RightTrigger = .8F;
            calibration.Sample(range, false);
        }
        Check(calibration.Valid(), "calibration accepts measured rest and range");
        calibration.Apply();
        Check(std::abs(GamepadOptions::LeftInner() - .04F) < .001F && std::abs(GamepadOptions::RightCalibration().CenterY - .06F) < .001F,
            "calibration separates center bias from radial noise");
        Check(GamepadCalibration::Trigger(.1F, .1F, .9F) == 0 && GamepadCalibration::Trigger(.9F, .1F, .9F) == 1,
            "calibrated trigger spans released to full press");
        Check(!GamepadCalibration().Valid(), "incomplete calibration cannot be applied");
    }

    void GamepadEnhancementChecks::CheckMapping()
    {
        const std::string guid = "030000005e040000130b000099090000";
        const auto rest = [&guid]()
        {
            return GamepadRawSample{"mapping-test", guid, "Xbox", {0, 0, 0, 0, -1, -1}, std::vector<bool>(10), std::vector<std::uint8_t>(1)};
        };
        GamepadMappingWizard wizard(rest());
        for (std::size_t i = 0; i < 10; i++)
        {
            GamepadRawSample sample = rest();
            sample.Buttons[i] = true;
            wizard.Sample(sample);
            wizard.Sample(rest());
        }
        for (const std::uint8_t hat : {1, 2, 4, 8})
        {
            GamepadRawSample sample = rest();
            sample.Hats[0] = hat;
            wizard.Sample(sample);
            wizard.Sample(rest());
        }
        for (std::size_t i = 0; i < 6; i++)
        {
            GamepadRawSample sample = rest();
            sample.Axes[i] = i == 1 || i == 3 ? -1 : 1;
            wizard.Sample(sample);
            wizard.Sample(rest());
        }
        Check(wizard.Complete() && wizard.Mapping().find("rightx:a2,") != std::string::npos
            && wizard.Mapping().find("lefttrigger:a4,") != std::string::npos,
            "mapping wizard produces complete raw axis/button/hat mapping");
        bool rejected = false;
        try
        {
            GamepadRawSample other = rest();
            other.DeviceId = "other";
            GamepadMappingWizard(rest()).Sample(other);
        }
        catch (const System::InvalidOperationException&)
        {
            rejected = true;
        }
        Check(rejected, "mapping wizard rejects a controller change");
    }

    void GamepadEnhancementChecks::CheckProfiles()
    {
        const std::string previous = Launcher::LauncherPrefs::Directory();
        const std::string directory = Runtime::PathCombine(Runtime::PathGetTempPath(),
            "fruity-profiles-" + Runtime::Guid::NewGuid().ToString("N"));
        Runtime::DirectoryCreateDirectory(directory);
        const auto cleanup = [&]()
        {
            Launcher::LauncherPrefs::Directory(previous);
            GamepadProfiles::Initialize();
            for (const std::string& file : Runtime::DirectoryGetFiles(directory))
            {
                Runtime::FileDelete(file);
            }
            Runtime::DirectoryDelete(directory);
        };
        try
        {
            Launcher::LauncherPrefs::Directory(directory);
            GamepadProfiles::Initialize();
            GamepadOptions::ScopedX(.65F);
            GamepadOptions::WheelToggle(true);
            GamepadOptions::SetWheelSlot(0, 3);
            PadBindings::SetSlot(PadAction::Imperialist, 1, GamepadButtons::A, GamepadButtons::LeftBumper);
            GamepadProfiles::Save("Precision");
            const std::string path = Runtime::PathCombine(directory, "export.json");
            GamepadProfiles::Export("Precision", path);
            Check(GamepadProfiles::Import(path) == "Precision 2", "profile import avoids overwriting an existing name");
            GamepadOptions::Reset();
            PadBindings::Reset();
            GamepadProfiles::Load("Precision");
            Check(GamepadOptions::ScopedX() == .65F && GamepadOptions::WheelToggle() && GamepadOptions::WheelOrder()[0] == 3
                && PadBindings::Modifier(PadAction::Imperialist, 1) == GamepadButtons::LeftBumper,
                "profile round-trip retains aim, wheel and combinations");
            const std::string stable = GamepadProfiles::DeviceKey("glfw:guid:0:1");
            Check(stable == GamepadProfiles::DeviceKey("glfw:guid:3:8"), "automatic profile key survives slot changes and reconnects");
            GamepadDeviceSnapshot device{};
            device.DeviceId = "glfw:guid:0:1";
            device.ProfileKey = stable;
            GamepadOptions::ScopedX(1);
            GamepadProfiles::Assign("Precision", device);
            GamepadManager::UpdateDevice(device.DeviceId, {}, true);
            GamepadManager::SelectDevice(device.DeviceId);
            Check(GamepadOptions::ScopedX() == .65F, "assigned controller automatically loads its saved profile");
            GamepadManager::RemoveDevice(device.DeviceId);
            Check(GamepadOptions::ScopedX() == 1, "unassigned device restores the prior manual settings");
            // JsonSerializer.Serialize(new GamepadProfile(1, "Bad", ...)).
            Runtime::FileWriteAllText(path, "{\"Version\":1,\"Name\":\"Bad\",\"Settings\":[\"Jump=Key:Enter\"]}");
            bool rejected = false;
            try
            {
                static_cast<void>(GamepadProfiles::Import(path));
            }
            catch (const System::IO::InvalidDataException&)
            {
                rejected = true;
            }
            Check(rejected && GamepadOptions::ScopedX() == 1, "invalid imports cannot change keyboard bindings or active controller settings");
            Runtime::FileWriteAllText(path, "null");
            rejected = false;
            try
            {
                static_cast<void>(GamepadProfiles::Import(path));
            }
            catch (const System::IO::InvalidDataException&)
            {
                rejected = true;
            }
            Check(rejected, "null profile is rejected");
        }
        catch (...)
        {
            cleanup();
            throw;
        }
        cleanup();
    }
}
