#pragma once

// System.Threading.Tasks: the members the game calls.

#include <functional>
#include <stop_token>

namespace MphRead::NativeRuntime
{
    // Task.Run(action, token): queues action to the thread pool, unless the token
    // is already canceled, in which case it never runs.
    void TaskRun(std::function<void()> action, std::stop_token token = {});
}
