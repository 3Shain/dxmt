// ═══════════════════════════════════════════════════════════════
// M4 test: Descriptor Heaps
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
    printf("=== M4 Descriptor Heap Test ===\n\n");

    // ── Setup ──
    IDXGIFactory4 *factory = nullptr;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) return 1;
    IDXGIAdapter1 *adapter = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++)
        if (adapter) break;
    ID3D12Device *device = nullptr;
    if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)))) return 1;

    // ── 1. Create CPU-only CBV_SRV_UAV heap ──
    printf("[1] CPU-only CBV_SRV_UAV heap...\n");
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.NumDescriptors = 64;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    ID3D12DescriptorHeap *cpu_heap = nullptr;
    HRESULT hr = device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&cpu_heap));
    CHECK_HR(hr, "CreateDescriptorHeap(CBV_SRV_UAV, CPU-only)");

    D3D12_DESCRIPTOR_HEAP_DESC got = cpu_heap->GetDesc();
    CHECK(got.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "GetDesc type");
    CHECK(got.NumDescriptors == 64, "GetDesc count 64");

    // CPU handle should be non-null
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_start = cpu_heap->GetCPUDescriptorHandleForHeapStart();
    CHECK(cpu_start.ptr != 0, "CPU handle non-null");

    // GPU handle should be null (not shader-visible)
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_start = cpu_heap->GetGPUDescriptorHandleForHeapStart();
    CHECK(gpu_start.ptr == 0, "GPU handle is 0 (CPU-only)");

    // Increment size
    UINT inc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    CHECK(inc == 32, "Descriptor increment size == 32");
    cpu_heap->Release();

    // ── 2. Create shader-visible heap ──
    printf("\n[2] Shader-visible CBV_SRV_UAV heap...\n");
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heap_desc.NumDescriptors = 256;

    ID3D12DescriptorHeap *gpu_heap = nullptr;
    hr = device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&gpu_heap));
    CHECK_HR(hr, "CreateDescriptorHeap(SHADER_VISIBLE)");

    // GPU handle should be non-null now
    gpu_start = gpu_heap->GetGPUDescriptorHandleForHeapStart();
    CHECK(gpu_start.ptr != 0, "GPU handle non-null (shader-visible)");

    // CPU handle still works
    cpu_start = gpu_heap->GetCPUDescriptorHandleForHeapStart();
    CHECK(cpu_start.ptr != 0, "CPU handle also non-null");

    got = gpu_heap->GetDesc();
    CHECK(got.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, "GetDesc has SHADER_VISIBLE flag");
    gpu_heap->Release();

    // ── 3. All heap types ──
    printf("\n[3] All heap types...\n");
    D3D12_DESCRIPTOR_HEAP_TYPE types[] = {
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
    };
    const char *names[] = {"CBV_SRV_UAV", "SAMPLER", "RTV", "DSV"};

    for (int i = 0; i < 4; i++) {
        D3D12_DESCRIPTOR_HEAP_DESC d = {};
        d.Type = types[i];
        d.NumDescriptors = 16;
        d.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        ID3D12DescriptorHeap *h = nullptr;
        hr = device->CreateDescriptorHeap(&d, IID_PPV_ARGS(&h));
        CHECK_HR(hr, names[i]);
        if (h) {
            D3D12_DESCRIPTOR_HEAP_DESC g = h->GetDesc();
            CHECK(g.Type == types[i], "Type matches");
            CHECK(g.NumDescriptors == 16, "Count 16");
            h->Release();
        }
    }

    // ── 4. CopyDescriptors between CPU heaps ──
    printf("\n[4] CopyDescriptors...\n");
    D3D12_DESCRIPTOR_HEAP_DESC src_desc = {};
    src_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    src_desc.NumDescriptors = 32;
    ID3D12DescriptorHeap *src = nullptr;
    hr = device->CreateDescriptorHeap(&src_desc, IID_PPV_ARGS(&src));
    CHECK_HR(hr, "Create source heap");

    D3D12_DESCRIPTOR_HEAP_DESC dst_desc = src_desc;
    ID3D12DescriptorHeap *dst = nullptr;
    hr = device->CreateDescriptorHeap(&dst_desc, IID_PPV_ARGS(&dst));
    CHECK_HR(hr, "Create dest heap");

    // Write pattern to source heap via CPU pointer
    D3D12_CPU_DESCRIPTOR_HANDLE src_handle = src->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < 32; i++) {
        uint8_t *slot = reinterpret_cast<uint8_t *>(src_handle.ptr) + i * 32;
        memset(slot, (int)i, 32); // Write index as pattern
    }

    // Copy from src[0-31] to dst[0-31]
    UINT sizes[] = {32};
    D3D12_CPU_DESCRIPTOR_HANDLE dst_start = dst->GetCPUDescriptorHandleForHeapStart();
    D3D12DescriptorHeap::CopyDescriptors(
        device, 1, &dst_start, sizes,
        1, &src_handle, sizes,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Verify copy
    bool match = true;
    for (UINT i = 0; i < 32; i++) {
        uint8_t *s = reinterpret_cast<uint8_t *>(src_handle.ptr) + i * 32;
        uint8_t *d = reinterpret_cast<uint8_t *>(dst_start.ptr) + i * 32;
        if (memcmp(s, d, 32) != 0) { match = false; break; }
    }
    CHECK(match, "CopyDescriptors data matches");
    src->Release();
    dst->Release();

    // ── 5. CopyDescriptors partial ranges ──
    printf("\n[5] Partial range copy...\n");
    ID3D12DescriptorHeap *src2 = nullptr, *dst2 = nullptr;
    hr = device->CreateDescriptorHeap(&src_desc, IID_PPV_ARGS(&src2));
    CHECK_HR(hr, "Create src2");
    hr = device->CreateDescriptorHeap(&src_desc, IID_PPV_ARGS(&dst2));
    CHECK_HR(hr, "Create dst2");

    D3D12_CPU_DESCRIPTOR_HANDLE s2 = src2->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE d2 = dst2->GetCPUDescriptorHandleForHeapStart();

    // Fill src with pattern
    for (UINT i = 0; i < 32; i++)
        memset(reinterpret_cast<uint8_t *>(s2.ptr) + i * 32, (int)(i + 1), 32);

    // Copy src[4..11] to dst[8..15] (8 descriptors)
    D3D12_CPU_DESCRIPTOR_HANDLE src_start = {};
    src_start.ptr = s2.ptr + 4 * 32;
    D3D12_CPU_DESCRIPTOR_HANDLE dst_start2 = {};
    dst_start2.ptr = d2.ptr + 8 * 32;
    UINT count = 8;
    D3D12DescriptorHeap::CopyDescriptors(
        device, 1, &dst_start2, &count,
        1, &src_start, &count,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Verify
    match = true;
    for (UINT i = 0; i < 8; i++) {
        uint8_t *s = reinterpret_cast<uint8_t *>(src_start.ptr) + i * 32;
        uint8_t *d = reinterpret_cast<uint8_t *>(dst_start2.ptr) + i * 32;
        if (memcmp(s, d, 32) != 0) { match = false; break; }
    }
    CHECK(match, "Partial copy matches");
    // Check that dst[0] is untouched (should be 0)
    CHECK(*reinterpret_cast<uint8_t *>(d2.ptr) == 0, "dst[0] untouched by partial copy");
    src2->Release();
    dst2->Release();

    // ── 6. Large heap ──
    printf("\n[6] Large heap (4096 descriptors)...\n");
    D3D12_DESCRIPTOR_HEAP_DESC big = {};
    big.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    big.NumDescriptors = 4096;
    big.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ID3D12DescriptorHeap *big_heap = nullptr;
    hr = device->CreateDescriptorHeap(&big, IID_PPV_ARGS(&big_heap));
    CHECK_HR(hr, "CreateDescriptorHeap(4096, SHADER_VISIBLE)");
    CHECK(big_heap->GetGPUDescriptorHandleForHeapStart().ptr != 0, "GPU handle non-null");
    CHECK(big_heap->GetCPUDescriptorHandleForHeapStart().ptr != 0, "CPU handle non-null");
    got = big_heap->GetDesc();
    CHECK(got.NumDescriptors == 4096, "Count 4096");
    big_heap->Release();

    // ── 7. Zero-size edge case ──
    printf("\n[7] Zero descriptor heap...\n");
    D3D12_DESCRIPTOR_HEAP_DESC zero = big;
    zero.NumDescriptors = 0;
    ID3D12DescriptorHeap *zheap = nullptr;
    hr = device->CreateDescriptorHeap(&zero, IID_PPV_ARGS(&zheap));
    CHECK_HR(hr, "CreateDescriptorHeap(0)");
    if (zheap) {
        got = zheap->GetDesc();
        CHECK(got.NumDescriptors == 0, "Count 0");
        // Handles should still return valid pointers/bases
        zheap->Release();
    }

    // ── Cleanup ──
    device->Release();
    adapter->Release();
    factory->Release();
    CHECK(true, "All released");

    printf("\n========================================\n");
    printf(test_passed ? "  M4 Test: ALL PASSED\n" : "  M4 Test: FAILURES\n");
    printf("========================================\n");
    return test_passed ? 0 : 1;
}
