#pragma once

#include "RawMouseMotion.hpp"
#include <QtCore/QAbstractNativeEventFilter>
#include <chrono>

namespace MphRead::Qt
{
    // Registration, decoding and accumulation only; QtWindow owns capture policy.
    // All methods run on the Qt GUI/event thread. Win32 types stay in the .cpp.
    class WindowsRawMouseInput final : public QAbstractNativeEventFilter
    {
    public:
        ~WindowsRawMouseInput() override;
        bool Attach(void* nativeHandle);
        void Detach() noexcept;
        void SetCapture(bool enabled) noexcept;
        [[nodiscard]] bool Available() const noexcept { return _registered && !_readFailed; }
        [[nodiscard]] bool CaptureActive() const noexcept { return Available() && _motion.CaptureActive(); }
        [[nodiscard]] std::pair<std::int64_t, std::int64_t> TakeDelta() noexcept;
        void Discard() noexcept { _motion.Discard(); }
        void EndFrame(bool qtFallback);
        bool nativeEventFilter(const QByteArray&, void* message, qintptr*) override;

    private:
        void* _target = nullptr;
        bool _registered = false, _ownsRegistration = false, _filterInstalled = false;
        bool _readFailed = false, _reportedReadFailure = false, _diagnostics = false;
        RawMouseMotion _motion;
        std::uint64_t _events = 0, _samples = 0, _failures = 0, _fallbackFrames = 0;
        std::uint64_t _frameEvents = 0, _frameSamples = 0, _maxEvents = 0;
        std::int64_t _sumX = 0, _sumY = 0;
        std::chrono::steady_clock::time_point _nextReport{};
    };
}
