#define INITGUID
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

static constexpr UINT kProbeWidth = 800;
static constexpr UINT kProbeHeight = 600;

static UINT alignUp(UINT value, UINT alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static bool readbackHasTrianglePixels(ID3D12Resource* readback, UINT rowPitch) {
    void* mapped = nullptr;
    HRESULT hr = readback->Map(0, nullptr, &mapped);
    if (FAILED(hr) || !mapped) {
        fprintf(stderr, "[FAIL] Pixel readback Map: 0x%08lx\n", hr);
        return false;
    }

    auto* bytes = static_cast<const uint8_t*>(mapped);
    uint64_t checksum = 1469598103934665603ull;
    uint32_t chromaPixels = 0;
    for (UINT y = 0; y < kProbeHeight; y += 2) {
        const uint8_t* row = bytes + y * rowPitch;
        for (UINT x = 0; x < kProbeWidth; x += 2) {
            const uint8_t* p = row + x * 4;
            uint8_t maxChannel = p[0] > p[1] ? p[0] : p[1];
            maxChannel = maxChannel > p[2] ? maxChannel : p[2];
            uint8_t minChannel = p[0] < p[1] ? p[0] : p[1];
            minChannel = minChannel < p[2] ? minChannel : p[2];
            checksum ^= p[0]; checksum *= 1099511628211ull;
            checksum ^= p[1]; checksum *= 1099511628211ull;
            checksum ^= p[2]; checksum *= 1099511628211ull;
            if (maxChannel > 80 && (maxChannel - minChannel) > 20) {
                chromaPixels++;
            }
        }
    }
    readback->Unmap(0, nullptr);

    fprintf(stdout, "[INFO] Pixel readback chroma_pixels=%u checksum=0x%016llx\n",
            chromaPixels, (unsigned long long)checksum);
    if (chromaPixels < 500) {
        fprintf(stderr, "[FAIL] Pixel readback did not find rendered triangle color\n");
        return false;
    }
    fprintf(stdout, "[PASS] Pixel readback confirmed rendered triangle color\n");
    return true;
}

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

static const Vertex triVerts[] = {
    { -0.5f, -0.5f, 0.0f,   1.0f, 0.2f, 0.2f, 1.0f },
    {  0.5f, -0.5f, 0.0f,   0.2f, 1.0f, 0.2f, 1.0f },
    {  0.0f,  0.5f, 0.0f,   0.2f, 0.4f, 1.0f, 1.0f },
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
    if (!pCreateDevice) { fprintf(stderr, "[FAIL] no D3D12CreateDevice\n"); return 1; }

    typedef HRESULT(WINAPI* PFN_CreateDXGIFactory2)(UINT, REFIID, void**);
    HMODULE dxgi = LoadLibraryA("dxgi.dll");
    auto pCreateFactory = (PFN_CreateDXGIFactory2)GetProcAddress(dxgi, "CreateDXGIFactory2");
    if (!pCreateFactory) { fprintf(stderr, "[FAIL] no CreateDXGIFactory2\n"); return 1; }

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "Probe3";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA("Probe3", "AFMT Probe 3 - Vertex Buffer Triangle",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, kProbeWidth, kProbeHeight,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) { fprintf(stderr, "[FAIL] CreateWindow\n"); return 1; }
    fprintf(stdout, "[PASS] Window created\n");

    IDXGIFactory4* factory = nullptr;
    HRESULT hr = pCreateFactory(0, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { fprintf(stderr, "[FAIL] CreateDXGIFactory2: 0x%08lx\n", hr); return 1; }

    ID3D12Device* device = nullptr;
    hr = pCreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if (FAILED(hr)) { fprintf(stderr, "[FAIL] CreateDevice: 0x%08lx\n", hr); return 1; }
    fprintf(stdout, "[PASS] Device\n");

    D3D12_COMMAND_QUEUE_DESC qd = {};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ID3D12CommandQueue* queue = nullptr;
    hr = device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));
    if (FAILED(hr)) { fprintf(stderr, "[FAIL] CommandQueue\n"); return 1; }

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = kProbeWidth;
    scd.BufferDesc.Height = kProbeHeight;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    IDXGISwapChain* swapChain = nullptr;
    hr = factory->CreateSwapChain(queue, &scd, &swapChain);
    if (FAILED(hr)) { fprintf(stderr, "[FAIL] SwapChain: 0x%08lx\n", hr); return 1; }
    factory->Release();

    D3D12_DESCRIPTOR_HEAP_DESC rtd = {};
    rtd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtd.NumDescriptors = 2;
    ID3D12DescriptorHeap* rtvHeap = nullptr;
    device->CreateDescriptorHeap(&rtd, IID_PPV_ARGS(&rtvHeap));
    UINT rtvStride = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    ID3D12Resource* renderTargets[2] = {};
    for (UINT i = 0; i < 2; i++) {
        swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
        device->CreateRenderTargetView(renderTargets[i], nullptr, rtvHandle);
        rtvHandle.ptr += rtvStride;
    }
    fprintf(stdout, "[PASS] Swapchain + RTs\n");

    ID3D12CommandAllocator* cmdAlloc = nullptr;
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
    ID3D12GraphicsCommandList* cmdList = nullptr;
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, nullptr, IID_PPV_ARGS(&cmdList));

    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeap.CreationNodeMask = 1;
    uploadHeap.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC vbDesc = {};
    vbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vbDesc.Width = sizeof(triVerts);
    vbDesc.Height = 1;
    vbDesc.DepthOrArraySize = 1;
    vbDesc.MipLevels = 1;
    vbDesc.SampleDesc.Count = 1;
    vbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ID3D12Resource* vb = nullptr;
    device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &vbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vb));
    void* vbData = nullptr;
    vb->Map(0, nullptr, &vbData);
    memcpy(vbData, triVerts, sizeof(triVerts));
    vb->Unmap(0, nullptr);

    D3D12_VERTEX_BUFFER_VIEW vbv = {};
    vbv.BufferLocation = vb->GetGPUVirtualAddress();
    vbv.StrideInBytes = sizeof(Vertex);
    vbv.SizeInBytes = sizeof(triVerts);
    fprintf(stdout, "[PASS] Vertex buffer (%zu bytes, stride=%zu)\n", sizeof(triVerts), sizeof(Vertex));

    ID3DBlob* vsBlob = nullptr, * psBlob = nullptr;
    ID3DBlob* vsErr = nullptr, * psErr = nullptr;
    D3DCompile(vs_hlsl, strlen(vs_hlsl), "vs.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vsBlob, &vsErr);
    D3DCompile(ps_hlsl, strlen(ps_hlsl), "ps.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &psBlob, &psErr);
    fprintf(stdout, "[PASS] Shaders (VS=%zu, PS=%zu)\n", vsBlob->GetBufferSize(), psBlob->GetBufferSize());

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
    psod.SampleDesc.Count = 1;
    psod.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psod.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psod.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psod.DepthStencilState.DepthEnable = FALSE;
    psod.DepthStencilState.StencilEnable = FALSE;

    ID3D12PipelineState* pso = nullptr;
    hr = device->CreateGraphicsPipelineState(&psod, IID_PPV_ARGS(&pso));
    vsBlob->Release();
    psBlob->Release();
    if (FAILED(hr)) { fprintf(stderr, "[FAIL] PSO: 0x%08lx\n", hr); return 1; }
    fprintf(stdout, "[PASS] Graphics PSO (with InputLayout)\n");

    ID3D12Fence* fence = nullptr;
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE fenceEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);

    const UINT readbackPitch = alignUp(kProbeWidth * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    const UINT readbackBytes = readbackPitch * kProbeHeight;
    D3D12_HEAP_PROPERTIES readbackHeap = {};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC readbackDesc = {};
    readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    readbackDesc.Width = readbackBytes;
    readbackDesc.Height = 1;
    readbackDesc.DepthOrArraySize = 1;
    readbackDesc.MipLevels = 1;
    readbackDesc.SampleDesc.Count = 1;
    readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource* readback = nullptr;
    hr = device->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &readbackDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback));
    if (FAILED(hr)) { fprintf(stderr, "[FAIL] Readback buffer: 0x%08lx\n", hr); return 1; }

    fprintf(stdout, "\n=== Rendering 180 frames ===\n\n");

    int pass = 0, fail = 0;
    for (UINT frame = 0; frame < 180; frame++) {
        cmdAlloc->Reset();
        cmdList->Reset(cmdAlloc, pso);

        D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += (frame % 2) * rtvStride;

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = renderTargets[frame % 2];
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        cmdList->ResourceBarrier(1, &barrier);

        cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

        const float clear[] = { 0.05f, 0.05f, 0.08f, 1.0f };
        cmdList->ClearRenderTargetView(rtv, clear, 0, nullptr);

        D3D12_VIEWPORT vp = { 0.0f, 0.0f, (float)kProbeWidth, (float)kProbeHeight, 0.0f, 1.0f };
        cmdList->RSSetViewports(1, &vp);
        D3D12_RECT scissor = { 0, 0, (LONG)kProbeWidth, (LONG)kProbeHeight };
        cmdList->RSSetScissorRects(1, &scissor);

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->IASetVertexBuffers(0, 1, &vbv);
        cmdList->DrawInstanced(3, 1, 0, 0);

        if (frame == 30) {
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
            cmdList->ResourceBarrier(1, &barrier);

            D3D12_TEXTURE_COPY_LOCATION dst = {};
            dst.pResource = readback;
            dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dst.PlacedFootprint.Offset = 0;
            dst.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            dst.PlacedFootprint.Footprint.Width = kProbeWidth;
            dst.PlacedFootprint.Footprint.Height = kProbeHeight;
            dst.PlacedFootprint.Footprint.Depth = 1;
            dst.PlacedFootprint.Footprint.RowPitch = readbackPitch;

            D3D12_TEXTURE_COPY_LOCATION src = {};
            src.pResource = renderTargets[frame % 2];
            src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            src.SubresourceIndex = 0;
            cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            cmdList->ResourceBarrier(1, &barrier);
        } else {
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        cmdList->ResourceBarrier(1, &barrier);
        }

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

    bool pixelPass = readbackHasTrianglePixels(readback, readbackPitch);

    queue->Signal(fence, 0xFFFFFF);
    fence->SetEventOnCompletion(0xFFFFFF, fenceEvent);
    WaitForSingleObject(fenceEvent, 10000);

    CloseHandle(fenceEvent);
    fence->Release();
    pso->Release();
    rootSig->Release();
    cmdList->Release();
    cmdAlloc->Release();
    readback->Release();
    vb->Release();
    renderTargets[0]->Release();
    renderTargets[1]->Release();
    rtvHeap->Release();
    swapChain->Release();
    queue->Release();
    device->Release();
    FreeLibrary(d3d12);
    FreeLibrary(dxgi);
    DestroyWindow(hwnd);

    fprintf(stdout, "\n=== PROBE 3: %d frames, %d fail ===\n", pass, fail);
    return fail > 0 || !pixelPass ? 1 : 0;
}
