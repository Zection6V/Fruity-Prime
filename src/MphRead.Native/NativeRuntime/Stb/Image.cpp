#include "Image.hpp"

#include <algorithm>
#include <cstring>

#include <zlib.h>

namespace
{
    std::int32_t& FlipOnWrite()
    {
        static std::int32_t flag = 0;
        return flag;
    }

    void PutBigEndian(std::vector<std::uint8_t>& out, std::uint32_t value)
    {
        out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    }

    void PutChunk(std::vector<std::uint8_t>& out, const char* type,
        const std::vector<std::uint8_t>& data)
    {
        PutBigEndian(out, static_cast<std::uint32_t>(data.size()));
        const std::size_t crcStart = out.size();
        out.insert(out.end(), type, type + 4);
        out.insert(out.end(), data.begin(), data.end());
        const uLong crc = ::crc32(
            ::crc32(0, nullptr, 0), out.data() + crcStart,
            static_cast<uInt>(out.size() - crcStart));
        PutBigEndian(out, static_cast<std::uint32_t>(crc));
    }

    [[nodiscard]] std::uint32_t ReadBigEndian(const std::uint8_t* data)
    {
        return (static_cast<std::uint32_t>(data[0]) << 24)
            | (static_cast<std::uint32_t>(data[1]) << 16)
            | (static_cast<std::uint32_t>(data[2]) << 8)
            | static_cast<std::uint32_t>(data[3]);
    }

    [[nodiscard]] std::uint8_t Paeth(std::uint8_t a, std::uint8_t b, std::uint8_t c)
    {
        const std::int32_t p = static_cast<std::int32_t>(a) + b - c;
        const std::int32_t pa = p > a ? p - a : a - p;
        const std::int32_t pb = p > b ? p - b : b - p;
        const std::int32_t pc = p > c ? p - c : c - p;
        if (pa <= pb && pa <= pc)
        {
            return a;
        }
        return pb <= pc ? b : c;
    }
}

extern "C"
{
    void stbi_flip_vertically_on_write(int flag)
    {
        FlipOnWrite() = flag;
    }

    int stbi_write_png_to_func(StbiWriteFunc func, void* context, int w, int h,
        int comp, const void* data, int stride_bytes)
    {
        if (func == nullptr || data == nullptr || w <= 0 || h <= 0
            || (comp != 3 && comp != 4))
        {
            return 0;
        }
        const int stride = stride_bytes != 0 ? stride_bytes : w * comp;
        const auto* const pixels = static_cast<const std::uint8_t*>(data);

        // Each row is preceded by its filter type; filter 0 (None) is what stb
        // writes at its default settings.
        std::vector<std::uint8_t> raw;
        raw.reserve(static_cast<std::size_t>(h) * (static_cast<std::size_t>(w) * comp + 1));
        for (int row = 0; row < h; ++row)
        {
            const int source = FlipOnWrite() != 0 ? h - 1 - row : row;
            raw.push_back(0);
            const std::uint8_t* const line
                = pixels + static_cast<std::ptrdiff_t>(source) * stride;
            raw.insert(raw.end(), line, line + static_cast<std::ptrdiff_t>(w) * comp);
        }

        uLongf bound = ::compressBound(static_cast<uLong>(raw.size()));
        std::vector<std::uint8_t> deflated(bound);
        if (::compress2(deflated.data(), &bound, raw.data(),
                static_cast<uLong>(raw.size()), Z_DEFAULT_COMPRESSION) != Z_OK)
        {
            return 0;
        }
        deflated.resize(bound);

        std::vector<std::uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        std::vector<std::uint8_t> header;
        PutBigEndian(header, static_cast<std::uint32_t>(w));
        PutBigEndian(header, static_cast<std::uint32_t>(h));
        header.push_back(8);                                            // bit depth
        header.push_back(static_cast<std::uint8_t>(comp == 4 ? 6 : 2)); // colour type
        header.push_back(0);                                            // deflate
        header.push_back(0);                                            // adaptive filtering
        header.push_back(0);                                            // no interlace
        PutChunk(png, "IHDR", header);
        PutChunk(png, "IDAT", deflated);
        PutChunk(png, "IEND", {});

        func(context, png.data(), static_cast<int>(png.size()));
        return 1;
    }
}

