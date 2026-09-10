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

        std::unique_ptr<HttpSendOperation> operation
            = client->SendAsync(request, completion, cancel);
        if (operation == nullptr)
        {
            throw NullReferenceException();
        }
        return operation->GetResult();
    }
}
