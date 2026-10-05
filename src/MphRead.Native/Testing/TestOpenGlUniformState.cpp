#include "../NativeRuntime/Rhi/OpenGL/OpenGlUniformState.hpp"
#include "../NativeRuntime/FrameTelemetry.hpp"
#include <iostream>
#include <stdexcept>

using namespace MphRead::NativeRuntime;
using namespace MphRead::NativeRuntime::Rhi::OpenGL;
namespace
{
    void Expect(bool condition, const char* why) { if (!condition) throw std::runtime_error(why); }
    void Check()
    {
        OpenGlUniformState first, second;
        using Kind = OpenGlUniformState::Kind;
        constexpr auto slot = Rhi::SceneShaderAbi::ConstantIndex("use_light");
        std::int32_t value = 1;
        const auto bytes = std::as_bytes(std::span(&value, 1));
        FrameTelemetry::Session session(128); FrameTelemetry::Eligible(true);
        {
            FrameTelemetry::Frame frame;
            FrameTelemetry::Scope measured(FrameTelemetry::Phase::Uniform);
            Expect(first.Update(slot, 3, Kind::Int, bytes), "First native program write was suppressed.");
            for (unsigned i = 0; i < 10'000; ++i)
                Expect(!first.Update(slot, 3, Kind::Int, bytes), "An unchanged native value was uploaded again.");
            Expect(second.Update(slot, 3, Kind::Int, bytes), "Two linked programs shared a value cache.");
            value = 0;
            Expect(first.Update(slot, 3, Kind::Int, bytes), "Changed native value was lost.");
            Expect(first.Update(slot, 3, Kind::Float, bytes), "Different API value type reused prior state.");
            Expect(first.Update(slot, 7, Kind::Float, bytes), "Changed native location reused prior state.");
            first.Invalidate();
            Expect(first.Update(slot, 7, Kind::Float, bytes), "External GL invalidation lost a re-upload.");
            Expect(!first.Update(slot, -1, Kind::Float, bytes), "Inactive uniform emitted a native call.");
            Expect(first.Update(Rhi::SceneShaderAbi::Constants.size(), 7, Kind::Float, bytes), "Unknown semantic lost fallback.");
            std::array<float, Rhi::MatrixStackCapacity * 16> matrices{};
            const auto matrixBytes = std::as_bytes(std::span(matrices));
            constexpr auto matrixSlot = Rhi::SceneShaderAbi::ConstantIndex("mtx_stack");
            Expect(first.Update(matrixSlot, 8, Kind::Matrix4, matrixBytes), "Full matrix stack was lost.");
            Expect(!first.Update(matrixSlot, 8, Kind::Matrix4, matrixBytes), "Full unchanged matrix stack was uploaded again.");
            matrices.back() = 2;
            Expect(first.Update(matrixSlot, 8, Kind::Matrix4, matrixBytes), "Matrix array tail mutation was lost.");
            Expect(first.Update(matrixSlot, 8, Kind::Matrix4Transposed, matrixBytes), "Transpose mutation was lost.");
            std::array<std::byte, OpenGlUniformState::Capacity + 4> oversized{};
            Expect(first.Update(matrixSlot, 8, Kind::Matrix4, oversized), "Oversized uniform lost native fallback.");
            Expect(first.Update(matrixSlot, 8, Kind::Matrix4, matrixBytes), "Fallback left an obsolete cached matrix.");
            std::uint32_t bits = 0x7fc00001;
            auto bitBytes = std::as_bytes(std::span(&bits, 1));
            Expect(first.Update(slot, 3, Kind::Float, bitBytes), "NaN write was lost.");
            Expect(!first.Update(slot, 3, Kind::Float, bitBytes), "Identical NaN bits forced a native upload.");
            bits = 0x7fc00002;
            Expect(first.Update(slot, 3, Kind::Float, bitBytes), "Different NaN bits were conflated.");
            bits = 0; Expect(first.Update(slot, 3, Kind::Float, bitBytes), "Zero write was lost.");
            bits = 0x80000000;
            Expect(first.Update(slot, 3, Kind::Float, bitBytes), "Signed zeros were conflated.");
            FrameTelemetry::Rendered();
        }
        Expect(FrameTelemetry::HasNewCounters() && session.Statistics().Frames == 1 && !session.Statistics().NewCalls,
            "Warm native uniform state operations allocated application heap storage.");
    }
}
int main()
{
    try { Check(); std::cout << "OpenGL uniform state PASS; linked-program independence; location/type/matrix mutation; interop invalidation; fallback; exact float bits; warm allocations=0\n"; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
