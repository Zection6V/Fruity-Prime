#pragma once

// Native counterpart of the editor display records in Formats/Formats.cs.
//
// The managed subclasses of DisplayVolume carry no fields of their own: each
// one only decides the two colours its volume is drawn with, and answers
// GetColor for one selection index.  Reproducing them as data means one
// record plus a table, rather than eight classes that differ by two lines.

#include "Formats/Types.hpp"
#include "Formats/raw_formats.hpp"
#include "Formats/enum_tables.hpp"
#include "Formats/formats_layouts.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace fruityprime::formats {

// Formats.DisplayVolume
struct DisplayVolume {
    CollisionVolume Volume{};
    Vector3 Color1{};
    Vector3 Color2{};
    // The selection index this volume answers GetColor for; the managed
    // subclasses hard-code one each.
    int SelectionIndex = -1;

    [[nodiscard]] std::optional<Vector3> GetColor(int index) const noexcept {
        return index == SelectionIndex ? std::optional<Vector3>(Color1)
                                       : std::nullopt;
    }
};

// The selection index each managed subclass answers to.
enum class DisplayVolumeKind : int {
    MorphCamera = 7,
    JumpPad = 8,
    Object = 9,
    FlagBase = 10,
    NodeDefense = 11,
    TriggerVolume = 3,
    AreaVolume = 5
};

// Formats.MorphCameraDisplay
[[nodiscard]] inline DisplayVolume MorphCameraDisplay(
    CollisionVolume volume) noexcept {
    return {volume, {1.0F, 1.0F, 0.0F}, {},
            static_cast<int>(DisplayVolumeKind::MorphCamera)};
}

// Formats.JumpPadDisplay
[[nodiscard]] inline DisplayVolume JumpPadDisplay(
    CollisionVolume volume) noexcept {
    return {volume, {0.0F, 1.0F, 0.0F}, {},
            static_cast<int>(DisplayVolumeKind::JumpPad)};
}

// Formats.ObjectDisplay
[[nodiscard]] inline DisplayVolume ObjectDisplay(
    CollisionVolume volume) noexcept {
    return {volume, {1.0F, 0.0F, 0.0F}, {},
            static_cast<int>(DisplayVolumeKind::Object)};
}

// Formats.FlagBaseDisplay
[[nodiscard]] inline DisplayVolume FlagBaseDisplay(
    CollisionVolume volume) noexcept {
    return {volume, {1.0F, 1.0F, 1.0F}, {},
            static_cast<int>(DisplayVolumeKind::FlagBase)};
}

// Formats.NodeDefenseDisplay
[[nodiscard]] inline DisplayVolume NodeDefenseDisplay(
    CollisionVolume volume) noexcept {
    return {volume, {1.0F, 1.0F, 1.0F}, {},
            static_cast<int>(DisplayVolumeKind::NodeDefense)};
}

// Formats.TriggerVolumeDisplay: both colours come from the event colours of
// the parent and child messages.
[[nodiscard]] inline DisplayVolume TriggerVolumeDisplay(
    CollisionVolume volume, Vector3 parent_color,
    Vector3 child_color) noexcept {
    return {volume, parent_color, child_color,
            static_cast<int>(DisplayVolumeKind::TriggerVolume)};
}

// Formats.AreaVolumeDisplay: the inside and exit message event colours.
[[nodiscard]] inline DisplayVolume AreaVolumeDisplay(
    CollisionVolume volume, Vector3 inside_color,
    Vector3 exit_color) noexcept {
    return {volume, inside_color, exit_color,
            static_cast<int>(DisplayVolumeKind::AreaVolume)};
}

} // namespace fruityprime::formats
