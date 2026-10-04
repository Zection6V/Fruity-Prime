#include "../../src/MphRead.Native.Qt/Platform/WindowsRawMouseInput.hpp"
#include <QtCore/QCoreApplication>
#include <windows.h>
#include <iostream>
#include <stdexcept>

using MphRead::Qt::WindowsRawMouseInput;
namespace
{
    void Expect(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
    struct Window final
    {
        HWND handle = CreateWindowExW(0, L"STATIC", L"Raw input registration test", WS_POPUP, 0, 0, 64, 64,
            nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        ~Window() { if (handle) DestroyWindow(handle); }
    };
    void Register(HWND hwnd)
    {
        RAWINPUTDEVICE device{1, 2, 0, hwnd};
        Expect(RegisterRawInputDevices(&device, 1, sizeof(device)), "Test registration failed.");
    }
    void Remove()
    {
        RAWINPUTDEVICE device{1, 2, RIDEV_REMOVE, nullptr};
        Expect(RegisterRawInputDevices(&device, 1, sizeof(device)), "Test cleanup failed.");
    }
    bool RegisteredTo(HWND hwnd)
    {
        RAWINPUTDEVICE devices[16]{};
        UINT count = 16;
        const UINT read = GetRegisteredRawInputDevices(devices, &count, sizeof(RAWINPUTDEVICE));
        Expect(read != UINT(-1), "Query failed.");
        for (UINT i = 0; i < read; ++i)
            if (devices[i].usUsagePage == 1 && devices[i].usUsage == 2)
                return devices[i].hwndTarget == hwnd && devices[i].dwFlags == 0;
        return false;
    }
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try
    {
        Window a, b;
        Expect(a.handle && b.handle, "Native window creation failed.");
        WindowsRawMouseInput raw;
        qputenv("FRUITY_RAW_MOUSE", "0");
        Expect(!raw.Attach(a.handle) && !raw.Available(), "Forced Qt fallback failed.");
        qunsetenv("FRUITY_RAW_MOUSE");
        qputenv("FRUITY_RAW_MOUSE_FAIL_REGISTER", "1");
        Expect(!raw.Attach(a.handle) && !raw.Available(), "Registration failure was fatal/available.");
        qunsetenv("FRUITY_RAW_MOUSE_FAIL_REGISTER");
        Register(b.handle);
        Expect(!raw.Attach(a.handle) && RegisteredTo(b.handle), "Foreign owner was stolen.");
        Remove();
        Expect(raw.Attach(a.handle) && RegisteredTo(a.handle), "Foreground mouse registration failed.");
        raw.SetCapture(true);
        Expect(raw.CaptureActive(), "Capture did not start.");
        MSG focus{};
        focus.hwnd = a.handle;
        focus.message = WM_KILLFOCUS;
        Expect(!raw.nativeEventFilter({}, &focus, nullptr) && !raw.CaptureActive(), "Focus loss was swallowed or capture retained.");
        raw.SetCapture(true);
        MSG invalid{};
        invalid.hwnd = a.handle;
        invalid.message = WM_INPUT;
        invalid.wParam = RIM_INPUT;
        Expect(!raw.nativeEventFilter({}, &invalid, nullptr) && !raw.Available(), "Read failure retained capture or swallowed cleanup.");
        raw.Detach();
        Expect(!RegisteredTo(a.handle), "Owned registration leaked.");
        Register(a.handle);
        Expect(raw.Attach(a.handle), "Same-HWND registration could not be borrowed.");
        raw.Detach();
        Expect(RegisteredTo(a.handle), "Borrowed registration was removed.");
        Remove();
        Expect(raw.Attach(a.handle), "Reattach failed.");
        Register(b.handle); // Simulate another subsystem taking ownership later.
        raw.Detach();
        Expect(RegisteredTo(b.handle), "Detach removed a later owner's registration.");
        Remove();
        Expect(raw.Attach(b.handle) && RegisteredTo(b.handle), "Renderer/new HWND registration failed.");
        raw.SetCapture(true);
        Expect(raw.TakeDelta() == std::pair<std::int64_t, std::int64_t>{0, 0}, "New HWND retained delta.");
        raw.Detach();
        std::cout << "PASS Windows native raw registration/fallback/ownership/focus/read-failure/rebind\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
