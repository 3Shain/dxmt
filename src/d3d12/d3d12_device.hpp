#pragma once

#include "com/com_pointer.hpp"
#include "d3d12.h"
#include "dxgi_interfaces.h"
#include "dxmt_device.hpp"
#include <atomic>
#include <unordered_map>
#include <mutex>

namespace dxmt {

class MTLD3D12Resource;

class MTLD3D12Device : public ID3D12Device {
public:
  MTLD3D12Device(std::unique_ptr<Device> &&device,
                  IMTLDXGIAdapter *pAdapter);
  ~MTLD3D12Device();

  WMT::Device GetMTLDevice();
  Device &GetDXMTDevice();

  void RegisterResource(MTLD3D12Resource *res);
  void UnregisterResource(MTLD3D12Resource *res);
  MTLD3D12Resource *LookupResourceByGPUAddress(D3D12_GPU_VIRTUAL_ADDRESS addr);

  /*** IUnknown ***/
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                            void **ppvObject) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;

  /*** ID3D12Object ***/
  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *data_size,
                                            void *data) override;
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT data_size,
                                            const void *data) override;
  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
      REFGUID guid, const IUnknown *data) override;
  HRESULT STDMETHODCALLTYPE SetName(LPCWSTR name) override;

  /*** ID3D12Device ***/
  UINT STDMETHODCALLTYPE GetNodeCount() override;

  HRESULT STDMETHODCALLTYPE CreateCommandQueue(
      const D3D12_COMMAND_QUEUE_DESC *desc, REFIID riid,
      void **command_queue) override;

  HRESULT STDMETHODCALLTYPE CreateCommandAllocator(
      D3D12_COMMAND_LIST_TYPE type, REFIID riid,
      void **command_allocator) override;

  HRESULT STDMETHODCALLTYPE CreateGraphicsPipelineState(
      const D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc, REFIID riid,
      void **pipeline_state) override;

  HRESULT STDMETHODCALLTYPE CreateComputePipelineState(
      const D3D12_COMPUTE_PIPELINE_STATE_DESC *desc, REFIID riid,
      void **pipeline_state) override;

  HRESULT STDMETHODCALLTYPE CreateCommandList(
      UINT node_mask, D3D12_COMMAND_LIST_TYPE type,
      ID3D12CommandAllocator *command_allocator,
      ID3D12PipelineState *initial_pipeline_state, REFIID riid,
      void **command_list) override;

  HRESULT STDMETHODCALLTYPE CheckFeatureSupport(
      D3D12_FEATURE feature, void *feature_data,
      UINT feature_data_size) override;

  HRESULT STDMETHODCALLTYPE CreateDescriptorHeap(
      const D3D12_DESCRIPTOR_HEAP_DESC *desc, REFIID riid,
      void **descriptor_heap) override;

  UINT STDMETHODCALLTYPE GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type) override;

  HRESULT STDMETHODCALLTYPE CreateRootSignature(
      UINT node_mask, const void *bytecode, SIZE_T bytecode_length,
      REFIID riid, void **root_signature) override;

  void STDMETHODCALLTYPE CreateConstantBufferView(
      const D3D12_CONSTANT_BUFFER_VIEW_DESC *desc,
      D3D12_CPU_DESCRIPTOR_HANDLE descriptor) override;

  void STDMETHODCALLTYPE CreateShaderResourceView(
      ID3D12Resource *resource,
      const D3D12_SHADER_RESOURCE_VIEW_DESC *desc,
      D3D12_CPU_DESCRIPTOR_HANDLE descriptor) override;

  void STDMETHODCALLTYPE CreateUnorderedAccessView(
      ID3D12Resource *resource, ID3D12Resource *counter_resource,
      const D3D12_UNORDERED_ACCESS_VIEW_DESC *desc,
      D3D12_CPU_DESCRIPTOR_HANDLE descriptor) override;

  void STDMETHODCALLTYPE CreateRenderTargetView(
      ID3D12Resource *resource,
      const D3D12_RENDER_TARGET_VIEW_DESC *desc,
      D3D12_CPU_DESCRIPTOR_HANDLE descriptor) override;

  void STDMETHODCALLTYPE CreateDepthStencilView(
      ID3D12Resource *resource,
      const D3D12_DEPTH_STENCIL_VIEW_DESC *desc,
      D3D12_CPU_DESCRIPTOR_HANDLE descriptor) override;

  void STDMETHODCALLTYPE CreateSampler(
      const D3D12_SAMPLER_DESC *desc,
      D3D12_CPU_DESCRIPTOR_HANDLE descriptor) override;

  void STDMETHODCALLTYPE CopyDescriptors(
      UINT dst_descriptor_range_count,
      const D3D12_CPU_DESCRIPTOR_HANDLE *dst_descriptor_range_offsets,
      const UINT *dst_descriptor_range_sizes,
      UINT src_descriptor_range_count,
      const D3D12_CPU_DESCRIPTOR_HANDLE *src_descriptor_range_offsets,
      const UINT *src_descriptor_range_sizes,
      D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type) override;

  void STDMETHODCALLTYPE CopyDescriptorsSimple(
      UINT descriptor_count,
      const D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor_range_offset,
      const D3D12_CPU_DESCRIPTOR_HANDLE src_descriptor_range_offset,
      D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type) override;

  D3D12_RESOURCE_ALLOCATION_INFO* STDMETHODCALLTYPE GetResourceAllocationInfo(
      D3D12_RESOURCE_ALLOCATION_INFO *__ret, UINT visible_mask,
      UINT resource_desc_count,
      const D3D12_RESOURCE_DESC *resource_descs) override;

  D3D12_HEAP_PROPERTIES* STDMETHODCALLTYPE GetCustomHeapProperties(
      D3D12_HEAP_PROPERTIES *__ret, UINT node_mask,
      D3D12_HEAP_TYPE heap_type) override;

  HRESULT STDMETHODCALLTYPE CreateCommittedResource(
      const D3D12_HEAP_PROPERTIES *heap_properties,
      D3D12_HEAP_FLAGS heap_flags, const D3D12_RESOURCE_DESC *desc,
      D3D12_RESOURCE_STATES initial_state,
      const D3D12_CLEAR_VALUE *optimized_clear_value, REFIID riid,
      void **resource) override;

  HRESULT STDMETHODCALLTYPE CreateHeap(const D3D12_HEAP_DESC *desc,
                                        REFIID riid,
                                        void **heap) override;

  HRESULT STDMETHODCALLTYPE CreatePlacedResource(
      ID3D12Heap *heap, UINT64 heap_offset,
      const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES initial_state,
      const D3D12_CLEAR_VALUE *optimized_clear_value, REFIID riid,
      void **resource) override;

  HRESULT STDMETHODCALLTYPE CreateReservedResource(
      const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES initial_state,
      const D3D12_CLEAR_VALUE *optimized_clear_value, REFIID riid,
      void **resource) override;

  HRESULT STDMETHODCALLTYPE CreateSharedHandle(
      ID3D12DeviceChild *object, const SECURITY_ATTRIBUTES *attributes,
      DWORD access, const WCHAR *name, HANDLE *handle) override;

  HRESULT STDMETHODCALLTYPE OpenSharedHandle(HANDLE handle, REFIID riid,
                                              void **object) override;

  HRESULT STDMETHODCALLTYPE OpenSharedHandleByName(const WCHAR *name,
                                                    DWORD access,
                                                    HANDLE *handle) override;

  HRESULT STDMETHODCALLTYPE MakeResident(
      UINT object_count, ID3D12Pageable *const *objects) override;

  HRESULT STDMETHODCALLTYPE Evict(UINT object_count,
                                    ID3D12Pageable *const *objects) override;

  HRESULT STDMETHODCALLTYPE CreateFence(UINT64 initial_value,
                                          D3D12_FENCE_FLAGS flags, REFIID riid,
                                          void **fence) override;

  HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason() override;

  void STDMETHODCALLTYPE GetCopyableFootprints(
      const D3D12_RESOURCE_DESC *desc, UINT first_sub_resource,
      UINT sub_resource_count, UINT64 base_offset,
      D3D12_PLACED_SUBRESOURCE_FOOTPRINT *layouts, UINT *row_count,
      UINT64 *row_size, UINT64 *total_bytes) override;

  HRESULT STDMETHODCALLTYPE CreateQueryHeap(const D3D12_QUERY_HEAP_DESC *desc,
                                              REFIID riid,
                                              void **heap) override;

  HRESULT STDMETHODCALLTYPE SetStablePowerState(WINBOOL enable) override;

  HRESULT STDMETHODCALLTYPE CreateCommandSignature(
      const D3D12_COMMAND_SIGNATURE_DESC *desc,
      ID3D12RootSignature *root_signature, REFIID riid,
      void **command_signature) override;

  void STDMETHODCALLTYPE GetResourceTiling(
      ID3D12Resource *resource, UINT *total_tile_count,
      D3D12_PACKED_MIP_INFO *packed_mip_info,
      D3D12_TILE_SHAPE *standard_tile_shape,
      UINT *sub_resource_tiling_count,
      UINT first_sub_resource_tiling,
      D3D12_SUBRESOURCE_TILING *sub_resource_tilings) override;

  LUID* STDMETHODCALLTYPE GetAdapterLuid(LUID *__ret) override;

private:
  std::unique_ptr<Device> m_device;
  Com<IMTLDXGIAdapter> m_adapter;
  std::atomic<uint32_t> m_refCount = {1ul};
  std::atomic<uint32_t> m_refPrivate = {1ul};
  std::mutex m_resource_mutex;
  std::unordered_map<uint64_t, MTLD3D12Resource *> m_resources_by_gpu_addr;
};

} // namespace dxmt
