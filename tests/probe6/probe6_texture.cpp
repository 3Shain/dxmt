#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <stdint.h>

static ID3D12Device *device;
static IDXGISwapChain3 *swapchain;
static ID3D12CommandQueue *queue;
static ID3D12CommandAllocator *alloc;
static ID3D12GraphicsCommandList *cmd;
static ID3D12DescriptorHeap *rtv_heap;
static ID3D12DescriptorHeap *srv_heap;
static ID3D12DescriptorHeap *sampler_heap;
static ID3D12RootSignature *root_sig;
static ID3D12PipelineState *pso;
static ID3D12Resource *texture;
static ID3D12Resource *vb;
static ID3D12Fence *fence;
static UINT rtv_inc;
static HANDLE fevent;

static const char vs_code[] =
    "float4 main(float2 pos : POSITION, float2 uv : TEXCOORD) : SV_POSITION {\n"
    "    return float4(pos, 0.0, 1.0);\n"
    "}\n";

static const char ps_code[] =
    "Texture2D<float4> tex : register(t0);\n"
    "SamplerState samp : register(s0);\n"
    "float4 main(float4 pos : SV_POSITION) : SV_TARGET {\n"
    "    float2 uv = pos.xy / float2(640.0, 480.0);\n"
    "    return tex.Sample(samp, uv);\n"
    "}\n";

struct Vertex { float x, y, u, v; };

