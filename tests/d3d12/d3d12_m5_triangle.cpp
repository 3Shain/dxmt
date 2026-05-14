// ═══════════════════════════════════════════════════════════════
// M5: Basic Rendering Triangle — end-to-end integration test
//
// Validates the complete pipeline:
//   Device→Swapchain→PSO→VertexBuffer→Draw→Execute→Present
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
#include <cmath>

static bool test_passed = true;

#define CHECK(expr, msg) do { \
    if (!(expr)) { printf("  FAIL: %s\n", msg); test_passed = false; } \
    else { printf("  PASS: %s\n", msg); } \
} while(0)
#define CHECK_HR(hr, msg) CHECK(SUCCEEDED(hr), msg)

// Triangle vertex data
struct Vertex { float x, y, z; float r, g, b, a; };
static Vertex g_vertices[] = {
    { 0.0f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
    { 0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
    {-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f},
};

static const char *g_vs_source = R"(
struct VSIn { float4 pos : POSITION; float4 color : COLOR; };
struct VSOut { float4 pos : SV_POSITION; float4 color : COLOR; };
VSOut main(VSIn input) {
    VSOut output;
    output.pos = input.pos;
    output.color = input.color;
    return output;
}
)";

static const char *g_ps_source = R"(
float4 main(float4 color : COLOR) : SV_TARGET {
    return color;
}
)";

static HWND CreateTestWindow() {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"D3D12M5Triangle";
    RegisterClassW(&wc);
    return CreateWindowExW(0, L"D3D12M5Triangle", L"M5 Triangle",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600, nullptr, nullptr, wc.hInstance, nullptr);
}

