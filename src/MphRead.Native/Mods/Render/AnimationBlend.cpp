#include "AnimationBlend.hpp"

#include "AnimationClock.hpp"
#include "../../Formats/Model.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <numbers>
#include <unordered_set>

namespace MphRead::Mods::Render
{
    namespace
    {
        // A tick further apart than this is a resumption, not a cadence.
        constexpr double MaxInterval = 8.0;

        struct ActiveSample
        {
            const Model* Model = nullptr;
            const NodeAnimationGroup* Group = nullptr;
            std::int32_t Frame = 0;
            std::int32_t Next = 0;
            float Fraction = 0;
        };

        struct Written
        {
            std::shared_ptr<Node> Target;
            OpenTK::Mathematics::Matrix4 Integer;
            OpenTK::Mathematics::Matrix4 Blended;
        };

        struct DrawState
        {
            std::int32_t Scopes = 0;
            std::int32_t Suppressed = 0;
            bool Busy = false;
            bool LookingAhead = false;
            std::optional<ActiveSample> Planned{};
            std::optional<ActiveSample> Active{};
            // Each blended pass's subtree, integer then blended.
            std::vector<Written> Log{};
            std::size_t PassStart = 0;
            std::unordered_set<const Node*> Seen{};
        };

        thread_local DrawState state;

        std::size_t NodeSlot(const AnimationInfo& info)
        {
            return static_cast<std::size_t>(NativeRuntime::RequireReference(info.Node).Slot);
        }

        bool Same(const OpenTK::Mathematics::Matrix4& a, const OpenTK::Mathematics::Matrix4& b)
        {
            return std::memcmp(&a, &b, sizeof(a)) == 0;
        }

        // The nodes AnimateNodes/AnimateNodes2 write from `index`: the sibling
        // chain and, unless a node says otherwise, its children.
        template<class Visit>
        void Walk(const Model& model, std::int32_t index, Visit&& visit)
        {
            const auto& nodes = NativeRuntime::RequireReference(model.Nodes);
            for (std::int32_t i = index; i != -1;)
            {
                const std::shared_ptr<Node>& node = nodes.at(static_cast<std::size_t>(i));
                visit(node);
                if (node->ChildIndex != -1 && !node->AnimIgnoreChild)
                {
                    Walk(model, node->ChildIndex, visit);
                }
                i = node->NextIndex;
            }
        }

