#pragma once

#include "Backend.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace MphRead::NativeRuntime::Rhi
{
    enum class BackendErrorKind : std::uint8_t
    { DeviceLost, SurfaceLost, OutOfMemory, Unsupported, Unknown };

    class BackendError final : public std::runtime_error
    {
    public:
        BackendError(GraphicsBackend backend, BackendErrorKind kind, std::int64_t nativeCode, std::string message)
            : std::runtime_error(std::move(message)), _backend(backend), _kind(kind), _nativeCode(nativeCode) {}
        [[nodiscard]] GraphicsBackend Backend() const noexcept { return _backend; }
        [[nodiscard]] BackendErrorKind Kind() const noexcept { return _kind; }
        [[nodiscard]] std::int64_t NativeCode() const noexcept { return _nativeCode; }
    private:
        GraphicsBackend _backend;
        BackendErrorKind _kind;
        std::int64_t _nativeCode;
    };
}
