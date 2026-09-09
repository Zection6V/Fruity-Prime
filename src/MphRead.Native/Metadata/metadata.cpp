#include "Metadata/metadata_records.hpp"
#include "Formats/enum_tables.hpp"
#include "Metadata/metadata_values.hpp"
#include "metadata_tables.hpp"

namespace fruityprime::metadata {

// The managed Metadata.cs file is the catalogue root.  The concrete tables
// and APIs live in the same-name native translation units next to it; this
// root unit keeps the complete catalogue available to source-tree tooling and
// verifies that the split did not silently change its cardinalities.
static_assert(detail::HunterTable.size() == HunterCount);
static_assert(detail::WeaponTable.size() == WeaponCount);
static_assert(detail::WeaponVisualTable.size() == WeaponCount);
static_assert(detail::EffectTable.size() == EffectCount);
static_assert(detail::ItemTable.size() == ItemTableCount);
static_assert(detail::FhItemTable.size() == FhItemCount);
static_assert(detail::EnemyTable.size() == EnemyCount);

} // namespace fruityprime::metadata

