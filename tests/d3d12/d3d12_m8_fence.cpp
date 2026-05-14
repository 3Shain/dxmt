// ═══════════════════════════════════════════════════════════════
// M8 test: Fences
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
    printf("=== M8 Fence Test ===\n\n");
    IDXGIFactory4 *factory = nullptr;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) return 1;
    IDXGIAdapter1 *adapter = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++)
        if (adapter) break;
    ID3D12Device *device = nullptr;
    if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)))) return 1;

    D3D12_COMMAND_QUEUE_DESC qdesc = {};
    qdesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ID3D12CommandQueue *queue = nullptr;
    device->CreateCommandQueue(&qdesc, IID_PPV_ARGS(&queue));

    // ── 1. Create fence ──
    printf("[1] Create fence...\n");
    ID3D12Fence *fence = nullptr;
    HRESULT hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    CHECK_HR(hr, "CreateFence(0)");
    CHECK(fence->GetCompletedValue() == 0, "Initial value == 0");

    // ── 2. CPU Signal ──
    printf("\n[2] CPU Signal...\n");
    hr = fence->Signal(5);
    CHECK_HR(hr, "Fence::Signal(5)");
    CHECK(fence->GetCompletedValue() >= 5, "Value >= 5 after Signal");

    // ── 3. CPU Signal higher ──
    printf("\n[3] CPU Signal higher...\n");
    hr = fence->Signal(10);
    CHECK_HR(hr, "Fence::Signal(10)");
    CHECK(fence->GetCompletedValue() >= 10, "Value >= 10");

    // ── 4. Queue Signal (GPU) ──
    printf("\n[4] Queue Signal...\n");
    hr = queue->Signal(fence, 15);
    CHECK_HR(hr, "Queue::Signal(15)");
    // Wait a bit for GPU to process
    Sleep(10);
    UINT64 val = fence->GetCompletedValue();
    printf("    Completed value: %llu\n", (unsigned long long)val);
    CHECK(val >= 15, "Value >= 15 after queue signal");

    // ── 5. Queue Wait (GPU→GPU) ──
    printf("\n[5] Queue Wait (GPU→GPU)...\n");
    ID3D12Fence *fence2 = nullptr;
    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence2));
    CHECK_HR(hr, "CreateFence #2");

    // Signal fence2 to 20
    hr = queue->Signal(fence2, 20);
    CHECK_HR(hr, "Queue::Signal fence2=20");

    // Wait on fence2 from queue
    hr = queue->Wait(fence2, 20);
    CHECK_HR(hr, "Queue::Wait(fence2, 20)");

    Sleep(10);
    CHECK(fence2->GetCompletedValue() >= 20, "fence2 >= 20");

    // ── 6. SetEventOnCompletion ──
    printf("\n[6] SetEventOnCompletion...\n");
    HANDLE evt = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    CHECK(evt != nullptr, "CreateEvent");

    ID3D12Fence *fence3 = nullptr;
    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence3));
    CHECK_HR(hr, "CreateFence #3");

    hr = fence3->SetEventOnCompletion(1, evt);
    CHECK_HR(hr, "SetEventOnCompletion(1)");

    // Signal fence3 to 1 → event should fire
    hr = fence3->Signal(1);
    CHECK_HR(hr, "Fence::Signal(1)");

    DWORD wait_result = WaitForSingleObject(evt, 1000);
    CHECK(wait_result == WAIT_OBJECT_0, "Event signaled after fence completes");

    // ── 7. Multiple fences ──
    printf("\n[7] Multiple fences...\n");
    for (int i = 0; i < 5; i++) {
        ID3D12Fence *f = nullptr;
        hr = device->CreateFence(i * 10, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&f));
        CHECK_HR(hr, "CreateFence");
        CHECK(f->GetCompletedValue() == (UINT64)(i * 10), "Initial value correct");
        f->Release();
    }

    // ── 8. Shared fence ──
    printf("\n[8] Shared fence...\n");
    ID3D12Fence *shared = nullptr;
    hr = device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&shared));
    CHECK_HR(hr, "CreateFence(SHARED)");
    shared->Release();

    // ── Cleanup ──
    CloseHandle(evt);
    fence3->Release();
    fence2->Release();
    fence->Release();
    queue->Release();
    device->Release();
    adapter->Release();
    factory->Release();
    CHECK(true, "All released");

    printf("\n========================================\n");
    printf(test_passed ? "  M8 Test: ALL PASSED\n" : "  M8 Test: FAILURES\n");
    printf("========================================\n");
    return test_passed ? 0 : 1;
}
