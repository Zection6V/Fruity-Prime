#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace MphRead::Droid
{
    class AndroidPng final
    {
    public:
        static void Write(
            std::span<const std::uint8_t> rgb,
            std::int32_t width,
            std::int32_t height,
            std::u16string_view path
        );

    private:
        AndroidPng() = delete;
        ~AndroidPng() = delete;
        AndroidPng(const AndroidPng&) = delete;
        AndroidPng& operator=(const AndroidPng&) = delete;
        AndroidPng(AndroidPng&&) = delete;
        AndroidPng& operator=(AndroidPng&&) = delete;
    };
}
