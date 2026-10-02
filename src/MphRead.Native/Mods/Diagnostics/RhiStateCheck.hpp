#pragma once
namespace MphRead::NativeRuntime::Rhi { class GraphicsDevice; }
namespace MphRead::Mods::Diagnostics
{
    void CheckRhiResourceStates(NativeRuntime::Rhi::GraphicsDevice& device);
}
