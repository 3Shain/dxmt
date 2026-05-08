#include "d3d12_command_queue.hpp"
#include "d3d12_device.hpp"
#include "log/log.hpp"
#include "util_string.hpp"

namespace dxmt {

MTLD3D12CommandQueue::MTLD3D12CommandQueue(MTLD3D12Device *device,
                                           CommandQueue &queue,
                                           D3D12_COMMAND_QUEUE_DESC desc)
    : m_device(device), m_queue(queue), m_desc(desc) {
  m_device->AddRef();
  Logger::info("D3D12CommandQueue created");
}

MTLD3D12CommandQueue::~MTLD3D12CommandQueue() {
  m_device->Release();
}

HRESULT STDMETHODCALLTYPE
MTLD3D12CommandQueue::QueryInterface(REFIID riid, void **ppvObject) {
  if (!ppvObject)
    return E_POINTER;
  *ppvObject = nullptr;

  if (riid == IID_IUnknown || riid == IID_ID3D12Object ||
      riid == IID_ID3D12DeviceChild || riid == IID_ID3D12Pageable ||
      riid == IID_ID3D12CommandQueue) {
    *ppvObject = ref(this);
    return S_OK;
  }
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE MTLD3D12CommandQueue::AddRef() {
  return ++m_refCount;
}

ULONG STDMETHODCALLTYPE MTLD3D12CommandQueue::Release() {
  uint32_t rc = --m_refCount;
  if (!rc) {
    uint32_t rp = --m_refPrivate;
    if (!rp) {
      m_refPrivate += 0x80000000;
      delete this;
    }
  }
  return rc;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12CommandQueue::GetPrivateData(REFGUID guid, UINT *data_size,
                                     void *data) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12CommandQueue::SetPrivateData(REFGUID guid, UINT data_size,
                                     const void *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12CommandQueue::SetPrivateDataInterface(REFGUID guid,
                                              const IUnknown *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D12CommandQueue::SetName(LPCWSTR name) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12CommandQueue::GetDevice(REFIID riid, void **device) {
  return m_device->QueryInterface(riid, device);
}

void STDMETHODCALLTYPE MTLD3D12CommandQueue::UpdateTileMappings(
    ID3D12Resource *resource, UINT region_count,
    const D3D12_TILED_RESOURCE_COORDINATE *region_start_coordinates,
    const D3D12_TILE_REGION_SIZE *region_sizes, ID3D12Heap *heap,
    UINT range_count, const D3D12_TILE_RANGE_FLAGS *range_flags,
    const UINT *heap_range_offsets, const UINT *range_tile_counts,
    D3D12_TILE_MAPPING_FLAGS flags) {}

void STDMETHODCALLTYPE MTLD3D12CommandQueue::CopyTileMappings(
    ID3D12Resource *dst_resource,
    const D3D12_TILED_RESOURCE_COORDINATE *dst_region_start_coordinate,
    ID3D12Resource *src_resource,
    const D3D12_TILED_RESOURCE_COORDINATE *src_region_start_coordinate,
    const D3D12_TILE_REGION_SIZE *region_size,
    D3D12_TILE_MAPPING_FLAGS flags) {}

void STDMETHODCALLTYPE MTLD3D12CommandQueue::ExecuteCommandLists(
    UINT command_list_count,
    ID3D12CommandList *const *command_lists) {
  Logger::info(str::format("ExecuteCommandLists: ", command_list_count, " lists"));
  for (UINT i = 0; i < command_list_count; i++) {
    // TODO: submit each command list to the DXMT command queue
  }
}

void STDMETHODCALLTYPE MTLD3D12CommandQueue::SetMarker(UINT metadata,
                                                       const void *data,
                                                       UINT size) {}

void STDMETHODCALLTYPE MTLD3D12CommandQueue::BeginEvent(UINT metadata,
                                                        const void *data,
                                                        UINT size) {}

void STDMETHODCALLTYPE MTLD3D12CommandQueue::EndEvent() {}

HRESULT STDMETHODCALLTYPE
MTLD3D12CommandQueue::Signal(ID3D12Fence *fence, UINT64 value) {
  Logger::warn("D3D12CommandQueue::Signal: stub");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12CommandQueue::Wait(ID3D12Fence *fence, UINT64 value) {
  Logger::warn("D3D12CommandQueue::Wait: stub");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12CommandQueue::GetTimestampFrequency(UINT64 *frequency) {
  if (frequency)
    *frequency = 1000000000;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12CommandQueue::GetClockCalibration(UINT64 *gpu_timestamp,
                                          UINT64 *cpu_timestamp) {
  if (gpu_timestamp)
    *gpu_timestamp = 0;
  if (cpu_timestamp)
    *cpu_timestamp = 0;
  return S_OK;
}

D3D12_COMMAND_QUEUE_DESC *STDMETHODCALLTYPE
MTLD3D12CommandQueue::GetDesc(D3D12_COMMAND_QUEUE_DESC *__ret) {
  *__ret = m_desc;
  return __ret;
}

} // namespace dxmt
