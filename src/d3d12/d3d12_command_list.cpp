#include "d3d12_command_list.hpp"
#include "d3d12_command_allocator.hpp"
#include "d3d12_device.hpp"
#include "d3d12_pipeline_state.hpp"
#include "d3d12_resource.hpp"
#include "d3d12_root_signature.hpp"
#include "d3d12_descriptor_heap.hpp"
#include "log/log.hpp"
#include "util_string.hpp"

#define CLTRACE(fmt, ...) do { FILE *_tf = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a"); if (_tf) { fprintf(_tf, "CmdList::" fmt "\n", ##__VA_ARGS__); fclose(_tf); } } while(0)

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
      riid == IID_ID3D12GraphicsCommandList ||
      riid == IID_ID3D12GraphicsCommandList1 ||
      riid == IID_ID3D12GraphicsCommandList2 ||
      riid == IID_ID3D12GraphicsCommandList3 ||
      riid == IID_ID3D12GraphicsCommandList4 ||
      riid == IID_ID3D12GraphicsCommandList5 ||
      riid == IID_ID3D12GraphicsCommandList6) {
    *ppvObject = ref(this);
    return S_OK;
  }
  CLTRACE("QI unknown IID %s -> E_NOINTERFACE", str::format(riid).c_str());
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
  CLTRACE("GetPrivateData E_NOTIMPL");
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
  CLTRACE("Close");
  m_closed = true;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::Reset(
    ID3D12CommandAllocator *allocator, ID3D12PipelineState *initial_state) {
  CLTRACE("Reset");
  m_closed = false;
  m_cmds.clear();
  if (initial_state) {
    CmdSetPipelineState cmd = {};
    cmd.header = {CmdType::SetPipelineState, sizeof(cmd)};
    cmd.pso = initial_state;
    Emit(cmd);
  }
  return S_OK;
}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::ClearState(ID3D12PipelineState *pipeline_state) {
  m_cmds.clear();
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::DrawInstanced(
    UINT vertex_count, UINT instance_count, UINT start_vertex,
    UINT start_instance) {
  CLTRACE("DrawInstanced v=%u i=%u", vertex_count, instance_count);
  CmdDrawInstanced cmd = {};
  cmd.header = {CmdType::DrawInstanced, sizeof(cmd)};
  cmd.vertex_count = vertex_count;
  cmd.instance_count = instance_count;
  cmd.start_vertex = start_vertex;
  cmd.start_instance = start_instance;
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::DrawIndexedInstanced(
    UINT index_count, UINT instance_count, UINT start_vertex,
    INT base_vertex, UINT start_instance) {
  CLTRACE("DrawIndexedInstanced idx=%u i=%u", index_count, instance_count);
  CmdDrawIndexedInstanced cmd = {};
  cmd.header = {CmdType::DrawIndexedInstanced, sizeof(cmd)};
  cmd.index_count = index_count;
  cmd.instance_count = instance_count;
  cmd.start_vertex = start_vertex;
  cmd.base_vertex = base_vertex;
  cmd.start_instance = start_instance;
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::Dispatch(UINT x, UINT y,
                                                             UINT z) {
  CLTRACE("Dispatch %ux%ux%u", x, y, z);
  CmdDispatch cmd = {};
  cmd.header = {CmdType::Dispatch, sizeof(cmd)};
  cmd.x = x;
  cmd.y = y;
  cmd.z = z;
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::CopyBufferRegion(
    ID3D12Resource *dst, UINT64 dst_offset, ID3D12Resource *src,
    UINT64 src_offset, UINT64 byte_count) {
  CmdCopyBufferRegion cmd = {};
  cmd.header = {CmdType::CopyBufferRegion, sizeof(cmd)};
  cmd.dst = dst;
  cmd.dst_offset = dst_offset;
  cmd.src = src;
  cmd.src_offset = src_offset;
  cmd.byte_count = byte_count;
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::CopyTextureRegion(
    const D3D12_TEXTURE_COPY_LOCATION *dst, UINT dst_x, UINT dst_y,
    UINT dst_z, const D3D12_TEXTURE_COPY_LOCATION *src,
    const D3D12_BOX *src_box) {
  if (!dst || !src) return;
  CmdCopyTextureRegion cmd = {};
  cmd.header = {CmdType::CopyTextureRegion, sizeof(cmd)};
  cmd.dst_resource = dst->pResource;
  cmd.dst_type = dst->Type;
  cmd.dst_x = dst_x;
  cmd.dst_y = dst_y;
  cmd.dst_z = dst_z;
  if (dst->Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX) {
    cmd.dst_subresource = dst->SubresourceIndex;
  } else {
    cmd.dst_offset = dst->PlacedFootprint.Offset;
    cmd.dst_footprint_width = dst->PlacedFootprint.Footprint.Width;
    cmd.dst_footprint_height = dst->PlacedFootprint.Footprint.Height;
    cmd.dst_footprint_depth = dst->PlacedFootprint.Footprint.Depth;
    cmd.dst_footprint_row_pitch = dst->PlacedFootprint.Footprint.RowPitch;
  }
  cmd.src_resource = src->pResource;
  cmd.src_type = src->Type;
  if (src->Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX) {
    cmd.src_subresource = src->SubresourceIndex;
  } else {
    cmd.src_offset = src->PlacedFootprint.Offset;
    cmd.src_footprint_width = src->PlacedFootprint.Footprint.Width;
    cmd.src_footprint_height = src->PlacedFootprint.Footprint.Height;
    cmd.src_footprint_depth = src->PlacedFootprint.Footprint.Depth;
    cmd.src_footprint_row_pitch = src->PlacedFootprint.Footprint.RowPitch;
  }
  if (src_box) {
    cmd.src_box = *src_box;
    cmd.has_src_box = 1;
  }
  Emit(cmd);
}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::CopyResource(ID3D12Resource *dst,
                                          ID3D12Resource *src) {
  if (!dst || !src) return;
  CmdCopyResource cmd = {};
  cmd.header = {CmdType::CopyResource, sizeof(cmd)};
  cmd.dst = dst;
  cmd.src = src;
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::CopyTiles(
    ID3D12Resource *tiled_resource,
    const D3D12_TILED_RESOURCE_COORDINATE *tile_region_start_coordinate,
    const D3D12_TILE_REGION_SIZE *tile_region_size,
    ID3D12Resource *buffer, UINT64 buffer_offset,
    D3D12_TILE_COPY_FLAGS flags) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::ResolveSubresource(
    ID3D12Resource *dst, UINT dst_sub, ID3D12Resource *src, UINT src_sub,
    DXGI_FORMAT format) {
  CmdResolveSubresource cmd = {};
  cmd.header = {CmdType::ResolveSubresource, sizeof(cmd)};
  cmd.dst = dst;
  cmd.dst_sub = dst_sub;
  cmd.src = src;
  cmd.src_sub = src_sub;
  cmd.format = format;
  cmd.mode = D3D12_RESOLVE_MODE_DECOMPRESS;
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::IASetPrimitiveTopology(
    D3D12_PRIMITIVE_TOPOLOGY topology) {
  CmdIASetPrimitiveTopology cmd = {};
  cmd.header = {CmdType::IASetPrimitiveTopology, sizeof(cmd)};
  cmd.topology = topology;
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::RSSetViewports(
    UINT count, const D3D12_VIEWPORT *viewports) {
  size_t extra = count * sizeof(D3D12_VIEWPORT);
  auto total = sizeof(CmdRSSetViewports) - sizeof(D3D12_VIEWPORT) + extra;
  auto offset = m_cmds.size();
  m_cmds.resize(offset + total);
  CmdRSSetViewports cmd = {};
  cmd.header = {CmdType::RSSetViewports, (uint32_t)total};
  cmd.count = count;
  memcpy(m_cmds.data() + offset, &cmd, sizeof(CmdRSSetViewports) - sizeof(D3D12_VIEWPORT));
  memcpy(m_cmds.data() + offset + sizeof(CmdRSSetViewports) - sizeof(D3D12_VIEWPORT),
         viewports, extra);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::RSSetScissorRects(
    UINT count, const D3D12_RECT *rects) {
  size_t extra = count * sizeof(D3D12_RECT);
  auto total = sizeof(CmdRSSetScissorRects) - sizeof(D3D12_RECT) + extra;
  auto offset = m_cmds.size();
  m_cmds.resize(offset + total);
  CmdRSSetScissorRects cmd = {};
  cmd.header = {CmdType::RSSetScissorRects, (uint32_t)total};
  cmd.count = count;
  memcpy(m_cmds.data() + offset, &cmd, sizeof(CmdRSSetScissorRects) - sizeof(D3D12_RECT));
  memcpy(m_cmds.data() + offset + sizeof(CmdRSSetScissorRects) - sizeof(D3D12_RECT),
         rects, extra);
}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::OMSetBlendFactor(const FLOAT blend_factor[4]) {
  CmdOMBlendFactor cmd = {};
  cmd.header = {CmdType::OMSetBlendFactor, sizeof(cmd)};
  memcpy(cmd.factor, blend_factor, 16);
  Emit(cmd);
}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::OMSetStencilRef(UINT stencil_ref) {
  CmdOMStencilRef cmd = {};
  cmd.header = {CmdType::OMSetStencilRef, sizeof(cmd)};
  cmd.stencil_ref = stencil_ref;
  Emit(cmd);
}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::SetPipelineState(
    ID3D12PipelineState *pipeline_state) {
  CmdSetPipelineState cmd = {};
  cmd.header = {CmdType::SetPipelineState, sizeof(cmd)};
  cmd.pso = pipeline_state;
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::ResourceBarrier(
    UINT barrier_count, const D3D12_RESOURCE_BARRIER *barriers) {
  size_t extra = barrier_count * sizeof(D3D12_RESOURCE_BARRIER);
  auto total = sizeof(CmdResourceBarrier) - sizeof(D3D12_RESOURCE_BARRIER) + extra;
  auto offset = m_cmds.size();
  m_cmds.resize(offset + total);
  CmdResourceBarrier cmd = {};
  cmd.header = {CmdType::ResourceBarrier, (uint32_t)total};
  cmd.count = barrier_count;
  memcpy(m_cmds.data() + offset, &cmd, sizeof(CmdResourceBarrier) - sizeof(D3D12_RESOURCE_BARRIER));
  memcpy(m_cmds.data() + offset + sizeof(CmdResourceBarrier) - sizeof(D3D12_RESOURCE_BARRIER),
         barriers, extra);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::ExecuteBundle(
    ID3D12GraphicsCommandList *command_list) {
  CLTRACE("ExecuteBundle cmds=%zu", command_list ? static_cast<MTLD3D12GraphicsCommandList*>(command_list)->GetCommands().size() : 0);
  if (command_list) {
    auto *bundle = static_cast<MTLD3D12GraphicsCommandList*>(command_list);
    const auto &bundle_cmds = bundle->GetCommands();
    m_cmds.insert(m_cmds.end(), bundle_cmds.begin(), bundle_cmds.end());
  }
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetDescriptorHeaps(
    UINT heap_count, ID3D12DescriptorHeap *const *heaps) {
  size_t extra = heap_count * sizeof(ID3D12DescriptorHeap *);
  auto total = sizeof(CmdSetDescriptorHeaps) - sizeof(ID3D12DescriptorHeap *) + extra;
  auto offset = m_cmds.size();
  m_cmds.resize(offset + total);
  CmdSetDescriptorHeaps cmd = {};
  cmd.header = {CmdType::SetDescriptorHeaps, (uint32_t)total};
  cmd.count = heap_count;
  memcpy(m_cmds.data() + offset, &cmd, sizeof(CmdSetDescriptorHeaps) - sizeof(ID3D12DescriptorHeap *));
  memcpy(m_cmds.data() + offset + sizeof(CmdSetDescriptorHeaps) - sizeof(ID3D12DescriptorHeap *),
         heaps, extra);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetComputeRootSignature(
    ID3D12RootSignature *root_signature) {
  CmdSetRootSignature cmd = {};
  cmd.header = {CmdType::SetComputeRootSignature, sizeof(cmd)};
  cmd.root_sig = root_signature;
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetGraphicsRootSignature(
    ID3D12RootSignature *root_signature) {
  CmdSetRootSignature cmd = {};
  cmd.header = {CmdType::SetGraphicsRootSignature, sizeof(cmd)};
  cmd.root_sig = root_signature;
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetComputeRootDescriptorTable(
    UINT root_parameter_index,
    D3D12_GPU_DESCRIPTOR_HANDLE base_descriptor) {
  CmdSetRootDescriptorTable cmd = {};
  cmd.header = {CmdType::SetComputeRootDescriptorTable, sizeof(cmd)};
  cmd.root_param_index = root_parameter_index;
  cmd.base_descriptor = base_descriptor;
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetGraphicsRootDescriptorTable(
    UINT root_parameter_index,
    D3D12_GPU_DESCRIPTOR_HANDLE base_descriptor) {
  CmdSetRootDescriptorTable cmd = {};
  cmd.header = {CmdType::SetGraphicsRootDescriptorTable, sizeof(cmd)};
  cmd.root_param_index = root_parameter_index;
  cmd.base_descriptor = base_descriptor;
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetComputeRoot32BitConstant(
    UINT root_parameter_index, UINT data, UINT dst_offset) {
  CmdSetRoot32BitConstants cmd = {};
  cmd.header = {CmdType::SetComputeRoot32BitConstants, sizeof(cmd)};
  cmd.root_param_index = root_parameter_index;
  cmd.count = 1;
  cmd.dst_offset = dst_offset;
  memcpy(cmd.data, &data, 4);
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetGraphicsRoot32BitConstant(
    UINT root_parameter_index, UINT data, UINT dst_offset) {
  CmdSetRoot32BitConstants cmd = {};
  cmd.header = {CmdType::SetGraphicsRoot32BitConstants, sizeof(cmd)};
  cmd.root_param_index = root_parameter_index;
  cmd.count = 1;
  cmd.dst_offset = dst_offset;
  memcpy(cmd.data, &data, 4);
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetComputeRoot32BitConstants(
    UINT root_parameter_index, UINT constant_count, const void *data,
    UINT dst_offset) {
  size_t extra = constant_count * 4;
  auto total = sizeof(CmdSetRoot32BitConstants) - 1 + extra;
  auto offset = m_cmds.size();
  m_cmds.resize(offset + total);
  CmdSetRoot32BitConstants cmd = {};
  cmd.header = {CmdType::SetComputeRoot32BitConstants, (uint32_t)total};
  cmd.root_param_index = root_parameter_index;
  cmd.count = constant_count;
  cmd.dst_offset = dst_offset;
  memcpy(m_cmds.data() + offset, &cmd, sizeof(CmdSetRoot32BitConstants) - 1);
  memcpy(m_cmds.data() + offset + sizeof(CmdSetRoot32BitConstants) - 1, data, extra);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetGraphicsRoot32BitConstants(
    UINT root_parameter_index, UINT constant_count, const void *data,
    UINT dst_offset) {
  size_t extra = constant_count * 4;
  auto total = sizeof(CmdSetRoot32BitConstants) - 1 + extra;
  auto offset = m_cmds.size();
  m_cmds.resize(offset + total);
  CmdSetRoot32BitConstants cmd = {};
  cmd.header = {CmdType::SetGraphicsRoot32BitConstants, (uint32_t)total};
  cmd.root_param_index = root_parameter_index;
  cmd.count = constant_count;
  cmd.dst_offset = dst_offset;
  memcpy(m_cmds.data() + offset, &cmd, sizeof(CmdSetRoot32BitConstants) - 1);
  memcpy(m_cmds.data() + offset + sizeof(CmdSetRoot32BitConstants) - 1, data, extra);
}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::SetComputeRootConstantBufferView(
    UINT root_parameter_index, D3D12_GPU_VIRTUAL_ADDRESS address) {
  CmdSetRootCBV cmd = {};
  cmd.header = {CmdType::SetComputeRootConstantBufferView, sizeof(cmd)};
  cmd.root_param_index = root_parameter_index;
  cmd.address = address;
  Emit(cmd);
}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::SetGraphicsRootConstantBufferView(
    UINT root_parameter_index, D3D12_GPU_VIRTUAL_ADDRESS address) {
  CmdSetRootCBV cmd = {};
  cmd.header = {CmdType::SetGraphicsRootConstantBufferView, sizeof(cmd)};
  cmd.root_param_index = root_parameter_index;
  cmd.address = address;
  Emit(cmd);
}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::SetComputeRootShaderResourceView(
    UINT root_parameter_index, D3D12_GPU_VIRTUAL_ADDRESS address) {
  CmdSetRootCBV cmd = {};
  cmd.header = {CmdType::SetComputeRootShaderResourceView, sizeof(cmd)};
  cmd.root_param_index = root_parameter_index;
  cmd.address = address;
  Emit(cmd);
}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::SetGraphicsRootShaderResourceView(
    UINT root_parameter_index, D3D12_GPU_VIRTUAL_ADDRESS address) {
  CmdSetRootCBV cmd = {};
  cmd.header = {CmdType::SetGraphicsRootShaderResourceView, sizeof(cmd)};
  cmd.root_param_index = root_parameter_index;
  cmd.address = address;
  Emit(cmd);
}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::SetComputeRootUnorderedAccessView(
    UINT root_parameter_index, D3D12_GPU_VIRTUAL_ADDRESS address) {
  CmdSetRootCBV cmd = {};
  cmd.header = {CmdType::SetComputeRootUnorderedAccessView, sizeof(cmd)};
  cmd.root_param_index = root_parameter_index;
  cmd.address = address;
  Emit(cmd);
}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::SetGraphicsRootUnorderedAccessView(
    UINT root_parameter_index, D3D12_GPU_VIRTUAL_ADDRESS address) {
  CmdSetRootCBV cmd = {};
  cmd.header = {CmdType::SetGraphicsRootUnorderedAccessView, sizeof(cmd)};
  cmd.root_param_index = root_parameter_index;
  cmd.address = address;
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::IASetIndexBuffer(
    const D3D12_INDEX_BUFFER_VIEW *view) {
  CmdIASetIndexBuffer cmd = {};
  cmd.header = {CmdType::IASetIndexBuffer, sizeof(cmd)};
  if (view)
    cmd.view = *view;
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::IASetVertexBuffers(
    UINT start_slot, UINT count,
    const D3D12_VERTEX_BUFFER_VIEW *views) {
  size_t extra = count * sizeof(D3D12_VERTEX_BUFFER_VIEW);
  auto total = sizeof(CmdIASetVertexBuffers) - sizeof(D3D12_VERTEX_BUFFER_VIEW) + extra;
  auto offset = m_cmds.size();
  m_cmds.resize(offset + total);
  CmdIASetVertexBuffers cmd = {};
  cmd.header = {CmdType::IASetVertexBuffers, (uint32_t)total};
  cmd.start_slot = start_slot;
  cmd.count = count;
  memcpy(m_cmds.data() + offset, &cmd, sizeof(CmdIASetVertexBuffers) - sizeof(D3D12_VERTEX_BUFFER_VIEW));
  if (views)
    memcpy(m_cmds.data() + offset + sizeof(CmdIASetVertexBuffers) - sizeof(D3D12_VERTEX_BUFFER_VIEW),
           views, extra);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SOSetTargets(
    UINT start_slot, UINT view_count,
    const D3D12_STREAM_OUTPUT_BUFFER_VIEW *views) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::OMSetRenderTargets(
    UINT rt_count, const D3D12_CPU_DESCRIPTOR_HANDLE *rts,
    WINBOOL single_handle,
    const D3D12_CPU_DESCRIPTOR_HANDLE *dsv) {
  CmdOMSetRenderTargets cmd = {};
  cmd.header = {CmdType::OMSetRenderTargets, sizeof(cmd)};
  cmd.rt_count = rt_count;
  cmd.single_handle = single_handle != 0;
  cmd.has_dsv = dsv != nullptr;
  if (rts) {
    for (UINT i = 0; i < rt_count && i < 8; i++)
      cmd.rts[i] = rts[i];
  }
  if (dsv)
    cmd.dsv = *dsv;
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::ClearDepthStencilView(
    D3D12_CPU_DESCRIPTOR_HANDLE dsv, D3D12_CLEAR_FLAGS flags, FLOAT depth,
    UINT8 stencil, UINT rect_count, const D3D12_RECT *rects) {
  CmdClearDSV cmd = {};
  cmd.header = {CmdType::ClearDepthStencilView, sizeof(cmd)};
  cmd.dsv = dsv;
  cmd.flags = flags;
  cmd.depth = depth;
  cmd.stencil = stencil;
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::ClearRenderTargetView(
    D3D12_CPU_DESCRIPTOR_HANDLE rtv, const FLOAT color[4], UINT rect_count,
    const D3D12_RECT *rects) {
  CmdClearRTV cmd = {};
  cmd.header = {CmdType::ClearRenderTargetView, sizeof(cmd)};
  cmd.rtv = rtv;
  if (color)
    memcpy(cmd.color, color, 16);
  else
    TRACE("ClearRenderTargetView called with null color pointer");
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::ClearUnorderedAccessViewUint(
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle,
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, ID3D12Resource *resource,
    const UINT values[4], UINT rect_count, const D3D12_RECT *rects) {
  CmdClearUAV cmd = {};
  cmd.header = {CmdType::ClearUnorderedAccessView, sizeof(cmd)};
  cmd.gpu_handle = gpu_handle;
  cmd.cpu_handle = cpu_handle;
  cmd.resource = resource;
  if (values)
    memcpy(cmd.values, values, sizeof(cmd.values));
  Emit(cmd);
}

void STDMETHODCALLTYPE
MTLD3D12GraphicsCommandList::ClearUnorderedAccessViewFloat(
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle,
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, ID3D12Resource *resource,
    const float values[4], UINT rect_count, const D3D12_RECT *rects) {
  CmdClearUAV cmd = {};
  cmd.header = {CmdType::ClearUnorderedAccessView, sizeof(cmd)};
  cmd.gpu_handle = gpu_handle;
  cmd.cpu_handle = cpu_handle;
  cmd.resource = resource;
  cmd.is_float = 1;
  if (values)
    memcpy(cmd.values, values, sizeof(cmd.values));
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::DiscardResource(
    ID3D12Resource *resource, const D3D12_DISCARD_REGION *region) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::BeginQuery(
    ID3D12QueryHeap *heap, D3D12_QUERY_TYPE type, UINT index) {
  CmdQuery cmd = {};
  cmd.header = {CmdType::BeginQuery, sizeof(cmd)};
  cmd.heap = heap;
  cmd.type = type;
  cmd.index = index;
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::EndQuery(
    ID3D12QueryHeap *heap, D3D12_QUERY_TYPE type, UINT index) {
  CmdQuery cmd = {};
  cmd.header = {CmdType::EndQuery, sizeof(cmd)};
  cmd.heap = heap;
  cmd.type = type;
  cmd.index = index;
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::ResolveQueryData(
    ID3D12QueryHeap *heap, D3D12_QUERY_TYPE type, UINT start_index,
    UINT query_count, ID3D12Resource *dst_buffer,
    UINT64 aligned_dst_buffer_offset) {
  CmdResolveQueryData cmd = {};
  cmd.header = {CmdType::ResolveQueryData, sizeof(cmd)};
  cmd.heap = heap;
  cmd.type = type;
  cmd.start_index = start_index;
  cmd.query_count = query_count;
  cmd.dst_buffer = dst_buffer;
  cmd.dst_offset = aligned_dst_buffer_offset;
  Emit(cmd);
}

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
    ID3D12Resource *count_buffer, UINT64 count_buffer_offset) {
  CLTRACE("ExecuteIndirect max=%u", max_command_count);
  CmdExecuteIndirect cmd = {};
  cmd.header = {CmdType::ExecuteIndirect, sizeof(cmd)};
  cmd.signature = command_signature;
  cmd.max_command_count = max_command_count;
  cmd.argument_buffer = arg_buffer;
  cmd.argument_buffer_offset = arg_buffer_offset;
  cmd.count_buffer = count_buffer;
  cmd.count_buffer_offset = count_buffer_offset;
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::AtomicCopyBufferUINT(
    ID3D12Resource *dst_buffer, UINT64 dst_offset,
    ID3D12Resource *src_buffer, UINT64 src_offset,
    UINT dependent_resource_count,
    ID3D12Resource *const *dependent_resources,
    const D3D12_SUBRESOURCE_RANGE_UINT64 *dependent_sub_resource_ranges) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::AtomicCopyBufferUINT64(
    ID3D12Resource *dst_buffer, UINT64 dst_offset,
    ID3D12Resource *src_buffer, UINT64 src_offset,
    UINT dependent_resource_count,
    ID3D12Resource *const *dependent_resources,
    const D3D12_SUBRESOURCE_RANGE_UINT64 *dependent_sub_resource_ranges) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::OMSetDepthBounds(
    FLOAT min, FLOAT max) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetSamplePositions(
    UINT sample_count, UINT pixel_count,
    D3D12_SAMPLE_POSITION *sample_positions) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::ResolveSubresourceRegion(
    ID3D12Resource *dst_resource, UINT dst_sub_resource_idx,
    UINT dst_x, UINT dst_y,
    ID3D12Resource *src_resource, UINT src_sub_resource_idx,
    D3D12_RECT *src_rect, DXGI_FORMAT format,
    D3D12_RESOLVE_MODE mode) {
  CmdResolveSubresource cmd = {};
  cmd.header = {CmdType::ResolveSubresource, sizeof(cmd)};
  cmd.dst = dst_resource;
  cmd.dst_sub = dst_sub_resource_idx;
  cmd.dst_x = dst_x;
  cmd.dst_y = dst_y;
  cmd.src = src_resource;
  cmd.src_sub = src_sub_resource_idx;
  cmd.format = format;
  cmd.mode = mode;
  if (src_rect) {
    cmd.has_src_rect = 1;
    cmd.src_rect = *src_rect;
  }
  Emit(cmd);
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetViewInstanceMask(
    UINT mask) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::WriteBufferImmediate(
    UINT count, const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER *parameters,
    const D3D12_WRITEBUFFERIMMEDIATE_MODE *modes) {
  if (!count || !parameters)
    return;
  std::vector<CmdWriteBufferImmediateEntry> entries(count);
  for (UINT i = 0; i < count; i++) {
    entries[i].parameter = parameters[i];
    entries[i].mode = modes ? modes[i] : D3D12_WRITEBUFFERIMMEDIATE_MODE_DEFAULT;
  }
  CmdWriteBufferImmediate cmd = {};
  size_t extra = count * sizeof(CmdWriteBufferImmediateEntry);
  auto total = sizeof(CmdWriteBufferImmediate) -
               sizeof(CmdWriteBufferImmediateEntry) + extra;
  auto offset = m_cmds.size();
  m_cmds.resize(offset + total);
  cmd.header = {CmdType::WriteBufferImmediate, (uint32_t)total};
  cmd.count = count;
  memcpy(m_cmds.data() + offset, &cmd,
         sizeof(CmdWriteBufferImmediate) -
             sizeof(CmdWriteBufferImmediateEntry));
  memcpy(m_cmds.data() + offset + sizeof(CmdWriteBufferImmediate) -
             sizeof(CmdWriteBufferImmediateEntry),
         entries.data(), extra);
}

/*** ID3D12GraphicsCommandList3 ***/
void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetProtectedResourceSession(
    ID3D12ProtectedResourceSession *protected_session) {
  CLTRACE("SetProtectedResourceSession -> noop");
}

/*** ID3D12GraphicsCommandList4 ***/
void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::BeginRenderPass(
    UINT num_render_targets,
    const D3D12_RENDER_PASS_RENDER_TARGET_DESC *render_targets,
    const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *depth_stencil,
    D3D12_RENDER_PASS_FLAGS flags) {
  CLTRACE("BeginRenderPass numRT=%u flags=0x%x", num_render_targets, (unsigned)flags);

  if (render_targets && num_render_targets > 0) {
    D3D12_CPU_DESCRIPTOR_HANDLE rt_handles[8];
    for (UINT i = 0; i < num_render_targets && i < 8; i++) {
      rt_handles[i] = render_targets[i].cpuDescriptor;
    }
    OMSetRenderTargets(num_render_targets, rt_handles, FALSE,
                       depth_stencil ? &depth_stencil->cpuDescriptor : nullptr);

    for (UINT i = 0; i < num_render_targets && i < 8; i++) {
      if (render_targets[i].BeginningAccess.Type ==
          D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR) {
        ClearRenderTargetView(render_targets[i].cpuDescriptor,
                              render_targets[i].BeginningAccess.Clear
                                  .ClearValue.Color,
                              0, nullptr);
      }
    }
  }

  if (depth_stencil) {
    D3D12_CLEAR_FLAGS clear_flags = (D3D12_CLEAR_FLAGS)0;
    FLOAT depth = 1.0f;
    UINT8 stencil = 0;
    if (depth_stencil->DepthBeginningAccess.Type ==
        D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR) {
      clear_flags =
          (D3D12_CLEAR_FLAGS)(clear_flags | D3D12_CLEAR_FLAG_DEPTH);
      depth = depth_stencil->DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth;
    }
    if (depth_stencil->StencilBeginningAccess.Type ==
        D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR) {
      clear_flags =
          (D3D12_CLEAR_FLAGS)(clear_flags | D3D12_CLEAR_FLAG_STENCIL);
      stencil =
          depth_stencil->StencilBeginningAccess.Clear.ClearValue.DepthStencil.Stencil;
    }
    if (clear_flags)
      ClearDepthStencilView(depth_stencil->cpuDescriptor, clear_flags, depth,
                            stencil, 0, nullptr);
  }
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::EndRenderPass() {
  CLTRACE("EndRenderPass");
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::InitializeMetaCommand(
    ID3D12MetaCommand *meta_command, const void *initialization_parameters_data,
    SIZE_T initialization_parameters_data_size_in_bytes) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::ExecuteMetaCommand(
    ID3D12MetaCommand *meta_command, const void *execution_parameters_data,
    SIZE_T execution_parameters_data_size_in_bytes) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::BuildRaytracingAccelerationStructure(
    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC *desc,
    UINT num_post_build_info_descs,
    const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *post_build_info_descs) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::EmitRaytracingAccelerationStructurePostbuildInfo(
    const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *descs,
    UINT num_acceleration_structures,
    const D3D12_GPU_VIRTUAL_ADDRESS *source_acceleration_structure_data) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::CopyRaytracingAccelerationStructure(
    D3D12_GPU_VIRTUAL_ADDRESS dest_acceleration_structure_data,
    D3D12_GPU_VIRTUAL_ADDRESS source_acceleration_structure_data,
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE mode) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::SetPipelineState1(
    ID3D12StateObject *state_object) {
  CLTRACE("SetPipelineState1 -> noop (raytracing)");
}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::DispatchRays(
    const D3D12_DISPATCH_RAYS_DESC *desc) {}

/*** ID3D12GraphicsCommandList5 ***/
void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::RSSetShadingRate(
    D3D12_SHADING_RATE base_shading_rate,
    const D3D12_SHADING_RATE_COMBINER *combiners) {}

void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::RSSetShadingRateImage(
    ID3D12Resource *shading_rate_image) {}

/*** ID3D12GraphicsCommandList6 ***/
void STDMETHODCALLTYPE MTLD3D12GraphicsCommandList::DispatchMesh(
    UINT thread_group_count_x, UINT thread_group_count_y,
    UINT thread_group_count_z) {}

} // namespace dxmt
