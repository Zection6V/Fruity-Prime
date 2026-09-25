#pragma once

// System.Threading.Tasks: the members the game calls.

#include <functional>
#include <string>
#include <stop_token>

namespace MphRead::NativeRuntime
{
    // Thread.CurrentThread.Name = name.
    void SetCurrentThreadName(const std::string& name);
    // Thread.Sleep(milliseconds).
    void ThreadSleep(std::int32_t milliseconds);

    // Task.Run(action, token): queues action to the thread pool, unless the token
    // is already canceled, in which case it never runs.
    void TaskRun(std::function<void()> action, std::stop_token token = {});
}
