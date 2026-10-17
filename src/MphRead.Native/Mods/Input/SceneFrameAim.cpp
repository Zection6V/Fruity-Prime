#include "../../Scene.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../GameState.hpp"
#include "../Chat/ChatBox.hpp"
#include "../Headless.hpp"
#include "../Network/Gameplay/Replication/NetSmoothing.hpp"
#include "../Network/Session/NetSession.hpp"
#include "../PauseMenu.hpp"
#include "../SpectatorMode.hpp"
#include "GamepadUiRouter.hpp"
#include "../Render/FrameTiming.hpp"

namespace MphRead
{
    bool Scene::ModPlayerInputBlocked() const
    {
        return _inputMode == InputMode::CameraOnly
            || Mods::PauseMenu::Open() || Mods::Chat::ChatBox::Composing()
            || (!Mods::Headless::Active() && !Mods::Input::GamepadContexts::Focused());
    }

    std::shared_ptr<Entities::PlayerEntity> Scene::ModFrameAimPlayer() const
    {
        if (_mouseState == nullptr || !ProcessFrame() || ModPlayerInputBlocked()
            || GameState::MenuPause() || GameState::DialogPause()
            || Mods::Network::NetSession::FreezeGameplay())
        {
            return nullptr;
        }
        return Entities::PlayerEntity::Main();
    }

    void Scene::ModApplyFrameInput()
    {
        // The free camera is a picture and nothing else: it moves at the
        // display's rate by the frame's own time (MoveRoamCamera blocks
        // itself where its input belongs to something else).
        MoveRoamCamera(static_cast<float>(Mods::Render::FrameTiming::FrameSteps()));
        if (const auto main = ModFrameAimPlayer())
        {
            main->ModApplyFrameAim(*_mouseState);
        }
    }

    Mods::Input::FrameAimPresentation Scene::ModPresentFrameInput()
    {
        if (Mods::Headless::Active()) return {};
        if (Mods::SpectatorMode::IsSpectating())
        {
            // The watched player, at this picture's instant: a replica on the
            // playout clock -- never past what has arrived, so nothing is
            // guessed, and nothing held back either -- and a player simulated
            // here (offline, or on the authority) carried on from its step.
            const auto target = Mods::SpectatorMode::CanUsePlayerCamera() ? Entities::PlayerEntity::Main() : nullptr;
            std::optional<OpenTK::Mathematics::Vector3> playout;
            if (target != nullptr && Mods::Network::NetSmoothing::Active())
            {
                playout = Mods::Network::NetSmoothing::AheadShift(target->SlotIndex(), Mods::Render::FrameTiming::StepPhase());
            }
            if (target == nullptr || !ProcessFrame() || !target->ModPresentWatchedView(playout))
            {
                return {};
            }
            return Mods::Input::FrameAimPresentation(target);
        }
        if (!ProcessFrame() || GameState::MenuPause() || GameState::DialogPause()
            || Mods::Network::NetSession::FreezeGameplay())
        {
            return {};
        }
        const auto main = Entities::PlayerEntity::Main();
        const auto aiming = ModFrameAimPlayer();
        if (main == nullptr || !main->ModPresentOwnView(aiming != nullptr ? _mouseState : nullptr)) return {};
        return Mods::Input::FrameAimPresentation(main);
    }

    void Scene::ModPresentFrameReticle()
    {
        if (const auto main = Entities::PlayerEntity::Main())
        {
            main->ModPresentFrameReticle();
        }
    }
}
