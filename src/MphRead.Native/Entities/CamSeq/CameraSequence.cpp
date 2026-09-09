// Native counterpart of src/MphRead/Entities/CamSeq/CameraSequence.cs.
// The full cartridge parser/playback implementation is shared by the format
// reader.  This source unit is the entity-facing ownership boundary and keeps
// the managed directory visible in the native tree without duplicating the
// parser or creating a second implementation to drift from it.
#include "Formats/camera_sequence.hpp"

namespace fruityprime::entities::cam_seq {

static_assert(camera::Keyframe::Size == 100);

} // namespace fruityprime::entities::cam_seq
