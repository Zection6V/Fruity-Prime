#pragma once

#include <filesystem>
#include <iosfwd>
#include <string_view>

namespace fruityprime::assets {
class Store;
}

namespace fruityprime::mapgen {

// Direct counterparts of MapReport.ListShaders and ListMaterials. Both
// functions write the managed command's human-readable report and return its
// process-style status code, leaving argument parsing to the frontend.
int list_shaders(std::ostream& output, const std::filesystem::path& source,
                 std::string_view map_name = {});
int list_materials(std::ostream& output, const assets::Store& assets,
                   std::string_view room_name);

} // namespace fruityprime::mapgen
