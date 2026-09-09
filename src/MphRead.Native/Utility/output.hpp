#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace fruityprime::utility::output {

// The managed Output class serializes console writes and reads on a small
// worker so callers do not interleave a prompt with another operation.  A
// numeric token is used instead of Guid: it is process-local, which is the
// only identity the original batch lock needs.
using BatchId = std::uint64_t;

class Console final {
public:
    Console() = delete;

    // Idempotent. The worker is started lazily by the first operation too,
    // matching Output.Begin followed by the normal utility calls.
    static void begin();

    // Only one batch may own the console at a time. Calls carrying the
    // returned token are allowed through while other callers wait.
    [[nodiscard]] static BatchId start_batch();
    static void end_batch();

    static void write(std::string_view message = {},
                      std::optional<BatchId> batch = std::nullopt);
    static void clear(std::optional<BatchId> batch = std::nullopt);

    // Queue a prompt and wait for the line read by the worker. An empty
    // prompt is still a real read; this is the useful native equivalent of
    // the nullable managed prompt API.
    [[nodiscard]] static std::string read(
        std::optional<std::string_view> message = std::nullopt,
        std::optional<BatchId> batch = std::nullopt);

    // Stop the worker and drain all queued operations. It is safe to call
    // this more than once and begin() can start it again.
    static void end();
};

} // namespace fruityprime::utility::output
