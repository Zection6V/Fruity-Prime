// Native counterpart of src/MphRead/Entities/CamSeq/CamSeqEntity.cs.
// The implementation is kept in this exact managed-class translation unit;
// the public camera_sequence_entity header remains the stable API for the
// scene host and existing tests.
#include "Entities/CamSeq/CamSeqEntity.hpp"

namespace fruityprime::entities::cam_seq {

static_assert(CameraSequenceEntity::HandoffFrames == 4);

} // namespace fruityprime::entities::cam_seq
