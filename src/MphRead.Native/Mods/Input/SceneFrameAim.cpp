#include "../../Scene.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../GameState.hpp"
#include "../Chat/ChatBox.hpp"
#include "../Headless.hpp"
#include "../Network/Session/NetSession.hpp"
#include "../PauseMenu.hpp"
#include "GamepadUiRouter.hpp"

namespace MphRead
{
    bool Scene::ModPlayerInputBlocked() const
    {
        return _inputMode == InputMode::CameraOnly
            || Mods::PauseMenu::Open() || Mods::Chat::ChatBox::Composing()
            || (!Mods::Headless::Active() && !Mods::Input::GamepadContexts::Focused());
    }

    void Scene::ModApplyFrameAim()
    {
        if (_mouseState == nullptr || !ProcessFrame() || ModPlayerInputBlocked()
            || GameState::MenuPause() || GameState::DialogPause()
            || Mods::Network::NetSession::FreezeGameplay())
        {
            return;
        }
        if (const auto main = Entities::PlayerEntity::Main())
        {
            main->ModApplyFrameAim(*_mouseState);
        }
    }
}
