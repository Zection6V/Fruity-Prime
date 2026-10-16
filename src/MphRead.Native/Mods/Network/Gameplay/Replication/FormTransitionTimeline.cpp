#include "FormTransitionTimeline.hpp"
#include <algorithm>
#include <cmath>

namespace MphRead::Mods::Network
{
    bool FormTransitionTimeline::Receive(const FormTransitionRecord& record)
    {
        if (!record.Valid()) return false;
        if (_latest)
        {
            // Lifecycle changes are made by the snapshot/roster owner, not by
            // a delayed event. The session resets this history at boundaries.
            if (record.Generation != _latest->Generation || record.Life != _latest->Life) return false;
            if (record.Id != _latest->Id)
            {
                if (static_cast<std::int16_t>(record.Id - _latest->Id) <= 0) return false;
                if (static_cast<std::int32_t>(record.StartFrame - _latest->StartFrame) <= 0) return false;
            }
            else
            {
                if (record.StartFrame != _latest->StartFrame || record.Direction != _latest->Direction
                    || record.ClipFrames != _latest->ClipFrames) return false;
                if (_latest->Status != FormTransitionStatus::Active
                    && record.Status == FormTransitionStatus::Active
                    && static_cast<std::int32_t>(record.StampFrame - _latest->StampFrame) >= 0) return false;
                for (const auto& seen : _history)
                    if (seen && seen->Id == record.Id && seen->StampFrame == record.StampFrame) return false;
            }
        }
        _history[_next++ % Capacity] = record;
        if (!_latest || record.Id != _latest->Id
            || static_cast<std::int32_t>(record.StampFrame - _latest->StampFrame) > 0) _latest = record;
        return true;
    }

    std::optional<float> FormTransitionTimeline::Sample(std::uint16_t generation, std::uint16_t life,
        double readFrame, ReplicatedFormState::Transition direction) const noexcept
    {
        if (direction == ReplicatedFormState::Transition::None || !std::isfinite(readFrame)
            || readFrame < 0 || readFrame > UINT32_MAX) return std::nullopt;
        const auto tick = static_cast<std::uint32_t>(std::floor(readFrame));
        const double sub = readFrame - std::floor(readFrame);
        const FormTransitionRecord* best = nullptr;
        for (const auto& entry : _history)
        {
            if (!entry || entry->Generation != generation || entry->Life != life
                || static_cast<std::int32_t>(tick - entry->StartFrame) < 0) continue;
            // A later terminal or opposite transition fences off older clips.
            if (static_cast<std::int32_t>(tick - entry->StampFrame) >= 0
                && (best == nullptr || static_cast<std::int32_t>(entry->StampFrame - best->StampFrame) > 0))
                best = &*entry;
        }
        if (best == nullptr || best->Direction != direction || best->Status != FormTransitionStatus::Active
            || static_cast<std::int32_t>(tick - best->StampFrame) > 90) return std::nullopt;
        const double elapsed = static_cast<std::int32_t>(tick - best->AnchorFrame) + sub;
        // Frozen clips hold their actual frame. All others retain the DS's
        // 30 Hz duration; the snapshot still owns stable-form completion.
        const double frame = best->ClipFrame + (best->Paused ? 0.0 : std::max(0.0, elapsed) / 2.0);
        if (frame > best->ClipFrames + 1.0) return std::nullopt;
        return static_cast<float>(std::clamp(frame / (best->ClipFrames - 1.0), 0.0, 1.0));
    }
}
