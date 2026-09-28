#pragma once

#include "../../../RendererGpuMesh.hpp"

#include <memory>

namespace MphRead::NativeRuntime::Rhi::OpenGL
{
    [[nodiscard]] std::shared_ptr<MphRead::GpuMeshResource> CreateGpuMeshResource(
        const MphRead::RendererGeometry& geometry);
}
