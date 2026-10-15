#include "AnimationBlend.hpp"

#include "AnimationClock.hpp"
#include "FrameTiming.hpp"
#include "../../Formats/Model.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstring>
#include <numbers>
#include <unordered_set>

namespace MphRead::Mods::Render
{
    namespace
    {
        using OpenTK::Mathematics::Matrix4;
        using OpenTK::Mathematics::Vector3;

        // A tick further apart than this is a resumption, not a cadence.
        constexpr double MaxInterval = 8.0;

        struct ActiveSample
        {
            const Model* Model = nullptr;
            // The value arrays of the channel's group; any other curve is left alone.
            std::array<const std::vector<float>*, 3> Curves{};
            std::int32_t Frame = 0;
            std::int32_t Next = 0;
            float Fraction = 0;
        };

        struct MaterialColors
        {
            Vector3 Diffuse;
            Vector3 Ambient;
            Vector3 Specular;
            float Alpha;
        };

        // What a blended pass wrote over an integer value, for the restore.
        template<class Target, class Value>
        struct Written
        {
            std::shared_ptr<Target> Owner;
            Value Integer;
            Value Blended;
        };

        struct DrawState
        {
            std::int32_t Scopes = 0;
            std::int32_t Suppressed = 0;
            bool Busy = false;
            bool LookingAhead = false;
            std::optional<ActiveSample> Planned{};
            std::optional<ActiveSample> Active{};
            AnimationChannel Channel = AnimationChannel::Node;
            std::vector<Written<Node, Matrix4>> Nodes{};
            std::vector<Written<Material, MaterialColors>> Materials{};
            std::size_t PassStart = 0;
            std::unordered_set<const void*> Seen{};
        };

        thread_local DrawState state;
        std::int32_t rate = AnimationBlend::DisplayRate;

        template<class Value>
        bool Same(const Value& a, const Value& b)
        {
            return std::memcmp(&a, &b, sizeof(Value)) == 0;
        }

        MaterialColors ColorsOf(const Material& material)
        {
            return {material.CurrentDiffuse, material.CurrentAmbient,
                material.CurrentSpecular, material.CurrentAlpha};
        }

        void SetColors(Material& material, const MaterialColors& colors)
        {
            material.CurrentDiffuse = colors.Diffuse;
            material.CurrentAmbient = colors.Ambient;
            material.CurrentSpecular = colors.Specular;
            material.CurrentAlpha = colors.Alpha;
        }

        // The slot a channel reads its frame from, and the group it animates.
        struct ChannelState
        {
            std::size_t Slot;
            const void* Group;
            std::array<const std::vector<float>*, 3> Curves;
        };

        ChannelState Channel(const AnimationInfo& info, AnimationChannel channel)
        {
            using NativeRuntime::RequireReference;
            switch (channel)
            {
            case AnimationChannel::Node:
            {
                const auto& slot = RequireReference(info.Node);
                const NodeAnimationGroup* group = slot.Group.get();
                return {static_cast<std::size_t>(slot.Slot), group, group == nullptr
                    ? std::array<const std::vector<float>*, 3>{}
                    : std::array{group->Scales.get(), group->Rotations.get(), group->Translations.get()}};
            }
            case AnimationChannel::Material:
            {
                const auto& slot = RequireReference(info.Material);
                const MaterialAnimationGroup* group = slot.Group.get();
                return {static_cast<std::size_t>(slot.Slot), group, group == nullptr
                    ? std::array<const std::vector<float>*, 3>{}
                    : std::array<const std::vector<float>*, 3>{group->Colors.get(), nullptr, nullptr}};
            }
            case AnimationChannel::Texcoord:
            default:
            {
                const auto& slot = RequireReference(info.Texcoord);
                const TexcoordAnimationGroup* group = slot.Group.get();
                return {static_cast<std::size_t>(slot.Slot), group, group == nullptr
                    ? std::array<const std::vector<float>*, 3>{}
                    : std::array{group->Scales.get(), group->Rotations.get(), group->Translations.get()}};
            }
            }
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

        // The frames both slots will show after the next tick, by the game's
        // own state machine. UpdateAnimFrames writes only Frame and Flags.
        std::array<std::int32_t, 2> LookAhead(ModelInstance& instance)
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
            return {(*info.Frame)[0], (*info.Frame)[1]};
        }

        std::optional<ActiveSample> SampleFor(const AnimationInfo& info, AnimationChannel channel)
        {
            const AnimationTick& tick = info.ModTick;
            const ChannelState current = Channel(info, channel);
            const std::size_t slot = current.Slot;
            if (tick.Clock == nullptr || tick.Interval <= 0 || current.Group == nullptr || slot > 1
                || tick.Group[static_cast<std::size_t>(channel)] != current.Group
                || tick.Next[slot] == tick.Frame[slot]
                || (*info.Frame)[slot] != tick.Frame[slot]
                || static_cast<std::uint16_t>((*info.Flags)[slot]) != tick.Flags[slot])
            {
                return std::nullopt;
            }
            // Past due (a paused world, an entity that stopped ticking) holds
            // the next frame, which is the value the next tick lands on.
            double fraction = std::clamp(
                (tick.Clock->DrawTime() - tick.Time) / tick.Interval, 0.0, 1.0);
            // The Animation setting: as many poses between two frames as the
            // chosen rate fits in their interval; one is the frame itself.
            if (rate != AnimationBlend::DisplayRate)
            {
                const double slices = std::round(tick.Interval * tick.Clock->SecondsPerStep() * rate);
                if (slices <= 1) return std::nullopt;
                fraction = std::floor(fraction * slices + 1e-9) / slices;
            }
            if (fraction <= 0) return std::nullopt;
            return ActiveSample{nullptr, current.Curves, tick.Frame[slot], tick.Next[slot],
                static_cast<float>(fraction)};
        }

