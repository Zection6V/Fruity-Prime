#include "Formats/formats_display.hpp"
#include "Formats/formats_layouts.hpp"
#include "Formats/raw_formats.hpp"

#include <array>

namespace fruityprime::raw {

namespace {

constexpr std::array kLayout{
    LayoutEntry{"Header", 100, sizeof(Header)},
    LayoutEntry{"Texture", 40, sizeof(Texture)},
    LayoutEntry{"Palette", 16, sizeof(Palette)},
    LayoutEntry{"RawMaterial", 132, sizeof(RawMaterial)},
    LayoutEntry{"RawNode", 240, sizeof(RawNode)},
    LayoutEntry{"RawMesh", 4, sizeof(RawMesh)},
    LayoutEntry{"DisplayList", 32, sizeof(DisplayList)},
    LayoutEntry{"AnimationHeader", 24, sizeof(AnimationHeader)},
    LayoutEntry{"RawMaterialAnimationGroup", 20,
                 sizeof(RawMaterialAnimationGroup)},
    LayoutEntry{"RawTextureAnimationGroup", 32,
                 sizeof(RawTextureAnimationGroup)},
    LayoutEntry{"RawTexcoordAnimationGroup", 28,
                 sizeof(RawTexcoordAnimationGroup)},
    LayoutEntry{"RawNodeAnimationGroup", 20, sizeof(RawNodeAnimationGroup)},
    LayoutEntry{"MaterialAnimation", 140, sizeof(MaterialAnimation)},
    LayoutEntry{"TextureAnimation", 44, sizeof(TextureAnimation)},
    LayoutEntry{"TexcoordAnimation", 60, sizeof(TexcoordAnimation)},
    LayoutEntry{"NodeAnimation", 48, sizeof(NodeAnimation)},
    LayoutEntry{"RawCollisionVolume", 64, sizeof(RawCollisionVolume)},
    LayoutEntry{"FhRawCollisionVolume", 64, sizeof(FhRawCollisionVolume)},
    LayoutEntry{"CameraSequenceHeader", 8, sizeof(CameraSequenceHeader)},
    LayoutEntry{"RawCameraSequenceKeyframe", 100,
                 sizeof(RawCameraSequenceKeyframe)},
    LayoutEntry{"RawEffect", 28, sizeof(RawEffect)},
    LayoutEntry{"RawEffectElement", 116, sizeof(RawEffectElement)},
    // Marshal.SizeOf uses the one-byte marshalled char at the end of this
    // record, so the actual managed stride is 12 (not the stale 11-byte
    // comment in the original source file).
    LayoutEntry{"RawStringTableEntry", 12, sizeof(RawStringTableEntry)},
    LayoutEntry{"TextFileEntry", 12, sizeof(TextFileEntry)},
};

} // namespace

std::span<const LayoutEntry> layout() noexcept {
    return kLayout;
}

bool validate_layout() noexcept {
    for (const LayoutEntry& entry : kLayout) {
        if (entry.name.empty() || entry.managed_size != entry.native_size) {
            return false;
        }
    }
    return true;
}

} // namespace fruityprime::raw
