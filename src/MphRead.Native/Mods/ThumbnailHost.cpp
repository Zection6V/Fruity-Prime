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
                return adapter.AwaitTask(host->RenderAsync(missing, report));
            }

            if (!adapter.ThumbnailBatchCanRun())
            {
                return adapter.CompletedTask(0);
            }

            ThumbnailTaskIntRef batchTask = adapter.RunThumbnailBatchAsync(missing, report);
            return adapter.AwaitTask(batchTask);
        }
        catch (...)
        {
            // Exceptions raised before an awaited task is returned fault the C#
            // async Task<int>; do not leak them synchronously from this adapter.
            return adapter.FaultedTask(std::current_exception());
        }
    }
}
