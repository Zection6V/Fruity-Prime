#pragma once

#include "AnimationTick.hpp"
#include "Mods/SimulationRate.hpp"
#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead
{
    class AnimationInfo;
    class Material;
    class Model;
    class ModelInstance;
    struct TexcoordAnimation;
}

namespace MphRead::Mods::Render
{
    // Draw-rate animation over the game's 30 Hz animation frames.
    //
    // The integer frames, their loop/reverse/ping-pong/end transitions and
    // everything the simulation reads stay exactly as the game advances them.
    // Inside a DrawScope, a node, material colour or texcoord curve is sampled
    // between the current frame and the frame the next tick will show (looked
    // ahead with the game's own UpdateAnimFrames when the tick lands), by how
    // far the instance's clock has run toward that tick. Texture swaps keep
    // integer frames: there is nothing between two pictures.
    //
    // Some simulation code reads what a draw left behind -- node matrices (a
    // platform's attached collision, Fire Spawn's wrists) and material colours
    // (Gorea's arm alpha). So every node matrix and material colour a blended
    // pass wrote is handed back to its integer value when the outermost
    // DrawScope ends, unless something later in the draw replaced it.
    class AnimationBlend final
    {
    public:
        AnimationBlend() = delete;

        // The Animation setting, in Hz: how often a draw may show a new pose
        // between the 30 Hz frames, on the FPS limit's stops
        // (FrameTiming::RateStops). DsRate draws the frames as the DS did;
        // DisplayRate (the stops' Unlimited) blends at whatever the screen
        // draws. Ticks are noted at any rate, so a change shows on the next
        // draw.
        static constexpr std::int32_t DsRate = SimulationRate::DsHz;
        static constexpr std::int32_t DisplayRate = -1;
        [[nodiscard]] static std::int32_t Rate() noexcept;
        // Anything below DsRate but DisplayRate reads as DsRate.
        static void Rate(std::int32_t hz) noexcept;
        // Where, in steps from `step` (the latest simulation step), a pose a
        // picture `phase` of the way to the next step shows: at the display
        // rate, `phase` itself; otherwise the latest instant on the rate's
        // grid, so 30 Hz holds each pose for two steps (-1 on the second),
        // 60 shows the step's own and 120 half a step on. For what the game
        // turns every step but the setting calls animation: the morph ball's
        // roll, Noxus's spin, the alt forms' tilt.
        [[nodiscard]] static double ShownStepFraction(std::uint64_t step, double phase) noexcept;
        // "display", or the rate in Hz: launcher.txt and -animrate.
        [[nodiscard]] static std::string RateName(std::int32_t hz);
        [[nodiscard]] static std::optional<std::int32_t> ParseRate(std::string_view text) noexcept;

        // Hook: ModelInstance::UpdateAnimFrames(), after the frames advanced.
        static void NoteTick(ModelInstance& instance);

        // Samples a specific continuous clip frame during one draw. Integer
        // animation frames and gameplay flags are never changed.
        class FrameOverride final
        {
        public:
            FrameOverride() = default;
            ~FrameOverride();
            FrameOverride(const FrameOverride&) = delete;
            FrameOverride& operator=(const FrameOverride&) = delete;
            void Set(AnimationInfo& info, float frame);
        private:
            AnimationInfo* _info = nullptr;
            float _drawFrame = -1.0F;
        };

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

        // Hook: around the code that samples one channel of one instance --
        // Model::AnimateNodes / AnimateNodes2 (from `index`), AnimateMaterials,
        // and the texcoord matrix. Where the channel leaves state behind,
        // `integer` runs the same pass first, unblended, so the integer values
        // are known; the body that follows runs blended. A texcoord matrix is
        // returned, not kept, and needs no integer pass.
        class Pass final
        {
        public:
            Pass(const Model& model, AnimationChannel channel,
                const std::shared_ptr<AnimationInfo>& info)
            {
                if (!Plan(model, channel, info)) return;
                Begin(model, channel, -1);
                _engaged = true;
            }

            template<class Integer>
            Pass(const Model& model, AnimationChannel channel, std::int32_t index,
                const std::shared_ptr<AnimationInfo>& info, Integer&& integer)
            {
                if (!Plan(model, channel, info)) return;
                try
                {
                    integer();
                    Begin(model, channel, index);
                }
                catch (...)
                {
                    End();
                    throw;
                }
                _engaged = true;
            }

            ~Pass();
            Pass(const Pass&) = delete;
            Pass& operator=(const Pass&) = delete;

        private:
            static bool Plan(const Model& model, AnimationChannel channel,
                const std::shared_ptr<AnimationInfo>& info);
            static void Begin(const Model& model, AnimationChannel channel, std::int32_t index);
            static void End() noexcept;
            bool _engaged = false;
        };

        // Hook: EntityBase::GetTexcoordMatrix. Blend UV curves with the same
        // clock, respecting each axis's sampler period at every frame pair.
        [[nodiscard]] static OpenTK::Mathematics::Matrix4 Texcoords(Model& model,
            const std::shared_ptr<AnimationInfo>& info, const Material& material,
            const TexcoordAnimation& animation);

        // Hook: Model::InterpolateAnimation. A value while a Pass is blending
        // one of its channel's curves.
        [[nodiscard]] static std::optional<float> Sample(const Model& model,
            const std::shared_ptr<const std::vector<float>>& values, std::int32_t start,
            std::int32_t frame, std::int32_t blend, std::int32_t length,
            std::int32_t count, bool rotation);

        // The frame this draw shows, part of the way to the next one where the
        // draw is blending toward it: what a value the game reads off the
        // integer frame (a fade) reads at the picture's instant.
        [[nodiscard]] static float FrameShown(const AnimationInfo& info, std::size_t slot = 0);

        // How far the channel's draw is toward its next frame, 0 when it would
        // not be blended. For checks.
        [[nodiscard]] static float Fraction(const AnimationInfo& info,
            AnimationChannel channel = AnimationChannel::Node);
    };
}
