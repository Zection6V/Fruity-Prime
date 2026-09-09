#include "image_decode.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#endif

namespace fruityprime::mapgen::detail::image {
namespace {

[[nodiscard]] std::uint16_t read_u16_le(
    std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("image header is truncated");
    }
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(bytes[offset + 1] << 8);
}

[[nodiscard]] std::string lower(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        result.push_back(static_cast<char>(std::tolower(
            static_cast<unsigned char>(character))));
    }
    return result;
}

[[nodiscard]] RgbImage decode_tga(std::span<const std::uint8_t> bytes,
                                  std::string_view source_name) {
    if (bytes.size() < 18) {
        throw std::runtime_error("TGA image is truncated: "
                                 + std::string(source_name));
    }
    if (bytes[1] != 0 || (bytes[2] != 2 && bytes[2] != 10)
        || (bytes[16] != 24 && bytes[16] != 32)) {
        throw std::runtime_error("unsupported TGA image: "
                                 + std::string(source_name));
    }
    const std::size_t id_length = bytes[0];
    const int width = read_u16_le(bytes, 12);
    const int height = read_u16_le(bytes, 14);
    if (width <= 0 || height <= 0
        || static_cast<std::uint64_t>(width) * height
            > std::numeric_limits<std::size_t>::max() / 3) {
        throw std::runtime_error("TGA image dimensions are invalid: "
                                 + std::string(source_name));
    }
    std::size_t cursor = 18 + id_length;
    if (cursor > bytes.size()) {
        throw std::runtime_error("TGA image ID is truncated: "
                                 + std::string(source_name));
    }
    const bool top_origin = (bytes[17] & 0x20) != 0;
    const std::size_t bytes_per_pixel = bytes[16] / 8;
    const std::size_t pixel_count = static_cast<std::size_t>(width) * height;
    RgbImage result;
    result.width = width;
    result.height = height;
    result.pixels.resize(pixel_count * 3);
    std::size_t written = 0;
    const auto put_pixel = [&](std::span<const std::uint8_t> source) {
        if (written >= pixel_count) {
            throw std::runtime_error("TGA image has too many pixels: "
                                     + std::string(source_name));
        }
        const int stored_y = static_cast<int>(written / width);
        const int x = static_cast<int>(written % width);
        const int y = top_origin ? stored_y : height - 1 - stored_y;
        const std::size_t destination =
            (static_cast<std::size_t>(y) * width + x) * 3;
        // TGA true-color data is BGR(A); the packer intentionally drops alpha
        // just like the managed RGB decoder does.
        result.pixels[destination + 0] = source[2];
        result.pixels[destination + 1] = source[1];
        result.pixels[destination + 2] = source[0];
        ++written;
    };
    while (written < pixel_count) {
        if (cursor >= bytes.size()) {
            throw std::runtime_error("TGA pixel data is truncated: "
                                     + std::string(source_name));
        }
        if (bytes[2] == 2) {
            if (bytes.size() - cursor < bytes_per_pixel) {
                throw std::runtime_error("TGA pixel data is truncated: "
                                         + std::string(source_name));
            }
            put_pixel(bytes.subspan(cursor, bytes_per_pixel));
            cursor += bytes_per_pixel;
            continue;
        }
        const std::uint8_t packet = bytes[cursor++];
        const std::size_t run = static_cast<std::size_t>(packet & 0x7f) + 1;
        if (run > pixel_count - written) {
            throw std::runtime_error("TGA RLE packet exceeds the image: "
                                     + std::string(source_name));
        }
        if ((packet & 0x80) != 0) {
            if (bytes.size() - cursor < bytes_per_pixel) {
                throw std::runtime_error("TGA RLE pixel is truncated: "
                                         + std::string(source_name));
            }
            const auto pixel = bytes.subspan(cursor, bytes_per_pixel);
            cursor += bytes_per_pixel;
            for (std::size_t i = 0; i < run; ++i) {
                put_pixel(pixel);
            }
        } else {
            if (run > (bytes.size() - cursor) / bytes_per_pixel) {
                throw std::runtime_error("TGA RLE raw packet is truncated: "
                                         + std::string(source_name));
            }
            for (std::size_t i = 0; i < run; ++i) {
                put_pixel(bytes.subspan(cursor, bytes_per_pixel));
                cursor += bytes_per_pixel;
            }
        }
    }
    return result;
}

