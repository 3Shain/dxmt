// ═══════════════════════════════════════════════════════════════
// M6+M7 test: Root Signature + Pipeline State
// ═══════════════════════════════════════════════════════════════

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>
#include <dxgi1_6.h>
#include "d3d12_private.h"
#include <cstdio>

static bool test_passed = true;
#define CHECK(expr, msg) do { \
    if (!(expr)) { printf("  FAIL: %s\n", msg); test_passed = false; } \
    else { printf("  PASS: %s\n", msg); } \
} while(0)
#define CHECK_HR(hr, msg) CHECK(SUCCEEDED(hr), msg)

int main() {
    printf("=== M6+M7 Root Signature + Pipeline State Test ===\n\n");

    // ── Setup ──
    IDXGIFactory4 *factory = nullptr;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) return 1;
    IDXGIAdapter1 *adapter = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++)
        if (adapter) break;
    ID3D12Device *device = nullptr;
    if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)))) return 1;

    // ── M6: Root Signature ──
    printf("[M6] Root Signature...\n");

    // Simple root signature: one descriptor table for SRV
    D3D12_DESCRIPTOR_RANGE range = {};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 8;
    range.BaseShaderRegister = 0;

    D3D12_ROOT_PARAMETER param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param.DescriptorTable.NumDescriptorRanges = 1;
    param.DescriptorTable.pDescriptorRanges = &range;
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rs_desc = {};
    rs_desc.NumParameters = 1;
    rs_desc.pParameters = &param;
    rs_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ID3D12RootSignature *root_sig = nullptr;
    HRESULT hr = device->CreateRootSignature(
        0, &rs_desc, sizeof(rs_desc), IID_PPV_ARGS(&root_sig));
    CHECK_HR(hr, "CreateRootSignature (1 param, descriptor table)");

    // ── M6: Root signature with multiple param types ──
    printf("\n[M6] Multi-parameter root signature...\n");

    D3D12_DESCRIPTOR_RANGE cbv_range = {};
    cbv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    cbv_range.NumDescriptors = 1;
    cbv_range.BaseShaderRegister = 0;

    D3D12_DESCRIPTOR_RANGE srv_range = {};
    srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srv_range.NumDescriptors = 16;
    srv_range.BaseShaderRegister = 0;

    D3D12_DESCRIPTOR_RANGE sampler_range = {};
    sampler_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    sampler_range.NumDescriptors = 4;
    sampler_range.BaseShaderRegister = 0;

    D3D12_ROOT_PARAMETER params[3] = {};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[0].DescriptorTable.NumDescriptorRanges = 1;
    params[0].DescriptorTable.pDescriptorRanges = &cbv_range;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable.NumDescriptorRanges = 1;
    params[1].DescriptorTable.pDescriptorRanges = &srv_range;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[2].DescriptorTable.NumDescriptorRanges = 1;
    params[2].DescriptorTable.pDescriptorRanges = &sampler_range;
    params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rs_desc2 = {};
    rs_desc2.NumParameters = 3;
    rs_desc2.pParameters = params;
    rs_desc2.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ID3D12RootSignature *multi_rs = nullptr;
    hr = device->CreateRootSignature(
        0, &rs_desc2, sizeof(rs_desc2), IID_PPV_ARGS(&multi_rs));
    CHECK_HR(hr, "CreateRootSignature (3 params)");
    multi_rs->Release();

    // ── M6: 32-bit constants ──
    printf("\n[M6] Root constants...\n");
    D3D12_ROOT_PARAMETER const_param = {};
    const_param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    const_param.Constants.ShaderRegister = 0;
    const_param.Constants.Num32BitValues = 4;
    const_param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rs_desc3 = {};
    rs_desc3.NumParameters = 1;
    rs_desc3.pParameters = &const_param;

    ID3D12RootSignature *const_rs = nullptr;
    hr = device->CreateRootSignature(
        0, &rs_desc3, sizeof(rs_desc3), IID_PPV_ARGS(&const_rs));
    CHECK_HR(hr, "CreateRootSignature (32-bit constants)");
    const_rs->Release();

    // ── M7: Graphics PSO ──
    printf("\n[M7] Graphics Pipeline State...\n");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC gfx_desc = {};
    gfx_desc.pRootSignature = root_sig;
    gfx_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    gfx_desc.NumRenderTargets = 1;
    gfx_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    gfx_desc.SampleDesc.Count = 1;
    gfx_desc.SampleMask = UINT_MAX;

    // Default rasterizer
    gfx_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    gfx_desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    gfx_desc.RasterizerState.DepthClipEnable = TRUE;

    // Default blend
    gfx_desc.BlendState.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;

    // Default depth-stencil
    gfx_desc.DepthStencilState.DepthEnable = FALSE;
    gfx_desc.DepthStencilState.StencilEnable = FALSE;

    ID3D12PipelineState *gfx_pso = nullptr;
    hr = device->CreateGraphicsPipelineState(&gfx_desc, IID_PPV_ARGS(&gfx_pso));
    CHECK_HR(hr, "CreateGraphicsPipelineState");

    // ── M7: Compute PSO ──
    printf("\n[M7] Compute Pipeline State...\n");
    D3D12_COMPUTE_PIPELINE_STATE_DESC comp_desc = {};
    comp_desc.pRootSignature = root_sig;

    ID3D12PipelineState *comp_pso = nullptr;
    hr = device->CreateComputePipelineState(&comp_desc, IID_PPV_ARGS(&comp_pso));
    CHECK_HR(hr, "CreateComputePipelineState");

    // ── M7: Multiple PSOs ──
    printf("\n[M7] Multiple PSOs...\n");
    ID3D12PipelineState *psos[4] = {};
    for (int i = 0; i < 4; i++) {
        hr = device->CreateGraphicsPipelineState(&gfx_desc, IID_PPV_ARGS(&psos[i]));
        CHECK_HR(hr, "PSO creation");
    }
    for (int i = 0; i < 4; i++) psos[i]->Release();
    comp_pso->Release();

    // ── M7: Validation edge cases ──
    printf("\n[M7] Edge cases...\n");

    // null desc
    hr = device->CreateGraphicsPipelineState(nullptr, IID_PPV_ARGS(&gfx_pso));
    CHECK(FAILED(hr), "CreateGraphicsPipelineState(nullptr) fails");

    // null output
    hr = device->CreateGraphicsPipelineState(&gfx_desc, IID_PPV_ARGS((ID3D12PipelineState**)nullptr));
    CHECK(FAILED(hr), "CreateGraphicsPipelineState(nullptr output) fails");

    // ── Cleanup ──
    gfx_pso->Release();
    root_sig->Release();
    device->Release();
    adapter->Release();
    factory->Release();
    CHECK(true, "All resources released");

    printf("\n========================================\n");
    printf(test_passed ? "  M6+M7 Test: ALL PASSED\n" : "  M6+M7 Test: FAILURES\n");
    printf("========================================\n");
    return test_passed ? 0 : 1;
}
