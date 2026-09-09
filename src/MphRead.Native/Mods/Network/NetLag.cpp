#include "Mods/Network/net_lag.hpp"

#include "Mods/Network/net_transport.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace fruityprime::net {
namespace {

int parse_integer(std::string_view value, int maximum,
                  std::string_view option) {
    if (value.empty()) {
        throw std::invalid_argument(std::string(option) + " needs a value");
    }
    std::size_t consumed = 0;
    int parsed = 0;
    try {
        parsed = std::stoi(std::string(value), &consumed);
    } catch (const std::exception&) {
        throw std::invalid_argument(std::string(option) + " is invalid");
    }
    if (consumed != value.size() || parsed < 0 || parsed > maximum) {
        throw std::invalid_argument(std::string(option) + " is out of range");
    }
    return parsed;
}

double parse_percent(std::string_view value) {
    if (value.empty()) {
        throw std::invalid_argument("-netloss needs a value");
    }
    std::size_t consumed = 0;
    double parsed = 0.0;
    try {
        parsed = std::stod(std::string(value), &consumed);
    } catch (const std::exception&) {
        throw std::invalid_argument("-netloss is invalid");
    }
    if (consumed != value.size() || !std::isfinite(parsed)
        || parsed < 0.0 || parsed > 100.0) {
        throw std::invalid_argument("-netloss is out of range");
    }
    return parsed;
}

std::string format_percent(double value) {
    std::ostringstream output;
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

NetworkConditions NetLag::from_options(std::string_view netlag,
                                       std::string_view netloss) {
    NetworkConditions result;
    if (!netlag.empty()) {
        const std::size_t colon = netlag.find(':');
        const std::size_t comma = netlag.find(',');
        const std::size_t separator = colon == std::string_view::npos
            ? comma
            : comma == std::string_view::npos
                ? colon : std::min(colon, comma);
        if (separator != std::string_view::npos
            && netlag.find_first_of(":,", separator + 1)
                != std::string_view::npos) {
            throw std::invalid_argument(
                "-netlag expects RTT or RTT:JITTER milliseconds");
        }
        if (separator != std::string_view::npos) {
            result.round_trip_ms = parse_integer(
                netlag.substr(0, separator), 10'000, "-netlag");
            result.jitter_ms = parse_integer(
                netlag.substr(separator + 1), 5'000, "-netlag");
        } else {
            result.round_trip_ms = parse_integer(netlag, 10'000, "-netlag");
        }
    }
    if (!netloss.empty()) {
        result.loss_percent = parse_percent(netloss);
    }
    return result;
}

std::string NetLag::describe(const NetworkConditions& conditions) {
    if (!conditions.active()) {
        return {};
    }
    std::ostringstream output;
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
