#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace MphRead
{
    class Model;
    class ModelInstance;
    class NodeAnimationGroup;
}

namespace MphRead::Mods::Render
{
    // Only the preview owns this clock. Integer animation state still advances
    // through the original 30 Hz UpdateAnimFrames state machine.
    class PreviewAnimation final
    {
    public:
        void Update(ModelInstance& instance);
        void Advance(ModelInstance& instance, double seconds);
        void Reset() noexcept;
        [[nodiscard]] float Fraction() const noexcept;
        [[nodiscard]] static std::int32_t NextNodeFrame(ModelInstance& instance);

    private:
        double _phase = 0;
        std::optional<std::chrono::steady_clock::time_point> _last{};
    };

    // Scoped to one preview draw and its node curves; materials, texture swaps,
    // other models and gameplay continue to use integer frames. The model's
    // existing curve decoder and hierarchy remain the single source of truth.
    class PreviewNodeInterpolation final
    {
    public:
        PreviewNodeInterpolation(const Model& model, const NodeAnimationGroup* group,
            std::int32_t frame, std::int32_t next, float fraction) noexcept;
        ~PreviewNodeInterpolation();
        PreviewNodeInterpolation(const PreviewNodeInterpolation&) = delete;
        PreviewNodeInterpolation& operator=(const PreviewNodeInterpolation&) = delete;

        [[nodiscard]] static std::optional<float> Sample(const Model& model,
            const std::shared_ptr<const std::vector<float>>& values, std::int32_t start,
            std::int32_t frame, std::int32_t blend, std::int32_t length,
            std::int32_t count, bool rotation);

    private:
        static thread_local const PreviewNodeInterpolation* _active;
        const PreviewNodeInterpolation* _previous;
        const Model* _model;
        const NodeAnimationGroup* _group;
        std::int32_t _frame;
        std::int32_t _next;
        float _fraction;
    };
}
