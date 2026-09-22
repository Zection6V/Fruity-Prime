#include "Tasks.hpp"

#include <thread>
#include <utility>

namespace MphRead::NativeRuntime
{
    void TaskRun(std::function<void()> action, std::stop_token token)
    {
        // Task.Run with a canceled token returns a canceled task without running.
        if (token.stop_requested())
        {
            return;
        }
        std::thread([work = std::move(action)]()
        {
            // An exception inside the task is captured by the task; one nobody
            // observes does not end the process.
            try
            {
                work();
            }
            catch (...)
            {
            }
        }).detach();
    }
}
