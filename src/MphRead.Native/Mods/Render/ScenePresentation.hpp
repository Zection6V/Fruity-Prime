#pragma once

#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

#include <cstdint>
#include <optional>

namespace MphRead
{
    class Scene;
}

namespace MphRead::Effects
{
    class EffectParticle;
}

namespace MphRead::Entities
{
    class EntityBase;
}

namespace MphRead::Mods::Render
{
    // The world drawn at the picture's instant. The simulation steps at
    // SimulationRate::Hz and the picture is drawn at the display's rate;
    // between steps, everything that moved in the last step is drawn moved
    // on by that step's own motion (MotionCarry), by as much of a step as
    // the picture is into the next one. Only what was drawn is moved -- the
    // render items an entity produced, a particle while it is drawn -- so
    // nothing the simulation reads ever sees a carried pose.
    class ScenePresentation final
    {
    public:
        ScenePresentation() = delete;

        // Hook: the top of Scene::OnSimulationFrame. A step that returns
        // early (a frozen session) moved nothing, and nothing is carried.
        static void BeginStep() noexcept;
        // Hook: the end of a step that ran the world. Every entity's pose,
        // as the step left it, against the one before.
        static void EndStep(Scene& scene);
        // Hook: Scene::AddEntity. A new life -- a pooled shot reused
        // included -- has no motion behind it.
        static void NoteAdded(const Entities::EntityBase& entity);
        static void Reset();

        // The steps that ran the world so far, which pose histories are stamped with.
        [[nodiscard]] static std::uint64_t CurrentStep() noexcept;
        // How far into the next step this picture is, [0, 1).
        [[nodiscard]] static double Phase() noexcept;
        // A harness drawing several pictures a step sets the instant of each.
        static void ForcePhase(std::optional<double> phase) noexcept;
        // The last step moved the world and this picture is past it.
        [[nodiscard]] static bool Moving() noexcept;
        // How far on from its step a picture shows what moved: Phase while
        // Moving, nothing on the step's own instant.
        [[nodiscard]] static float ShownPhase() noexcept;

        // An entity's drawn items, carried on from its last step.
        [[nodiscard]] static std::optional<OpenTK::Mathematics::Matrix4> EntityCarry(
            const Entities::EntityBase& entity);
        // How many drawn items were carried, for the harness.
        static void NoteCarried(std::size_t items) noexcept;
        [[nodiscard]] static std::uint64_t CarriedItems() noexcept;

        // While alive, one effect particle is drawn where its own speed takes
        // it by the picture's instant (ProcessEffects moves it a step's worth
        // a step); it is put back when the scope ends.
        class ParticleCarry final
        {
        public:
            ParticleCarry(Effects::EffectParticle& particle, bool collides) noexcept;
            ~ParticleCarry();
            ParticleCarry(const ParticleCarry&) = delete;
            ParticleCarry& operator=(const ParticleCarry&) = delete;

        private:
            Effects::EffectParticle* _particle = nullptr;
            OpenTK::Mathematics::Vector3 _position{};
        };
    };
}
