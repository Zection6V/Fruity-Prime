#pragma once

#include <cstddef>
#include <exception>

namespace MphRead::Mods
{
    // Thin non-owning reference adapters for the managed IReadOnlyList<string>,
    // Action<string>, and Task<int> values used by ThumbnailHost.cs. Native is
    // the identity-bearing platform object; this unit never owns or copies it.
    struct ThumbnailRoomsRef
    {
        void* Native;
    };

    struct ThumbnailReportRef
    {
        void* Native;
    };

    struct ThumbnailTaskIntRef
    {
        void* Native;
    };

    /// <summary>
    /// Platform-specific host for thumbnail rendering. Desktop and Android
    /// register their own implementation so the launcher can request thumbnails
    /// without taking a dependency on any windowing or GL stack.
    /// </summary>
    class IThumbnailHost
    {
    public:
        virtual ~IThumbnailHost() = default;

        virtual ThumbnailTaskIntRef RenderAsync(ThumbnailRoomsRef rooms,
            ThumbnailReportRef report) = 0;
    };

    // Narrow bridge for ThumbnailGenerator/ThumbnailBatch and managed-style task
    // completion. Those dependencies do not yet have native counterparts; this
    // declaration expresses only the operations consumed by ThumbnailHost.cs.
    class ThumbnailHostAdapter
    {
    public:
        virtual ~ThumbnailHostAdapter() = default;

        [[nodiscard]] virtual ThumbnailRoomsRef ThumbnailGeneratorMissingThumbnails() = 0;
        [[nodiscard]] virtual std::size_t ThumbnailRoomsCount(ThumbnailRoomsRef rooms) const = 0;

        [[nodiscard]] virtual bool ThumbnailBatchCanRun() const = 0;

        // CompletedTask and FaultedTask are the platform task equivalents used
        // for C# async-method completion before an awaited operation is reached.
        [[nodiscard]] virtual ThumbnailTaskIntRef CompletedTask(int result) noexcept = 0;
        [[nodiscard]] virtual ThumbnailTaskIntRef FaultedTask(std::exception_ptr error) noexcept = 0;

        // Produce the outer async-method task for `return await task`, preserving
        // the platform await continuation/context, result, exception, and
        // cancellation behavior rather than merely returning the inner task.
        [[nodiscard]] virtual ThumbnailTaskIntRef AwaitTask(ThumbnailTaskIntRef task) = 0;

        // Exact platform equivalent of:
        // Task.Run(() => ThumbnailBatch.Run(
        //     rooms,
        //     ThumbnailBatch.DefaultParallelism,
        //     ThumbnailGenerator.ThumbnailWidth,
        //     ThumbnailGenerator.ThumbnailHeight,
        //     report))
        // The three property reads and Run call therefore occur inside the
        // asynchronously dispatched work. The exact rooms/report references must
        // be retained for that work; this unit deliberately provides no worker.
        [[nodiscard]] virtual ThumbnailTaskIntRef RunThumbnailBatchAsync(
            ThumbnailRoomsRef rooms,
            ThumbnailReportRef report) = 0;
    };

    // Supplied by the eventual native ThumbnailGenerator/ThumbnailBatch platform
    // bridge. It is intentionally declared, not implemented, in this migration.
    [[nodiscard]] ThumbnailHostAdapter& GetThumbnailHostAdapter() noexcept;

    class ThumbnailHost final
    {
    public:
        /// <summary>Active platform renderer, or null when none is available.</summary>
        [[nodiscard]] static IThumbnailHost* Current() noexcept;
        static void Current(IThumbnailHost* value) noexcept;

        [[nodiscard]] static bool CanRender();

        /// <summary>
        /// Generate thumbnails that are currently missing. The platform host is
        /// preferred; desktop falls back to the existing ThumbnailBatch pipeline.
        /// </summary>
        [[nodiscard]] static ThumbnailTaskIntRef RenderMissingAsync(ThumbnailReportRef report);

    private:
        ThumbnailHost() = delete;

        static IThumbnailHost* _current;
    };
}
