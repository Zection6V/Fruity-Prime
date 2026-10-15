#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace MphRead
{
    class AnimationInfo;
    class Model;
    class ModelInstance;
    class NodeAnimationGroup;
}

namespace MphRead::Mods::Render
{
    // Draw-rate skeletal poses over the game's 30 Hz animation frames.
    //
    // The integer frames, their loop/reverse/ping-pong/end transitions and
    // everything the simulation reads stay exactly as the game advances them.
    // Inside a DrawScope, a node curve is sampled between the current frame
    // and the frame the next tick will show (looked ahead with the game's own
    // UpdateAnimFrames when the tick lands), by how far the instance's clock
    // has run toward that tick. Materials, texture swaps and texcoords keep
    // integer frames.
    //
    // Some simulation code reads node matrices a draw left behind (a
    // platform's attached collision, Fire Spawn's wrists). So every matrix a
    // blended pass wrote is handed back to the integer pose when the outermost
    // DrawScope ends, unless something later in the draw replaced it.
    class AnimationBlend final
    {
    public:
        AnimationBlend() = delete;

        // Hook: ModelInstance::UpdateAnimFrames(), after the frames advanced.
        static void NoteTick(ModelInstance& instance);

        class DrawScope final
        {
        public:
            DrawScope() noexcept;
            ~DrawScope();
            DrawScope(const DrawScope&) = delete;
            DrawScope& operator=(const DrawScope&) = delete;
        };

        // For draw code that derives simulation state from node matrices.
        class Suppress final
        {
        public:
            Suppress() noexcept;
            ~Suppress();
            Suppress(const Suppress&) = delete;
            Suppress& operator=(const Suppress&) = delete;
        };

        // Hook: Model::AnimateNodes / AnimateNodes2. When the instance can be
        // blended, `integer` runs the same pass first, unblended, so the
        // integer pose is known; the body that follows runs blended.
        class NodePass final
        {
        public:
            template<class Integer>
            NodePass(const Model& model, std::int32_t index,
                const std::shared_ptr<AnimationInfo>& info, Integer&& integer)
            {
                if (!Plan(model, info)) return;
                try
                {
                    integer();
                    Begin(model, index);
                }
                catch (...)
                {
                    End();
                    throw;
                }
                _engaged = true;
            }
            ~NodePass();
            NodePass(const NodePass&) = delete;
            NodePass& operator=(const NodePass&) = delete;

        private:
            static bool Plan(const Model& model, const std::shared_ptr<AnimationInfo>& info);
            static void Begin(const Model& model, std::int32_t index);
            static void End() noexcept;
            bool _engaged = false;
        };

        // Hook: Model::InterpolateAnimation. A value while a NodePass is
        // blending one of its group's node curves.
        [[nodiscard]] static std::optional<float> Sample(const Model& model,
            const std::shared_ptr<const std::vector<float>>& values, std::int32_t start,
            std::int32_t frame, std::int32_t blend, std::int32_t length,
            std::int32_t count, bool rotation);

        // How far the instance's draw is toward its next frame, 0 when it
        // would not be blended. For checks.
        [[nodiscard]] static float Fraction(const AnimationInfo& info);
    };
}
