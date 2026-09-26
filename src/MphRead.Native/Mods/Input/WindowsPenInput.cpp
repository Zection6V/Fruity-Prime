#include "WindowsPenInput.hpp"

#include "../DebugLog.hpp"
#include "../../Entities/Players/PlayerInput.hpp"

#include <exception>
#include <string>

#if defined(_WIN32)
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0602
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

namespace MphRead::Mods::Input
{
#if defined(_WIN32)
    namespace
    {
        LRESULT CALLBACK Callback(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR data);

        // [DllImport("comctl32.dll")] and user32's pointer calls, resolved on
        // first use so that a system without them is a DllNotFoundException.
        struct Library final
        {
            decltype(&::SetWindowSubclass) SetWindowSubclass = nullptr;
            decltype(&::RemoveWindowSubclass) RemoveWindowSubclass = nullptr;
            decltype(&::DefSubclassProc) DefSubclassProc = nullptr;
            decltype(&::GetPointerType) GetPointerType = nullptr;
            decltype(&::GetPointerPenInfo) GetPointerPenInfo = nullptr;
        };

        const Library& Api()
        {
            static const Library library = []()
            {
                Library result{};
                if (const HMODULE comctl = ::LoadLibraryW(L"comctl32.dll"))
                {
                    result.SetWindowSubclass = reinterpret_cast<decltype(result.SetWindowSubclass)>(
                        reinterpret_cast<void*>(::GetProcAddress(comctl, "SetWindowSubclass")));
                    result.RemoveWindowSubclass = reinterpret_cast<decltype(result.RemoveWindowSubclass)>(
                        reinterpret_cast<void*>(::GetProcAddress(comctl, "RemoveWindowSubclass")));
                    result.DefSubclassProc = reinterpret_cast<decltype(result.DefSubclassProc)>(
                        reinterpret_cast<void*>(::GetProcAddress(comctl, "DefSubclassProc")));
                }
                if (const HMODULE user = ::GetModuleHandleW(L"user32.dll"))
                {
                    result.GetPointerType = reinterpret_cast<decltype(result.GetPointerType)>(
                        reinterpret_cast<void*>(::GetProcAddress(user, "GetPointerType")));
                    result.GetPointerPenInfo = reinterpret_cast<decltype(result.GetPointerPenInfo)>(
                        reinterpret_cast<void*>(::GetProcAddress(user, "GetPointerPenInfo")));
                }
                return result;
            }();
            return library;
        }

        LRESULT CALLBACK Callback(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR data)
        {
            return static_cast<LRESULT>(WindowsPenInput::WindowProc(hwnd, message, wParam, lParam, id, data));
        }
    }
#endif

    void WindowsPenInput::Attach(GLFWwindow* window)
    {
#if defined(_WIN32)
        if (Attached())
        {
            return;
        }
        const HWND handle = ::glfwGetWin32Window(window);
        if (Api().SetWindowSubclass == nullptr || Api().DefSubclassProc == nullptr)
        {
            DebugLog::Line("input", "Windows pen observer unavailable: Unable to load DLL 'comctl32.dll'.");
            return;
        }
        if (Api().SetWindowSubclass(handle, &Callback, 1, 0) != FALSE)
        {
            _window = handle;
            _failed = false;
            DebugLog::Line("input", "Windows pen observer attached; GLFW fallback available");
        }
        else
        {
            DebugLog::Line("input", "Windows pen observer unavailable; using GLFW pointer input");
        }
#else
        static_cast<void>(window);
#endif
    }

    PointerSample WindowsPenInput::Read(const ::OpenTK::Windowing::GraphicsLibraryFramework::MouseState& mouse,
        std::int32_t width, std::int32_t height, bool& independentPrimary)
    {
        independentPrimary = false;
#if defined(_WIN32)
        if (Attached() && !_failed && (_pen.InRange || _pen.InContact || _releasePending))
        {
            _releasePending = false;
            independentPrimary = _physicalPrimary;
            float scaleX = 1;
            float scaleY = 1;
            RECT rect{};
            if (::GetClientRect(static_cast<HWND>(_window), &rect) != FALSE && rect.right > 0 && rect.bottom > 0)
            {
                scaleX = static_cast<float>(width) / static_cast<float>(rect.right);
                scaleY = static_cast<float>(height) / static_cast<float>(rect.bottom);
            }
            PointerSample sample = _pen;
            sample.X = _pen.X * scaleX;
            sample.Y = _pen.Y * scaleY;
            return sample;
        }
#else
        static_cast<void>(width);
        static_cast<void>(height);
#endif
        const bool down = mouse.IsButtonDown(::OpenTK::Windowing::GraphicsLibraryFramework::MouseButton::Left);
        return PointerSample{PointerDeviceType::Unknown, 0, mouse.X, mouse.Y, down, down, true};
    }

    bool WindowsPenInput::IsPromotedPointer(std::uintptr_t extraInfo) noexcept
    {
        return (extraInfo & 0xFFFFFF00U) == 0xFF515700U;
    }

