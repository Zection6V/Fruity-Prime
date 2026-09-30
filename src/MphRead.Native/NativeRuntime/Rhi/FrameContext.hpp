#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

// The GPU lifetime contract, the same for every backend.
//
// Frames are numbered from 1. At most FramesInFlight frames' GPU work is
// outstanding: BeginFrame for frame N first retires frame N - FramesInFlight,
// which is the last one to have used the same FrameContext slot. A resource
// the frontend destroys is not destroyed then -- the GPU may still be reading
// it for a frame already submitted -- but retired: queued with the number of
// the frame it was last usable in, and destroyed natively once the device
// knows that frame's work is complete (OpenGL: a GLsync fence; Vulkan: the
// frame's fence or timeline value). WaitIdle waits for everything and
// destroys every retired resource at once, which is what a scene's release
// does, in the context it drew with.
namespace MphRead::NativeRuntime::Rhi
{
    inline constexpr std::uint32_t FramesInFlight = 2;

    struct FrameContext final
    {
        // The frame's number; the retirement value of what is destroyed during it.
        std::uint64_t Number = 0;
        // Which of the FramesInFlight per-frame slots it uses.
        std::uint32_t Slot = 0;
    };

    struct GpuResourceStatistics final
    {
        std::uint32_t Textures = 0;
        std::uint32_t Buffers = 0;
        std::uint32_t Renderbuffers = 0;
        std::uint32_t Shaders = 0;
        std::uint32_t Programs = 0;
        std::uint32_t Framebuffers = 0;
        std::uint32_t Retired = 0;
        std::uint64_t CompletedFrame = 0;
        // Times the CPU has stopped to wait for the GPU (a fence or the whole
        // device). A frame should cost none in steady state; OpenGL, which
        // waits inside the driver, reports 0.
        std::uint64_t HostWaits = 0;

        bool operator==(const GpuResourceStatistics&) const = default;
    };

    // Native objects waiting for the GPU to finish with them. Backend
    // neutral: T is whatever a backend destroys (a GL name and its kind, a
    // VkImage and its memory).
    template <typename T>
    class RetirementQueue final
    {
    public:
        void Retire(T object, std::uint64_t lastUsedFrame)
        {
            _entries.push_back(Entry{std::move(object), lastUsedFrame});
        }

        // Destroy everything last used in a frame the GPU has completed.
        template <typename Destroy>
        std::size_t Collect(std::uint64_t completedFrame, Destroy&& destroy)
        {
            std::size_t destroyed = 0;
            auto keep = _entries.begin();
            for (auto it = _entries.begin(); it != _entries.end(); ++it)
            {
                if (it->LastUsedFrame <= completedFrame)
                {
                    destroy(it->Object);
                    ++destroyed;
                }
                else
                {
                    if (keep != it)
                    {
                        *keep = std::move(*it);
                    }
                    ++keep;
                }
            }
            _entries.erase(keep, _entries.end());
            return destroyed;
        }

        template <typename Destroy>
        std::size_t CollectAll(Destroy&& destroy)
        {
            for (Entry& entry : _entries)
            {
                destroy(entry.Object);
            }
            const std::size_t destroyed = _entries.size();
            _entries.clear();
            return destroyed;
        }

        // Take back objects that are about to be used again rather than
        // destroyed (a texture re-created under the handle it had).
        template <typename Matches>
        std::size_t Cancel(Matches&& matches)
        {
            const auto end = std::remove_if(_entries.begin(), _entries.end(),
                [&matches](const Entry& entry) { return matches(entry.Object); });
            const auto cancelled = static_cast<std::size_t>(std::distance(end, _entries.end()));
            _entries.erase(end, _entries.end());
            return cancelled;
        }

        [[nodiscard]] std::size_t Size() const noexcept { return _entries.size(); }

    private:
        struct Entry final
        {
            T Object;
            std::uint64_t LastUsedFrame;
        };

        std::vector<Entry> _entries{};
    };
}
