#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace fruityprime::update::http {

struct Response {
    int status_code = 0;
    std::uint64_t content_length = 0;
    std::string body;
    std::string error;

    [[nodiscard]] bool success() const noexcept {
        return status_code >= 200 && status_code < 300;
    }
};

// Synchronous by design: callers invoke it from a command/front-end worker,
// and this is the native counterpart of the managed SyncHttp adapter.
[[nodiscard]] Response get(std::string_view url, int timeout_ms,
                           std::string_view user_agent);

} // namespace fruityprime::update::http

namespace fruityprime::update {

class SyncHttp final {
public:
    [[nodiscard]] static http::Response Send(
        std::string_view url, int timeout_ms = 20'000,
        std::string_view user_agent = "FruityPrime");
};

} // namespace fruityprime::update

namespace MphReadNative::Mods::Update {
using SyncHttp = ::fruityprime::update::SyncHttp;
}
