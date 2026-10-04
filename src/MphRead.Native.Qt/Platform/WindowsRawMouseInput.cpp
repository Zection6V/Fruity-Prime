#include "WindowsRawMouseInput.hpp"
#include <QtCore/QCoreApplication>
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace MphRead::Qt
{
#if defined(_WIN32)
    namespace
    {
        // Cold path only. Never overwrite another process-internal mouse owner.
        bool MouseRegistration(RAWINPUTDEVICE& mouse, bool& found)
        {
            UINT count = 0;
            found = false;
            if (GetRegisteredRawInputDevices(nullptr, &count, sizeof(RAWINPUTDEVICE)) == UINT(-1)) return false;
            if (count == 0) return true;
            std::vector<RAWINPUTDEVICE> devices;
            try { devices.resize(count); }
            catch (...) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return false; }
            const UINT read = GetRegisteredRawInputDevices(devices.data(), &count, sizeof(RAWINPUTDEVICE));
            if (read == UINT(-1)) return false;
            for (UINT i = 0; i < read; ++i)
            {
                if (devices[i].usUsagePage == 1 && devices[i].usUsage == 2)
                {
                    mouse = devices[i];
                    found = true;
                    break;
                }
            }
            return true;
        }
    }
#endif

    WindowsRawMouseInput::~WindowsRawMouseInput() { Detach(); }

    bool WindowsRawMouseInput::Attach(void* nativeHandle)
    {
        if (nativeHandle == _target && Available()) return true;
        Detach();
        _readFailed = false;
        _reportedReadFailure = false;
        _diagnostics = qEnvironmentVariableIntValue("FRUITY_RAW_MOUSE_DIAGNOSTICS") != 0;
        _nextReport = std::chrono::steady_clock::now() + std::chrono::seconds(2);
#if defined(_WIN32)
        if (qgetenv("FRUITY_RAW_MOUSE") == "0")
        {
            std::cout << "[raw-mouse] disabled by FRUITY_RAW_MOUSE=0; Qt warp fallback\n";
            return false;
        }
        const auto hwnd = static_cast<HWND>(nativeHandle);
        if (!hwnd || !IsWindow(hwnd) || !QCoreApplication::instance()) return false;
        RAWINPUTDEVICE existing{};
        bool found = false;
        if (!MouseRegistration(existing, found))
        {
            const DWORD error = GetLastError();
            std::cerr << "[raw-mouse] registration query failed error=" << error << "; Qt warp fallback\n";
            return false;
        }
        if (found && (existing.hwndTarget != hwnd || existing.dwFlags != 0))
        {
            std::cerr << "[raw-mouse] mouse registration already owned target=" << existing.hwndTarget
                << " flags=" << existing.dwFlags << "; Qt warp fallback\n";
            return false;
        }
        RAWINPUTDEVICE device{1, 2, 0, hwnd};
        // Testable failure without changing process registrations.
        const bool forceFailure = qEnvironmentVariableIntValue("FRUITY_RAW_MOUSE_FAIL_REGISTER") != 0;
        if (forceFailure
            || (!found && !RegisterRawInputDevices(&device, 1, sizeof(device))))
        {
            const DWORD error = forceFailure ? ERROR_ACCESS_DENIED : GetLastError();
            std::cerr << "[raw-mouse] registration failed error=" << error << "; Qt warp fallback\n";
            return false;
        }
        _target = nativeHandle;
        _registered = true;
        _ownsRegistration = !found;
        QCoreApplication::instance()->installNativeEventFilter(this);
        _filterInstalled = true;
        std::cout << "[raw-mouse] registered target=" << hwnd << " foreground relative motion\n";
        return true;
#else
        (void)nativeHandle;
        return false;
#endif
    }

    void WindowsRawMouseInput::Detach() noexcept
    {
        SetCapture(false);
        Discard();
        if (_filterInstalled && QCoreApplication::instance())
            QCoreApplication::instance()->removeNativeEventFilter(this);
#if defined(_WIN32)
        if (_ownsRegistration)
        {
            RAWINPUTDEVICE existing{};
            bool found = false;
            // Do not remove a new owner's registration (or a borrowed one).
            if (MouseRegistration(existing, found) && found && existing.hwndTarget == _target && existing.dwFlags == 0)
            {
                RAWINPUTDEVICE remove{1, 2, RIDEV_REMOVE, nullptr};
                if (!RegisterRawInputDevices(&remove, 1, sizeof(remove)))
                {
                    const DWORD error = GetLastError();
                    std::cerr << "[raw-mouse] unregister failed error=" << error << '\n';
                }
            }
        }
#endif
        _target = nullptr;
        _registered = _ownsRegistration = _filterInstalled = false;
    }

    void WindowsRawMouseInput::SetCapture(bool enabled) noexcept
    {
        _motion.SetCapture(enabled && Available());
    }

    std::pair<std::int64_t, std::int64_t> WindowsRawMouseInput::TakeDelta() noexcept
    {
        const auto delta = _motion.TakeDelta();
        _sumX = RawMouseMotion::AddClamped(_sumX, delta.first);
        _sumY = RawMouseMotion::AddClamped(_sumY, delta.second);
        return delta;
    }

    bool WindowsRawMouseInput::nativeEventFilter(const QByteArray&, void* message, qintptr*)
    {
#if defined(_WIN32)
        const auto* msg = static_cast<MSG*>(message);
        if (!_registered || !msg || msg->hwnd != _target) return false;
        if (msg->message == WM_KILLFOCUS)
        {
            SetCapture(false);
            Discard();
        }
        if (msg->message != WM_INPUT) return false;
        ++_events;
        ++_frameEvents;
        // Menus and a failed reader still receive WM_INPUT, but own no aim.
        // Count the dispatch and let Qt clean up without decoding unused data.
        if (!CaptureActive()) return false;
        // Stack storage; no allocation, logging, formatting or locks here.
        RAWINPUT raw{};
        UINT size = sizeof(raw);
        const UINT read = GetRawInputData(reinterpret_cast<HRAWINPUT>(msg->lParam), RID_INPUT,
            &raw, &size, sizeof(RAWINPUTHEADER));
        if (read == UINT(-1) || read < sizeof(RAWINPUTHEADER)
            || (raw.header.dwType == RIM_TYPEMOUSE && read < offsetof(RAWINPUT, data) + sizeof(RAWMOUSE)))
        {
            // Another TLC may share this HWND. An oversized HID packet does
            // not invalidate the fixed-size mouse reader.
            RAWINPUTHEADER header{};
            UINT headerSize = sizeof(header);
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(msg->lParam), RID_HEADER, &header,
                &headerSize, sizeof(header)) == sizeof(header) && header.dwType != RIM_TYPEMOUSE)
                return false;
            ++_failures;
            _readFailed = true;
            SetCapture(false);
            Discard();
            return false;
        }
        if (GET_RAWINPUT_CODE_WPARAM(msg->wParam) == RIM_INPUT && GetForegroundWindow() == _target
            && CaptureActive() && raw.header.dwType == RIM_TYPEMOUSE)
        {
            const auto& mouse = raw.data.mouse;
            const bool absolute = (mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0;
            if (!absolute && (mouse.lLastX != 0 || mouse.lLastY != 0)) { ++_samples; ++_frameSamples; }
            _motion.Add(mouse.lLastX, mouse.lLastY, absolute);
        }
#else
        (void)message;
#endif
        // Qt/WindowProc must still perform foreground WM_INPUT cleanup.
        return false;
    }

    void WindowsRawMouseInput::EndFrame(bool qtFallback)
    {
        if (_readFailed && !_reportedReadFailure)
        {
            _reportedReadFailure = true;
            std::cerr << "[raw-mouse] raw data read failed; Qt warp fallback\n";
        }
        if (qtFallback) ++_fallbackFrames;
        _maxEvents = std::max(_maxEvents, _frameEvents);
        if (_diagnostics && std::chrono::steady_clock::now() >= _nextReport)
        {
            std::cout << "[raw-mouse-metrics] raw_registered=" << _registered
                << " raw_capture_active=" << CaptureActive()
                << " wm_input_events_per_frame=" << _frameEvents
                << " raw_nonzero_samples_per_frame=" << _frameSamples
                << " wm_input_events_total=" << _events << " raw_nonzero_samples_total=" << _samples
                << " raw_get_data_failures=" << _failures << " raw_dx_sum=" << _sumX << " raw_dy_sum=" << _sumY
                << " qt_fallback_frames=" << _fallbackFrames << " max_wm_input_events_per_frame=" << _maxEvents << '\n';
            _nextReport = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        }
        _frameEvents = _frameSamples = 0;
    }
}