    bool WindowsPenInput::IsPromotedPrimaryRelease(std::uint32_t message, std::uintptr_t extraInfo) noexcept
    {
        return message == 0x0202 && IsPromotedPointer(extraInfo);
    }

    void WindowsPenInput::EndContact() noexcept
    {
        _releasePending = _pen.InRange || _pen.InContact;
        _pen.PrimaryDown = false;
        _pen.InContact = false;
        _pen.InRange = false;
    }

    std::intptr_t WindowsPenInput::WindowProc(void* hwnd, std::uint32_t message, std::uintptr_t wParam,
        std::intptr_t lParam, std::uintptr_t id, std::uintptr_t data)
    {
        static_cast<void>(data);
#if defined(_WIN32)
        try
        {
            if (message == 0x0082) // WM_NCDESTROY
            {
                Api().RemoveWindowSubclass(static_cast<HWND>(hwnd), &Callback, id);
                _window = nullptr;
                _pen = {};
                _physicalPrimary = _releasePending = false;
                PointerDevice::Reset();
            }
            else if (message == 0x0008) // WM_KILLFOCUS
            {
                EndContact();
                _physicalPrimary = false;
                PointerDevice::Reset();
            }
            else if (message == 0x0215) // WM_CAPTURECHANGED
            {
                _physicalPrimary = false;
                if (_pen.InContact)
                {
                    EndContact();
                }
            }
            else if (message == 0x0201 || message == 0x0202 || message == 0x0203)
            {
                const auto extraInfo = static_cast<std::uintptr_t>(::GetMessageExtraInfo());
                if (IsPromotedPrimaryRelease(message, extraInfo))
                {
                    if (_pen.InContact)
                    {
                        EndContact();
                    }
                }
                else if (!IsPromotedPointer(extraInfo))
                {
                    _physicalPrimary = message != 0x0202;
                }
            }
            else if (!_failed)
            {
                const auto pointerId = static_cast<std::uint32_t>(wParam & 0xFFFF);
                if (message == PointerCaptureChanged && pointerId == _pen.Id)
                {
                    EndContact();
                }
                else if (message == PointerDown || message == PointerUpdate || message == PointerUp
                    || message == PointerEnter || message == PointerLeave)
                {
                    ReadPen(hwnd, message, pointerId);
                }
            }
        }
        catch (const std::exception& ex)
        {
            if (!_failed)
            {
                _failed = true;
                EndContact();
                PointerDevice::Reset();
                try
                {
                    DebugLog::Line("input", std::string("Windows pen observer failed; using GLFW: ") + ex.what());
                }
                catch (...)
                {
                }
            }
        }
        return static_cast<std::intptr_t>(Api().DefSubclassProc(static_cast<HWND>(hwnd), message, wParam, lParam));
#else
        static_cast<void>(hwnd);
        static_cast<void>(message);
        static_cast<void>(wParam);
        static_cast<void>(lParam);
        static_cast<void>(id);
        return 0;
#endif
    }

    void WindowsPenInput::ReadPen(void* hwnd, std::uint32_t message, std::uint32_t id)
    {
#if defined(_WIN32)
        if (_pen.InContact && id != _pen.Id)
        {
            return;
        }
        POINTER_INPUT_TYPE type = 0;
        if (Api().GetPointerType == nullptr || Api().GetPointerType(id, &type) == FALSE)
        {
            if (id == _pen.Id)
            {
                EndContact();
            }
            return;
        }
        if (type != 3) // PT_PEN
        {
            return;
        }
        POINTER_PEN_INFO info{};
        if (Api().GetPointerPenInfo == nullptr || Api().GetPointerPenInfo(id, &info) == FALSE)
        {
            if (id == _pen.Id)
            {
                EndContact();
            }
            return;
        }
        POINT point = info.pointerInfo.ptPixelLocation;
        if (::ScreenToClient(static_cast<HWND>(hwnd), &point) == FALSE)
        {
            EndContact();
            return;
        }
        const std::uint32_t flags = info.pointerInfo.pointerFlags;
        bool contact = (flags & 0x4) != 0 && message != PointerUp;
        bool inRange = (flags & 0x2) != 0;
        if ((flags & 0x8000) != 0 // POINTER_FLAG_CANCELED
            || (message == PointerLeave && !contact))
        {
            contact = inRange = false;
        }
        _pen = PointerSample{PointerDeviceType::Pen, id, static_cast<float>(point.x), static_cast<float>(point.y),
            contact, contact, inRange,
            (info.penMask & 1) != 0 ? static_cast<float>(info.pressure) / 1024.0F : 0,
            (info.penMask & 4) != 0 ? static_cast<float>(info.tiltX) : 0,
            (info.penMask & 8) != 0 ? static_cast<float>(info.tiltY) : 0};
        _releasePending = !inRange;
#else
        static_cast<void>(hwnd);
        static_cast<void>(message);
        static_cast<void>(id);
#endif
    }
}
