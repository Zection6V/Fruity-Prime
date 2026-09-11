#pragma once

#include <cstdint>
#include <exception>
#include <memory>

namespace MphRead::Mods::Update
{
    enum class HttpCompletionOption : std::int32_t
    {
        ResponseContentRead = 0,
        ResponseHeadersRead = 1
    };

    // Opaque adapter value for System.Threading.CancellationToken. nullptr is
    // the default, non-cancelable token; non-default values are forwarded
    // unchanged through SendAsync.
    using CancellationToken = const void*;

    class HttpRequestMessage
    {
    public:
        virtual ~HttpRequestMessage() = default;
    };

    class HttpResponseMessage
    {
    public:
        virtual ~HttpResponseMessage() = default;
    };

    // Native counterpart for a managed null-reference failure while evaluating
    // the SendAsync(...).GetAwaiter().GetResult() call chain.
    class NullReferenceException final : public std::exception
    {
    public:
        [[nodiscard]] const char* what() const noexcept override;
    };

    class HttpResponseAwaiter
    {
    public:
        virtual ~HttpResponseAwaiter() = default;

        // TaskAwaiter<HttpResponseMessage>.GetResult(): block until completion,
        // return the task result, and rethrow the task's stored failure directly.
        [[nodiscard]] virtual std::unique_ptr<HttpResponseMessage> GetResult() = 0;
    };

    class HttpResponseTask
    {
    public:
        virtual ~HttpResponseTask() = default;

        // Task<HttpResponseMessage>.GetAwaiter(). The returned awaiter remains
        // valid for the lifetime of this task adapter.
        [[nodiscard]] virtual HttpResponseAwaiter& GetAwaiter() = 0;
    };

    class HttpClient
    {
    public:
        virtual ~HttpClient() = default;

        [[nodiscard]] virtual std::unique_ptr<HttpResponseTask> SendAsync(
            HttpRequestMessage* request,
            HttpCompletionOption completion,
            CancellationToken cancel) = 0;
    };

    class SyncHttp final
    {
    public:
        [[nodiscard]] static std::unique_ptr<HttpResponseMessage> Send(
            HttpClient* client,
            HttpRequestMessage* request,
            HttpCompletionOption completion = HttpCompletionOption::ResponseContentRead,
            CancellationToken cancel = nullptr);

        SyncHttp() = delete;
        SyncHttp(const SyncHttp&) = delete;
        SyncHttp& operator=(const SyncHttp&) = delete;
    };
}
