// ShowInTaskbar = false, the one window property GLFW has no attribute for.
// Kept out of Host.cpp so that <windows.h> and its macros stay out of it.

#include <GLFW/glfw3.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

namespace MphRead::NativeRuntime::Gui::Detail
{
    // What Avalonia does on Windows for ShowInTaskbar = false: a tool window
    // has no taskbar button and no Alt+Tab entry. It has to be set while the
    // window is still hidden, which is when the constructor calls this.
    // Elsewhere there is nothing to do here: macOS lists applications rather
    // than windows, and an X11 or Wayland session decides for itself.
    void HideFromTaskbar(GLFWwindow* handle)
    {
#if defined(_WIN32)
        HWND hwnd = ::glfwGetWin32Window(handle);
        if (hwnd == nullptr)
        {
            return;
        }
        LONG_PTR style = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        style &= ~static_cast<LONG_PTR>(WS_EX_APPWINDOW);
        style |= WS_EX_TOOLWINDOW;
        ::SetWindowLongPtrW(hwnd, GWL_EXSTYLE, style);
#else
        (void)handle;
#endif
    }
}
