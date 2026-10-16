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
#include <unordered_map>
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
            const Model* SourceModel = nullptr;
            // The value arrays of the channel's group; any other curve is left alone.
            std::array<const std::vector<float>*, 3> Curves{};
            std::int32_t Frame = 0;
            std::int32_t Next = 0;
            float Fraction = 0;
            std::int32_t FirstOverride = -1;
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
            // Sample is reading its two ends: their own reads are not blended.
            bool Sampling = false;
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

        bool CanBlend()
        {
            return state.Scopes != 0 && state.Suppressed == 0 && !state.Busy;
        }

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

        std::optional<ActiveSample> SampleFor(const AnimationInfo& info, AnimationChannel channel,
            bool includeStart = false)
        {
            const AnimationTick& tick = info.ModTick;
            const ChannelState current = Channel(info, channel);
            const std::size_t slot = current.Slot;
            if (tick.DrawFrame >= 0 && slot == 0 && current.Group != nullptr
                && channel == AnimationChannel::Node)
            {
                const auto frame = static_cast<std::int32_t>(std::floor(tick.DrawFrame));
                const float fraction = tick.DrawFrame - frame;
                return ActiveSample{nullptr, current.Curves, (*info.Frame)[0],
                    std::min(frame + 1, (*info.FrameCount)[0] - 1), fraction, frame};
            }
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
            if (fraction <= 0 && !includeStart) return std::nullopt;
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

    void AnimationBlend::FrameOverride::Set(AnimationInfo& info, float frame)
    {
        if (_info != nullptr || info.Frame == nullptr || info.FrameCount == nullptr
            || !std::isfinite(frame) || (*info.FrameCount)[0] <= 1) return;
        _info = &info;
        _drawFrame = info.ModTick.DrawFrame;
        float shown = std::clamp(frame, 0.0F, static_cast<float>((*info.FrameCount)[0] - 1));
        if (rate != DisplayRate)
        {
            const float slices = static_cast<float>(rate) / DsRate;
            shown = std::floor(shown * slices + 1e-6F) / slices;
        }
        info.ModTick.DrawFrame = shown;
    }

    AnimationBlend::FrameOverride::~FrameOverride()
    {
        if (_info == nullptr) return;
        _info->ModTick.DrawFrame = _drawFrame;
    }

    float AnimationBlend::FrameShown(const AnimationInfo& info, std::size_t slot)
    {
        const float frame = info.Frame != nullptr && slot < info.Frame->Length()
            ? static_cast<float>((*info.Frame)[slot]) : 0.0F;
        const auto sample = SampleFor(info, AnimationChannel::Node);
        if (slot == 0 && sample && sample->FirstOverride >= 0)
            return static_cast<float>(sample->FirstOverride) + sample->Fraction;
        // A neighbouring frame only: a loop's wrap is a jump, not a sweep back.
        if (!sample || Channel(info, AnimationChannel::Node).Slot != slot
            || std::abs(sample->Next - sample->Frame) != 1)
        {
            return frame;
        }
        return static_cast<float>(sample->Frame)
            + static_cast<float>(sample->Next - sample->Frame) * sample->Fraction;
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
        if (!CanBlend() || !info) return false;
        state.Planned = SampleFor(*info, channel);
        if (!state.Planned) return false;
        state.Planned->SourceModel = &model;
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

    namespace
    {
        // A looping UV curve's closure -- where its last frame lands against
        // its first, inside the sampler's period -- depends on the curve
        // alone, so it is read once per curve and kept.
        struct ClosureKey final
        {
            const void* Values = nullptr;
            std::int32_t Start = 0, Blend = 0, Length = 0, Count = 0;
            float Period = 0;
            bool operator==(const ClosureKey&) const = default;
        };

        struct ClosureHash final
        {
            std::size_t operator()(const ClosureKey& key) const noexcept
            {
                std::size_t hash = std::hash<const void*>{}(key.Values);
                for (const std::int32_t part : {key.Start, key.Blend, key.Length, key.Count})
                {
                    hash ^= std::hash<std::int32_t>{}(part) + 0x9E3779B97F4A7C15ULL + (hash << 6) + (hash >> 2);
                }
                return hash ^ std::hash<float>{}(key.Period);
            }
        };

        // The curve is held weakly: a room unloaded and another loaded may put
        // new curves where old ones were, and an entry outliving its curve is
        // worked out again.
        struct Closure final
        {
            std::weak_ptr<const std::vector<float>> Curve;
            float Value = 0;
            // A steady scroll whose cycle does not close (Regular): the step
            // its seam (and a duplicated first frame) is given, and the miss
            // that is left, spread over the cycle. One that runs back by
            // more than that is the DS's jump, at the tick (Held).
            bool Regular = false, Held = false, Duplicate = false;
            float Step = 0, Miss = 0;
        };

        thread_local std::unordered_map<ClosureKey, Closure, ClosureHash> closures;
    }

    Matrix4 AnimationBlend::Texcoords(Model& model, const std::shared_ptr<AnimationInfo>& info,
        const Material& material, const TexcoordAnimation& animation)
    {
        const auto& animationInfo = NativeRuntime::RequireReference(info);
        const auto& group = NativeRuntime::RequireReference(animationInfo.Texcoord).Group;
        // A closed UV cycle also needs its retimed value at a tick boundary.
        const auto sample = CanBlend() ? SampleFor(animationInfo, AnimationChannel::Texcoord, true) : std::nullopt;
        Matrix4 result;
        {
            const Pass pass(model, AnimationChannel::Texcoord, info);
            result = model.AnimateTexcoords(group, animation, animationInfo.TexcoordFrame());
        }
        // UVs repeat in space, independently of the animation slot's clock.
        // A curve may reset several times before the slot itself loops.
        // Clamp, scale and colour curves retain their authored interpolation.
        // Normal texgen applies a further texture-size-dependent transform;
        // its curve values are not in the sampler's normalized UV space.
        if (!sample || material.TexgenMode == TexgenMode::Normal) return result;
        const auto& curves = NativeRuntime::RequireReference(group);
        const auto slot = static_cast<std::size_t>(animationInfo.Texcoord->Slot);
        const auto flags = (*animationInfo.Flags)[slot];
        const bool looping = (flags & (AnimFlags::NoLoop | AnimFlags::PingPong)) == AnimFlags::None
            && (*animationInfo.Step)[slot] == 1 && (*animationInfo.FrameCount)[slot] == curves.FrameCount;
        const auto correct = [&](float& translation, RepeatMode mode,
            std::int32_t start, std::int32_t blend, std::int32_t length)
        {
            const float period = mode == RepeatMode::Repeat ? 1.0F : mode == RepeatMode::Mirror ? 2.0F : 0.0F;
            if (period == 0 || length <= 1) return;
            const auto at = [&](std::int32_t frame)
            {
                return model.InterpolateAnimation(curves.Translations, start, frame, blend, length, curves.FrameCount);
            };
            const float first = at(sample->Frame);
            const float second = at(sample->Next);
            const float delta = second - first;
            float shown = first + std::remainder(delta, period) * sample->Fraction;
            if (looping && curves.FrameCount > 2)
            {
                const ClosureKey key{curves.Translations.get(), start, blend, length, curves.FrameCount, period};
                Closure& kept = closures[key];
                constexpr float Unit = 1.0F / 4096.0F;
                const std::int32_t last = curves.FrameCount - 1;
                if (kept.Curve.expired() || kept.Curve.lock() != curves.Translations)
                {
                    kept = {curves.Translations, std::remainder(at(last) - at(0), period)};
                    // A scroll authored as a steady step can still miss its
                    // own start at the seam -- the Helm Room's scanning floor
                    // ends four steps past it, after a duplicated first frame,
                    // so every 32 seconds the DS ran its lines back and held
                    // them. Its step is the one the cycle ends on.
                    kept.Step = std::remainder(at(last) - at(last - 1), period);
                    kept.Duplicate = std::abs(std::remainder(at(1) - at(0), period)) <= Unit;
                    kept.Miss = std::remainder(at(0) - at(last) - kept.Step
                        - (kept.Duplicate ? kept.Step : 0.0F), period);
                    // A scroll: a step well inside the tile, so its direction
                    // is not a guess. Only a seam that runs back or holds,
                    // and only a small miss, which spread over the cycle
                    // changes the speed by under half and never the direction.
                    const float seam = std::remainder(at(0) - at(last), period);
                    const bool scroll = std::abs(kept.Value) > Unit && std::abs(kept.Step) > Unit
                        && std::abs(kept.Step) < period / 8;
                    kept.Regular = scroll && (seam * kept.Step <= 0 || kept.Duplicate)
                        && std::abs(kept.Miss) > Unit
                        && std::abs(kept.Miss) < .5F * std::abs(kept.Step) * static_cast<float>(curves.FrameCount);
                    kept.Held = scroll && !kept.Regular && seam * kept.Step < 0;
                }
                const float closure = kept.Value;
                if (kept.Regular)
                {
                    // Forward cycle time: the seam is the last frame's span.
                    const bool reverse = (flags & AnimFlags::Reverse) != AnimFlags::None;
                    const float count = static_cast<float>(curves.FrameCount);
                    float time = reverse ? sample->Frame - sample->Fraction : sample->Frame + sample->Fraction;
                    if (time < 0) time += count;
                    const auto frame = std::clamp(static_cast<std::int32_t>(time), 0, last);
                    const bool given = frame == last || (frame == 0 && kept.Duplicate);
                    const float step = given ? kept.Step : std::remainder(at(frame + 1) - at(frame), period);
                    const float from = at(frame) + (kept.Duplicate && frame > 0 ? kept.Step : 0.0F);
                    shown = from + step * (time - frame) + kept.Miss * (time / count);
                }
                else if (kept.Held && ((sample->Frame == last && sample->Next == 0)
                    || (sample->Frame == 0 && sample->Next == last)))
                {
                    // A whole tile back in one tick would be swept, not jumped.
                    shown = first;
                }
                // fx32 authoring can leave a closed cycle one quantization
                // unit short. Its duplicate endpoint is one pose, not a hold.
                else if (std::abs(closure) <= Unit)
                {
                    const bool reverse = (flags & AnimFlags::Reverse) != AnimFlags::None;
                    const float time = reverse ? sample->Frame + 1.0F - sample->Fraction
                        : sample->Frame + sample->Fraction;
                    const float steps = static_cast<float>(curves.FrameCount - 1);
                    const float position = time * steps / static_cast<float>(curves.FrameCount);
                    const auto frame = std::min(static_cast<std::int32_t>(position), curves.FrameCount - 2);
                    const float from = at(frame);
                    shown = from + std::remainder(at(frame + 1) - from, period) * (position - frame)
                        - closure * (position / steps);
                }
            }
            // Translation is applied after scale/rotation by AnimateTexcoords.
            // Correct the two axes separately, even if they share a LUT.
            translation += shown - (first + delta * sample->Fraction);
        };
        correct(result.M41, material.XRepeat, animation.TranslateLutIndexS,
            animation.TranslateBlendS, animation.TranslateLutLengthS);
        correct(result.M42, material.YRepeat, animation.TranslateLutIndexT,
            animation.TranslateBlendT, animation.TranslateLutLengthT);
        return result;
    }

    std::optional<float> AnimationBlend::Sample(const Model& model,
        const std::shared_ptr<const std::vector<float>>& values, std::int32_t start,
        std::int32_t frame, std::int32_t blend, std::int32_t length,
        std::int32_t count, bool rotation)
    {
        // Called for every curve read in the game, blended or not: one
        // thread-local access and no copies on the way out.
        DrawState& current = state;
        if (!current.Active || current.Sampling) return std::nullopt;
        const ActiveSample& sample = *current.Active;
        if (sample.SourceModel != &model || sample.Frame != frame || !values
            || std::find(sample.Curves.begin(), sample.Curves.end(), values.get()) == sample.Curves.end())
        {
            return std::nullopt;
        }

        // Both ends come from the game's own LUT decoder, compressed tails
        // included. Marking the read prevents recursion, exception safe.
        struct Suspend
        {
            bool& Sampling;
            ~Suspend() { Sampling = false; }
        } suspend{current.Sampling};
        current.Sampling = true;
        const std::int32_t firstFrame = sample.FirstOverride >= 0 ? sample.FirstOverride : frame;
        const float first = model.InterpolateAnimation(values, start, firstFrame,
            blend, length, count, rotation);
        const float second = model.InterpolateAnimation(values, start, sample.Next,
            blend, length, count, rotation);
        float delta = second - first;
        if (rotation)
        {
            constexpr float Turn = 2.0F * std::numbers::pi_v<float>;
            delta = std::remainder(delta, Turn);
            if (count > 2
                && ((firstFrame == count - 1 && sample.Next == 0)
                || (firstFrame == 0 && sample.Next == count - 1)))
            {
                // Symmetric rotating geometry, and a texture turning on a
                // floor (the Helm Room's), can reset by less than a full
                // turn. Blending that authored reset invents a fast reverse
                // rotation. Keep the reset when it exceeds both edge steps;
                // a continuous full-turn loop still blends through its seam.
                const auto at = [&](std::int32_t index)
                {
                    return model.InterpolateAnimation(values, start, index, blend, length, count, true);
                };
                const float firstStep = std::abs(std::remainder(at(1) - at(0), Turn));
                const float lastStep = std::abs(std::remainder(at(count - 1) - at(count - 2), Turn));
                constexpr float RotationUnit = Turn / 65536.0F;
                if (std::abs(delta) > 2 * std::max(firstStep, lastStep) + RotationUnit)
                    return first;
            }
        }
        return first + delta * sample.Fraction;
    }
}