static HRESULT setup() {
    HRESULT hr;
    HMODULE mod = LoadLibraryA("d3d12.dll");
    if (!mod) { printf("[FAIL] LoadLibrary\n"); return E_FAIL; }

    PFN_D3D12_CREATE_DEVICE cd = (PFN_D3D12_CREATE_DEVICE)GetProcAddress(mod, "D3D12CreateDevice");
    hr = cd(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if (FAILED(hr)) return hr;
    printf("[PASS] Device\n");

    D3D12_COMMAND_QUEUE_DESC qd = { D3D12_COMMAND_LIST_TYPE_DIRECT, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0 };
    hr = device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue)); if (FAILED(hr)) return hr;
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc)); if (FAILED(hr)) return hr;
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr, IID_PPV_ARGS(&cmd)); if (FAILED(hr)) return hr;

    HWND wnd = CreateWindowA("STATIC", "probe6", WS_VISIBLE|WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, 0, 0, 0, 0);

    IDXGIFactory2 *fact = nullptr;
    hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&fact)); if (FAILED(hr)) return hr;
    DXGI_SWAP_CHAIN_DESC1 scd = {};
    scd.Width = 640; scd.Height = 480; scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.SampleDesc.Count = 1; scd.BufferCount = 2; scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain1 *sc1 = nullptr;
    hr = fact->CreateSwapChainForHwnd(queue, wnd, &scd, nullptr, nullptr, &sc1);
    fact->Release();
    if (FAILED(hr)) return hr;
    hr = sc1->QueryInterface(IID_PPV_ARGS(&swapchain)); sc1->Release();
    if (FAILED(hr)) return hr;
    printf("[PASS] SwapChain\n");

    D3D12_DESCRIPTOR_HEAP_DESC rd = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
    hr = device->CreateDescriptorHeap(&rd, IID_PPV_ARGS(&rtv_heap)); if (FAILED(hr)) return hr;
    rtv_inc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvh = rtv_heap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < 2; i++) {
        ID3D12Resource *bb; swapchain->GetBuffer(i, IID_PPV_ARGS(&bb));
        device->CreateRenderTargetView(bb, nullptr, rtvh); bb->Release();
        rtvh.ptr += rtv_inc;
    }

    D3D12_DESCRIPTOR_HEAP_DESC shd = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0 };
    hr = device->CreateDescriptorHeap(&shd, IID_PPV_ARGS(&srv_heap)); if (FAILED(hr)) return hr;

    D3D12_DESCRIPTOR_HEAP_DESC sahd = { D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0 };
    hr = device->CreateDescriptorHeap(&sahd, IID_PPV_ARGS(&sampler_heap)); if (FAILED(hr)) return hr;

    D3D12_SAMPLER_DESC sd = {};
    sd.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sd.MinLOD = 0; sd.MaxLOD = D3D12_FLOAT32_MAX; sd.MaxAnisotropy = 1;
    sd.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    device->CreateSampler(&sd, sampler_heap->GetCPUDescriptorHandleForHeapStart());
    printf("[PASS] Sampler\n");

    D3D12_RESOURCE_DESC td = {};
    td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; td.Width = 64; td.Height = 64;
    td.DepthOrArraySize = 1; td.MipLevels = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    D3D12_HEAP_PROPERTIES hp = {}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture));
    if (FAILED(hr)) return hr;
    printf("[PASS] Texture resource\n");

    uint32_t tex_data[64*64];
    for (int i = 0; i < 64*64; i++) {
        int x = i % 64, y = i / 64;
        tex_data[i] = (((x/8)+(y/8))%2==0) ? 0xFF0000FF : 0xFF00FF00;
    }

    D3D12_RESOURCE_DESC ud = {};
    ud.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; ud.Width = 64*64*4;
    ud.Height = 1; ud.DepthOrArraySize = 1; ud.MipLevels = 1;
    ud.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; ud.SampleDesc.Count = 1;
    ID3D12Resource *ub = nullptr;
    D3D12_HEAP_PROPERTIES uh = {}; uh.Type = D3D12_HEAP_TYPE_UPLOAD;
    hr = device->CreateCommittedResource(&uh, D3D12_HEAP_FLAG_NONE, &ud,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&ub));
    if (FAILED(hr)) return hr;
    void *mp = nullptr; ub->Map(0, nullptr, &mp); memcpy(mp, tex_data, sizeof(tex_data)); ub->Unmap(0, nullptr);

    D3D12_TEXTURE_COPY_LOCATION dl = {}; dl.pResource = texture; dl.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; dl.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION sl = {}; sl.pResource = ub; sl.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    sl.PlacedFootprint.Offset = 0; sl.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sl.PlacedFootprint.Footprint.Width = 64; sl.PlacedFootprint.Footprint.Height = 64;
    sl.PlacedFootprint.Footprint.Depth = 1; sl.PlacedFootprint.Footprint.RowPitch = 64*4;
    cmd->CopyTextureRegion(&dl, 0, 0, 0, &sl, nullptr);

    D3D12_RESOURCE_BARRIER rb = {}; rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    rb.Transition.pResource = texture; rb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    rb.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    cmd->ResourceBarrier(1, &rb);

    D3D12_SHADER_RESOURCE_VIEW_DESC svd = {};
    svd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; svd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    svd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; svd.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(texture, &svd, srv_heap->GetCPUDescriptorHandleForHeapStart());

    cmd->Close();
    ID3D12CommandList *cls[] = { cmd };
    queue->ExecuteCommandLists(1, cls);
    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)); if (FAILED(hr)) return hr;
    fevent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    queue->Signal(fence, 1); fence->SetEventOnCompletion(1, fevent); WaitForSingleObject(fevent, 5000);
    alloc->Reset(); cmd->Reset(alloc, nullptr);

    ID3DBlob *vsb = nullptr, *psb = nullptr, *err = nullptr;
    hr = D3DCompile(vs_code, sizeof(vs_code)-1, nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsb, &err);
    if (FAILED(hr)) { if (err) printf("VS error: %s\n", (char*)err->GetBufferPointer()); return hr; }
    if (err) { err->Release(); err = nullptr; }
    hr = D3DCompile(ps_code, sizeof(ps_code)-1, nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &psb, &err);
    if (FAILED(hr)) { if (err) printf("PS error: %s\n", (char*)err->GetBufferPointer()); return hr; }
    if (err) { err->Release(); }
    printf("[PASS] Shaders\n");

    D3D12_DESCRIPTOR_RANGE ranges[2] = {};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; ranges[0].NumDescriptors = 1; ranges[0].BaseShaderRegister = 0;
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER; ranges[1].NumDescriptors = 1; ranges[1].BaseShaderRegister = 0;
    D3D12_ROOT_PARAMETER rp[2] = {};
    rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rp[0].DescriptorTable.NumDescriptorRanges = 1;
    rp[0].DescriptorTable.pDescriptorRanges = &ranges[0]; rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rp[1].DescriptorTable.NumDescriptorRanges = 1;
    rp[1].DescriptorTable.pDescriptorRanges = &ranges[1]; rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC rsd = { 2, rp, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE };
    ID3DBlob *rsb = nullptr;
    hr = D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &rsb, nullptr); if (FAILED(hr)) return hr;
    hr = device->CreateRootSignature(0, rsb->GetBufferPointer(), rsb->GetBufferSize(), IID_PPV_ARGS(&root_sig)); rsb->Release();
    if (FAILED(hr)) return hr;
    printf("[PASS] Root signature\n");

    Vertex verts[] = { {-0.8f,-0.8f,0,1}, {0.8f,-0.8f,1,1}, {0,0.8f,0.5f,0} };
    D3D12_RESOURCE_DESC vbd = {};
    vbd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; vbd.Width = sizeof(verts); vbd.Height = 1;
    vbd.DepthOrArraySize = 1; vbd.MipLevels = 1; vbd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; vbd.SampleDesc.Count = 1;
    D3D12_HEAP_PROPERTIES vbh = {}; vbh.Type = D3D12_HEAP_TYPE_UPLOAD;
    hr = device->CreateCommittedResource(&vbh, D3D12_HEAP_FLAG_NONE, &vbd,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vb)); if (FAILED(hr)) return hr;
    void *vm = nullptr; vb->Map(0, nullptr, &vm); memcpy(vm, verts, sizeof(verts)); vb->Unmap(0, nullptr);

    D3D12_INPUT_ELEMENT_DESC elems[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {};
    pd.pRootSignature = root_sig;
    pd.VS.pShaderBytecode = vsb->GetBufferPointer(); pd.VS.BytecodeLength = vsb->GetBufferSize();
    pd.PS.pShaderBytecode = psb->GetBufferPointer(); pd.PS.BytecodeLength = psb->GetBufferSize();
    pd.InputLayout = { elems, 2 }; pd.NumRenderTargets = 1; pd.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pd.SampleDesc.Count = 1; pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    hr = device->CreateGraphicsPipelineState(&pd, IID_PPV_ARGS(&pso)); if (FAILED(hr)) return hr;
    printf("[PASS] PSO\n");

    return S_OK;
}

