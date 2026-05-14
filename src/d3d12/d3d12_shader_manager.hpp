#pragma once

#include "d3d12_private.h"
#include "Metal.hpp"
#include "airconv_public.h"
#include "rc/util_rc_ptr.hpp"
#include "log/log.hpp"
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>

namespace dxmt::d3d12 {

class D3D12Device;

// ── Compiled shader wrapper ──

struct CompiledShader {
    sm50_shader_t handle = nullptr;
    MTL_SHADER_REFLECTION reflection = {};
    std::string bytecode_hash;

    ~CompiledShader() {
        if (handle) SM50Destroy(handle);
    }
    CompiledShader() = default;
    CompiledShader(CompiledShader&& o) noexcept
        : handle(o.handle), reflection(o.reflection)
        , bytecode_hash(std::move(o.bytecode_hash)) {
        o.handle = nullptr;
    }
    CompiledShader(const CompiledShader&) = delete;
    CompiledShader& operator=(const CompiledShader&) = delete;
};

// ── Metal function pair ──

struct MetalShaderPair {
    WMT::Reference<WMT::Function> vertex;
    WMT::Reference<WMT::Function> fragment;
};

struct MetalComputeKernel {
    WMT::Reference<WMT::Function> kernel;
};

// ── ShaderManager: owns DXBC→MSL compilation and caching ──

class ShaderManager {
public:
    explicit ShaderManager(D3D12Device *device);
    ~ShaderManager();

    // Compile a DXBC shader bytecode. Returns cached result when available.
    CompiledShader *initializeShader(const void *bytecode, size_t size);

    // Compile vertex + pixel shaders to Metal functions.
    bool compileGraphicsPipeline(
        CompiledShader *vs, CompiledShader *ps, MetalShaderPair *out);

    // Compile a compute shader to a Metal kernel function.
    bool compileComputePipeline(
        CompiledShader *cs, MetalComputeKernel *out);

    WMT::Device getMTLDevice() const;

private:
    D3D12Device *device_;
    std::unordered_map<std::string, std::unique_ptr<CompiledShader>> cache_;
    std::mutex mutex_;
};

} // namespace dxmt::d3d12
