#include "PointerCheck.hpp"

#include "GamepadInput.hpp"
#include "GamepadManager.hpp"
#include "GamepadUiRouter.hpp"
#include "PadBindings.hpp"
#include "PointerDevice.hpp"
#include "PointerInput.hpp"
#include "StylusZone.hpp"
#include "SyntheticInput.hpp"
#include "WindowsPenInput.hpp"
#include "../InputSettings.hpp"
#include "../Launcher/Portable/LauncherPrefs.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Scene.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/Exceptions.hpp"
#include "../../NativeRuntime/System/Guid.hpp"
#include "../../NativeRuntime/System/IO.hpp"

#include <algorithm>
#include <vector>

namespace MphRead::Mods::Input
{
    namespace Runtime = ::MphRead::NativeRuntime;
    using Entities::ButtonType;
    using Entities::Keybind;
    using OpenTK::Windowing::GraphicsLibraryFramework::Keys;
    using OpenTK::Windowing::GraphicsLibraryFramework::MouseButton;

    namespace
    {
        using Delta = std::pair<float, float>;
    }

    std::int32_t PointerCheck::Run()
    {
        std::int32_t result = 0;
        try
        {
            _checks = 0;
            CheckBindings();
            CheckMovement();
            CheckZone();
            CheckPlayerInput();
            CheckSettings();
            Require(!WindowsPenInput::IsPromotedPointer(0), "physical mouse signature");
            Require(WindowsPenInput::IsPromotedPointer(0xFF515701), "promoted pen signature");
            Require(WindowsPenInput::IsPromotedPointer(0xFF515781), "promoted touch signature");
            Require(WindowsPenInput::IsPromotedPrimaryRelease(0x0202, 0xFF515701),
                "promoted pen mouse-up is a release fallback");
            Require(!WindowsPenInput::IsPromotedPrimaryRelease(0x0201, 0xFF515701),
                "promoted pen mouse-down is not a release fallback");
            Runtime::ConsoleWriteLine("POINTERCHECK PASS (" + std::to_string(_checks) + " assertions)");
            result = 0;
        }
        catch (const std::exception& ex)
        {
            Runtime::ConsoleWriteLine(std::string("POINTERCHECK FAIL: ") + ex.what());
            result = 1;
        }
        PointerDevice::Reset();
        return result;
    }

    void PointerCheck::Require(bool condition, std::string_view label)
    {
        _checks++;
        if (!condition)
        {
            throw System::InvalidOperationException(label);
        }
    }

    void PointerCheck::CheckBindings()
    {
        PointerBindings source{};
        source.Update(true, true);
        std::vector<Keybind> independent{Keybind(Keys::F), Keybind(Keys::G), Keybind(MouseButton::Right),
            Keybind(MouseButton::Middle), Keybind(ButtonType::ScrollUp), Keybind(ButtonType::ScrollDown)};
        for (Keybind& bind : independent)
        {
            bind.SetIsDown(true);
            bind.SetIsPressed(true);
            Require(!source.Resolve(bind) && bind.IsDown() && bind.IsPressed(),
                "independent " + std::string(ToString(bind.Type())) + "/" + bind.ToString() + " preserved during capture");
        }
        Entities::PlayerControls controls = Entities::PlayerControls::GetDefault();
        controls.Shoot().SetType(ButtonType::Mouse);
        controls.AltAttack().SetType(ButtonType::Mouse);
        controls.Shoot().SetMouseButton(MouseButton::Left);
        controls.AltAttack().SetMouseButton(MouseButton::Left);
        Keybind extra(MouseButton::Left);
        for (Keybind* bind : {&controls.Shoot(), &controls.AltAttack(), &extra})
        {
            bind->SetIsDown(true);
            bind->SetIsPressed(true);
            bind->SetIsReleased(true);
            Require(source.Resolve(*bind) && !bind->IsDown() && !bind->IsPressed() && !bind->IsReleased(),
                "every primary binding suppressed before additive actions");
        }
        Keybind& shoot = controls.Shoot();
        source.Update(false, false);
        source.Resolve(shoot);
        Require(!shoot.IsReleased(), "captured tip release does not leak an action edge");
        source.Update(true, false);
        source.Resolve(shoot);
        Require(shoot.IsDown() && shoot.IsPressed(), "ordinary mouse press");
        source.Update(true, false);
        source.Resolve(shoot);
        Require(shoot.IsDown() && !shoot.IsPressed(), "ordinary mouse hold");
        source.Update(false, false);
        source.Resolve(shoot);
        Require(!shoot.IsDown() && shoot.IsReleased(), "ordinary mouse release");
        source.Update(true, true, true);
        source.Resolve(shoot);
        Require(shoot.IsDown() && shoot.IsPressed(), "physical mouse fires beside captured native pen");
        source.Update(true, true, false);
        source.Resolve(shoot);
        Require(!shoot.IsDown() && shoot.IsReleased(), "physical mouse releases while pen stays down");
    }

