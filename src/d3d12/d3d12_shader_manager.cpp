#include "d3d12_shader_manager.hpp"
#include "d3d12_device.hpp"
#include "sha1/sha1_util.hpp"
#include "log/log.hpp"
#include <cstring>

namespace dxmt::d3d12 {

ShaderManager::ShaderManager(D3D12Device *device)
    : device_(device) {
    TRACE("ShaderManager created");
}

ShaderManager::~ShaderManager() {
    TRACE("ShaderManager destroyed: ", cache_.size(), " cached shaders");
}

WMT::Device ShaderManager::getMTLDevice() const {
    return device_->GetMTLDevice();
}

CompiledShader *ShaderManager::initializeShader(
    const void *bytecode, size_t size) {

    if (!bytecode || size == 0) {
        ERR("ShaderManager: null bytecode");
        return nullptr;
    }

    // Compute hash for caching
    auto hash = Sha1HashState::compute(bytecode, size);
    std::string hash_str = hash.string();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(hash_str);
        if (it != cache_.end()) {
            TRACE("ShaderManager: cache hit for ", hash_str.substr(0, 16));
            return it->second.get();
        }
    }

    sm50_shader_t handle = nullptr;
    MTL_SHADER_REFLECTION refl = {};
    sm50_error_t err = nullptr;

    int result = SM50Initialize(bytecode, size, &handle, &refl, &err);
    if (result != 0) {
        ERR("ShaderManager: SM50Initialize failed: ",
            SM50GetErrorMessageString(err));
        SM50FreeError(err);
        return nullptr;
    }

    auto shader = std::make_unique<CompiledShader>();
    shader->handle = handle;
    shader->reflection = refl;
    shader->bytecode_hash = hash_str;

    TRACE("ShaderManager: compiled shader ", hash_str.substr(0, 16),
          " cbufs=", refl.NumConstantBuffers,
          " args=", refl.NumArguments);

    CompiledShader *ptr = shader.get();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_[hash_str] = std::move(shader);
    }

    return ptr;
}

// ── Graphics pipeline compilation ──

