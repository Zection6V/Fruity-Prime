/*
 * Native counterpart of MphRead/Mods/Update/SyncHttp.cs.
 *
 * HttpClient has no portable synchronous Send method on the Android handler;
 * the native side has the same explicit blocking adapter. Windows builds use
 * WinHTTP so the MinGW build has no third-party runtime dependency.
 */
#include "update_http.hpp"

#include <algorithm>
#include <array>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#endif

namespace fruityprime::update::http {
namespace {

#ifdef _WIN32

class Handle final {
public:
    explicit Handle(HINTERNET value = nullptr) noexcept : value_(value) {}
    ~Handle() {
        if (value_ != nullptr) {
            WinHttpCloseHandle(value_);
        }
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept : value_(other.value_) {
        other.value_ = nullptr;
    }
    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) {
            if (value_ != nullptr) {
                WinHttpCloseHandle(value_);
            }
            value_ = other.value_;
            other.value_ = nullptr;
        }
        return *this;
    }
    [[nodiscard]] HINTERNET get() const noexcept { return value_; }
    [[nodiscard]] explicit operator bool() const noexcept {
        return value_ != nullptr;
    }

private:
    HINTERNET value_ = nullptr;
};

[[nodiscard]] std::wstring widen(std::string_view value) {
    if (value.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                           value.data(),
                                           static_cast<int>(value.size()),
                                           nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), result.data(),
                            length) != length) {
        return {};
    }
    return result;
}

[[nodiscard]] std::string win_error(const char* operation) {
    return std::string(operation) + " failed (WinHTTP "
        + std::to_string(GetLastError()) + ")";
}

#endif

} // namespace

Response get(std::string_view url, int timeout_ms,
             std::string_view user_agent) {
    Response result;
    if (url.empty()) {
        result.error = "HTTP URL is empty";
        return result;
    }
#ifdef _WIN32
    const std::wstring wide_url = widen(url);
    const std::wstring wide_agent = widen(user_agent);
    if (wide_url.empty() || wide_agent.empty()) {
        result.error = "HTTP URL or user agent is not valid UTF-8";
        return result;
    }

    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    wchar_t host[256]{};
    wchar_t path[32768]{};
    components.lpszHostName = host;
    components.dwHostNameLength = static_cast<DWORD>(std::size(host));
    components.lpszUrlPath = path;
    components.dwUrlPathLength = static_cast<DWORD>(std::size(path));
    if (WinHttpCrackUrl(wide_url.c_str(), static_cast<DWORD>(wide_url.size()),
                        0, &components) == FALSE) {
        result.error = win_error("WinHttpCrackUrl");
        return result;
    }
    // This API is only used for release metadata/assets. Refuse plain HTTP
    // even if a caller bypasses UpdateCheck::is_allowed_url.
    if (components.nScheme != INTERNET_SCHEME_HTTPS) {
        result.error = "HTTP update requests must use HTTPS";
        return result;
    }

    const int timeout = std::clamp(timeout_ms, 1, 10 * 60 * 1000);
    Handle session(WinHttpOpen(wide_agent.c_str(),
                               WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                               WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
        result.error = win_error("WinHttpOpen");
        return result;
    }
    if (WinHttpSetTimeouts(session.get(), timeout, timeout, timeout, timeout)
        == FALSE) {
        result.error = win_error("WinHttpSetTimeouts");
        return result;
    }
    Handle connection(WinHttpConnect(session.get(), components.lpszHostName,
                                     components.nPort, 0));
    if (!connection) {
        result.error = win_error("WinHttpConnect");
        return result;
    }
    // lpszUrlPath does not include the query when WinHttpCrackUrl fills the
    // components manually, so append lpszExtraInfo when present.
    std::wstring request_path(components.lpszUrlPath,
                              components.dwUrlPathLength);
    if (components.lpszExtraInfo != nullptr
        && components.dwExtraInfoLength > 0) {
        request_path.append(components.lpszExtraInfo,
                            components.dwExtraInfoLength);
    }
    Handle request(WinHttpOpenRequest(
        connection.get(), L"GET", request_path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE));
    if (!request) {
        result.error = win_error("WinHttpOpenRequest");
        return result;
    }
    const wchar_t headers[] =
        L"Accept: application/vnd.github+json\r\n";
    if (WinHttpSendRequest(request.get(), headers, -1L,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) == FALSE
        || WinHttpReceiveResponse(request.get(), nullptr) == FALSE) {
        result.error = win_error("WinHTTP request");
        return result;
    }

    DWORD status = 0;
    DWORD status_size = sizeof(status);
    if (WinHttpQueryHeaders(request.get(),
                            WINHTTP_QUERY_STATUS_CODE
                                | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status,
                            &status_size, WINHTTP_NO_HEADER_INDEX) == FALSE) {
        result.error = win_error("WinHttpQueryHeaders(status)");
        return result;
    }
    result.status_code = static_cast<int>(status);
    DWORD length = 0;
    DWORD length_size = sizeof(length);
    if (WinHttpQueryHeaders(request.get(),
                            WINHTTP_QUERY_CONTENT_LENGTH
                                | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &length,
                            &length_size, WINHTTP_NO_HEADER_INDEX) != FALSE) {
        result.content_length = length;
    }

    std::array<char, 64 * 1024> buffer{};
    for (;;) {
        DWORD available = 0;
        if (WinHttpQueryDataAvailable(request.get(), &available) == FALSE) {
            result.error = win_error("WinHttpQueryDataAvailable");
            result.body.clear();
            return result;
        }
        if (available == 0) {
            break;
        }
        while (available > 0) {
            const DWORD wanted = std::min<DWORD>(
                available, static_cast<DWORD>(buffer.size()));
            DWORD read = 0;
            if (WinHttpReadData(request.get(), buffer.data(), wanted, &read)
                == FALSE) {
                result.error = win_error("WinHttpReadData");
                result.body.clear();
                return result;
            }
            if (read == 0) {
                break;
            }
            result.body.append(buffer.data(), read);
            available -= read;
        }
    }
    return result;
#else
    static_cast<void>(timeout_ms);
    static_cast<void>(user_agent);
    result.error = "native HTTP updates are only available on Windows";
    return result;
#endif
}

} // namespace fruityprime::update::http

namespace fruityprime::update {

http::Response SyncHttp::Send(std::string_view url, int timeout_ms,
                              std::string_view user_agent) {
    return http::get(url, timeout_ms, user_agent);
}

} // namespace fruityprime::update
