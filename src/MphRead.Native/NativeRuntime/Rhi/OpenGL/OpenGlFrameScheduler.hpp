#pragma once

#include "../Submission.hpp"
#include "../BackendError.hpp"
#include <deque>
#include <exception>

namespace MphRead::NativeRuntime::Rhi::OpenGL
{
    // Completion of one GL context's command stream, independent of frame
    // slots. Markers also cover commands issued by the native/Skia adapters.
    class OpenGlFrameScheduler final
    {
    public:
        enum class WaitStatus : unsigned
        { AlreadySignaled = 0x911A, Timeout = 0x911B, Satisfied = 0x911C, Failed = 0x911D };
        struct Dispatch final
        {
            void* Context;
            void* (*Fence)(void*);
            WaitStatus (*Wait)(void*, void*, bool, std::uint64_t);
            void (*Delete)(void*, void*);
            void (*Flush)(void*);
            void (*Finish)(void*);
            unsigned (*Error)(void*);
            bool SyncSupported;
        };
        explicit OpenGlFrameScheduler(Dispatch dispatch) : _dispatch(dispatch) {}
        ~OpenGlFrameScheduler() { ReleaseCompleted(true); }
        OpenGlFrameScheduler(const OpenGlFrameScheduler&) = delete;
        OpenGlFrameScheduler& operator=(const OpenGlFrameScheduler&) = delete;

        SubmissionSerial Submit(bool flush = false)
        {
            RequireHealthy();
            const auto serial = _progress.Next();
            if (!_dispatch.SyncSupported)
            {
                // GL 2.1 diagnostics without ARB_sync keep their explicit
                // synchronous compatibility path; production desktop uses GLsync.
                Finish();
                _progress.Submitted(serial);
                _progress.Complete(serial);
                return serial;
            }
            // Allocate bookkeeping before asking the driver to accept a marker.
            _fences.push_back({serial, nullptr});
            try
            {
                _fences.back().Sync = _dispatch.Fence(_dispatch.Context);
                if (!_fences.back().Sync) Fail("glFenceSync");
            }
            catch (...) { _fences.pop_back(); throw; }
            _progress.Submitted(serial);
            if (flush) _dispatch.Flush(_dispatch.Context);
            return serial;
        }

        // Resource destructors cannot throw a driver failure. Preserve the
        // first failure for the next explicit operation and retain the object
        // until context teardown: no completion token covers a failed marker.
        SubmissionSerial SubmitForRetirement()
        {
            try { return Submit(); }
            catch (const BackendError&)
            {
                if (!_retirementFailure) _retirementFailure = std::current_exception();
                return {std::numeric_limits<std::uint64_t>::max()};
            }
        }

        SubmissionSerial Poll()
        {
            RequireHealthy();
            while (!_fences.empty())
            {
                const auto status = _dispatch.Wait(_dispatch.Context, _fences.front().Sync, false, 0);
                if (status == WaitStatus::Timeout) break;
                RequireCompletion(status);
                _progress.Complete(_fences.front().Serial);
                ReleaseCompleted();
            }
            return Completed();
        }

        void Wait(SubmissionSerial serial)
        {
            if (serial > Submitted()) throw std::logic_error("Unsubmitted OpenGL completion token.");
            if (serial <= Poll()) return;
            const auto found = std::find_if(_fences.begin(), _fences.end(),
                [serial](const Entry& entry) { return entry.Serial == serial; });
            if (found == _fences.end()) throw std::logic_error("Missing OpenGL completion fence.");
            ++_hostWaits;
            for (;;)
            {
                const auto status = _dispatch.Wait(_dispatch.Context, found->Sync, true, 1'000'000'000ULL);
                if (status == WaitStatus::Timeout) continue;
                RequireCompletion(status);
                break;
            }
            // Completion of this marker proves completion of all earlier work
            // in the same stream, even when earlier fences were not polled.
            _progress.Complete(serial);
            ReleaseCompleted();
        }

        void Finish()
        {
            RequireHealthy();
            ++_hostWaits;
            ++_deviceWideWaits;
            _dispatch.Finish(_dispatch.Context);
            const auto error = _dispatch.Error(_dispatch.Context);
            if (error) Fail("glFinish", error);
            _progress.Complete(Submitted());
            ReleaseCompleted();
        }
        [[nodiscard]] SubmissionSerial Submitted() const noexcept { return _progress.Submitted(); }
        [[nodiscard]] SubmissionSerial Completed() const noexcept { return _progress.Completed(); }
        [[nodiscard]] std::uint64_t HostWaits() const noexcept { return _hostWaits; }
        [[nodiscard]] std::uint64_t DeviceWideWaits() const noexcept { return _deviceWideWaits; }

    private:
        struct Entry final { SubmissionSerial Serial; void* Sync; };
        void RequireHealthy() const
        {
            if (_retirementFailure) std::rethrow_exception(_retirementFailure);
        }
        [[noreturn]] void Fail(const char* operation)
        {
            Fail(operation, _dispatch.Error(_dispatch.Context));
        }
        [[noreturn]] void Fail(const char* operation, unsigned error)
        {
            try
            {
                throw BackendError(GraphicsBackend::OpenGl,
                    error == 0x0507 ? BackendErrorKind::DeviceLost
                        : error == 0x0505 ? BackendErrorKind::OutOfMemory : BackendErrorKind::Unknown,
                    error, std::string(operation) + " failed; completion was not established.");
            }
            catch (const BackendError&)
            {
                // glGetError consumes its code. A consumed context-loss error
                // must not make a later glFinish appear successful.
                if (error == 0x0507 && !_retirementFailure) _retirementFailure = std::current_exception();
                throw;
            }
        }
        void RequireCompletion(WaitStatus status)
        {
            if (status != WaitStatus::AlreadySignaled && status != WaitStatus::Satisfied)
                Fail("glClientWaitSync");
        }
        void ReleaseCompleted(bool all = false)
        {
            while (!_fences.empty() && (all || _fences.front().Serial <= Completed()))
            {
                _dispatch.Delete(_dispatch.Context, _fences.front().Sync);
                _fences.pop_front();
            }
        }
        Dispatch _dispatch;
        SubmissionProgress _progress;
        std::deque<Entry> _fences;
        std::exception_ptr _retirementFailure;
        std::uint64_t _hostWaits = 0, _deviceWideWaits = 0;
    };
}