    void PointerCheck::CheckMovement()
    {
        PointerInput::GuardJumps(true);
        PointerInput::StylusMode(true);
        PointerInput::JumpPixels(600);
        PointerInput::Reset();
        Require(PointerInput::Filter(700, 400) == Delta(0, 0), "vector teleport");
        Require(PointerInput::JumpsIgnored() == 1, "one rejected sample counted once");
        Require(PointerInput::Filter(450, 450) == Delta(0, 0), "diagonal threshold uses distance");
        Require(PointerInput::Filter(300, 250) == Delta(300, 250), "legal fast movement");
        Require(PointerInput::Filter(-600, 0) == Delta(0, 0), "negative threshold boundary");
        PointerInput::GuardJumps(false);
        Require(PointerInput::Filter(700, 400) == Delta(700, 400), "filter off independently of stylus mode");
        PointerInput::GuardJumps(true);
        PointerInput::StylusMode(false);
        Require(PointerInput::Filter(900, 900) == Delta(900, 900), "normal high-DPI mouse unchanged");
        PointerInput::StylusMode(true);
        PointerInput::JumpPixels(0);
        Require(PointerInput::Filter(900, 900) == Delta(900, 900), "zero threshold disables filtering");
        PointerInput::JumpPixels(600);
    }

    void PointerCheck::Frame(float x, float y, bool down, bool independentDown, bool acceptsInput, std::uint32_t id)
    {
        PointerDevice::Update(PointerSample{PointerDeviceType::Pen, id, x, y, down, down, true},
            1920, 1080, independentDown, acceptsInput);
    }

    void PointerCheck::CheckZone()
    {
        PointerDevice::Reset();
        PointerInput::StylusMode(true);
        PointerInput::GuardJumps(false);
        StylusZone::Enabled(true);
        StylusZone::AspectCorrection(1920.0F / 1080);
        StylusZone::SetRect(0, 0, 1);
        Frame(100, 600, false);
        Frame(1500, 600, true);
        Require(StylusZone::Held() == StylusRegion::Aim && !StylusZone::Aiming(), "first contact belongs to aim without rotating");
        Require(!PointerDevice::PrimaryDown() && PointerDevice::TakeDelta() == Delta(0, 0), "contact teleport cannot aim or fire");
        Frame(1510, 605, true);
        Frame(1520, 610, true);
        Require(StylusZone::Aiming() && PointerDevice::TakeDelta() == Delta(20, 10), "drag accumulates between simulation steps");
        Require(PointerDevice::TakeDelta() == Delta(0, 0), "catch-up simulation cannot apply movement twice");
        Frame(1530, 615, true, true);
        Require(PointerDevice::PrimaryDown(), "native independent mouse is not captured");
        Frame(1540, 620, false);
        Frame(100, 600, false);
        Frame(1500, 600, true);
        Frame(1505, 602, true);
        Require(PointerDevice::TakeDelta() == Delta(5, 2), "high-refresh touchdown excludes hover teleport");
        Frame(1510, 604, true, false, false);
        Frame(1800, 650, true, false, false);
        Frame(1810, 650, true);
        Require(PointerDevice::TakeDelta() == Delta(0, 0), "pause/focus return cannot replay accumulated aim");
        Frame(1820, 650, true, false, true, 2);
        Require(!StylusZone::Aiming() && PointerDevice::TakeDelta() == Delta(0, 0), "new pointer identity starts a fresh contact");
        for (const StylusZone::Button& button : StylusZone::Buttons)
        {
            StylusZone::Reset();
            const float x = button.X / StylusZone::DsWidth;
            const float y = button.Y / StylusZone::DsHeight * StylusZone::Height();
            StylusZone::Update(x, y, true);
            Require(StylusZone::CapturingPointer() && StylusZone::CapturingPrimaryButton(), button.Label + " captures tip");
            Require(!StylusZone::Aiming(), button.Label + " never aims");
            if (button.Region == StylusRegion::WeaponSelect)
            {
                StylusZone::Update(-1, -1, true);
                Require(StylusZone::MenuHeld(), "SEL holds wheel after dragging outside");
            }
            else
            {
                Require(StylusZone::TakePressed() == button.Region, button.Label + " action latched");
                Require(StylusZone::TakePressed() == StylusRegion::None, "latched action consumed once");
            }
            StylusZone::Update(x, y, false);
            Require(!StylusZone::CapturingPointer() && !StylusZone::MenuHeld(), "release ends ownership");
        }
        StylusZone::Reset();
        StylusZone::Update(-1, -1, true);
        StylusZone::Update(.5F, .4F, true);
        Require(!StylusZone::CapturingPointer() && StylusZone::Held() == StylusRegion::None, "outside contact stays ordinary");
        StylusZone::Reset();
        StylusZone::Update(.5F, .4F, true);
        StylusZone::Update(-1, -1, true);
        Require(StylusZone::Aiming() && StylusZone::Held() == StylusRegion::Aim, "aim ownership stays sticky outside zone");
        StylusZone::BeginPlacement();
        StylusZone::Update(.5F, .4F, true);
        Require(StylusZone::CapturingPointer() && !StylusZone::CapturingPrimaryButton() && !StylusZone::Aiming(),
            "placement owns pointer separately from zone contact");
        StylusZone::CancelPlacement();
        PointerInput::StylusMode(false);
        StylusZone::Update(.5F, .4F, true);
        Require(!StylusZone::Enabled() && !StylusZone::CapturingPointer(), "master off disables DS zone");
    }

