#pragma once

// VXDS bitstream and frame records.
// Transliterated from the managed sources so the field names and order stay
// checkable against them.  A C# reference member becomes a pointer.

#include "Formats/Types.hpp"
#include "Formats/raw_formats.hpp"
#include "Formats/enum_tables.hpp"


#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::formats {

// Decoded audio frames are owned by the movie decoder.
struct AudioFrame;



struct VxBuffers;
struct Vector2ir;
struct VideoFrame;
struct VLCData;
struct VxFrame;

// Movie.cs
struct VxBuffers {
    std::vector<VideoFrame> PrevVideoFrames;
    std::vector<std::int32_t> QuantizerTable;
    std::vector<std::int16_t> PrevSampleBuffer;
    std::vector<std::int32_t> PrevPulseBuffer;
    std::vector<std::int32_t> LpcFilterBuffer;
    std::vector<std::int32_t> InfluenceBuffer;
    std::vector<std::int16_t> SampleBuffer;
};

// Movie.cs
struct Vector2ir {
    std::int32_t X{};
    std::int32_t Y{};
};

// Movie.cs
struct VideoFrame {
    std::int32_t FrameWidth{};
    std::int32_t FrameHeight{};
};

// Movie.cs
struct VLCData {
    std::int32_t MaxBitCount{};
};

// Movie.cs
struct VxFrame {
    VideoFrame VideoFrame{};
    std::int32_t AudioFrameCount{};
    std::vector<AudioFrame> AudioFrames;
};

} // namespace fruityprime::formats
