#include "ThumbnailHost.hpp"

namespace MphRead::Mods
{
    IThumbnailHost* ThumbnailHost::_current = nullptr;

    IThumbnailHost* ThumbnailHost::Current() noexcept
    {
        return _current;
    }

    void ThumbnailHost::Current(IThumbnailHost* value) noexcept
    {
        _current = value;
    }

    bool ThumbnailHost::CanRender()
    {
        return Current() != nullptr || GetThumbnailHostAdapter().ThumbnailBatchCanRun();
    }

    ThumbnailTaskIntRef ThumbnailHost::RenderMissingAsync(ThumbnailReportRef report)
    {
        ThumbnailHostAdapter& adapter = GetThumbnailHostAdapter();

        try
        {
            ThumbnailRoomsRef missing = adapter.ThumbnailGeneratorMissingThumbnails();
            if (adapter.ThumbnailRoomsCount(missing) == 0)
            {
                return adapter.CompletedTask(0);
            }

            IThumbnailHost* host = Current();
            if (host != nullptr)
            {
                // There is no work after the await in the C# method. Returning
                // the host task directly preserves its completion, result, and
                // exception while keeping the exact missing/report identities.
                return host->RenderAsync(missing, report);
            }

            if (!adapter.ThumbnailBatchCanRun())
            {
                return adapter.CompletedTask(0);
            }

            // C# evaluates these property arguments from left to right. Keep the
            // reads per-call and ordered; none of these values are retained.
            int parallelism = adapter.ThumbnailBatchDefaultParallelism();
            int width = adapter.ThumbnailGeneratorThumbnailWidth();
            int height = adapter.ThumbnailGeneratorThumbnailHeight();

            return adapter.RunThumbnailBatchAsync(
                missing,
                parallelism,
                width,
                height,
                report);
        }
        catch (...)
        {
            // Exceptions raised before an awaited task is returned fault the C#
            // async Task<int>; do not leak them synchronously from this adapter.
            return adapter.FaultedTask(std::current_exception());
        }
    }
}