    void PointerCheck::CheckPlayerInput()
    {
        const auto keyboard = SyntheticInput::CreateKeyboard();
        const auto mouse = SyntheticInput::CreateMouse();
        // RuntimeHelpers.GetUninitializedObject(typeof(Scene)) with the two
        // fields the input pass reads set by reflection; a constructed scene
        // already has no movie frame.
        auto scene = std::make_shared<Scene>(OpenTK::Mathematics::Vector2i(256, 192), *keyboard, *mouse,
            [](std::string) {}, []() {});
        scene->_cameraMode = CameraMode::Player;
        Entities::PlayerEntity::Reset();
        Entities::PlayerEntity::Construct(scene.get());
        Entities::PlayerEntity& player = Runtime::RequireReference(Entities::PlayerEntity::Main());
        player.SetLoadFlags(Entities::LoadFlags::Active);
        Entities::PlayerControls& controls = player.Controls();
        const auto process = [&]() { Entities::PlayerEntity::ProcessInput(*keyboard, *mouse, false); };
        PointerDevice::Reset();
        PointerInput::StylusMode(true);
        StylusZone::Enabled(true);
        StylusZone::SetRect(0, 0, 1);
        Frame(1000, 600, false);
        Frame(1000, 600, true);
        mouse->SetButtonDown(MouseButton::Left, true);
        controls.Shoot().SetType(ButtonType::Key);
        controls.AltAttack().SetType(ButtonType::Key);
        controls.Shoot().SetKey(Keys::F);
        controls.AltAttack().SetKey(Keys::G);
        keyboard->SetKeyDown(Keys::F, true);
        keyboard->SetKeyDown(Keys::G, true);
        process();
        Require(controls.Shoot().IsDown() && controls.Shoot().IsPressed() && controls.AltAttack().IsDown(),
            "real input pass preserves rebound Shoot and AltAttack during tip contact");
        Frame(1010, 605, true);
        process();
        Require(controls.Shoot().IsDown() && !controls.Shoot().IsPressed() && StylusZone::Aiming(),
            "real input pass keeps firing while aiming");
        for (Keybind* bind : {&controls.Shoot(), &controls.AltAttack(), &controls.Jump()})
        {
            bind->SetType(ButtonType::Mouse);
            bind->SetMouseButton(MouseButton::Left);
        }
        process();
        Require(!controls.Shoot().IsDown() && !controls.AltAttack().IsDown() && !controls.Jump().IsDown(),
            "real input pass captures all LMB-bound actions");
        GamepadContexts::Current(GamepadContext::Gameplay);
        PadBindings::Reset();
        GamepadState pad{};
        pad.Connected = true;
        GamepadManager::UpdateDevice("pointercheck", pad, true);
        GamepadInput::BeginFrame();
        pad.Buttons = GamepadButtons::RightTrigger;
        GamepadManager::UpdateDevice("pointercheck", pad, true);
        GamepadInput::BeginFrame();
        GamepadInput::Apply(&player);
        Require(controls.Shoot().IsDown() && controls.AltAttack().IsDown(), "real controller contribution survives stylus capture");
        GamepadManager::RemoveDevice("pointercheck");
        GamepadInput::BeginFrame();
        for (const StylusZone::Button& button : StylusZone::Buttons)
        {
            Frame(0, 0, false);
            Frame(button.X / StylusZone::DsWidth * 1920, button.Y / StylusZone::DsHeight * StylusZone::Height() * 1080, true);
            process();
            Require(!controls.Shoot().IsDown() && !controls.AltAttack().IsDown(), button.Label + " does not fire in real input pass");
            Keybind* bind = nullptr;
            switch (button.Region)
            {
            case StylusRegion::PowerBeam: bind = &controls.PowerBeam(); break;
            case StylusRegion::Missile: bind = &controls.Missile(); break;
            case StylusRegion::Weapons: bind = &controls.NextWeapon(); break;
            case StylusRegion::WeaponSelect: bind = &controls.WeaponMenu(); break;
            default: bind = &controls.Morph(); break;
            }
            if (button.Region == StylusRegion::WeaponSelect)
            {
                Require(!bind->IsDown() && StylusZone::MenuHeld(),
                    "SEL stays source-owned instead of mutating the shared WeaponMenu bind");
            }
            else
            {
                Require(bind->IsDown(), button.Label + " reaches its gameplay action");
            }
        }
        player.SetWeaponSelection(BeamType::None);
        Frame(0, 0, false);
        const auto select = std::find_if(StylusZone::Buttons.begin(), StylusZone::Buttons.end(),
            [](const StylusZone::Button& button) { return button.Region == StylusRegion::WeaponSelect; });
        Frame(select->X / StylusZone::DsWidth * 1920, select->Y / StylusZone::DsHeight * StylusZone::Height() * 1080, true);
        process();
        player.ProcessTouchInput();
        Require(TestFlag(player.Flags1(), Entities::PlayerFlags1::WeaponMenuOpen), "stylus SEL opens weapon menu");
        Frame(select->X / StylusZone::DsWidth * 1920, select->Y / StylusZone::DsHeight * StylusZone::Height() * 1080, false);
        process();
        player.ProcessTouchInput();
        Require(!TestFlag(player.Flags1(), Entities::PlayerFlags1::WeaponMenuOpen)
            && !TestFlag(player.Flags1(), Entities::PlayerFlags1::NoAimInput), "stylus SEL release closes weapon menu");
        PointerInput::StylusMode(false);
        Frame(1000, 600, true);
        mouse->SetButtonDown(MouseButton::Left, false);
        process();
        mouse->SetButtonDown(MouseButton::Left, true);
        process();
        Require(controls.Shoot().IsDown() && controls.AltAttack().IsDown() && controls.Jump().IsPressed(),
            "normal mouse restores all primary bindings");
        Entities::PlayerEntity::Reset();
    }

