#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace MphRead::Droid
{
    class AndroidPng final
    {
    public:
        static void Write(
            std::vector<std::uint8_t>& rgb,
            std::int32_t width,
            std::int32_t height,
            const std::string& path
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