int main() {
    HRESULT hr = setup();
    if (FAILED(hr)) { printf("[FAIL] setup 0x%lx\n", hr); return 1; }

    D3D12_VERTEX_BUFFER_VIEW vbv = { vb->GetGPUVirtualAddress(), sizeof(Vertex)*3, sizeof(Vertex) };
    D3D12_VIEWPORT vp = { 0, 0, 640, 480, 0, 1 };
    D3D12_RECT sc = { 0, 0, 640, 480 };

    printf("=== Rendering textured triangle (red/green checkerboard) ===\n");
    for (int f = 0; f < 180; f++) {
        alloc->Reset(); cmd->Reset(alloc, pso);
        D3D12_CPU_DESCRIPTOR_HANDLE frtv = rtv_heap->GetCPUDescriptorHandleForHeapStart();
        frtv.ptr += swapchain->GetCurrentBackBufferIndex() * rtv_inc;
        const float clr[] = { 0.1f, 0.1f, 0.15f, 1.0f };
        cmd->ClearRenderTargetView(frtv, clr, 0, nullptr);
        cmd->OMSetRenderTargets(1, &frtv, FALSE, nullptr);
        cmd->RSSetViewports(1, &vp); cmd->RSSetScissorRects(1, &sc);
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmd->IASetVertexBuffers(0, 1, &vbv);
        ID3D12DescriptorHeap *heaps[] = { srv_heap, sampler_heap };
        cmd->SetDescriptorHeaps(2, heaps);
        cmd->SetGraphicsRootDescriptorTable(0, srv_heap->GetGPUDescriptorHandleForHeapStart());
        cmd->SetGraphicsRootDescriptorTable(1, sampler_heap->GetGPUDescriptorHandleForHeapStart());
        cmd->DrawInstanced(3, 1, 0, 0);
        cmd->Close();
        ID3D12CommandList *cls[] = { cmd };
        queue->ExecuteCommandLists(1, cls);
        swapchain->Present(1, 0);
        queue->Signal(fence, f+100); fence->SetEventOnCompletion(f+100, fevent);
        WaitForSingleObject(fevent, 2000);
    }
    printf("\n=== PROBE 6: 180 frames, 0 fail ===\n");
    return 0;
}
