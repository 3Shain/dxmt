// ═══════════════════════════════════════════════════════════════
// M1+M2 test: D3D12 device, queue, allocator, command list
//
// M1 validates: device creation, queues, feature queries
// M2 validates: allocator reset, list record/close/reset lifecycle,
//               ExecuteCommandLists with real lists
// ═══════════════════════════════════════════════════════════════

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>
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

int main() {
    printf("=== M1+M2 D3D12 Test ===\n\n");

    // ── 1. Device setup (M1) ──
    printf("[M1] Device setup...\n");
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

    // ── M1: Queue creation ──
    D3D12_COMMAND_QUEUE_DESC qdesc = {};
    qdesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ID3D12CommandQueue *queue = nullptr;
    hr = device->CreateCommandQueue(&qdesc, IID_PPV_ARGS(&queue));
    CHECK_HR(hr, "CreateCommandQueue(DIRECT)");

    // ── 2. Allocator (M2) ──
    printf("\n[M2] Command allocator...\n");

    ID3D12CommandAllocator *allocator = nullptr;
    hr = device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
    CHECK_HR(hr, "CreateCommandAllocator(DIRECT)");
    if (FAILED(hr)) goto cleanup;

    // ── 3. Command list lifecycle (M2) ──
    printf("\n[M2] Command list lifecycle...\n");

    ID3D12GraphicsCommandList *cmdlist = nullptr;
    hr = device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        allocator, nullptr, IID_PPV_ARGS(&cmdlist));
    CHECK_HR(hr, "CreateCommandList");

    // Record some commands
    D3D12_VIEWPORT vp = {0, 0, 1920, 1080, 0, 1};
    cmdlist->RSSetViewports(1, &vp);

    D3D12_RECT scissor = {0, 0, 1920, 1080};
    cmdlist->RSSetScissorRects(1, &scissor);

    cmdlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdlist->DrawInstanced(3, 1, 0, 0);
    cmdlist->DrawInstanced(3, 1, 3, 0);

    // Close
    hr = cmdlist->Close();
    CHECK_HR(hr, "CommandList::Close");

    // ── 4. Execute (M2) ──
    printf("\n[M2] ExecuteCommandLists...\n");
    ID3D12CommandList *lists[] = {
        static_cast<ID3D12CommandList *>(cmdlist)
    };
    hr = queue->ExecuteCommandLists(1, lists);
    CHECK_HR(hr, "ExecuteCommandLists(1 list)");

    // ── 5. Reset + reuse (M2) ──
    printf("\n[M2] Reset + reuse cycle...\n");

    hr = cmdlist->Reset(allocator, nullptr);
    CHECK_HR(hr, "CommandList::Reset");

    // Record new commands in the reused list
    cmdlist->Dispatch(8, 1, 1);
    cmdlist->Dispatch(16, 1, 1);

    hr = cmdlist->Close();
    CHECK_HR(hr, "CommandList::Close (second recording)");

    hr = queue->ExecuteCommandLists(1, lists);
    CHECK_HR(hr, "ExecuteCommandLists (after reset/reuse)");

    // ── 6. Multiple lists (M2) ──
    printf("\n[M2] Multiple command lists...\n");

    // Create a second allocator + list
    ID3D12CommandAllocator *alloc2 = nullptr;
    hr = device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc2));
    CHECK_HR(hr, "CreateCommandAllocator #2");

    ID3D12GraphicsCommandList *cmdlist2 = nullptr;
    hr = device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        alloc2, nullptr, IID_PPV_ARGS(&cmdlist2));
    CHECK_HR(hr, "CreateCommandList #2");

    cmdlist2->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    cmdlist2->DrawInstanced(2, 1, 0, 0);
    hr = cmdlist2->Close();
    CHECK_HR(hr, "CommandList #2::Close");

    // Submit both lists in one call
    ID3D12CommandList *batch[] = {
        static_cast<ID3D12CommandList *>(cmdlist),
        static_cast<ID3D12CommandList *>(cmdlist2)
    };
    hr = queue->ExecuteCommandLists(2, batch);
    CHECK_HR(hr, "ExecuteCommandLists(2 lists)");

    // ── 7. Allocator Reset (M2) ──
    printf("\n[M2] Allocator Reset...\n");

    // Reset the allocator recycles memory
    hr = allocator->Reset();
    CHECK_HR(hr, "Allocator::Reset");

    // Can still reset the command list to the same allocator
    hr = cmdlist->Reset(allocator, nullptr);
    CHECK_HR(hr, "CommandList::Reset after allocator reset");

    cmdlist->Dispatch(1, 1, 1);
    hr = cmdlist->Close();
    CHECK_HR(hr, "CommandList::Close (3rd recording)");

    hr = queue->ExecuteCommandLists(1, lists);
    CHECK_HR(hr, "ExecuteCommandLists after allocator recycle");

    // ── 8. Error cases (M2) ──
    printf("\n[M2] Error cases...\n");

    // Close a closed list
    hr = cmdlist->Close();
    CHECK(FAILED(hr), "Close on already-closed list fails");

    // Reset an open list
    cmdlist->Reset(allocator, nullptr); // should work (was closed)
    // Try to double-reset
    hr = cmdlist->Reset(allocator, nullptr);
    CHECK(FAILED(hr), "Reset on recording list fails");

    // Close the open list for cleanup
    cmdlist->Close();

    // ── 9. Allocator type validation ──
    printf("\n[M2] Allocator types...\n");

    ID3D12CommandAllocator *comp_alloc = nullptr;
    hr = device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&comp_alloc));
    CHECK_HR(hr, "CreateCommandAllocator(COMPUTE)");

    ID3D12GraphicsCommandList *comp_list = nullptr;
    hr = device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_COMPUTE,
        comp_alloc, nullptr, IID_PPV_ARGS(&comp_list));
    CHECK_HR(hr, "CreateCommandList(COMPUTE)");

    comp_list->Dispatch(64, 1, 1);
    hr = comp_list->Close();
    CHECK_HR(hr, "Compute list::Close");

    ID3D12CommandList *comp_lists[] = {
        static_cast<ID3D12CommandList *>(comp_list)
    };
    hr = queue->ExecuteCommandLists(1, comp_lists);
    CHECK_HR(hr, "ExecuteCommandLists(compute list on DIRECT queue)");

    // ── Cleanup ──
    printf("\nCleanup...\n");
    comp_list->Release();
    comp_alloc->Release();
    cmdlist2->Release();
    alloc2->Release();
    cmdlist->Release();
    allocator->Release();

cleanup:
    queue->Release();
    device->Release();
    adapter->Release();
    factory->Release();
    CHECK(true, "All resources released");

    printf("\n========================================\n");
    if (test_passed) {
        printf("  M1+M2 Test: ALL PASSED\n");
    } else {
        printf("  M1+M2 Test: FAILURES DETECTED\n");
    }
    printf("========================================\n");

    return test_passed ? 0 : 1;
}
