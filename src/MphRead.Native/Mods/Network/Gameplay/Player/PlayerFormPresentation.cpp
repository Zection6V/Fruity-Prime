#include "Entities/Players/PlayerEntity.hpp"
#include "Mods/Network/Gameplay/Player/FormTransitionDraw.hpp"
#include "Mods/Network/Gameplay/Replication/FormTransitionAuthorityTracker.hpp"
#include "Mods/Network/Gameplay/Replication/NetSmoothing.hpp"
#include "Mods/Network/Session/SessionFormTransition.hpp"
#include "Mods/Network/Session/NetSession.hpp"
#include "Mods/Render/ScenePresentation.hpp"
#include "Scene.hpp"
#include <algorithm>

namespace MphRead::Mods::Network
{
    FormTransitionDraw::FormTransitionDraw(Entities::PlayerEntity& player)
    {
        player.ModPresentFormTransition(*this);
    }
}

namespace MphRead::Entities
{
    Mods::Network::FormClipObservation PlayerEntity::ModObserveFormClip(std::uint32_t frame) const
    {
        Mods::Network::FormClipObservation result{};
        if (_bipedModel2 == nullptr || _bipedModel2->AnimInfo == nullptr) return result;
        const auto& info = *_bipedModel2->AnimInfo;
        if (info.Frame == nullptr || info.FrameCount == nullptr) return result;
        result.Frame = static_cast<std::uint16_t>(std::clamp((*info.Frame)[0], 0, 4095));
        result.Frames = static_cast<std::uint16_t>(std::clamp((*info.FrameCount)[0], 0, 4096));
        result.Paused = _frozenTimer > 0 || (info.Flags != nullptr && ((*info.Flags)[0] & AnimFlags::Paused) != AnimFlags::None);
        // PlayerEntity::UpdateAnimFrames ticks the biped on even scene steps.
        result.AnchorFrame = frame - (result.Paused || _scene == nullptr ? 0U
            : static_cast<std::uint32_t>(_scene->FrameCount() % 2));
        return result;
    }

    void PlayerEntity::ModPresentFormTransition(Mods::Network::FormTransitionDraw& draw)
    {
        using namespace Mods::Network;
        if (!NetSmoothing::Active() || !ModIsInPlay() || SlotIndex() == NetSession::LocalSlot()) return;
        const auto direction = IsMorphing() ? ReplicatedFormState::Transition::Morph
            : IsUnmorphing() ? ReplicatedFormState::Transition::Unmorph : ReplicatedFormState::Transition::None;
        const auto phase = SessionFormTransition::Sample(SlotIndex(),
            NetSmoothing::PresentationFrame(Mods::Render::ScenePresentation::Phase()), direction);
        if (!phase) return;
        const auto animation = direction == ReplicatedFormState::Transition::Morph ? PlayerAnimation::Morph : PlayerAnimation::Unmorph;
        std::size_t index = 0;
        for (const auto& model : {_bipedModel1, _bipedModel2})
        {
            if (model && model->AnimInfo && model->AnimInfo->Index && model->AnimInfo->FrameCount
                && (*model->AnimInfo->Index)[0] == static_cast<std::int32_t>(animation))
                draw._frames[index].Set(*model->AnimInfo, *phase * ((*model->AnimInfo->FrameCount)[0] - 1));
            ++index;
        }
    }
}