        // The frame the node slot will show after the next tick, by the game's
        // own state machine. UpdateAnimFrames writes only Frame and Flags.
        std::int32_t LookAhead(ModelInstance& instance)
        {
            AnimationInfo& info = NativeRuntime::RequireReference(instance.AnimInfo);
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
                    state.LookingAhead = false;
                }
            } restore{info, {(*info.Frame)[0], (*info.Frame)[1]},
                {(*info.Flags)[0], (*info.Flags)[1]}};
            state.LookingAhead = true;
            instance.UpdateAnimFrames();
            return info.NodeFrame();
        }

        std::optional<ActiveSample> SampleFor(const AnimationInfo& info)
        {
            const AnimationTick& tick = info.ModTick;
            const NodeAnimationGroup* group
                = NativeRuntime::RequireReference(info.Node).Group.get();
            if (tick.Clock == nullptr || tick.Interval <= 0 || group == nullptr
                || tick.Group != group || tick.Next == tick.Frame
                || info.NodeFrame() != tick.Frame
                || static_cast<std::uint16_t>((*info.Flags)[NodeSlot(info)]) != tick.Flags)
            {
                return std::nullopt;
            }
            // Past due (a paused world, an entity that stopped ticking) holds
            // the next frame, which is the pose the next tick lands on.
            const double fraction = std::clamp(
                (tick.Clock->DrawTime() - tick.Time) / tick.Interval, 0.0, 1.0);
            if (fraction <= 0) return std::nullopt;
            return ActiveSample{nullptr, group, tick.Frame, tick.Next, static_cast<float>(fraction)};
        }

        void Restore()
        {
            // Newest first: the last pass to write a node decides what it was.
            for (auto it = state.Log.rbegin(); it != state.Log.rend(); ++it)
            {
                if (!state.Seen.insert(it->Target.get()).second) continue;
                if (Same(it->Target->Animation, it->Blended))
                {
                    it->Target->Animation = it->Integer;
                }
            }
            state.Log.clear();
            state.Seen.clear();
        }
    }

    void AnimationBlend::NoteTick(ModelInstance& instance)
    {
        if (state.LookingAhead) return;
        AnimationInfo& info = NativeRuntime::RequireReference(instance.AnimInfo);
        AnimationTick& tick = info.ModTick;
        const AnimationClock* clock = AnimationClock::Ticking();
        if (clock == nullptr)
        {
            tick = {};
            return;
        }
        const double time = clock->TickTime();
        if (tick.Clock != clock || time < tick.Time)
        {
            tick.Interval = 0;
        }
        else if (time > tick.Time)
        {
            const double interval = time - tick.Time;
            tick.Interval = interval <= MaxInterval ? interval : 0;
        }
        tick.Clock = clock;
        tick.Time = time;
        tick.Group = NativeRuntime::RequireReference(info.Node).Group.get();
        tick.Frame = info.NodeFrame();
        tick.Flags = static_cast<std::uint16_t>((*info.Flags)[NodeSlot(info)]);
        tick.Next = tick.Group != nullptr ? LookAhead(instance) : tick.Frame;
    }

    float AnimationBlend::Fraction(const AnimationInfo& info)
    {
        const auto sample = SampleFor(info);
        return sample ? sample->Fraction : 0.0F;
    }

    AnimationBlend::DrawScope::DrawScope() noexcept
    {
        ++state.Scopes;
    }

    AnimationBlend::DrawScope::~DrawScope()
    {
        if (--state.Scopes == 0) Restore();
    }

    AnimationBlend::Suppress::Suppress() noexcept
    {
        ++state.Suppressed;
    }

    AnimationBlend::Suppress::~Suppress()
    {
        --state.Suppressed;
    }

    bool AnimationBlend::NodePass::Plan(const Model& model, const std::shared_ptr<AnimationInfo>& info)
    {
        if (state.Scopes == 0 || state.Suppressed != 0 || state.Busy || !info) return false;
        state.Planned = SampleFor(*info);
        if (!state.Planned) return false;
        state.Planned->Model = &model;
        // The integer pass and every recursion of the blended one are inert.
        state.Busy = true;
        return true;
    }

    void AnimationBlend::NodePass::Begin(const Model& model, std::int32_t index)
    {
        state.PassStart = state.Log.size();
        Walk(model, index, [](const std::shared_ptr<Node>& node)
        {
            state.Log.push_back({node, node->Animation, node->Animation});
        });
        state.Active = state.Planned;
    }

    AnimationBlend::NodePass::~NodePass()
    {
        if (!_engaged) return;
        for (std::size_t i = state.PassStart; i < state.Log.size(); ++i)
        {
            state.Log[i].Blended = state.Log[i].Target->Animation;
        }
        End();
    }

    void AnimationBlend::NodePass::End() noexcept
    {
        state.Active.reset();
        state.Planned.reset();
        state.Busy = false;
    }

    std::optional<float> AnimationBlend::Sample(const Model& model,
        const std::shared_ptr<const std::vector<float>>& values, std::int32_t start,
        std::int32_t frame, std::int32_t blend, std::int32_t length,
        std::int32_t count, bool rotation)
    {
        if (!state.Active) return std::nullopt;
        const ActiveSample sample = *state.Active;
        if (sample.Model != &model || sample.Frame != frame
            || (values != sample.Group->Scales && values != sample.Group->Rotations
                && values != sample.Group->Translations)) return std::nullopt;

        // Both ends come from the game's own LUT decoder, compressed tails
        // included. Suspending the sample prevents recursion, exception safe.
        struct Suspend
        {
            std::optional<ActiveSample>& Slot;
            ActiveSample Saved;
            ~Suspend() { Slot = Saved; }
        } suspend{state.Active, sample};
        state.Active.reset();
        const float first = model.InterpolateAnimation(values, start, frame,
            blend, length, count, rotation);
        const float second = model.InterpolateAnimation(values, start, sample.Next,
            blend, length, count, rotation);
        float delta = second - first;
        if (rotation) delta = std::remainder(delta, 2.0F * std::numbers::pi_v<float>);
        return first + delta * sample.Fraction;
    }
}