bool ShaderManager::compileGraphicsPipeline(
    CompiledShader *vs,
    CompiledShader *ps,
    MetalShaderPair *out) {

    if (!vs || !ps || !out) {
        ERR("ShaderManager: invalid args for compileGraphicsPipeline");
        return false;
    }

    WMT::Device mtl_device = getMTLDevice();

    // ── Compile vertex shader (minimal args) ──
    SM50_SHADER_COMMON_DATA vs_common = {};
    vs_common.type = SM50_SHADER_COMMON;
    vs_common.metal_version = SM50_SHADER_METAL_310;
    vs_common.flags = SM50_SHADER_FLAG_SAMPLE_NAN_TO_ZERO;

    std::string vs_name = "vs_" + vs->bytecode_hash.substr(0, 8);

    sm50_bitcode_t vs_bitcode = nullptr;
    sm50_error_t vs_err = nullptr;
    int vs_result = SM50Compile(
        vs->handle,
        (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&vs_common,
        vs_name.c_str(),
        &vs_bitcode, &vs_err);

    if (vs_result != 0) {
        ERR("ShaderManager: VS compile failed: ",
            SM50GetErrorMessageString(vs_err));
        SM50FreeError(vs_err);
        return false;
    }

    // ── Compile pixel shader ──
    SM50_SHADER_PSO_PIXEL_SHADER_DATA ps_data = {};
    ps_data.type = SM50_SHADER_PSO_PIXEL_SHADER;
    ps_data.sample_mask = 0xFFFFFFFF;
    ps_data.dual_source_blending = false;
    ps_data.disable_depth_output = false;
    ps_data.unorm_output_reg_mask = 0;

    SM50_SHADER_COMMON_DATA ps_common = {};
    ps_common.type = SM50_SHADER_COMMON;
    ps_common.metal_version = SM50_SHADER_METAL_310;
    ps_common.flags = SM50_SHADER_FLAG_SAMPLE_NAN_TO_ZERO;

    ps_data.next = &ps_common;

    std::string ps_name = "ps_" + ps->bytecode_hash.substr(0, 8);

    sm50_bitcode_t ps_bitcode = nullptr;
    sm50_error_t ps_err = nullptr;
    int ps_result = SM50Compile(
        ps->handle,
        (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&ps_data,
        ps_name.c_str(),
        &ps_bitcode, &ps_err);

    if (ps_result != 0) {
        ERR("ShaderManager: PS compile failed: ",
            SM50GetErrorMessageString(ps_err));
        SM50FreeError(ps_err);
        SM50DestroyBitcode(vs_bitcode);
        return false;
    }

    // ── Load Metal libraries ──
    SM50_COMPILED_BITCODE vs_bc = {};
    SM50GetCompiledBitcode(vs_bitcode, &vs_bc);

    SM50_COMPILED_BITCODE ps_bc = {};
    SM50GetCompiledBitcode(ps_bitcode, &ps_bc);

    WMT::Library vs_lib = mtl_device.newLibraryWithData(
        vs_bc.Data, vs_bc.Size);
    WMT::Library ps_lib = mtl_device.newLibraryWithData(
        ps_bc.Data, ps_bc.Size);

    SM50DestroyBitcode(vs_bitcode);
    SM50DestroyBitcode(ps_bitcode);

    if (!vs_lib || !ps_lib) {
        ERR("ShaderManager: failed to load Metal libraries");
        return false;
    }

    out->vertex = vs_lib.newFunctionWithName(vs_name.c_str());
    out->fragment = ps_lib.newFunctionWithName(ps_name.c_str());

    if (!out->vertex || !out->fragment) {
        ERR("ShaderManager: failed to get Metal functions");
        return false;
    }

    TRACE("ShaderManager: graphics pipeline compiled (VS=", vs_name,
          " PS=", ps_name, ")");
    return true;
}

// ── Compute pipeline compilation ──

bool ShaderManager::compileComputePipeline(
    CompiledShader *cs,
    MetalComputeKernel *out) {

    if (!cs || !out) {
        ERR("ShaderManager: invalid args for compileComputePipeline");
        return false;
    }

    WMT::Device mtl_device = getMTLDevice();

    SM50_SHADER_COMMON_DATA cs_common = {};
    cs_common.type = SM50_SHADER_COMMON;
    cs_common.metal_version = SM50_SHADER_METAL_310;
    cs_common.flags = SM50_SHADER_FLAG_SAMPLE_NAN_TO_ZERO;

    std::string cs_name = "cs_" + cs->bytecode_hash.substr(0, 8);

    sm50_bitcode_t cs_bitcode = nullptr;
    sm50_error_t cs_err = nullptr;
    int cs_result = SM50Compile(
        cs->handle,
        (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&cs_common,
        cs_name.c_str(),
        &cs_bitcode, &cs_err);

    if (cs_result != 0) {
        ERR("ShaderManager: CS compile failed: ",
            SM50GetErrorMessageString(cs_err));
        SM50FreeError(cs_err);
        return false;
    }

    SM50_COMPILED_BITCODE cs_bc = {};
    SM50GetCompiledBitcode(cs_bitcode, &cs_bc);

    WMT::Library cs_lib = mtl_device.newLibraryWithData(
        cs_bc.Data, cs_bc.Size);

    SM50DestroyBitcode(cs_bitcode);

    if (!cs_lib) {
        ERR("ShaderManager: failed to load Metal compute library");
        return false;
    }

    out->kernel = cs_lib.newFunctionWithName(cs_name.c_str());

    if (!out->kernel) {
        ERR("ShaderManager: failed to get Metal kernel function");
        return false;
    }

    TRACE("ShaderManager: compute pipeline compiled (CS=", cs_name, ")");
    return true;
}

} // namespace dxmt::d3d12
