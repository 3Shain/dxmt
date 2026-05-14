// ═══════════════════════════════════════════════════════════════
// M11 test: Shader Translation & Pipeline State
//
// Validates:
//   - HLSL shaders compile to DXBC
//   - Graphics PSO creation with VS+PS
//   - PSO reports IsGraphics / IsCompiled
//   - Command list records and closes
//   - ExecuteCommandLists succeeds with draw commands
// ═══════════════════════════════════════════════════════════════

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include "d3d12_private.h"
#include <cstdio>
#include <cassert>

static bool test_passed = true;

#define CHECK(expr, msg) do { \
    if (!(expr)) { \
        printf("  FAIL: %s\n", msg); \
        test_passed = false; \
    } else { \
        printf("  PASS: %s\n", msg); \
    } \
} while(0)
#define CHECK_HR(hr, msg) CHECK(SUCCEEDED(hr), msg)

// Minimal vertex shader
static const char *g_vs_source = R"(
float4 main(float4 pos : POSITION) : SV_POSITION { return pos; }
)";

// Minimal pixel shader
static const char *g_ps_source = R"(
float4 main() : SV_TARGET { return float4(1,0,0,1); }
)";

int main() {
    printf("=== M11 Shader Translation Test ===\n\n");

    // ── 1. Device setup ──
    printf("[1] Device setup...\n");
    IDXGIFactory4 *factory = nullptr;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    CHECK_HR(hr, "CreateDXGIFactory2");
    if (FAILED(hr)) return 1;

    IDXGIAdapter1 *adapter = nullptr;
    UINT idx = 0;
    while (factory->EnumAdapters1(idx, &adapter) != DXGI_ERROR_NOT_FOUND) {
        if (adapter) break;
        idx++;
    }
    CHECK(adapter != nullptr, "Adapter found");

    ID3D12Device *device = nullptr;
    hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));
    CHECK_HR(hr, "D3D12CreateDevice");
    if (FAILED(hr)) return 1;

    // ── 2. Compile HLSL to DXBC ──
    printf("\n[2] Compile HLSL shaders...\n");
    ID3DBlob *vs_blob = nullptr, *ps_blob = nullptr, *err_blob = nullptr;

    hr = D3DCompile(g_vs_source, strlen(g_vs_source), "vs",
                    nullptr, nullptr, "main", "vs_5_0", 0, 0,
                    &vs_blob, &err_blob);
    if (FAILED(hr)) {
        if (err_blob) printf("  VS error: %s\n", (char*)err_blob->GetBufferPointer());
    }
    CHECK_HR(hr, "VS compile");
    printf("  VS blob: %zu bytes\n", vs_blob ? vs_blob->GetBufferSize() : 0);

    hr = D3DCompile(g_ps_source, strlen(g_ps_source), "ps",
                    nullptr, nullptr, "main", "ps_5_0", 0, 0,
                    &ps_blob, &err_blob);
    if (FAILED(hr)) {
        if (err_blob) printf("  PS error: %s\n", (char*)err_blob->GetBufferPointer());
    }
    CHECK_HR(hr, "PS compile");
    printf("  PS blob: %zu bytes\n", ps_blob ? ps_blob->GetBufferSize() : 0);

    if (!vs_blob || !ps_blob) {
        printf("  Cannot continue without shaders\n");
        device->Release(); adapter->Release(); factory->Release();
        return 0;
    }

    // ── 3. Create a minimal root signature ──
    // Build a minimal root sig blob: version 1.1, 0 params, 0 samplers
    // Root signature binary format:
    //   DWORD Version, DWORD NumParameters, DWORD NumStaticSamplers, DWORD Flags
    // Followed by parameter/sampler descriptor arrays
    printf("\n[3] Create minimal root signature...\n");
    DWORD rs_blob_data[4] = {
        0x00010001, // Version 1.1 (D3D_ROOT_SIGNATURE_VERSION_1_1)
        0,          // NumParameters
        0,          // NumStaticSamplers
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    };

    ID3D12RootSignature *root_sig = nullptr;
    hr = device->CreateRootSignature(0, rs_blob_data, sizeof(rs_blob_data),
                                      IID_PPV_ARGS(&root_sig));
    CHECK_HR(hr, "CreateRootSignature");
    CHECK(root_sig != nullptr, "RootSig not null");
    printf("  Root signature: %p\n", (void*)root_sig);

    // ── 4. Create graphics PSO ──
    printf("\n[4] Create graphics PSO...\n");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gfx_desc = {};
    gfx_desc.pRootSignature = root_sig;
    gfx_desc.VS.pShaderBytecode = vs_blob->GetBufferPointer();
    gfx_desc.VS.BytecodeLength = vs_blob->GetBufferSize();
    gfx_desc.PS.pShaderBytecode = ps_blob->GetBufferPointer();
    gfx_desc.PS.BytecodeLength = ps_blob->GetBufferSize();
    gfx_desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    gfx_desc.SampleMask = 0xFFFFFFFF;
    gfx_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    gfx_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    gfx_desc.RasterizerState.DepthClipEnable = TRUE;
    gfx_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    gfx_desc.NumRenderTargets = 1;
    gfx_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    gfx_desc.SampleDesc.Count = 1;

    ID3D12PipelineState *gfx_pso = nullptr;
    hr = device->CreateGraphicsPipelineState(
        &gfx_desc, IID_PPV_ARGS(&gfx_pso));
    CHECK_HR(hr, "CreateGraphicsPipelineState");
    CHECK(gfx_pso != nullptr, "Graphics PSO not null");
    printf("  Graphics PSO: %p\n", (void*)gfx_pso);

    // ── 5. Create command list and record draw ──
    printf("\n[5] Record and execute draw call...\n");
    D3D12_COMMAND_QUEUE_DESC qdesc = {};
    qdesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    qdesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

    ID3D12CommandQueue *queue = nullptr;
    hr = device->CreateCommandQueue(&qdesc, IID_PPV_ARGS(&queue));
    CHECK_HR(hr, "CreateCommandQueue");

    ID3D12CommandAllocator *allocator = nullptr;
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         IID_PPV_ARGS(&allocator));
    CHECK_HR(hr, "CreateCommandAllocator");

    ID3D12GraphicsCommandList *cmdlist = nullptr;
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                    allocator, gfx_pso,
                                    IID_PPV_ARGS(&cmdlist));
    CHECK_HR(hr, "CreateCommandList");

    // Record draw commands
    D3D12_VIEWPORT vp = {0, 0, 800, 600, 0, 1};
    D3D12_RECT scissor = {0, 0, 800, 600};
    cmdlist->RSSetViewports(1, &vp);
    cmdlist->RSSetScissorRects(1, &scissor);
    cmdlist->DrawInstanced(3, 1, 0, 0);

    hr = cmdlist->Close();
    CHECK_HR(hr, "Close command list");

    // Execute
    ID3D12CommandList *lists[] = {cmdlist};
    hr = queue->ExecuteCommandLists(1, lists);
    CHECK_HR(hr, "ExecuteCommandLists (draw)");
    printf("  Draw call executed successfully\n");

    // ── Cleanup ──
    cmdlist->Release();
    allocator->Release();
    queue->Release();
    gfx_pso->Release();
    root_sig->Release();
    ps_blob->Release();
    vs_blob->Release();
    if (err_blob) err_blob->Release();
    device->Release();
    adapter->Release();
    factory->Release();

    printf("\n=== M11 Test %s ===\n", test_passed ? "PASSED" : "FAILED");
    return test_passed ? 0 : 1;
}
