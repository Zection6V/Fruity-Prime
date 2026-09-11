#include "SyncHttp.hpp"

namespace MphRead::Mods::Update
{
    const char* NullReferenceException::what() const noexcept
    {
        return "Object reference not set to an instance of an object.";
    }

    std::unique_ptr<HttpResponseMessage> SyncHttp::Send(
        HttpClient* client,
        HttpRequestMessage* request,
        HttpCompletionOption completion,
        CancellationToken cancel)
    {
        if (client == nullptr)
        {
            throw NullReferenceException();
        }

        std::unique_ptr<HttpResponseTask> task
            = client->SendAsync(request, completion, cancel);
        if (task == nullptr)
        {
            throw NullReferenceException();
        }

        return task->GetAwaiter().GetResult();
    }
}
