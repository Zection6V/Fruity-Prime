#pragma once

// The platform difference behind the room transition's worker thread.
//
// RoomEntity.StartTransition hands ProcessTransition to Task.Run, and the
// worker then builds the next room while the main thread keeps running
// frames: inserting into the scene's entity list and id map, replacing the
// room's collision and model, and filling Read's model cache, all while the
// main thread walks those same structures. C# survives that because a
// reference read is atomic and the collector keeps whatever the other thread
// dropped alive. The same sequence on shared_ptr, unordered_map and vector is
// undefined behaviour: a torn reference count or a freed element, and the
// process dies during the door transition.
//
// The gate is held by the main thread for each simulation step and each
// picture, and by the worker for its work, so the two never touch the scene
// at the same moment. The worker lets go while it waits for the main thread
// to initialize what it queued, which is the one place the two hand off.
// Nothing about what either side does, or in what order, changes.

#include <mutex>

namespace MphRead::NativeRuntime
{
    [[nodiscard]] std::recursive_mutex& SceneGate() noexcept;
}
