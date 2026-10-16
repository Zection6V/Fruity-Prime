#include "FormTransitionAuthorityTracker.hpp"

namespace MphRead::Mods::Network
{
    bool FormTransitionAuthorityTracker::Observe(std::uint32_t frame, std::uint8_t slot,
        std::uint16_t generation, std::uint16_t life, bool live, const ReplicatedFormState& form,
        const FormClipObservation& clip)
    {
        if (generation != _generation || life != _life)
        {
            Reset();
            _generation = generation;
            _life = life;
        }
        if (frame == 0 || (_frame != 0 && static_cast<std::int32_t>(frame - _frame) <= 0)) return false;
        _frame = frame;
        const bool transitioning = live && form.IsValid()
            && form.Direction != ReplicatedFormState::Transition::None && clip.Frames > 1;
        bool changed = false;
        if (transitioning)
        {
            if (!_latest || _latest->Status != FormTransitionStatus::Active || _latest->Direction != form.Direction)
            {
                ++_nextId;
                if (_nextId == 0) ++_nextId;
                _latest = FormTransitionRecord{};
                _latest->Slot = slot;
                _latest->Generation = generation;
                _latest->Life = life;
                _latest->Id = _nextId;
                _latest->StartFrame = frame;
                _latest->Direction = form.Direction;
                changed = true;
            }
            changed = changed || _latest->Paused != clip.Paused;
            _latest->StampFrame = frame;
            _latest->AnchorFrame = clip.AnchorFrame;
            _latest->ClipFrame = clip.Frame;
            _latest->ClipFrames = clip.Frames;
            _latest->Paused = clip.Paused;
            _latest->AltForm = form.AltForm;
        }
        else if (_latest && _latest->Status == FormTransitionStatus::Active)
        {
            const bool targetAlt = _latest->Direction == ReplicatedFormState::Transition::Morph;
            _latest->Status = live && form.AltForm == targetAlt
                ? FormTransitionStatus::Completed : FormTransitionStatus::Cancelled;
            _latest->StampFrame = frame;
            _latest->AltForm = form.AltForm;
            changed = true;
        }
        return changed;
    }
}
