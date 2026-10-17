#include "../../Scene.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Entities/BeamProjectileEntity.hpp"
#include "../../Formats/Effects.hpp"
#include "MotionCarry.hpp"
#include "ScenePresentation.hpp"

namespace MphRead
{
    Scene::ModDrawMark Scene::ModMarkDrawn() const noexcept
    {
        return {_usedRenderItems.size(), _singleParticleCount};
    }

    void Scene::ModCarryDrawn(const ModDrawMark& from, const OpenTK::Mathematics::Matrix4& carry,
        std::optional<RenderItemType> keep)
    {
        std::size_t carried = 0;
        for (std::size_t i = from.Items; i < _usedRenderItems.size(); ++i)
        {
            if (_usedRenderItems[i] == nullptr || (keep && _usedRenderItems[i]->Type == *keep)) continue;
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

    namespace
    {
        // A continuous beam (the Shock Coil, the Judicator's...) is fired again
        // every step and lives one, drawn out of the muzzle it left: a new life
        // each step, with no motion of its own to carry, under a gun that moves
        // every frame. It is drawn out of the gun the picture has instead.
        std::optional<OpenTK::Mathematics::Matrix4> BeamFromGun(const Entities::EntityBase& entity)
        {
            if (entity.Type != EntityType::BeamProjectile) return std::nullopt;
            const auto& beam = static_cast<const Entities::BeamProjectileEntity&>(entity);
            if (!TypeExtensions::TestFlag(beam.Flags(), Entities::BeamFlags::Continuous)) return std::nullopt;
            const auto owner = beam.Owner();
            if (owner == nullptr || owner->Type != EntityType::Player) return std::nullopt;
            const auto gun = static_cast<const Entities::PlayerEntity&>(*owner).ModPresentedMuzzle();
            if (!gun) return std::nullopt;
            // Where its trail is drawn from (DrawTrail4) to where it is.
            const OpenTK::Mathematics::Vector3 origin = beam.PastPositions()[8];
            const OpenTK::Mathematics::Vector3 tip = beam.Position;
            // Locked on a player, it ends on them as the picture has them --
            // the player it is locked on is often the one watching.
            const auto target = beam.Target();
            if (target != nullptr && target->Type == EntityType::Player)
            {
                const auto& locked = static_cast<const Entities::PlayerEntity&>(*target);
                return Mods::Render::MotionCarry::Span(origin, tip, gun->first, locked.ModPresentedPoint(tip));
            }
            // Otherwise it is drawn the way it was fired, out of the gun.
            return Mods::Render::MotionCarry::Anchor(origin, beam.Direction(), gun->first, gun->second);
        }
    }

    void Scene::ModCarryEntityDrawn(const ModDrawMark& from, const Entities::EntityBase& entity)
    {
        if (const auto carry = BeamFromGun(entity)) ModCarryDrawn(from, *carry);
        else if (const auto carry = Mods::Render::ScenePresentation::EntityCarry(entity)) ModCarryDrawn(from, *carry);
    }

    void Scene::ModCarryPlayerDrawn(const ModDrawMark& from, const Entities::PlayerEntity& player)
    {
        // The morph ball's trail is where the ball has been, laid a step at a
        // time: it stays where the steps left it (its head kept on the ball
        // by ModTrailSegment). Carried with the ball's roll, it swung up and
        // down, the more the more pictures were drawn.
        if (const auto carry = player.ModPresentationCarry()) ModCarryDrawn(from, *carry, RenderItemType::TrailStack);
    }
}
