#pragma once

#include <string>

namespace MphRead::Qt
{
    class UiCapture final
    {
    public:
        UiCapture() = delete;

        // Writes DIR/<screen>.png for each screen -uishot knows; 0 when all were.
        [[nodiscard]] static int Run(const std::string& directory,
            int benchmarkFrames = 0, const std::string& report = {});
    };
}
