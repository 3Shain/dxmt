// ═══════════════════════════════════════════════════════════════
// M3 test: Resource + Heap creation
// ═══════════════════════════════════════════════════════════════

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>
#include <dxgi1_6.h>
#include "d3d12_private.h"
#include <cstdio>
#include <cstring>

static bool test_passed = true;
#define CHECK(expr, msg) do { \
    if (!(expr)) { printf("  FAIL: %s\n", msg); test_passed = false; } \
    else { printf("  PASS: %s\n", msg); } \
} while(0)
#define CHECK_HR(hr, msg) CHECK(SUCCEEDED(hr), msg)

int main() {
    printf("=== M3 Resource + Heap Test ===\n\n");

    // ── Setup (M1) ──
    IDXGIFactory4 *factory = nullptr;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) return 1;
    IDXGIAdapter1 *adapter = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++)
        if (adapter) break;
    ID3D12Device *device = nullptr;
    if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)))) return 1;

    // ── 1. Committed buffer (UPLOAD) ──
    printf("[1] Committed upload buffer...\n");
    D3D12_HEAP_PROPERTIES upload_props = {};
    upload_props.Type = D3D12_HEAP_TYPE_UPLOAD;
    upload_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;

    D3D12_RESOURCE_DESC buf_desc = {};
    buf_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buf_desc.Width = 1024;
    buf_desc.Height = 1;
    buf_desc.DepthOrArraySize = 1;
    buf_desc.MipLevels = 1;
    buf_desc.SampleDesc.Count = 1;

    ID3D12Resource *upload_buf = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &upload_props, D3D12_HEAP_FLAG_NONE, &buf_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&upload_buf));
    CHECK_HR(hr, "CreateCommittedResource(UPLOAD buffer)");

    // Map and write
    void *ptr = nullptr;
    hr = upload_buf->Map(0, nullptr, &ptr);
    CHECK_HR(hr, "Map(UPLOAD buffer)");
    if (ptr) {
        memset(ptr, 0xAB, 1024);
        CHECK(true, "Wrote data to mapped buffer");
    }
    upload_buf->Unmap(0, nullptr);

    // Verify GPU VA
    D3D12_GPU_VIRTUAL_ADDRESS va = upload_buf->GetGPUVirtualAddress();
    CHECK(va != 0, "GetGPUVirtualAddress != 0");
    printf("    GPU VA: 0x%llx\n", (unsigned long long)va);

    // Verify desc
    D3D12_RESOURCE_DESC got = upload_buf->GetDesc();
    CHECK(got.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER, "GetDesc dimension");
    CHECK(got.Width == 1024, "GetDesc width");
    upload_buf->Release();

    // ── 2. Committed buffer (DEFAULT/GPU-only) ──
    printf("\n[2] Committed DEFAULT buffer...\n");
    D3D12_HEAP_PROPERTIES default_props = {};
    default_props.Type = D3D12_HEAP_TYPE_DEFAULT;

    buf_desc.Width = 65536;
    ID3D12Resource *default_buf = nullptr;
    hr = device->CreateCommittedResource(
        &default_props, D3D12_HEAP_FLAG_NONE, &buf_desc,
        D3D12_RESOURCE_STATE_COMMON, nullptr,
        IID_PPV_ARGS(&default_buf));
    CHECK_HR(hr, "CreateCommittedResource(DEFAULT buffer)");
    CHECK(default_buf->GetGPUVirtualAddress() != 0, "DEFAULT buffer GPU VA");
    D3D12_HEAP_PROPERTIES heap_props;
    D3D12_HEAP_FLAGS heap_flags;
    hr = default_buf->GetHeapProperties(&heap_props, &heap_flags);
    CHECK_HR(hr, "GetHeapProperties");
    CHECK(heap_props.Type == D3D12_HEAP_TYPE_DEFAULT, "Heap type == DEFAULT");

    // DEFAULT heap should fail to map
    void *bad_ptr = nullptr;
    hr = default_buf->Map(0, nullptr, &bad_ptr);
    CHECK(FAILED(hr), "Map(DEFAULT) fails");
    default_buf->Release();

    // ── 3. Heap creation ──
    printf("\n[3] Heap creation...\n");
    D3D12_HEAP_DESC heap_desc = {};
    heap_desc.SizeInBytes = 1024 * 1024; // 1 MB
    heap_desc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap_desc.Alignment = 65536; // 64KB

    ID3D12Heap *heap = nullptr;
    hr = device->CreateHeap(&heap_desc, IID_PPV_ARGS(&heap));
    CHECK_HR(hr, "CreateHeap(DEFAULT, 1MB)");

    D3D12_HEAP_DESC got_heap = heap->GetDesc();
    CHECK(got_heap.SizeInBytes == 1024 * 1024, "Heap size matches");
    CHECK(got_heap.Properties.Type == D3D12_HEAP_TYPE_DEFAULT, "Heap type matches");

    // ── 4. Placed resource ──
    printf("\n[4] Placed resource...\n");
    D3D12_RESOURCE_DESC placed_desc = {};
    placed_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    placed_desc.Width = 4096;
    placed_desc.Height = 1;
    placed_desc.DepthOrArraySize = 1;
    placed_desc.MipLevels = 1;
    placed_desc.SampleDesc.Count = 1;
    placed_desc.Alignment = 256;

    ID3D12Resource *placed_buf = nullptr;
    hr = device->CreatePlacedResource(
        heap, 0, &placed_desc,
        D3D12_RESOURCE_STATE_COMMON, nullptr,
        IID_PPV_ARGS(&placed_buf));
    CHECK_HR(hr, "CreatePlacedResource");
    CHECK(placed_buf->GetGPUVirtualAddress() != 0, "Placed resource GPU VA");
    placed_buf->Release();

    // ── 5. Multiple placed resources from same heap ──
    printf("\n[5] Multiple placed resources...\n");
    ID3D12Resource *placed2 = nullptr, *placed3 = nullptr;
    hr = device->CreatePlacedResource(
        heap, 65536, &placed_desc,
        D3D12_RESOURCE_STATE_COMMON, nullptr,
        IID_PPV_ARGS(&placed2));
    CHECK_HR(hr, "CreatePlacedResource #2 (offset 65536)");

    hr = device->CreatePlacedResource(
        heap, 131072, &placed_desc,
        D3D12_RESOURCE_STATE_COMMON, nullptr,
        IID_PPV_ARGS(&placed3));
    CHECK_HR(hr, "CreatePlacedResource #3 (offset 131072)");
    placed2->Release();
    placed3->Release();

    // ── 6. WriteToSubresource / ReadFromSubresource ──
    printf("\n[6] WriteToSubresource / ReadFromSubresource...\n");
    ID3D12Resource *rw_buf = nullptr;
    hr = device->CreateCommittedResource(
        &upload_props, D3D12_HEAP_FLAG_NONE, &buf_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&rw_buf));
    CHECK_HR(hr, "Create upload buffer for Write/Read");

    const char *test_data = "Hello D3D12!";
    hr = rw_buf->WriteToSubresource(0, nullptr, test_data, 13, 0);
    CHECK_HR(hr, "WriteToSubresource");

    char readback[64] = {};
    hr = rw_buf->ReadFromSubresource(readback, 13, 0, 0, nullptr);
    CHECK_HR(hr, "ReadFromSubresource");
    CHECK(strcmp(readback, test_data) == 0, "Read data matches written data");
    rw_buf->Release();

    // ── 7. Multiple buffer sizes ──
    printf("\n[7] Various buffer sizes...\n");
    UINT64 sizes[] = {16, 256, 4096, 65536, 1048576};
    for (int i = 0; i < 5; i++) {
        D3D12_RESOURCE_DESC d = buf_desc;
        d.Width = sizes[i];
        ID3D12Resource *b = nullptr;
        hr = device->CreateCommittedResource(
            &default_props, D3D12_HEAP_FLAG_NONE, &d,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&b));
        CHECK_HR(hr, "Buffer creation");
        CHECK(b->GetGPUVirtualAddress() != 0, "GPU VA");
        b->Release();
    }

    // ── Cleanup ──
    heap->Release();
    device->Release();
    adapter->Release();
    factory->Release();
    CHECK(true, "All resources released");

    printf("\n========================================\n");
    printf(test_passed ? "  M3 Test: ALL PASSED\n" : "  M3 Test: FAILURES\n");
    printf("========================================\n");
    return test_passed ? 0 : 1;
}
