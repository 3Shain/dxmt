#define INITGUID
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstdint>
#include <cstring>

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

static const Vertex verts[] = {
    { -0.6f, -0.4f, 0.1f,   1.0f, 0.2f, 0.2f, 1.0f },
    {  0.0f, -0.4f, 0.1f,   1.0f, 0.2f, 0.2f, 1.0f },
    { -0.3f,  0.6f, 0.1f,   1.0f, 0.2f, 0.2f, 1.0f },

    { -0.1f, -0.6f, 0.9f,   0.2f, 1.0f, 0.2f, 1.0f },
    {  0.7f, -0.6f, 0.9f,   0.2f, 1.0f, 0.2f, 1.0f },
    {  0.3f,  0.4f,  0.9f,   0.2f, 1.0f, 0.2f, 1.0f },
};

static const char* vs_hlsl =
    "struct VS_IN {\n"
    "    float3 pos : POSITION;\n"
    "    float4 col : COLOR0;\n"
    "};\n"
    "struct VS_OUT {\n"
    "    float4 pos : SV_Position;\n"
    "    float4 col : COLOR0;\n"
    "};\n"
    "VS_OUT VSMain(VS_IN i) {\n"
    "    VS_OUT o;\n"
    "    o.pos = float4(i.pos, 1.0);\n"
    "    o.col = i.col;\n"
    "    return o;\n"
    "}\n";

