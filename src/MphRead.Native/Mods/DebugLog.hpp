#pragma once

#include <chrono>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace MphRead::Mods
{
    class DebugLog final
    {
    public:
        class Timed final
        {
        public:
            Timed(const Timed&) = delete;
            Timed& operator=(const Timed&) = delete;
            Timed(Timed&&) = delete;
            Timed& operator=(Timed&&) = delete;
            ~Timed() noexcept(false);

            // IDisposable.Dispose is observable and is not idempotent in the
            // C# source. The destructor supplies the using/RAII call site.
            void Dispose();

        private:
            friend class DebugLog;
            Timed(std::string category, std::string what);

            std::string _category;
            std::string _what;
            std::chrono::steady_clock::time_point _started;
        };

        [[nodiscard]] static bool Active() noexcept;
        [[nodiscard]] static std::optional<std::string> Path();
        [[nodiscard]] static std::optional<std::string> NativePath();

        static void Force() noexcept;
        static void Attach();
        static void Detach();

        static void Line(std::string_view category, std::string_view message);
        static void Exception(std::string_view category, std::exception_ptr exception);
        static void Exception(std::string_view category, const std::exception& exception);

        [[nodiscard]] static std::unique_ptr<Timed> Step(
            std::string category, std::string what);

        DebugLog() = delete;
        DebugLog(const DebugLog&) = delete;
        DebugLog& operator=(const DebugLog&) = delete;
    };
}
