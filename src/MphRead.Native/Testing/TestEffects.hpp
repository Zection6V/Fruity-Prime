#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace MphRead::Testing
{
    class TestEffects final
    {
    public:
        static void TestEffectMath();
        static void TestEffectMathFloat();
        static void TestAllEffects();
        static void TestEffectBases();
        [[nodiscard]] static std::int32_t FxDiv(std::int32_t a, std::int32_t b);
        [[nodiscard]] static std::int32_t TestFx41(
            const std::vector<std::int32_t>& parameters, std::int32_t percent);
        static void TestEntityEffects();

    private:
        TestEffects() = delete;
        TestEffects(const TestEffects&) = delete;
        TestEffects& operator=(const TestEffects&) = delete;
        TestEffects(TestEffects&&) = delete;
        TestEffects& operator=(TestEffects&&) = delete;

        class RandomState;

        static const std::shared_ptr<RandomState> _random;

        static void Nop() noexcept;
    };
}
