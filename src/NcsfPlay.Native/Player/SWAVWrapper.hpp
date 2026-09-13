#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace NCSFCommon
{
    class NDSSoundRegister;
}

namespace NCSFPlayer
{
    class SWAVWrapper final
    {
    public:
        explicit SWAVWrapper(NCSFCommon::NDSSoundRegister* registerValue);

        SWAVWrapper(const SWAVWrapper&) = delete;
        SWAVWrapper(SWAVWrapper&&) = delete;
        SWAVWrapper& operator=(const SWAVWrapper&) = delete;
        SWAVWrapper& operator=(SWAVWrapper&&) = delete;

        [[nodiscard]] std::span<const float> Slice(std::int32_t index, std::int32_t len) const;

    private:
        std::vector<float> data;
        NCSFCommon::NDSSoundRegister* soundRegister = nullptr;
    };
}
