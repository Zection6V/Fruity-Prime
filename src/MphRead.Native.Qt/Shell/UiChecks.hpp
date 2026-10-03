#pragma once

#include <optional>
#include <string>

namespace MphRead::Qt
{
    void RunUiChecks(const std::optional<std::string>& shots);
    int RunTapChecks();
}
