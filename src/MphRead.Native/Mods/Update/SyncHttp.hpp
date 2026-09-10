#pragma once

#include <exception>
#include <memory>

namespace MphRead::Mods::Update
{
    enum class HttpCompletionOption
    {
        ResponseContentRead,
        ResponseHeadersRead
    };

    // Opaque adapter value. nullptr is CancellationToken's default value;
    // non-default values are forwarded unchanged to the HTTP adapter.
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

    // Direct C++ counterpart for the managed null-reference failure that can
    // occur while evaluating this file's SendAsync(...).GetAwaiter() chain.
    class NullReferenceException final : public std::exception
    {
    public:
        [[nodiscard]] const char* what() const noexcept override;
    };

    class HttpSendOperation
    {
    public:
        virtual ~HttpSendOperation() = default;

        // Adapter boundary for TaskAwaiter<HttpResponseMessage>.GetResult().
        // It blocks to completion and rethrows the operation's stored failure.
        [[nodiscard]] virtual std::unique_ptr<HttpResponseMessage> GetResult() = 0;
    };

    class HttpClient
    {
    public:
        virtual ~HttpClient() = default;

        [[nodiscard]] virtual std::unique_ptr<HttpSendOperation> SendAsync(
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