static const char* ps_hlsl =
    "struct PS_IN {\n"
    "    float4 pos : SV_Position;\n"
    "    float4 col : COLOR0;\n"
    "};\n"
    "float4 PSMain(PS_IN i) : SV_Target {\n"
    "    return i.col;\n"
    "}\n";

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    if (msg == WM_KEYDOWN && wp == VK_ESCAPE) { PostQuitMessage(0); return 0; }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int main() {
    HMODULE d3d12 = LoadLibraryA("d3d12.dll");
    if (!d3d12) { fprintf(stderr, "[SKIP] no d3d12.dll\n"); return 0; }
    typedef HRESULT(WINAPI* PFN_D3D12CreateDevice)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
    auto pCreateDevice = (PFN_D3D12CreateDevice)GetProcAddress(d3d12, "D3D12CreateDevice");
    typedef HRESULT(WINAPI* PFN_CreateDXGIFactory2)(UINT, REFIID, void**);
    HMODULE dxgi = LoadLibraryA("dxgi.dll");
    auto pCreateFactory = (PFN_CreateDXGIFactory2)GetProcAddress(dxgi, "CreateDXGIFactory2");

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "Probe5";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowA("Probe5", "AFMT Probe 5 - Depth Test",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr, nullptr, wc.hInstance, nullptr);
    fprintf(stdout, "[PASS] Window\n");

    IDXGIFactory4* factory = nullptr;
    pCreateFactory(0, IID_PPV_ARGS(&factory));
    ID3D12Device* device = nullptr;
    pCreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    fprintf(stdout, "[PASS] Device\n");

    D3D12_COMMAND_QUEUE_DESC qd = {};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ID3D12CommandQueue* queue = nullptr;
    device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = 800;
    scd.BufferDesc.Height = 600;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    IDXGISwapChain* swapChain = nullptr;
    factory->CreateSwapChain(queue, &scd, &swapChain);
    factory->Release();

    D3D12_DESCRIPTOR_HEAP_DESC rtd = {};
    rtd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtd.NumDescriptors = 2;
    ID3D12DescriptorHeap* rtvHeap = nullptr;
    device->CreateDescriptorHeap(&rtd, IID_PPV_ARGS(&rtvHeap));
    UINT rtvStride = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvH = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    ID3D12Resource* rt[2] = {};
    for (UINT i = 0; i < 2; i++) {
        swapChain->GetBuffer(i, IID_PPV_ARGS(&rt[i]));
        device->CreateRenderTargetView(rt[i], nullptr, rtvH);
        rtvH.ptr += rtvStride;
    }

    D3D12_DESCRIPTOR_HEAP_DESC dsd = {};
    dsd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsd.NumDescriptors = 1;
    ID3D12DescriptorHeap* dsvHeap = nullptr;
    device->CreateDescriptorHeap(&dsd, IID_PPV_ARGS(&dsvHeap));
    D3D12_CPU_DESCRIPTOR_HANDLE dsvH = dsvHeap->GetCPUDescriptorHandleForHeapStart();

    D3D12_HEAP_PROPERTIES depthHeap = {};
    depthHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    depthHeap.CreationNodeMask = 1;
    depthHeap.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Width = 800;
    depthDesc.Height = 600;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearDepth = {};
    clearDepth.Format = DXGI_FORMAT_D32_FLOAT;
    clearDepth.DepthStencil.Depth = 1.0f;
    clearDepth.DepthStencil.Stencil = 0;

    ID3D12Resource* depthTex = nullptr;
    device->CreateCommittedResource(&depthHeap, D3D12_HEAP_FLAG_NONE, &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearDepth, IID_PPV_ARGS(&depthTex));
    device->CreateDepthStencilView(depthTex, nullptr, dsvH);
    fprintf(stdout, "[PASS] Depth buffer\n");

    ID3D12CommandAllocator* cmdAlloc = nullptr;
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
    ID3D12GraphicsCommandList* cmdList = nullptr;
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, nullptr, IID_PPV_ARGS(&cmdList));

    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeap.CreationNodeMask = 1;
    uploadHeap.VisibleNodeMask = 1;

    auto makeBuf = [&](SIZE_T sz) -> ID3D12Resource* {
        D3D12_RESOURCE_DESC d = {};
        d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        d.Width = sz; d.Height = 1; d.DepthOrArraySize = 1; d.MipLevels = 1;
        d.SampleDesc.Count = 1; d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ID3D12Resource* r = nullptr;
        device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &d,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&r));
        return r;
    };

    ID3D12Resource* vb = makeBuf(sizeof(verts));
    void* vbd = nullptr;
    vb->Map(0, nullptr, &vbd);
    memcpy(vbd, verts, sizeof(verts));
    vb->Unmap(0, nullptr);

    D3D12_VERTEX_BUFFER_VIEW vbv = {};
    vbv.BufferLocation = vb->GetGPUVirtualAddress();
    vbv.StrideInBytes = sizeof(Vertex);
    vbv.SizeInBytes = sizeof(verts);

    ID3DBlob* vsBlob = nullptr, * psBlob = nullptr;
    D3DCompile(vs_hlsl, strlen(vs_hlsl), "vs.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(ps_hlsl, strlen(ps_hlsl), "ps.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &psBlob, nullptr);

    ID3DBlob* rsBlob = nullptr;
    D3D12_ROOT_SIGNATURE_DESC rsd = {};
    rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &rsBlob, nullptr);
    ID3D12RootSignature* rootSig = nullptr;
    device->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(&rootSig));
    rsBlob->Release();

    D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psod = {};
    psod.pRootSignature = rootSig;
    psod.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psod.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psod.InputLayout = { layout, 2 };
    psod.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psod.NumRenderTargets = 1;
    psod.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psod.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psod.SampleDesc.Count = 1;
    psod.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psod.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psod.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psod.DepthStencilState.DepthEnable = TRUE;
    psod.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psod.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psod.DepthStencilState.StencilEnable = FALSE;

    ID3D12PipelineState* pso = nullptr;
    HRESULT hr = device->CreateGraphicsPipelineState(&psod, IID_PPV_ARGS(&pso));
    vsBlob->Release(); psBlob->Release();
    if (FAILED(hr)) { fprintf(stderr, "[FAIL] PSO: 0x%08lx\n", hr); return 1; }
    fprintf(stdout, "[PASS] PSO (depth enabled)\n");

    ID3D12Fence* fence = nullptr;
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE fenceEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);

    fprintf(stdout, "\n=== 180 frames, RED=near(z=0.1) GREEN=far(z=0.9) ===\n");
    fprintf(stdout, "    If depth works: green OVERLAPS red (green is drawn second, but further)\n\n");

    int pass = 0, fail = 0;
    for (UINT frame = 0; frame < 180; frame++) {
        cmdAlloc->Reset();
        cmdList->Reset(cmdAlloc, pso);

        D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += (frame % 2) * rtvStride;

        D3D12_RESOURCE_BARRIER barriers[2] = {};
        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Transition.pResource = rt[frame % 2];
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        cmdList->ResourceBarrier(1, barriers);

        cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsvH);

        const float clearCol[] = { 0.02f, 0.02f, 0.06f, 1.0f };
        cmdList->ClearRenderTargetView(rtv, clearCol, 0, nullptr);
        cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        D3D12_VIEWPORT vp = { 0.0f, 0.0f, 800.0f, 600.0f, 0.0f, 1.0f };
        cmdList->RSSetViewports(1, &vp);
        D3D12_RECT scissor = { 0, 0, 800, 600 };
        cmdList->RSSetScissorRects(1, &scissor);

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->IASetVertexBuffers(0, 1, &vbv);
        cmdList->DrawInstanced(6, 1, 0, 0);

        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        cmdList->ResourceBarrier(1, barriers);

        cmdList->Close();
        ID3D12CommandList* lists[] = { cmdList };
        queue->ExecuteCommandLists(1, lists);
        swapChain->Present(1, 0);

        queue->Signal(fence, frame + 1);
        if (fence->GetCompletedValue() < frame + 1) {
            fence->SetEventOnCompletion(frame + 1, fenceEvent);
            WaitForSingleObject(fenceEvent, 5000);
        }

        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (msg.message == WM_QUIT) break;
        pass++;
    }

    queue->Signal(fence, 0xFFFFFF);
    fence->SetEventOnCompletion(0xFFFFFF, fenceEvent);
    WaitForSingleObject(fenceEvent, 10000);

    CloseHandle(fenceEvent);
    fence->Release(); pso->Release(); rootSig->Release();
    cmdList->Release(); cmdAlloc->Release(); vb->Release();
    depthTex->Release(); dsvHeap->Release();
    rt[0]->Release(); rt[1]->Release(); rtvHeap->Release();
    swapChain->Release(); queue->Release(); device->Release();
    FreeLibrary(d3d12); FreeLibrary(dxgi); DestroyWindow(hwnd);

    fprintf(stdout, "\n=== PROBE 5: %d frames, %d fail ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
