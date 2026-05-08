#include "d3d12_command_list.hpp"
#include "d3d12_command_allocator.hpp"
#include "d3d12_device.hpp"
#include "log/log.hpp"
#include "util_string.hpp"

namespace dxmt {

MTLD3D12GraphicsCommandList::MTLD3D12GraphicsCommandList(
    MTLD3D12Device *device, MTLD3D12CommandAllocator *allocator,
    D3D12_COMMAND_LIST_TYPE type, ID3D12PipelineState *initial_state)
    : m_device(device), m_allocator(allocator), m_type(type) {
  m_device->AddRef();
  if (m_allocator)
    m_allocator->AddRef();
}

MTLD3D12GraphicsCommandList::~MTLD3D12GraphicsCommandList() {
  if (m_allocator)
    m_allocator->Release();
  m_device->Release();
}

HRESULT STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::QueryInterface(REFIID riid, void **ppvObject) {
  if (!ppvObject)
    return E_POINTER;
  *ppvObject = nullptr;

  if (riid == IID_IUnknown || riid == IID_ID3D12Object ||
      riid == IID_ID3D12DeviceChild || riid == IID_ID3D12CommandList ||
      riid == IID_ID3D12GraphicsCommandList) {
    *ppvObject = ref(this);
    return S_OK;
  }
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::AddRef() {
  return ++m_refCount;
}

ULONG STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::Release() {
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
MTLD3D12GraphicsCommandList::GetPrivateData(REFGUID guid, UINT *data_size,
                                            void *data) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::SetPrivateData(REFGUID guid, UINT data_size,
                                            const void *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::SetPrivateDataInterface(REFGUID guid,
                                                     const IUnknown *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::SetName(LPCWSTR name) { return S_OK; }

HRESULT STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::GetDevice(REFIID riid, void **device) {
  return m_device->QueryInterface(riid, device);
}

D3D12_COMMAND_LIST_TYPE STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::GetType() {
  return m_type;
}

HRESULT STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::Close() {
  m_closed = true;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::Reset(
    ID3D12CommandAllocator *allocator, ID3D12PipelineState *initial_state) {
  m_closed = false;
  return S_OK;
}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::ClearState(ID3D12PipelineState *pipeline_state) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::DrawInstanced(
    UINT vertex_count_per_instance, UINT instance_count,
    UINT start_vertex_location, UINT start_instance_location) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::DrawIndexedInstanced(
    UINT index_count_per_instance, UINT instance_count,
    UINT start_vertex_location, INT base_vertex_location,
    UINT start_instance_location) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::Dispatch(UINT x, UINT u,
                                                             UINT z) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::CopyBufferRegion(
    ID3D12Resource *dst_buffer, UINT64 dst_offset,
    ID3D12Resource *src_buffer, UINT64 src_offset, UINT64 byte_count) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::CopyTextureRegion(
    const D3D12_TEXTURE_COPY_LOCATION *dst, UINT dst_x, UINT dst_y,
    UINT dst_z, const D3D12_TEXTURE_COPY_LOCATION *src,
    const D3D12_BOX *src_box) {}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::CopyResource(ID3D12Resource *dst_resource,
                                          ID3D12Resource *src_resource) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::CopyTiles(
    ID3D12Resource *tiled_resource,
    const D3D12_TILED_RESOURCE_COORDINATE *tile_region_start_coordinate,
    const D3D12_TILE_REGION_SIZE *tile_region_size,
    ID3D12Resource *buffer, UINT64 buffer_offset,
    D3D12_TILE_COPY_FLAGS flags) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::ResolveSubresource(
    ID3D12Resource *dst_resource, UINT dst_sub_resource,
    ID3D12Resource *src_resource, UINT src_sub_resource,
    DXGI_FORMAT format) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::IASetPrimitiveTopology(
    D3D12_PRIMITIVE_TOPOLOGY primitive_topology) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::RSSetViewports(
    UINT viewport_count, const D3D12_VIEWPORT *viewports) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::RSSetScissorRects(
    UINT rect_count, const D3D12_RECT *rects) {}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::OMSetBlendFactor(const FLOAT blend_factor[4]) {}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::OMSetStencilRef(UINT stencil_ref) {}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::SetPipelineState(
    ID3D12PipelineState *pipeline_state) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::ResourceBarrier(
    UINT barrier_count, const D3D12_RESOURCE_BARRIER *barriers) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::ExecuteBundle(
    ID3D12GraphicsCommandList *command_list) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetDescriptorHeaps(
    UINT heap_count, ID3D12DescriptorHeap *const *heaps) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetComputeRootSignature(
    ID3D12RootSignature *root_signature) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetGraphicsRootSignature(
    ID3D12RootSignature *root_signature) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetComputeRootDescriptorTable(
    UINT root_parameter_index,
    D3D12_GPU_DESCRIPTOR_HANDLE base_descriptor) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetGraphicsRootDescriptorTable(
    UINT root_parameter_index,
    D3D12_GPU_DESCRIPTOR_HANDLE base_descriptor) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetComputeRoot32BitConstant(
    UINT root_parameter_index, UINT data, UINT dst_offset) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetGraphicsRoot32BitConstant(
    UINT root_parameter_index, UINT data, UINT dst_offset) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetComputeRoot32BitConstants(
    UINT root_parameter_index, UINT constant_count, const void *data,
    UINT dst_offset) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetGraphicsRoot32BitConstants(
    UINT root_parameter_index, UINT constant_count, const void *data,
    UINT dst_offset) {}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::SetComputeRootConstantBufferView(
    UINT root_parameter_index, D3D12_GPU_VIRTUAL_ADDRESS address) {}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::SetGraphicsRootConstantBufferView(
    UINT root_parameter_index, D3D12_GPU_VIRTUAL_ADDRESS address) {}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::SetComputeRootShaderResourceView(
    UINT root_parameter_index, D3D12_GPU_VIRTUAL_ADDRESS address) {}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::SetGraphicsRootShaderResourceView(
    UINT root_parameter_index, D3D12_GPU_VIRTUAL_ADDRESS address) {}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::SetComputeRootUnorderedAccessView(
    UINT root_parameter_index, D3D12_GPU_VIRTUAL_ADDRESS address) {}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::SetGraphicsRootUnorderedAccessView(
    UINT root_parameter_index, D3D12_GPU_VIRTUAL_ADDRESS address) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::IASetIndexBuffer(
    const D3D12_INDEX_BUFFER_VIEW *view) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::IASetVertexBuffers(
    UINT start_slot, UINT view_count,
    const D3D12_VERTEX_BUFFER_VIEW *views) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SOSetTargets(
    UINT start_slot, UINT view_count,
    const D3D12_STREAM_OUTPUT_BUFFER_VIEW *views) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::OMSetRenderTargets(
    UINT render_target_descriptor_count,
    const D3D12_CPU_DESCRIPTOR_HANDLE *render_target_descriptors,
    WINBOOL single_descriptor_handle,
    const D3D12_CPU_DESCRIPTOR_HANDLE *depth_stencil_descriptor) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::ClearDepthStencilView(
    D3D12_CPU_DESCRIPTOR_HANDLE dsv, D3D12_CLEAR_FLAGS flags, FLOAT depth,
    UINT8 stencil, UINT rect_count, const D3D12_RECT *rects) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::ClearRenderTargetView(
    D3D12_CPU_DESCRIPTOR_HANDLE rtv, const FLOAT color[4], UINT rect_count,
    const D3D12_RECT *rects) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::ClearUnorderedAccessViewUint(
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle,
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, ID3D12Resource *resource,
    const UINT values[4], UINT rect_count, const D3D12_RECT *rects) {}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::ClearUnorderedAccessViewFloat(
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle,
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, ID3D12Resource *resource,
    const float values[4], UINT rect_count, const D3D12_RECT *rects) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::DiscardResource(
    ID3D12Resource *resource, const D3D12_DISCARD_REGION *region) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::BeginQuery(
    ID3D12QueryHeap *heap, D3D12_QUERY_TYPE type, UINT index) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::EndQuery(
    ID3D12QueryHeap *heap, D3D12_QUERY_TYPE type, UINT index) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::ResolveQueryData(
    ID3D12QueryHeap *heap, D3D12_QUERY_TYPE type, UINT start_index,
    UINT query_count, ID3D12Resource *dst_buffer,
    UINT64 aligned_dst_buffer_offset) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetPredication(
    ID3D12Resource *buffer, UINT64 aligned_buffer_offset,
    D3D12_PREDICATION_OP operation) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetMarker(
    UINT metadata, const void *data, UINT size) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::BeginEvent(
    UINT metadata, const void *data, UINT size) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::EndEvent() {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::ExecuteIndirect(
    ID3D12CommandSignature *command_signature, UINT max_command_count,
    ID3D12Resource *arg_buffer, UINT64 arg_buffer_offset,
    ID3D12Resource *count_buffer, UINT64 count_buffer_offset) {}

} // namespace dxmt
