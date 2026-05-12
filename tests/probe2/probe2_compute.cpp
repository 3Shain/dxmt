#define INITGUID
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <cstdio>

int main() {
    HMODULE d3d12 = LoadLibraryA("d3d12.dll");
    if (!d3d12) {
        fprintf(stdout, "[SKIP] d3d12.dll not loadable (no AFMT runtime)\n");
        return 0;
    }
    fprintf(stdout, "[PASS] d3d12.dll loaded at %p\n", (void*)d3d12);

    typedef HRESULT (WINAPI *PFN_D3D12CreateDevice)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
    PFN_D3D12CreateDevice pCreate = (PFN_D3D12CreateDevice)GetProcAddress(d3d12, "D3D12CreateDevice");
    if (!pCreate) {
        fprintf(stdout, "[FAIL] D3D12CreateDevice not found\n");
        return 1;
    }
    fprintf(stdout, "[PASS] D3D12CreateDevice resolved\n");

    ID3D12Device *device = nullptr;
    HRESULT hr = pCreate(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if (FAILED(hr)) {
        fprintf(stdout, "[FAIL] D3D12CreateDevice -> 0x%08lx\n", hr);
        return 1;
    }
    fprintf(stdout, "[PASS] Device created: %p\n", (void*)device);

    static const IID IID_ID3D12Device2_ = {0x30baa41e, 0xb15b, 0x475c, {0xa0, 0xbb, 0x1a, 0xf5, 0xc5, 0xb6, 0x43, 0x28}};
    static const IID IID_ID3D12Device5_ = {0x8b4f173b, 0x2fea, 0x4b80, {0x8f, 0x58, 0x43, 0x07, 0x19, 0x1a, 0xb9, 0x5d}};
    static const IID IID_ID3D12Device10_ = {0x517f8718, 0xaa66, 0x49f9, {0xb0, 0x2b, 0xa7, 0xab, 0x89, 0xc0, 0x60, 0x31}};
    static const IID IID_ID3D12Device11_ = {0x5405c344, 0xd457, 0x444e, {0xb4, 0xdd, 0x23, 0x66, 0xe4, 0x5a, 0xee, 0x39}};
    static const IID IID_ID3D12Device12_ = {0x5af5c532, 0x4c91, 0x4cd0, {0xb5, 0x41, 0x15, 0xa4, 0x05, 0x39, 0x5f, 0xc5}};

    struct { const char *name; IID iid; } tests[] = {
        {"ID3D12Device", IID_ID3D12Device},
        {"ID3D12Device1", IID_ID3D12Device1},
        {"ID3D12Device2", IID_ID3D12Device2_},
        {"ID3D12Device5", IID_ID3D12Device5_},
        {"ID3D12Device10", IID_ID3D12Device10_},
        {"ID3D12Device11", IID_ID3D12Device11_},
        {"ID3D12Device12", IID_ID3D12Device12_},
    };

    int pass = 0, fail = 0;
    for (int i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
        void *ptr = nullptr;
        hr = device->QueryInterface(tests[i].iid, &ptr);
        if (SUCCEEDED(hr)) {
            fprintf(stdout, "[PASS] QI(%s)\n", tests[i].name);
            ((IUnknown*)ptr)->Release();
            pass++;
        } else {
            fprintf(stdout, "[FAIL] QI(%s) -> 0x%08lx\n", tests[i].name, hr);
            fail++;
        }
    }

    D3D12_COMMAND_QUEUE_DESC qd = {};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ID3D12CommandQueue *queue = nullptr;
    hr = device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));
    fprintf(stdout, "[%s] CreateCommandQueue\n", SUCCEEDED(hr) ? "PASS" : "FAIL");
    if (SUCCEEDED(hr)) { pass++; queue->Release(); } else fail++;

    ID3D12CommandAllocator *alloc = nullptr;
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
    fprintf(stdout, "[%s] CreateCommandAllocator\n", SUCCEEDED(hr) ? "PASS" : "FAIL");
    if (SUCCEEDED(hr)) { pass++; alloc->Release(); } else fail++;

    ID3D12GraphicsCommandList *cl = nullptr;
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr, IID_PPV_ARGS(&cl));
    fprintf(stdout, "[%s] CreateCommandList\n", SUCCEEDED(hr) ? "PASS" : "FAIL");
    if (SUCCEEDED(hr)) { pass++; cl->Release(); } else fail++;

    D3D12_ROOT_SIGNATURE_DESC rsd = {};
    rsd.NumParameters = 0;
    rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    ID3DBlob *blob = nullptr;
    hr = D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr);
    fprintf(stdout, "[%s] SerializeRootSignature\n", SUCCEEDED(hr) ? "PASS" : "FAIL");
    if (SUCCEEDED(hr)) { pass++; } else fail++;

    ID3D12RootSignature *rs = nullptr;
    if (blob) {
        hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rs));
        fprintf(stdout, "[%s] CreateRootSignature\n", SUCCEEDED(hr) ? "PASS" : "FAIL");
        if (SUCCEEDED(hr)) { pass++; rs->Release(); } else fail++;
        blob->Release();
    }

    D3D12_HEAP_PROPERTIES hp = {};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    hp.CreationNodeMask = 1;
    hp.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC bd = {};
    bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bd.Width = 4096;
    bd.Height = 1;
    bd.DepthOrArraySize = 1;
    bd.MipLevels = 1;
    bd.SampleDesc.Count = 1;
    bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource *res = nullptr;
    hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&res));
    fprintf(stdout, "[%s] CreateCommittedResource (buffer)\n", SUCCEEDED(hr) ? "PASS" : "FAIL");
    if (SUCCEEDED(hr)) { pass++; res->Release(); } else fail++;

    ID3D12Fence *fence = nullptr;
    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    fprintf(stdout, "[%s] CreateFence\n", SUCCEEDED(hr) ? "PASS" : "FAIL");
    if (SUCCEEDED(hr)) { pass++; fence->Release(); } else fail++;

    device->Release();
    FreeLibrary(d3d12);

    fprintf(stdout, "\n=== PROBE 2: %d pass, %d fail ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
