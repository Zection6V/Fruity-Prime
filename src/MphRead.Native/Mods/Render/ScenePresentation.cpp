#include "ScenePresentation.hpp"

#include "../../Entities/EntityBase.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Formats/Effects.hpp"
#include "../../GameState.hpp"
#include "../../Scene.hpp"
#include "../Headless.hpp"
#include "../SimulationRate.hpp"
#include "FrameTiming.hpp"
#include "MotionCarry.hpp"

#include <unordered_map>

namespace MphRead::Mods::Render
{
    namespace
    {
        using OpenTK::Mathematics::Matrix4;
        using OpenTK::Mathematics::Vector3;

        // An entity's pose at the last two steps it was seen in, of one life.
        struct PoseHistory final
        {
            std::uint64_t Step = 0;
            bool HasPrevious = false;
            Matrix4 Previous{};
            Matrix4 Current{};
        };

        std::unordered_map<const Entities::EntityBase*, PoseHistory> poses;
        std::uint64_t step = 0;
        bool advanced = false;
        std::optional<double> forcedPhase;
        std::uint64_t carriedItems = 0;
    }

    void ScenePresentation::BeginStep() noexcept
    {
        advanced = false;
    }

    void ScenePresentation::EndStep(Scene& scene)
    {
        ++step;
        advanced = true;
        // The players' poses are a handful of matrices; every entity's is a
        // map a step, which a headless process, drawing nothing, never reads.
        for (auto it = scene.GetPlayerEntities().GetEnumerator(); it.MoveNext();)
        {
            if (const auto player = it.Current()) player->ModNotePresentationStep(step);
        }
        if (Headless::Active()) return;
        for (auto it = scene.Entities().GetEnumerator(); it.MoveNext();)
        {
            const auto entity = it.Current();
            if (entity == nullptr || entity->Type == EntityType::Room || entity->Type == EntityType::Player) continue;
            PoseHistory& pose = poses[entity.get()];
            const Matrix4 now = static_cast<Matrix4>(entity->Transform);
            pose.HasPrevious = pose.Step + 1 == step;
            pose.Previous = pose.Current;
            pose.Current = now;
            pose.Step = step;
        }
        // An entity not seen this step has left the scene, or is not part of
        // it any more; its address may be a new entity's by the next one.
        std::erase_if(poses, [](const auto& entry) { return entry.second.Step != step; });
    }

    void ScenePresentation::NoteAdded(const Entities::EntityBase& entity)
    {
        poses.erase(&entity);
    }

    void ScenePresentation::Reset()
    {
        poses.clear();
        advanced = false;
        forcedPhase.reset();
    }

    std::uint64_t ScenePresentation::CurrentStep() noexcept
    {
        return step;
    }

    double ScenePresentation::Phase() noexcept
    {
        return forcedPhase.has_value() ? *forcedPhase : FrameTiming::StepPhase();
    }

    void ScenePresentation::ForcePhase(std::optional<double> phase) noexcept
    {
        forcedPhase = phase;
    }

    bool ScenePresentation::Moving() noexcept
    {
        return advanced && Phase() > 0.0;
    }

    std::optional<Matrix4> ScenePresentation::EntityCarry(const Entities::EntityBase& entity)
    {
        if (!Moving()) return std::nullopt;
        const auto found = poses.find(&entity);
        if (found == poses.end() || !found->second.HasPrevious || found->second.Step != step) return std::nullopt;
        // The pose the entity has now is the one its last step recorded; one
        // changed since (by the draw itself) is not the step's motion.
        const Matrix4 now = static_cast<Matrix4>(entity.Transform);
        if (std::memcmp(&now, &found->second.Current, sizeof(Matrix4)) != 0) return std::nullopt;
        return MotionCarry::Between(found->second.Previous, found->second.Current, static_cast<float>(Phase()));
    }

    void ScenePresentation::NoteCarried(std::size_t items) noexcept
    {
        carriedItems += items;
    }

    std::uint64_t ScenePresentation::CarriedItems() noexcept
    {
        return carriedItems;
    }

    ScenePresentation::ParticleCarry::ParticleCarry(Effects::EffectParticle& particle, bool collides) noexcept
    {
        // A particle that stops at what it hits would be drawn through it.
        if (collides || !Moving() || GameState::MatchState() != MatchState::InProgress || GameState::DialogPause())
        {
            return;
        }
        const Vector3 speed = particle.Speed;
        if (speed.LengthSquared() <= 0.0F) return;
        _particle = &particle;
        _position = particle.Position;
        const float seconds = SimulationRate::StepSecondsF * static_cast<float>(Phase());
        particle.Position = _position + OpenTK::Mathematics::Multiply(speed, seconds);
    }

    ScenePresentation::ParticleCarry::~ParticleCarry()
    {
        if (_particle != nullptr) _particle->Position = _position;
    }
}
