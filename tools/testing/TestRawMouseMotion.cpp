#include "../../src/MphRead.Native.Qt/Platform/RawMouseMotion.hpp"
#include <iostream>
#include <chrono>
#include <stdexcept>

using MphRead::Qt::RawMouseMotion;
namespace
{
    void Expect(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
}
int main()
{
    try
    {
        RawMouseMotion motion;
        motion.Add(10, -10, false);
        Expect(motion.TakeDelta() == std::pair<std::int64_t, std::int64_t>{0, 0}, "Inactive motion leaked.");
        motion.SetCapture(true);
        motion.Add(8, 0, false);
        motion.SetCapture(true); // Repeated capture policy must retain this pump.
        motion.Add(0, -4, false);
        motion.Add(0, 0, false);
        motion.Add(999, 999, true);
        Expect(motion.TakeDelta() == std::pair<std::int64_t, std::int64_t>{8, -4}, "Relative/absolute axis ownership differs.");
        Expect(motion.TakeDelta() == std::pair<std::int64_t, std::int64_t>{0, 0}, "Delta consumed twice.");
        motion.Add(12, 14, false);
        motion.Discard();
        Expect(motion.TakeDelta() == std::pair<std::int64_t, std::int64_t>{0, 0}, "Discard retained motion.");
        motion.Add(12, 14, false);
        motion.SetCapture(false);
        motion.Add(100, 100, false);
        motion.SetCapture(true);
        Expect(motion.TakeDelta() == std::pair<std::int64_t, std::int64_t>{0, 0}, "Focus/grab transition retained stale motion.");
        constexpr auto largest = std::numeric_limits<std::int32_t>::max();
        for (int i = 0; i < 8000; ++i) motion.Add(largest, -largest, false);
        Expect(motion.TakeDelta() == std::pair<std::int64_t, std::int64_t>{std::int64_t(largest) * 8000, -std::int64_t(largest) * 8000},
            "Burst overflowed a 32-bit accumulator.");
        Expect(RawMouseMotion::AddClamped(std::numeric_limits<std::int64_t>::max(), 1) == std::numeric_limits<std::int64_t>::max(),
            "Positive saturation overflow.");
        Expect(RawMouseMotion::AddClamped(std::numeric_limits<std::int64_t>::min(), -1) == std::numeric_limits<std::int64_t>::min(),
            "Negative saturation overflow.");
        // Synthetic polling workload only, not a physical high-polling claim.
        const auto start = std::chrono::steady_clock::now();
        for (int frame = 0; frame < 6000; ++frame)
        {
            for (int i = 0; i < 134; ++i) motion.Add(i % 2 ? -3 : 3, 1, false);
            Expect(motion.TakeDelta() == std::pair<std::int64_t, std::int64_t>{0, 134}, "8000Hz-equivalent burst lost samples.");
        }
        std::cout << "PASS raw motion contract; synthetic 804000 samples in "
            << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() << " ms\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