[[nodiscard]] RgbImage decode_ppm(std::span<const std::uint8_t> bytes,
                                  std::string_view source_name) {
    // Small dependency-free fixture format. It is not searched for in PK3
    // files by the baker, but keeping it here makes the portable decoder
    // deterministic and easy to test without a platform image library.
    if (bytes.size() < 3 || bytes[0] != 'P' || bytes[1] != '6') {
        throw std::runtime_error("unsupported image format: "
                                 + std::string(source_name));
    }
    std::size_t cursor = 2;
    const auto skip_space = [&] {
        while (cursor < bytes.size()) {
            if (std::isspace(bytes[cursor]) != 0) {
                ++cursor;
            } else if (bytes[cursor] == '#') {
                while (cursor < bytes.size() && bytes[cursor] != '\n') {
                    ++cursor;
                }
            } else {
                break;
            }
        }
    };
    const auto read_number = [&]() -> int {
        skip_space();
        const std::size_t start = cursor;
        while (cursor < bytes.size()
               && std::isdigit(bytes[cursor]) != 0) {
            ++cursor;
        }
        if (start == cursor) {
            throw std::runtime_error("PPM header is invalid: "
                                     + std::string(source_name));
        }
        int value = 0;
        for (std::size_t i = start; i < cursor; ++i) {
            if (value > (std::numeric_limits<int>::max()
                         - (bytes[i] - '0')) / 10) {
                throw std::runtime_error("PPM header value is too large: "
                                         + std::string(source_name));
            }
            value = value * 10 + bytes[i] - '0';
        }
        return value;
    };
    const int width = read_number();
    const int height = read_number();
    const int maximum = read_number();
    if (width <= 0 || height <= 0 || maximum != 255
        || static_cast<std::uint64_t>(width) * height
            > std::numeric_limits<std::size_t>::max() / 3) {
        throw std::runtime_error("PPM dimensions or range are invalid: "
                                 + std::string(source_name));
    }
    if (cursor >= bytes.size() || std::isspace(bytes[cursor]) == 0) {
        throw std::runtime_error("PPM pixel separator is missing: "
                                 + std::string(source_name));
    }
    while (cursor < bytes.size() && std::isspace(bytes[cursor]) != 0) {
        ++cursor;
    }
    const std::size_t size = static_cast<std::size_t>(width) * height * 3;
    if (size > bytes.size() - cursor) {
        throw std::runtime_error("PPM pixel data is truncated: "
                                 + std::string(source_name));
    }
    return {width, height,
            std::vector<std::uint8_t>(bytes.begin()
                                          + static_cast<std::ptrdiff_t>(cursor),
                                      bytes.begin()
                                          + static_cast<std::ptrdiff_t>(cursor + size))};
}

#ifdef _WIN32

