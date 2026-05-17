#pragma once

#include "com/com_pointer.hpp"
#include "d3d12.h"
#include "dxgi_interfaces.h"
#include "dxmt_command_queue.hpp"
#include "Metal.hpp"
#include <atomic>

namespace dxmt {

class MTLD3D12Device;

class MTLD3D12CommandQueue : public ID3D12CommandQueue {
public:
  MTLD3D12CommandQueue(MTLD3D12Device *device, CommandQueue &queue,
                       D3D12_COMMAND_QUEUE_DESC desc);
  ~MTLD3D12CommandQueue();

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;

  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *data_size,
                                          void *data) override;
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT data_size,
                                          const void *data) override;
  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
      REFGUID guid, const IUnknown *data) override;
  HRESULT STDMETHODCALLTYPE SetName(LPCWSTR name) override;

  HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void **device) override;

  void STDMETHODCALLTYPE UpdateTileMappings(
      ID3D12Resource *resource, UINT region_count,
      const D3D12_TILED_RESOURCE_COORDINATE *region_start_coordinates,
      const D3D12_TILE_REGION_SIZE *region_sizes, ID3D12Heap *heap,
      UINT range_count, const D3D12_TILE_RANGE_FLAGS *range_flags,
      const UINT *heap_range_offsets, const UINT *range_tile_counts,
      D3D12_TILE_MAPPING_FLAGS flags) override;

  void STDMETHODCALLTYPE CopyTileMappings(
      ID3D12Resource *dst_resource,
      const D3D12_TILED_RESOURCE_COORDINATE *dst_region_start_coordinate,
      ID3D12Resource *src_resource,
      const D3D12_TILED_RESOURCE_COORDINATE *src_region_start_coordinate,
      const D3D12_TILE_REGION_SIZE *region_size,
      D3D12_TILE_MAPPING_FLAGS flags) override;

  void STDMETHODCALLTYPE ExecuteCommandLists(
      UINT command_list_count,
      ID3D12CommandList *const *command_lists) override;

  void STDMETHODCALLTYPE SetMarker(UINT metadata, const void *data,
                                   UINT size) override;

  void STDMETHODCALLTYPE BeginEvent(UINT metadata, const void *data,
                                    UINT size) override;

  void STDMETHODCALLTYPE EndEvent() override;

  HRESULT STDMETHODCALLTYPE Signal(ID3D12Fence *fence,
                                   UINT64 value) override;

  HRESULT STDMETHODCALLTYPE Wait(ID3D12Fence *fence, UINT64 value) override;

  HRESULT STDMETHODCALLTYPE
  GetTimestampFrequency(UINT64 *frequency) override;

  HRESULT STDMETHODCALLTYPE
  GetClockCalibration(UINT64 *gpu_timestamp, UINT64 *cpu_timestamp) override;

  D3D12_COMMAND_QUEUE_DESC *STDMETHODCALLTYPE
  GetDesc(D3D12_COMMAND_QUEUE_DESC *__ret) override;

  CommandQueue &GetDXMTCommandQueue() { return m_queue; }

private:
  MTLD3D12Device *m_device;
  CommandQueue &m_queue;
  D3D12_COMMAND_QUEUE_DESC m_desc;
  std::atomic<uint32_t> m_refCount = {1ul};
  std::atomic<uint32_t> m_refPrivate = {1ul};
  WMT::Reference<WMT::CommandQueue> m_wmt_queue;
  WMT::Reference<WMT::Event> m_barrier_event;
  uint64_t m_barrier_seq = 0;
};

} // namespace dxmt