        Matrix4 PoseOf(const Node& node)
        {
            return node.Animation;
        }

        void SetPose(Node& node, const Matrix4& pose)
        {
            node.Animation = pose;
        }

        template<class Target, class Value, class Get>
        void Record(std::vector<Written<Target, Value>>& log, const std::shared_ptr<Target>& owner, Get&& get)
        {
            const Value value = get(*owner);
            log.push_back({owner, value, value});
        }

        template<class Target, class Value, class Get>
        void RecordBlended(std::vector<Written<Target, Value>>& log, Get&& get)
        {
            for (std::size_t i = state.PassStart; i < log.size(); ++i)
            {
                log[i].Blended = get(*log[i].Owner);
            }
        }

        template<class Target, class Value, class Get, class Set>
        void RestoreAll(std::vector<Written<Target, Value>>& log, Get&& get, Set&& set)
        {
            // Newest first: the last pass to write a value decides what it was.
            for (auto it = log.rbegin(); it != log.rend(); ++it)
            {
                if (!state.Seen.insert(it->Owner.get()).second) continue;
                if (Same(get(*it->Owner), it->Blended)) set(*it->Owner, it->Integer);
            }
            log.clear();
            state.Seen.clear();
        }

        void Restore()
        {
            RestoreAll(state.Nodes, PoseOf, SetPose);
            RestoreAll(state.Materials, ColorsOf, SetColors);
        }
    }

    static_assert(AnimationBlend::DisplayRate == FrameTiming::Unlimited
        && AnimationBlend::DsRate == FrameTiming::RateStops.front(),
        "the Animation row shares the FPS limit's stops");

    std::int32_t AnimationBlend::Rate() noexcept
    {
        return rate;
    }

    void AnimationBlend::Rate(std::int32_t hz) noexcept
    {
        rate = hz == DisplayRate ? DisplayRate : std::max(hz, DsRate);
    }

    std::string AnimationBlend::RateName(std::int32_t hz)
    {
        return hz == DisplayRate ? std::string("display") : std::to_string(hz);
    }

    std::optional<std::int32_t> AnimationBlend::ParseRate(std::string_view text) noexcept
    {
        if (text == "display") return DisplayRate;
        if (text.ends_with("hz")) text.remove_suffix(2);
        std::int32_t hz = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), hz);
        if (error != std::errc{} || end != text.data() + text.size() || hz < DsRate) return std::nullopt;
        return hz;
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
        bool animated = false;
        for (const auto channel : {AnimationChannel::Node, AnimationChannel::Material, AnimationChannel::Texcoord})
        {
            const void* group = Channel(info, channel).Group;
            tick.Group[static_cast<std::size_t>(channel)] = group;
            animated = animated || group != nullptr;
        }
        for (std::size_t slot = 0; slot < 2; ++slot)
        {
            tick.Frame[slot] = (*info.Frame)[slot];
            tick.Flags[slot] = static_cast<std::uint16_t>((*info.Flags)[slot]);
        }
        tick.Next = animated ? LookAhead(instance) : tick.Frame;
    }

    float AnimationBlend::Fraction(const AnimationInfo& info, AnimationChannel channel)
    {
        const auto sample = SampleFor(info, channel);
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

    bool AnimationBlend::Pass::Plan(const Model& model, AnimationChannel channel,
        const std::shared_ptr<AnimationInfo>& info)
    {
        if (state.Scopes == 0 || state.Suppressed != 0 || state.Busy || !info) return false;
        state.Planned = SampleFor(*info, channel);
        if (!state.Planned) return false;
        state.Planned->Model = &model;
        state.Channel = channel;
        // The integer pass and every recursion of the blended one are inert.
        state.Busy = true;
        return true;
    }

    void AnimationBlend::Pass::Begin(const Model& model, AnimationChannel channel, std::int32_t index)
    {
        if (channel == AnimationChannel::Node)
        {
            state.PassStart = state.Nodes.size();
            Walk(model, index, [](const std::shared_ptr<Node>& node)
            {
                Record(state.Nodes, node, PoseOf);
            });
        }
        else if (channel == AnimationChannel::Material)
        {
            state.PassStart = state.Materials.size();
            for (const auto& material : NativeRuntime::RequireReference(model.Materials))
            {
                Record(state.Materials, material, ColorsOf);
            }
        }
        state.Active = state.Planned;
    }

    AnimationBlend::Pass::~Pass()
    {
        if (!_engaged) return;
        if (state.Channel == AnimationChannel::Node) RecordBlended(state.Nodes, PoseOf);
        else if (state.Channel == AnimationChannel::Material) RecordBlended(state.Materials, ColorsOf);
        End();
    }

    void AnimationBlend::Pass::End() noexcept
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
        if (sample.Model != &model || sample.Frame != frame || !values
            || std::find(sample.Curves.begin(), sample.Curves.end(), values.get()) == sample.Curves.end())
        {
            return std::nullopt;
        }

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