[[nodiscard]] RgbImage decode_wic(std::span<const std::uint8_t> bytes,
                                  std::string_view source_name) {
    using Microsoft::WRL::ComPtr;
    if (bytes.empty() || bytes.size() > std::numeric_limits<DWORD>::max()) {
        throw std::runtime_error("image is empty or too large: "
                                 + std::string(source_name));
    }
    struct ComInitializer {
        bool uninitialize = false;

        ComInitializer() {
            const HRESULT status = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            uninitialize = status == S_OK || status == S_FALSE;
            if (FAILED(status) && status != RPC_E_CHANGED_MODE) {
                throw std::runtime_error(
                    "could not initialize Windows Imaging Component");
            }
        }

        ~ComInitializer() {
            if (uninitialize) {
                CoUninitialize();
            }
        }
    } com_initializer;
    ComPtr<IWICImagingFactory> factory;
    HRESULT status = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(status)) {
        throw std::runtime_error("could not create Windows Imaging Component");
    }
    ComPtr<IWICStream> stream;
    status = factory->CreateStream(&stream);
    if (FAILED(status)) {
        throw std::runtime_error("could not create WIC image stream");
    }
    status = stream->InitializeFromMemory(
        const_cast<BYTE*>(reinterpret_cast<const BYTE*>(bytes.data())),
        static_cast<DWORD>(bytes.size()));
    if (FAILED(status)) {
        throw std::runtime_error("could not initialize WIC image stream");
    }
    ComPtr<IWICBitmapDecoder> decoder;
    status = factory->CreateDecoderFromStream(
        stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(status)) {
        throw std::runtime_error("WIC could not decode image "
                                 + std::string(source_name));
    }
    ComPtr<IWICBitmapFrameDecode> frame;
    status = decoder->GetFrame(0, &frame);
    if (FAILED(status)) {
        throw std::runtime_error("WIC image has no frame: "
                                 + std::string(source_name));
    }
    UINT width = 0;
    UINT height = 0;
    status = frame->GetSize(&width, &height);
    if (FAILED(status) || width == 0 || height == 0
        || width > static_cast<UINT>(std::numeric_limits<int>::max())
        || height > static_cast<UINT>(std::numeric_limits<int>::max())
        || width > std::numeric_limits<UINT>::max() / 3
        || static_cast<std::uint64_t>(width) * height
            > std::numeric_limits<std::size_t>::max() / 3
        || static_cast<std::uint64_t>(width) * height * 3
            > std::numeric_limits<UINT>::max()) {
        throw std::runtime_error("WIC image dimensions are invalid: "
                                 + std::string(source_name));
    }
    ComPtr<IWICFormatConverter> converter;
    status = factory->CreateFormatConverter(&converter);
    if (FAILED(status)) {
        throw std::runtime_error("could not create WIC format converter");
    }
    status = converter->Initialize(
        frame.Get(), GUID_WICPixelFormat24bppRGB,
        WICBitmapDitherTypeNone, nullptr, 0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(status)) {
        throw std::runtime_error("WIC could not convert image pixels: "
                                 + std::string(source_name));
    }
    RgbImage result;
    result.width = static_cast<int>(width);
    result.height = static_cast<int>(height);
    result.pixels.resize(static_cast<std::size_t>(width) * height * 3);
    status = converter->CopyPixels(
        nullptr, width * 3, static_cast<UINT>(result.pixels.size()),
        result.pixels.data());
    if (FAILED(status)) {
        throw std::runtime_error("WIC could not copy image pixels: "
                                 + std::string(source_name));
    }
    return result;
}

#endif

} // namespace

RgbImage decode(std::span<const std::uint8_t> bytes,
                std::string_view source_name) {
    const std::string extension = lower(source_name);
    if (extension.size() >= 4
        && extension.compare(extension.size() - 4, 4, ".tga") == 0) {
        return decode_tga(bytes, source_name);
    }
    if (extension.size() >= 4
        && extension.compare(extension.size() - 4, 4, ".ppm") == 0) {
        return decode_ppm(bytes, source_name);
    }
#ifdef _WIN32
    return decode_wic(bytes, source_name);
#else
    // TGA files are sometimes supplied without a useful extension.
    if (bytes.size() >= 18 && bytes[1] == 0
        && (bytes[2] == 2 || bytes[2] == 10)) {
        return decode_tga(bytes, source_name);
    }
    return decode_ppm(bytes, source_name);
#endif
}

} // namespace fruityprime::mapgen::detail::image
