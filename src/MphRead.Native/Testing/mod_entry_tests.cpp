#include "Mods/mod_entry.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    char arg0[] = "FruityPrime";
    char help[] = "--HELP";
    char seconds[] = "-seconds";
    char value[] = "12";
    char spectate[] = "-spectate";
    char rejoin[] = "-rejoin";
    char rejoin_value[] = "4.5";
    char* argv[] = {arg0, help, seconds, value, spectate, rejoin,
                    rejoin_value};
    constexpr int argc = static_cast<int>(std::size(argv));

    assert(fruityprime::mods::has_flag(argc, argv, "-help"));
    assert(fruityprime::mods::has_flag(argc, argv, "help"));
    assert(fruityprime::mods::index_of_flag(argc, argv, "spectate") == 4);
    assert(fruityprime::mods::value_after(argc, argv, "-seconds") == "12");
    assert(fruityprime::mods::integer_after(argc, argv, "-seconds", 1) == 12);
    assert(fruityprime::mods::integer_after(argc, argv, "-missing", 7) == 7);
    char hunter_flag[] = "-HuNtEr";
    char hunter_value[] = "tRaCe";
    char* hunter_argv[] = {arg0, hunter_flag, hunter_value};
    assert(fruityprime::mods::hunter_after(
               static_cast<int>(std::size(hunter_argv)), hunter_argv,
               "hunter") == 2);
    char invalid_hunter[] = "not-a-hunter";
    char* invalid_hunter_argv[] = {arg0, hunter_flag, invalid_hunter};
    assert(fruityprime::mods::hunter_after(
               static_cast<int>(std::size(invalid_hunter_argv)),
               invalid_hunter_argv, "hunter", 6) == 6);
    assert(!fruityprime::mods::seconds_value("-seconds").has_value());
    const auto parsed = fruityprime::mods::seconds_value("1.25");
    assert(parsed.has_value() && std::abs(*parsed - 1.25) < 0.000001);
    assert(!fruityprime::mods::seconds_value("1.2x").has_value());

    const auto schedule = fruityprime::mods::spectate_arguments(argc, argv);
    assert(schedule.spectate_at_seconds == 0.0);
    assert(schedule.rejoin_at_seconds == 4.5);
    const auto repeated = fruityprime::mods::values_after(argc, argv, "seconds");
    assert(repeated.size() == 1 && repeated.front() == "12");

    int first = 0;
    int last = 0;
    assert(fruityprime::mods::parse_port_range("27900-27919", first, last)
        && first == 27900 && last == 27919);
    assert(!fruityprime::mods::parse_port_range("0-27919", first, last)
        && !fruityprime::mods::parse_port_range("27919-27900", first, last)
        && !fruityprime::mods::parse_port_range("27900", first, last));

    std::cout << "native mod entry tests passed\n";
    return 0;
}
