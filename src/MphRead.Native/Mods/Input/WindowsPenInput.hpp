#pragma once

#include "PointerDevice.hpp"

#include <cstdint>

struct GLFWwindow;

namespace OpenTK::Windowing::GraphicsLibraryFramework
{
    class MouseState;
}

namespace MphRead::Mods::Input
{
    // The pen as Windows reports it (WM_POINTER*), observed by subclassing the
    // game window; GLFW sees a pen only as a mouse it has promoted.
    class WindowsPenInput final
    {
    public:
        WindowsPenInput() = delete;

        [[nodiscard]] static bool Attached() noexcept { return _window != nullptr; }
        // Attach(NativeWindow): the window is GLFW's own, WindowPtr.
        static void Attach(GLFWwindow* window);
        [[nodiscard]] static PointerSample Read(const ::OpenTK::Windowing::GraphicsLibraryFramework::MouseState& mouse,
            std::int32_t width, std::int32_t height, bool& independentPrimary);
        [[nodiscard]] static bool IsPromotedPointer(std::uintptr_t extraInfo) noexcept;
        [[nodiscard]] static bool IsPromotedPrimaryRelease(std::uint32_t message, std::uintptr_t extraInfo) noexcept;

        // The subclass procedure; public only so the Win32 thunk can reach it.
        static std::intptr_t WindowProc(void* hwnd, std::uint32_t message, std::uintptr_t wParam, std::intptr_t lParam,
            std::uintptr_t id, std::uintptr_t data);

    private:
        static void EndContact() noexcept;
        static void ReadPen(void* hwnd, std::uint32_t message, std::uint32_t id);

        static constexpr std::uint32_t PointerUpdate = 0x0245;
        static constexpr std::uint32_t PointerDown = 0x0246;
        static constexpr std::uint32_t PointerUp = 0x0247;
        static constexpr std::uint32_t PointerEnter = 0x0249;
        static constexpr std::uint32_t PointerLeave = 0x024A;
        static constexpr std::uint32_t PointerCaptureChanged = 0x024C;

        inline static void* _window = nullptr;
        inline static PointerSample _pen{};
        inline static bool _releasePending = false;
        inline static bool _physicalPrimary = false;
        inline static bool _failed = false;
    };
}
