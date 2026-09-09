#pragma once

namespace fruityprime::input {

// Run the controller diagnostic without loading a ROM or creating a match.
// The return values match the managed probe: zero means a controller was
// observed and produced input, one means the diagnostic could not verify it.
int run_gamepad_probe(double seconds);

} // namespace fruityprime::input
