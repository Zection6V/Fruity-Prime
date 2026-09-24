#pragma once

#include <chrono>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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
        // A line followed by the current call stack, for a state that should
        // not happen and is not an exception (Windows: with function names).
        static void Stack(std::string_view category, std::string_view message);
        // The current call stack, to print later with StackFrom (empty where
        // the platform cannot walk one).
        [[nodiscard]] static std::vector<void*> CaptureStack();
        static void StackFrom(std::string_view category, std::string_view message,
            const std::vector<void*>& frames);
        static void Exception(std::string_view category, std::exception_ptr exception);
        static void Exception(std::string_view category, const std::exception& exception);

        [[nodiscard]] static std::unique_ptr<Timed> Step(
            std::string category, std::string what);

        DebugLog() = delete;
        DebugLog(const DebugLog&) = delete;
        DebugLog& operator=(const DebugLog&) = delete;
    };
}