namespace MphRead::NativeRuntime
{
    Image LoadPng(const std::vector<std::uint8_t>& bytes, std::int32_t desiredComponents)
    {
        Image image;
        static constexpr std::uint8_t Signature[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        if (bytes.size() < sizeof(Signature)
            || std::memcmp(bytes.data(), Signature, sizeof(Signature)) != 0)
        {
            return image;
        }

        std::int32_t width = 0;
        std::int32_t height = 0;
        std::int32_t components = 0;
        std::vector<std::uint8_t> deflated;
        std::size_t offset = sizeof(Signature);
        while (offset + 8 <= bytes.size())
        {
            const std::uint32_t length = ReadBigEndian(bytes.data() + offset);
            const char* const type = reinterpret_cast<const char*>(bytes.data() + offset + 4);
            const std::size_t body = offset + 8;
            if (body + length + 4 > bytes.size())
            {
                return Image();
            }
            if (std::memcmp(type, "IHDR", 4) == 0 && length >= 13)
            {
                width = static_cast<std::int32_t>(ReadBigEndian(bytes.data() + body));
                height = static_cast<std::int32_t>(ReadBigEndian(bytes.data() + body + 4));
                const std::uint8_t depth = bytes[body + 8];
                const std::uint8_t colour = bytes[body + 9];
                const std::uint8_t interlace = bytes[body + 12];
                // Only the two true-colour forms at eight bits, uninterlaced:
                // the map pipeline writes nothing else.
                if (depth != 8 || interlace != 0 || (colour != 2 && colour != 6))
                {
                    return Image();
                }
                components = colour == 6 ? 4 : 3;
            }
            else if (std::memcmp(type, "IDAT", 4) == 0)
            {
                deflated.insert(
                    deflated.end(), bytes.begin() + static_cast<std::ptrdiff_t>(body),
                    bytes.begin() + static_cast<std::ptrdiff_t>(body + length));
            }
            else if (std::memcmp(type, "IEND", 4) == 0)
            {
                break;
            }
            offset = body + length + 4;
        }
        if (width <= 0 || height <= 0 || components == 0 || deflated.empty())
        {
            return Image();
        }

        const std::size_t rowBytes = static_cast<std::size_t>(width) * components;
        std::vector<std::uint8_t> raw(height * (rowBytes + 1));
        uLongf produced = static_cast<uLongf>(raw.size());
        if (::uncompress(raw.data(), &produced, deflated.data(),
                static_cast<uLong>(deflated.size())) != Z_OK
            || produced != raw.size())
        {
            return Image();
        }

        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(height) * rowBytes);
        for (std::int32_t row = 0; row < height; ++row)
        {
            const std::uint8_t filter = raw[static_cast<std::size_t>(row) * (rowBytes + 1)];
            const std::uint8_t* const source
                = raw.data() + static_cast<std::size_t>(row) * (rowBytes + 1) + 1;
            std::uint8_t* const target = pixels.data() + static_cast<std::size_t>(row) * rowBytes;
            const std::uint8_t* const above
                = row > 0 ? target - rowBytes : nullptr;
            for (std::size_t i = 0; i < rowBytes; ++i)
            {
                const std::uint8_t left = i >= static_cast<std::size_t>(components)
                    ? target[i - components] : 0;
                const std::uint8_t up = above != nullptr ? above[i] : 0;
                const std::uint8_t upLeft
                    = above != nullptr && i >= static_cast<std::size_t>(components)
                        ? above[i - components] : 0;
                std::uint8_t value = source[i];
                switch (filter)
                {
                case 0: break;
                case 1: value = static_cast<std::uint8_t>(value + left); break;
                case 2: value = static_cast<std::uint8_t>(value + up); break;
                case 3:
                    value = static_cast<std::uint8_t>(
                        value + ((static_cast<std::int32_t>(left) + up) / 2));
                    break;
                case 4: value = static_cast<std::uint8_t>(value + Paeth(left, up, upLeft)); break;
                default: return Image();
                }
                target[i] = value;
            }
        }

        image.Width = width;
        image.Height = height;
        image.Components = components;
        if (desiredComponents == 0 || desiredComponents == components)
        {
            image.Pixels = std::move(pixels);
            return image;
        }

        // stbi_load's own conversion between the two true-colour widths.
        std::vector<std::uint8_t> converted(
            static_cast<std::size_t>(width) * height * desiredComponents);
        for (std::size_t pixel = 0;
            pixel < static_cast<std::size_t>(width) * height; ++pixel)
        {
            const std::uint8_t* const from = pixels.data() + pixel * components;
            std::uint8_t* const to = converted.data() + pixel * desiredComponents;
            to[0] = from[0];
            to[1] = from[1];
            to[2] = from[2];
            if (desiredComponents == 4)
            {
                to[3] = components == 4 ? from[3] : 0xFF;
            }
        }
        image.Components = desiredComponents;
        image.Pixels = std::move(converted);
        return image;
    }
}
