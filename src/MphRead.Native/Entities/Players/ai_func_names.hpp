#pragma once

// Native counterpart of PlayerAi's GetFuncs1Name/GetFuncs2Name/
// GetFuncs3Name/GetFuncs4Name and the two list wrappers.
//
// A bot's behaviour is a tree of numbered subroutines, and the numbers are all
// the cartridge gives them.  These tables put a name on each one so a dump of
// an execution path is readable; nothing in the simulation depends on them.
// The tables themselves are generated from the managed switch expressions --
// see tools/gen-ai-func-names.py -- because there are 331 arms between them.

#include <span>
#include <string_view>
#include <vector>

namespace fruityprime::players {

// PlayerAi.GetFuncs1Name: the init (d3a) and process (d3b) subroutines.
[[nodiscard]] std::string_view funcs1_name(int id);
// PlayerAi.GetFuncs2Name: the per-frame (f*2*4) subroutines.
[[nodiscard]] std::string_view funcs2_name(int id);
// PlayerAi.GetFuncs3Name: the preconditions and path-weight updates.
[[nodiscard]] std::string_view funcs3_name(int id);
// PlayerAi.GetFuncs4Name: the init (f2*4*) subroutines.
[[nodiscard]] std::string_view funcs4_name(int id);

// PlayerAi.GetFuncs1Names / GetFuncs3Names
[[nodiscard]] std::vector<std::string_view> funcs1_name_list(
    std::span<const int> ids);
[[nodiscard]] std::vector<std::string_view> funcs3_name_list(
    std::span<const int> ids);

} // namespace fruityprime::players
