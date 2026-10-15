#include "PreviewAnimation.hpp"

#include "../../Formats/Model.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace MphRead::Mods::Render
{
    void PreviewAnimation::Update(ModelInstance& instance)
    {
        const auto now = std::chrono::steady_clock::now();
        // A stalled window (minimized, dragged) resumes instead of replaying
        // every missed 30 Hz tick in one draw.
        if (_last)
            Advance(instance, std::min(std::chrono::duration<double>(now - *_last).count(), 0.25));
        _last = now;
    }

    void PreviewAnimation::Advance(ModelInstance& instance, double seconds)
    {
        if (!std::isfinite(seconds) || seconds < 0) return;
        _phase += seconds * 30.0;
        const auto ticks = static_cast<std::uint64_t>(std::floor(_phase + 1e-9));
        _phase = std::max(0.0, _phase - static_cast<double>(ticks));
        for (std::uint64_t i = 0; i < ticks; ++i) instance.UpdateAnimFrames();
    }

    void PreviewAnimation::Reset() noexcept
    {
        _last.reset();
        _phase = 0;
    }

    float PreviewAnimation::Fraction() const noexcept
    {
        return static_cast<float>(_phase);
    }

    std::int32_t PreviewAnimation::NextNodeFrame(ModelInstance& instance)
    {
        auto& info = *instance.AnimInfo;
        // UpdateAnimFrames only writes these two arrays. Look ahead with the
        // actual loop/reverse/ping-pong/end rules, then restore before drawing.
        struct Restore
        {
            AnimationInfo& Info;
            std::array<std::int32_t, 2> Frames;
            std::array<AnimFlags, 2> Flags;
            ~Restore()
            {
                for (std::size_t i = 0; i < 2; ++i)
                {
                    (*Info.Frame)[i] = Frames[i];
                    (*Info.Flags)[i] = Flags[i];
                }
            }
        } restore{info, {(*info.Frame)[0], (*info.Frame)[1]},
            {(*info.Flags)[0], (*info.Flags)[1]}};
        instance.UpdateAnimFrames();
        return info.NodeFrame();
    }

    thread_local const PreviewNodeInterpolation* PreviewNodeInterpolation::_active = nullptr;

    PreviewNodeInterpolation::PreviewNodeInterpolation(const Model& model,
        const NodeAnimationGroup* group, std::int32_t frame, std::int32_t next,
        float fraction) noexcept
        : _previous(_active), _model(&model), _group(group), _frame(frame),
          _next(next), _fraction(std::clamp(fraction, 0.0F, 1.0F))
    {
        _active = this;
    }

    PreviewNodeInterpolation::~PreviewNodeInterpolation()
    {
        _active = _previous;
    }

    std::optional<float> PreviewNodeInterpolation::Sample(const Model& model,
        const std::shared_ptr<const std::vector<float>>& values, std::int32_t start,
        std::int32_t frame, std::int32_t blend, std::int32_t length,
        std::int32_t count, bool rotation)
    {
        const auto* sample = _active;
        if (!sample || sample->_model != &model || !sample->_group
            || sample->_frame != frame || sample->_frame == sample->_next
            || sample->_fraction <= 0
            || (values != sample->_group->Scales && values != sample->_group->Rotations
                && values != sample->_group->Translations)) return std::nullopt;

        // Endpoint calls use the original LUT decoder, including compressed
        // tails. Suspending the scope prevents recursion and is exception safe.
        struct Suspend
        {
            const PreviewNodeInterpolation*& Active;
            const PreviewNodeInterpolation* Saved;
            ~Suspend() { Active = Saved; }
        } suspend{_active, _active};
        _active = nullptr;
        const float first = model.InterpolateAnimation(values, start, frame,
            blend, length, count, rotation);
        const float second = model.InterpolateAnimation(values, start, sample->_next,
            blend, length, count, rotation);
        float delta = second - first;
        if (rotation) delta = std::remainder(delta, 2.0F * std::numbers::pi_v<float>);
        return first + delta * sample->_fraction;
    }
}
