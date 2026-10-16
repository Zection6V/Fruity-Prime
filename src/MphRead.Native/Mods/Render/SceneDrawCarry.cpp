#include "../../Scene.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Formats/Effects.hpp"
#include "MotionCarry.hpp"
#include "ScenePresentation.hpp"

namespace MphRead
{
    Scene::ModDrawMark Scene::ModMarkDrawn() const noexcept
    {
        return {_usedRenderItems.size(), _singleParticleCount};
    }

    void Scene::ModCarryDrawn(const ModDrawMark& from, const OpenTK::Mathematics::Matrix4& carry)
    {
        std::size_t carried = 0;
        for (std::size_t i = from.Items; i < _usedRenderItems.size(); ++i)
        {
            if (_usedRenderItems[i] == nullptr) continue;
            Mods::Render::MotionCarry::Apply(*_usedRenderItems[i], carry);
            ++carried;
        }
        for (std::int32_t i = from.Singles; i < _singleParticleCount; ++i)
        {
            auto& single = _singleParticles[static_cast<std::size_t>(i)];
            if (single == nullptr) continue;
            single->Position = Mods::Render::MotionCarry::Point(single->Position, carry);
            ++carried;
        }
        Mods::Render::ScenePresentation::NoteCarried(carried);
    }

    void Scene::ModCarryEntityDrawn(const ModDrawMark& from, const Entities::EntityBase& entity)
    {
        if (const auto carry = Mods::Render::ScenePresentation::EntityCarry(entity)) ModCarryDrawn(from, *carry);
    }

    void Scene::ModCarryPlayerDrawn(const ModDrawMark& from, const Entities::PlayerEntity& player)
    {
        const auto shift = player.ModPresentationShift();
        if (!shift) return;
        if (const auto carry = Mods::Render::MotionCarry::By(*shift)) ModCarryDrawn(from, *carry);
    }
}
