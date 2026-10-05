#include "AndroidGlContextGate.hpp"

#include "../MphRead.Native/NativeRuntime/Rhi/SceneBackend.hpp"

#include <iostream>
#include <optional>

#if !defined(__ANDROID__)
#error "AndroidGlContextGate is only valid for the Android native target."
#endif

namespace
{
    std::mutex g_androidGlContextGate;
    std::mutex g_pendingRendererLock;
    std::optional<MphRead::NativeRuntime::Rhi::SceneBackendRequest> g_pendingRenderer;

    // Every scene on Android (the match, the hunter preview, the map
    // previews) holds the lease for as long as it has GPU objects, so the
    // moment one is granted is the one point where no scene uses the
    // renderer session: release it there and the next scene starts on the
    // newly chosen backend, without restarting the app.
    void ApplyPendingRenderer() noexcept
    {
        std::optional<MphRead::NativeRuntime::Rhi::SceneBackendRequest> request;
        {
            std::lock_guard guard(g_pendingRendererLock);
            request.swap(g_pendingRenderer);
        }
        if (!request) return;
        namespace Rhi = MphRead::NativeRuntime::Rhi;
        Rhi::DetachSceneWindow();
        Rhi::ReselectSceneBackend(*request);
        std::cout << "[android] renderer: now " << Rhi::SceneBackendRequestName(*request) << std::endl;
    }
}

namespace MphRead::Droid
{
    AndroidGlContextLease::AndroidGlContextLease()
        : _lock(g_androidGlContextGate)
    {
        ApplyPendingRenderer();
        // The match, the hunter preview and the previews each draw from
        // their own thread; the Vulkan device they share is theirs in turn.
        NativeRuntime::Rhi::AdoptSceneSessionThread();
    }

    std::optional<NativeRuntime::Rhi::SceneBackendRequest> TakePendingAndroidRenderer() noexcept
    {
        std::lock_guard guard(g_pendingRendererLock);
        std::optional<NativeRuntime::Rhi::SceneBackendRequest> request;
        request.swap(g_pendingRenderer);
        return request;
    }

    void RequestAndroidRenderer(NativeRuntime::Rhi::SceneBackendRequest request) noexcept
    {
        {
            std::lock_guard guard(g_pendingRendererLock);
            // Saving the settings without touching the row, or changing it
            // back before it was applied, keeps the session as it is.
            if (request == NativeRuntime::Rhi::RequestedSceneBackend()) g_pendingRenderer.reset();
            else g_pendingRenderer = request;
            if (!g_pendingRenderer) return;
        }
        // Nothing holds the GPU right now: apply it at once, so the hunter
        // preview and the map previews redraw on the new backend too.
        std::unique_lock lock(g_androidGlContextGate, std::try_to_lock);
        if (lock.owns_lock()) ApplyPendingRenderer();
    }
}
