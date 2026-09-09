#pragma once

#include <functional>
#include <string>
#include <vector>

namespace fruityprime::test {

struct Result {
    std::string name;
    bool passed = false;
    std::string detail;
};

class Runner {
public:
    void add(std::string name, std::function<void()> test);
    [[nodiscard]] std::vector<Result> run() const;
    [[nodiscard]] bool all_passed() const;

private:
    struct Case {
        std::string name;
        std::function<void()> test;
    };
    std::vector<Case> cases_;
};

} // namespace fruityprime::test

namespace MphReadNative {
using TestRunner = ::fruityprime::test::Runner;
using TestResult = ::fruityprime::test::Result;
namespace Test = ::fruityprime::test;
}
