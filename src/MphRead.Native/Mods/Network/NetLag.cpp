#include "Mods/Network/net_lag.hpp"

#include "Mods/Network/net_transport.hpp"

#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <mutex>
#include <random>
#include <sstream>
#include <stdexcept>

namespace fruityprime::net {
namespace {

std::atomic<int> g_round_trip_ms{0};
std::atomic<int> g_jitter_ms{0};
std::atomic<double> g_loss_percent{0.0};
std::mutex g_random_mutex;
std::mt19937 g_random{std::random_device{}()};

[[nodiscard]] bool is_space(char value) noexcept {
    return std::isspace(static_cast<unsigned char>(value)) != 0;
}

[[nodiscard]] std::string_view trim(std::string_view value) noexcept {
    while (!value.empty() && is_space(value.front())) {
        value.remove_prefix(1);
    }
    while (!value.empty() && is_space(value.back())) {
        value.remove_suffix(1);
    }
    return value;
}

// Int32.TryParse(..., NumberStyles.Integer, InvariantCulture) accepts only
// decimal digits, an optional sign, and surrounding whitespace.
[[nodiscard]] bool try_parse_int(std::string_view value, int& result) noexcept {
    value = trim(value);
    if (value.empty()) {
        return false;
    }
    bool negative = false;
    if (value.front() == '+' || value.front() == '-') {
        negative = value.front() == '-';
        value.remove_prefix(1);
    }
    if (value.empty()) {
        return false;
    }
    constexpr std::uint64_t max_positive =
        static_cast<std::uint64_t>(std::numeric_limits<int>::max());
    constexpr std::uint64_t max_negative = max_positive + 1;
    std::uint64_t magnitude = 0;
    for (const char character : value) {
        if (character < '0' || character > '9') {
            return false;
        }
        magnitude = magnitude * 10U
            + static_cast<std::uint64_t>(character - '0');
        if (magnitude > (negative ? max_negative : max_positive)) {
            return false;
        }
    }
    if (negative) {
        result = magnitude == max_negative
            ? std::numeric_limits<int>::min()
            : -static_cast<int>(magnitude);
    } else {
        result = static_cast<int>(magnitude);
    }
    return true;
}

// NumberStyles.Float with the invariant culture. A classic-locale stream
// gives the same decimal-point/exponent grammar and rejects trailing data.
[[nodiscard]] bool try_parse_double(
    std::string_view value, double& result) {
    const std::string owned(value);
    std::istringstream input(owned);
    input.imbue(std::locale::classic());
    input >> result;
    if (input.fail()) {
        return false;
    }
    input >> std::ws;
    return input.eof();
}

[[nodiscard]] int parse_rtt_or_jitter(
    std::string_view value, int maximum) {
    int parsed = 0;
    if (!try_parse_int(value, parsed) || parsed < 0 || parsed > maximum) {
        return -1;
    }
    return parsed;
}

[[nodiscard]] NetworkConditions parse_values(
    std::string_view netlag, std::string_view netloss,
    bool reject_extra_netlag_fields = false) {
    NetworkConditions result;
    if (!netlag.empty()) {
        // C# uses value.Split(':', ','). It accepts more than two resulting
        // fields but only reads the first two, so preserve that quirk.
        const std::size_t separator = netlag.find_first_of(":,");
        const std::string_view rtt_text = netlag.substr(0, separator);
        const int rtt = parse_rtt_or_jitter(rtt_text, 10'000);
        if (rtt < 0) {
            throw std::invalid_argument("-netlag is invalid");
        }
        result.round_trip_ms = rtt;
        if (separator != std::string_view::npos) {
            const std::size_t next_separator =
                netlag.find_first_of(":,", separator + 1);
            if (reject_extra_netlag_fields
                && next_separator != std::string_view::npos) {
                throw std::invalid_argument("-netlag has too many fields");
            }
            const std::string_view jitter_text = netlag.substr(
                separator + 1,
                next_separator == std::string_view::npos
                    ? std::string_view::npos
                    : next_separator - separator - 1);
            const int jitter = parse_rtt_or_jitter(jitter_text, 5'000);
            if (jitter < 0) {
                throw std::invalid_argument("-netlag is invalid");
            }
            result.jitter_ms = jitter;
        }
    }
    if (!netloss.empty()) {
        double loss = 0.0;
        if (!try_parse_double(netloss, loss) || loss < 0.0 || loss > 100.0) {
            throw std::invalid_argument("-netloss is invalid");
        }
        result.loss_percent = loss;
    }
    return result;
}

[[nodiscard]] std::string format_percent(double value) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(2) << value;
    std::string text = output.str();
    while (text.size() > 1 && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text;
}

} // namespace

int NetLag::round_trip_ms() noexcept {
    return g_round_trip_ms.load(std::memory_order_relaxed);
}

int NetLag::jitter_ms() noexcept {
    return g_jitter_ms.load(std::memory_order_relaxed);
}

double NetLag::loss_percent() noexcept {
    return g_loss_percent.load(std::memory_order_relaxed);
}

bool NetLag::active() noexcept {
    // This intentionally excludes jitter-only configuration. It mirrors
    // NetLag.cs exactly: Active is RTT-or-loss, while HoldTicks still includes
    // jitter when a transport has already been enabled by that property.
    return round_trip_ms() > 0 || loss_percent() > 0.0;
}

bool NetLag::configure(std::string_view value) noexcept {
    if (trim(value).empty()) {
        return false;
    }
    try {
        const NetworkConditions parsed = parse_values(value, {});
        g_round_trip_ms.store(parsed.round_trip_ms, std::memory_order_relaxed);
        g_jitter_ms.store(parsed.jitter_ms, std::memory_order_relaxed);
        return true;
    } catch (...) {
        return false;
    }
}

bool NetLag::configure_loss(std::string_view value) noexcept {
    try {
        const NetworkConditions parsed = parse_values({}, value);
        g_loss_percent.store(parsed.loss_percent, std::memory_order_relaxed);
        return true;
    } catch (...) {
        return false;
    }
}

std::string NetLag::describe() {
    return describe(current());
}

std::chrono::steady_clock::duration NetLag::hold_duration() {
    const int round_trip = round_trip_ms();
    const int jitter = jitter_ms();
    if (round_trip <= 0 && jitter <= 0) {
        return std::chrono::steady_clock::duration::zero();
    }
    double milliseconds = round_trip / 2.0;
    if (jitter > 0) {
        std::lock_guard lock(g_random_mutex);
        std::uniform_real_distribution<double> distribution(0.0, 1.0);
        milliseconds += distribution(g_random) * jitter;
    }
    return std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double, std::milli>(milliseconds));
}

bool NetLag::drops() {
    const double loss = loss_percent();
    if (!(loss > 0.0)) {
        return false;
    }
    std::lock_guard lock(g_random_mutex);
    std::uniform_real_distribution<double> distribution(0.0, 100.0);
    return distribution(g_random) < loss;
}

NetworkConditions NetLag::current() noexcept {
    return NetworkConditions{
        round_trip_ms(), jitter_ms(), loss_percent()};
}

NetworkConditions NetLag::from_options(
    std::string_view netlag, std::string_view netloss) {
    return parse_values(netlag, netloss, true);
}

std::string NetLag::describe(const NetworkConditions& conditions) {
    if (!conditions.active()) {
        return {};
    }
    std::ostringstream output;
    output.imbue(std::locale::classic());
    if (conditions.round_trip_ms > 0) {
        output << '+' << conditions.round_trip_ms << " ms round trip";
    } else {
        output << "no added latency";
    }
    if (conditions.jitter_ms > 0) {
        output << " (jitter up to " << conditions.jitter_ms
               << " ms each way)";
    }
    if (conditions.loss_percent > 0.0) {
        output << ", " << format_percent(conditions.loss_percent)
               << "% packet loss each way";
    }
    return output.str();
}

} // namespace fruityprime::net
