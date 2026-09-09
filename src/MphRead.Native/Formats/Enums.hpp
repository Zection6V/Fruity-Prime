#pragma once

// src/MphRead/Formats/Enums.cs.  The enums it declares are shared by nearly
// every value type in Formats/Types.cs, so in the native tree they live in
// Types.hpp beside the types that use them rather than in a header of their
// own -- splitting them back out would put a cycle between the two.
//
// This file exists so the native tree keeps a file at the managed path, which
// is what source_tree_contract_tests checks: every managed source has a native
// counterpart at the same path, so the C# application cannot silently collapse
// into one aggregate C++ file.
#include "Formats/Types.hpp"