    void PointerCheck::CheckSettings()
    {
        const std::string originalDirectory = Launcher::LauncherPrefs::Directory();
        const std::string directory = Runtime::PathCombine(Runtime::PathGetTempPath(),
            "fruity-pointer-check-" + Runtime::Guid::NewGuid().ToString("N"));
        Runtime::DirectoryCreateDirectory(directory);
        const std::string path = Runtime::PathCombine(directory, "controls.txt");
        const auto cleanup = [&]()
        {
            Launcher::LauncherPrefs::Directory(originalDirectory);
            Runtime::FileDelete(path);
            Runtime::DirectoryDelete(directory);
        };
        try
        {
            Launcher::LauncherPrefs::Directory(directory);
            Runtime::FileWriteAllText(path, "pointer_jump_guard=true\nstylus_zone=true\n");
            InputSettings::Load();
            Require(PointerInput::StylusMode() && PointerInput::GuardJumps() && StylusZone::Enabled(), "legacy enabled file migrates");
            Runtime::FileWriteAllText(path, "pointer_jump_guard=false\n");
            InputSettings::Load();
            Require(!PointerInput::StylusMode(), "legacy disabled file migrates");
            Runtime::FileWriteAllText(path, "stylus_mode=true\npointer_jump_guard=false\n");
            InputSettings::Load();
            Require(PointerInput::StylusMode() && !PointerInput::GuardJumps() && StylusZone::Enabled(),
                "mode independent of guard, explicit setting first");
            Runtime::FileWriteAllText(path, "pointer_jump_guard=false\nstylus_mode=true\n");
            InputSettings::Load();
            Require(PointerInput::StylusMode() && !PointerInput::GuardJumps(), "explicit setting last");
            InputSettings::Save();
            PointerInput::StylusMode(false);
            PointerInput::GuardJumps(true);
            InputSettings::Load();
            Require(PointerInput::StylusMode() && !PointerInput::GuardJumps(), "independent settings round trip");
            InputSettings::Reset();
            Require(!PointerInput::StylusMode() && PointerInput::GuardJumps() && !StylusZone::Enabled(),
                "reset restores ordinary mouse defaults");
        }
        catch (...)
        {
            cleanup();
            throw;
        }
        cleanup();
    }
}
