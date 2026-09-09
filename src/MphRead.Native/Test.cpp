#include "Test.hpp"

#include <exception>
#include <utility>

namespace fruityprime::test {

void Runner::add(std::string name, std::function<void()> test) {
    cases_.push_back(Case{std::move(name), std::move(test)});
}

std::vector<Result> Runner::run() const {
    std::vector<Result> results;
    results.reserve(cases_.size());
    for (const auto& test : cases_) {
        Result result;
        result.name = test.name;
        try {
            if (test.test) {
                test.test();
            }
            result.passed = true;
        } catch (const std::exception& error) {
            result.detail = error.what();
        } catch (...) {
            result.detail = "unknown test failure";
        }
        results.push_back(std::move(result));
    }
    return results;
}

bool Runner::all_passed() const {
    for (const auto& result : run()) {
        if (!result.passed) {
            return false;
        }
    }
    return true;
}

} // namespace fruityprime::test
