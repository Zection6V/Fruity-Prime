#pragma once

#include "../Track.hpp"

namespace NCSFPlayer
{
    class Track : public NCSFCommon::Track
    {
    public:
        [[nodiscard]] bool StepTicks() override;
    };
}
