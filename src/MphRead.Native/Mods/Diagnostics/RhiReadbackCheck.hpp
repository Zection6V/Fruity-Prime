#pragma once
#include <string>

namespace MphRead::Mods::Diagnostics
{
    // Run only with a current desktop context, after drawing a shellshot frame.
    bool CheckRhiImageExports(const std::string& outputDirectory);
}