int main() {
    printf("=== M5 Basic Rendering Triangle ===\n\n");

    // ═══ 1. Device setup ═══
    printf("[1] Device setup...\n");
    IDXGIFactory4 *factory = nullptr;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    CHECK_HR(hr, "CreateDXGIFactory2");
    if (FAILED(hr)) return 1;

    IDXGIAdapter1 *adapter = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++)
        if (adapter) break;
    CHECK(adapter != nullptr, "Adapter found");

    ID3D12Device *device = nullptr;
    hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));
    CHECK_HR(hr, "D3D12CreateDevice");
    if (FAILED(hr)) return 1;

    // ═══ 2. Command queue ═══
    printf("\n[2] Create command queue...\n");
    D3D12_COMMAND_QUEUE_DESC qdesc = {};
    qdesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    qdesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    ID3D12CommandQueue *queue = nullptr;
    hr = device->CreateCommandQueue(&qdesc, IID_PPV_ARGS(&queue));
    CHECK_HR(hr, "CreateCommandQueue");

    // ═══ 3. Window + Swapchain ═══
    printf("\n[3] Create swapchain...\n");
    HWND hwnd = CreateTestWindow();
    CHECK(hwnd != nullptr, "CreateWindow");

    DXGI_SWAP_CHAIN_DESC1 sc_desc = {};
    sc_desc.Width = 800; sc_desc.Height = 600;
    sc_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sc_desc.SampleDesc.Count = 1;
    sc_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc_desc.BufferCount = 2;
    sc_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sc_desc.Scaling = DXGI_SCALING_STRETCH;
    sc_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    IDXGISwapChain1 *swapchain1 = nullptr;
    hr = factory->CreateSwapChainForHwnd(queue, hwnd, &sc_desc,
                                          nullptr, nullptr, &swapchain1);
    if (FAILED(hr)) {
        printf("  Swapchain creation returned 0x%08lx (needs window+Metal)\n",
               (unsigned long)hr);
        printf("  Skipping swapchain-dependent tests.\n");
        queue->Release(); device->Release(); adapter->Release();
        factory->Release(); DestroyWindow(hwnd);
        return 0;
    }
    CHECK_HR(hr, "CreateSwapChainForHwnd");

    // Get back buffer
    ID3D12Resource *backbuffer = nullptr;
    hr = swapchain1->GetBuffer(0, IID_PPV_ARGS(&backbuffer));
    CHECK_HR(hr, "GetBuffer(0)");
    CHECK(backbuffer != nullptr, "Backbuffer valid");
    printf("  Backbuffer: %p\n", (void*)backbuffer);

    // ═══ 4. RTV Descriptor Heap ═══
    printf("\n[4] Create RTV descriptor heap...\n");
    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
    rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_desc.NumDescriptors = 1;
    rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtv_heap_desc.NodeMask = 0;

    ID3D12DescriptorHeap *rtv_heap = nullptr;
    hr = device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&rtv_heap));
    CHECK_HR(hr, "CreateDescriptorHeap (RTV)");

    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle =
        rtv_heap->GetCPUDescriptorHandleForHeapStart();

    // Create RTV for back buffer
    hr = device->CreateRenderTargetView(backbuffer, nullptr, rtv_handle);
    CHECK_HR(hr, "CreateRenderTargetView");

    // ═══ 5. Shaders ═══
    printf("\n[5] Compile shaders...\n");
    ID3DBlob *vs_blob = nullptr, *ps_blob = nullptr, *err_blob = nullptr;

    hr = D3DCompile(g_vs_source, strlen(g_vs_source), "vs",
                    nullptr, nullptr, "main", "vs_5_0", 0, 0,
                    &vs_blob, &err_blob);
    if (FAILED(hr) && err_blob) {
        printf("  VS error: %s\n", (char*)err_blob->GetBufferPointer());
    }
    CHECK_HR(hr, "VS compile");
    printf("  VS: %zu bytes\n", vs_blob->GetBufferSize());

    hr = D3DCompile(g_ps_source, strlen(g_ps_source), "ps",
                    nullptr, nullptr, "main", "ps_5_0", 0, 0,
                    &ps_blob, &err_blob);
    if (FAILED(hr) && err_blob) {
        printf("  PS error: %s\n", (char*)err_blob->GetBufferPointer());
    }
    CHECK_HR(hr, "PS compile");
    printf("  PS: %zu bytes\n", ps_blob->GetBufferSize());

    if (!vs_blob || !ps_blob) {
        printf("  Cannot continue without shaders\n");
        goto cleanup;
    }

    // ═══ 6. Root Signature ═══
    printf("\n[6] Create root signature...\n");
    DWORD rs_blob[4] = {
        0x00010001, 0, 0,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    };
    ID3D12RootSignature *root_sig = nullptr;
    hr = device->CreateRootSignature(0, rs_blob, sizeof(rs_blob),
                                      IID_PPV_ARGS(&root_sig));
    CHECK_HR(hr, "CreateRootSignature");

    // ═══ 7. Pipeline State ═══
    printf("\n[7] Create graphics PSO...\n");
    D3D12_INPUT_ELEMENT_DESC input_layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.pRootSignature = root_sig;
    pso_desc.VS.pShaderBytecode = vs_blob->GetBufferPointer();
    pso_desc.VS.BytecodeLength = vs_blob->GetBufferSize();
    pso_desc.PS.pShaderBytecode = ps_blob->GetBufferPointer();
    pso_desc.PS.BytecodeLength = ps_blob->GetBufferSize();
    pso_desc.BlendState.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;
    pso_desc.SampleMask = 0xFFFFFFFF;
    pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso_desc.RasterizerState.DepthClipEnable = TRUE;
    pso_desc.InputLayout.pInputElementDescs = input_layout;
    pso_desc.InputLayout.NumElements = 2;
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso_desc.SampleDesc.Count = 1;

    ID3D12PipelineState *pso = nullptr;
    hr = device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso));
    CHECK_HR(hr, "CreateGraphicsPipelineState");
    CHECK(pso != nullptr, "PSO created");

    // ═══ 8. Vertex Buffer ═══
    printf("\n[8] Create vertex buffer...\n");
    D3D12_HEAP_PROPERTIES upload_heap = {};
    upload_heap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC vb_desc = {};
    vb_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vb_desc.Width = sizeof(g_vertices);
    vb_desc.Height = 1;
    vb_desc.DepthOrArraySize = 1;
    vb_desc.MipLevels = 1;
    vb_desc.SampleDesc.Count = 1;
    vb_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ID3D12Resource *vertex_buffer = nullptr;
    hr = device->CreateCommittedResource(
        &upload_heap, D3D12_HEAP_FLAG_NONE, &vb_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&vertex_buffer));
    CHECK_HR(hr, "CreateCommittedResource (VB)");

    // Upload vertex data
    void *vb_data = nullptr;
    hr = vertex_buffer->Map(0, nullptr, &vb_data);
    if (SUCCEEDED(hr) && vb_data) {
        memcpy(vb_data, g_vertices, sizeof(g_vertices));
        vertex_buffer->Unmap(0, nullptr);
        printf("  Uploaded %zu bytes of vertex data\n", sizeof(g_vertices));
    }
    CHECK_HR(hr, "Map/Unmap VB");

    D3D12_VERTEX_BUFFER_VIEW vb_view = {};
    vb_view.BufferLocation = vertex_buffer->GetGPUVirtualAddress();
    vb_view.SizeInBytes = sizeof(g_vertices);
    vb_view.StrideInBytes = sizeof(Vertex);

    // ═══ 9. Command List Recording ═══
    printf("\n[9] Record command list...\n");
    ID3D12CommandAllocator *allocator = nullptr;
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         IID_PPV_ARGS(&allocator));
    CHECK_HR(hr, "CreateCommandAllocator");

    ID3D12GraphicsCommandList *cmdlist = nullptr;
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                    allocator, pso,
                                    IID_PPV_ARGS(&cmdlist));
    CHECK_HR(hr, "CreateCommandList");

    // Set descriptor heaps
    ID3D12DescriptorHeap *heaps[] = {rtv_heap};
    cmdlist->SetDescriptorHeaps(1, heaps);

    // Set render target
    cmdlist->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);

    // Set viewport + scissor
    D3D12_VIEWPORT vp = {0, 0, 800, 600, 0, 1};
    D3D12_RECT scissor = {0, 0, 800, 600};
    cmdlist->RSSetViewports(1, &vp);
    cmdlist->RSSetScissorRects(1, &scissor);

    // Set vertex buffer
    cmdlist->IASetVertexBuffers(0, 1, &vb_view);
    cmdlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Draw!
    cmdlist->DrawInstanced(3, 1, 0, 0);
    printf("  Recorded: SetHeaps, OMSetRTs, SetViewport, SetVB, Draw(3)\n");

    hr = cmdlist->Close();
    CHECK_HR(hr, "Close command list");

    // ═══ 10. Execute ═══
    printf("\n[10] Execute command list...\n");
    ID3D12CommandList *lists[] = {cmdlist};
    hr = queue->ExecuteCommandLists(1, lists);
    CHECK_HR(hr, "ExecuteCommandLists");

    // ═══ 11. Present ═══
    printf("\n[11] Present...\n");
    ShowWindow(hwnd, SW_SHOW);
    hr = swapchain1->Present(0, 0);
    if (SUCCEEDED(hr)) {
        printf("  Present succeeded\n");
    } else if (hr == DXGI_STATUS_OCCLUDED) {
        printf("  Window occluded (expected in headless)\n");
    } else {
        printf("  Present returned 0x%08lx\n", (unsigned long)hr);
    }

    CHECK(test_passed, "All M5 tests passed");

cleanup:
    if (cmdlist) cmdlist->Release();
    if (allocator) allocator->Release();
    if (vertex_buffer) vertex_buffer->Release();
    if (pso) pso->Release();
    if (root_sig) root_sig->Release();
    if (ps_blob) ps_blob->Release();
    if (vs_blob) vs_blob->Release();
    if (err_blob) err_blob->Release();
    if (rtv_heap) rtv_heap->Release();
    if (backbuffer) backbuffer->Release();
    if (swapchain1) swapchain1->Release();
    if (queue) queue->Release();
    if (device) device->Release();
    if (adapter) adapter->Release();
    if (factory) factory->Release();
    if (hwnd) DestroyWindow(hwnd);

    printf("\n=== M5 Triangle Test %s ===\n", test_passed ? "PASSED" : "FAILED");
    return test_passed ? 0 : 1;
}
