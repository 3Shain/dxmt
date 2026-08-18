/*
 * Copyright 2026 Feifan He for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "d3d12_command_allocator.hpp"
#include "com/com_pointer.hpp"

namespace dxmt {

MTLD3D12CommandAllocatorImpl::MTLD3D12CommandAllocatorImpl(MTLD3D12Device *pDevice, D3D12_COMMAND_LIST_TYPE Type) :
    MTLD3D12Pageable<MTLD3D12CommandAllocator>(pDevice),
    type_(Type),
    clear_uav_(device_->GetMTLDevice(), *this),
    copy_temp_allocator_(
        {device_->GetMTLDevice(), WMTResourceHazardTrackingModeUntracked | WMTResourceStorageModePrivate}
    ),
    copy_temp_version_(0) {}

HRESULT
CreateCommandAllocator(MTLD3D12Device *pDevice, D3D12_COMMAND_LIST_TYPE Type, REFIID riid, void **ppCommandAllocator) {
  switch (Type) {
  case D3D12_COMMAND_LIST_TYPE_DIRECT:
  case D3D12_COMMAND_LIST_TYPE_BUNDLE:
  case D3D12_COMMAND_LIST_TYPE_COMPUTE:
  case D3D12_COMMAND_LIST_TYPE_COPY:
    break;
  default:
    return E_INVALIDARG;
  }
  auto command_allocator = Com(new MTLD3D12CommandAllocatorImpl(pDevice, Type));
  HRESULT hr = command_allocator->Initialize();
  if (FAILED(hr))
    return hr;
  return command_allocator->QueryInterface(riid, ppCommandAllocator);
}

HRESULT
MTLD3D12CommandAllocatorImpl::Initialize() {

  if (!cpu_heap_)
    cpu_heap_ = malloc(kCPUHeapSize);
  if (!cpu_heap_)
    return E_OUTOFMEMORY;
  cpu_heap_offset_ = 0;

  if (!gpu_heap_)
    gpu_heap_ = malloc(kGPUHeapSize);
  if (!gpu_heap_)
    return E_OUTOFMEMORY;
  gpu_heap_offset_ = 0;

  WMTBufferInfo buffer_info;
  buffer_info.memory.set(gpu_heap_);
  buffer_info.length = kGPUHeapSize;
  buffer_info.options = WMTResourceHazardTrackingModeUntracked;
  gpu_heap_buffer_ = device_->GetMTLDevice().newBuffer(buffer_info);

  if (!gpu_heap_buffer_) {
    ERR("CommandAllocator: failed to allocate gpu buffer");
    return E_FAIL;
  }
  gpu_heap_buffer_address_ = buffer_info.gpu_address;

  encoder_current = nullptr;
  encoder_last = nullptr;
  encoder_count_ = 0;

  icb_.clear();

  copy_temp_allocator_.free_blocks(copy_temp_version_++);

  return S_OK;
}

HRESULT
STDMETHODCALLTYPE
MTLD3D12CommandAllocatorImpl::QueryInterface(REFIID riid, void **ppvObject) {
  if (ppvObject == nullptr)
    return E_POINTER;

  *ppvObject = nullptr;

  if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D12Object) || riid == __uuidof(ID3D12DeviceChild) ||
      riid == __uuidof(ID3D12Pageable) || riid == __uuidof(ID3D12CommandAllocator)) {
    *ppvObject = ref(this);
    return S_OK;
  }

  if (logQueryInterfaceError(__uuidof(ID3D12CommandQueue), riid)) {
    WARN("D3D12CommandAllocator: Unknown interface query ", str::format(riid));
  }

  return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12CommandAllocatorImpl::Reset() {
  if (encoder_last)
    return E_FAIL;

  for (auto &encoder_list : encoder_lists_) {
    EncoderData *next = encoder_list.next;
    while (next) {
      switch (next->type) {
      case EncoderType::Null:
        break;
      case EncoderType::Clear:
        reinterpret_cast<ClearEncoderData *>(next)->~ClearEncoderData();
        break;
      case EncoderType::Render:
        reinterpret_cast<RenderEncoderData *>(next)->~RenderEncoderData();
        break;
      case EncoderType::Blit:
        reinterpret_cast<BlitEncoderData *>(next)->~BlitEncoderData();
        break;
      case EncoderType::Compute:
        reinterpret_cast<ComputeEncoderData *>(next)->~ComputeEncoderData();
        break;
      case EncoderType::Resolve:
        reinterpret_cast<ResolveEncoderData *>(next)->~ResolveEncoderData();
        break;
      }
      next = next->next;
    }
  }
  encoder_lists_.clear();

  return Initialize();
};

IndirectComputeCommandData *
MTLD3D12CommandAllocatorImpl::EncodeIndirectComputeCommand(MTLD3D12CommandSignature *pCmdSig, MTLD3D12ComputePipelineState *pPSO, size_t MaxCount) {
  WMTIndirectCommandBufferInfo info;
  info.inherit_buffers = !pCmdSig->UpdateRootArguments;
  info.inherit_pso = 1;
  info.inherit_cull_mode = 0;
  info.inherit_fill_mode = 0;
  info.inherit_front_facing = 0;
  info.inherit_depth_bias = 0;
  info.inherit_depth_clip_mode = 0;
  info.inherit_depth_stencil_state = 0;
  info.support_color_attachment_mapping = 0;
  info.support_dynamic_attribute_stride = 0;
  info.support_ray_tracing = 0;
  info.type = WMTIndirectCommandTypeConcurrentDispatch;
  info.max_vertex_buffer_binding = 0;
  info.max_fragment_buffer_binding = 0;
  info.max_object_buffer_binding = 0;
  info.max_mesh_buffer_binding = 0;
  info.max_kernel_buffer_binding = 31;
  info.max_kernel_threadgroup_memory_binding = 0;
  info.max_object_threadgroup_memory_binding = 0;
  info.gpu_resource_id = 0;

  auto icb = device_->GetMTLDevice().newIndirectCommandBuffer(info, MaxCount, WMTResourceStorageModeShared);

  auto [Ptr, Offset] = AllocateGPUHeap(sizeof(IndirectComputeCommandData), 16);

  auto data = reinterpret_cast<IndirectComputeCommandData *>(Ptr);

  data->cmd_buf = info.gpu_resource_id;
  data->max_count = MaxCount;
  data->tgsize_x = pPSO->threadgroup_size.width;
  data->tgsize_y = pPSO->threadgroup_size.height;
  data->tgsize_z = pPSO->threadgroup_size.depth;

  {
    // populated outside
    data->max_count_buffer = 0;
    data->argument_buffer = 0;
    data->rootsig_qwords = 0;
    data->rootsig_qwords_stride = 0;
    data->static_samplers = 0;
  }

  {
    /**
     * TODO: move these out?
     */

    auto &cmd_use_icb = EncodeComputeCommand<wmtcmd_compute_useresource>();
    cmd_use_icb.type = WMTComputeCommandUseResource;
    cmd_use_icb.usage = WMTResourceUsageRead | WMTResourceUsageWrite;
    cmd_use_icb.resource = icb;

    auto &cmd_setpso_res = EncodeComputeCommand<wmtcmd_compute_setpso>();
    cmd_setpso_res.type = WMTComputeCommandSetPSO;
    cmd_setpso_res.pso = pCmdSig->compute_resolver;
    cmd_setpso_res.threadgroup_size = {1, 1, 1};

    auto &cmd_argbuf_res = EncodeComputeCommand<wmtcmd_compute_setbuffer>();
    cmd_argbuf_res.type = WMTComputeCommandSetBuffer;
    cmd_argbuf_res.buffer = gpu_heap_buffer_;
    cmd_argbuf_res.offset = Offset;
    cmd_argbuf_res.index = 30;

    auto &cmd_dispatch_res = EncodeComputeCommand<wmtcmd_compute_dispatch>();
    cmd_dispatch_res.type = WMTComputeCommandDispatch;
    cmd_dispatch_res.size = {1, 1, 1};

    auto &cmd_setpso = EncodeComputeCommand<wmtcmd_compute_setpso>();
    cmd_setpso.type = WMTComputeCommandSetPSO;
    cmd_setpso.pso = pPSO->pso;
    cmd_setpso.threadgroup_size = pPSO->threadgroup_size; // not really used
  }

  auto &cmd = EncodeComputeCommand<wmtcmd_compute_executecommands>();
  cmd.type = WMTComputeCommandExecuteCommandsInBuffer;
  cmd.indirect_command_buffer = icb;
  cmd.location = 0;
  cmd.length = MaxCount;

  icb_.push_back(std::move(icb));

  return data;
}

IndirectRenderCommandData *
MTLD3D12CommandAllocatorImpl::EncodeIndirectRenderCommand(
    MTLD3D12CommandSignature *pCmdSig, MTLD3D12GraphicsPipelineState *pPSO, size_t MaxCount
) {
  WMTIndirectCommandBufferInfo info;
  info.inherit_buffers = !(pCmdSig->UpdateVertexBuffers || pCmdSig->UpdateIndexBuffer || pCmdSig->UpdateRootArguments);
  info.inherit_pso = 1;
  info.inherit_cull_mode = 1;
  info.inherit_fill_mode = 1;
  info.inherit_front_facing = 1;
  info.inherit_depth_bias = 1;
  info.inherit_depth_clip_mode = 1;
  info.inherit_depth_stencil_state = 1;
  info.support_color_attachment_mapping = 0;
  info.support_dynamic_attribute_stride = 0;
  info.support_ray_tracing = 0;
  info.type = pCmdSig->CommandType == D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED ? WMTIndirectCommandTypeDrawIndexed
                                                                                : WMTIndirectCommandTypeDraw;
  info.max_vertex_buffer_binding = 31;
  info.max_fragment_buffer_binding = 31;
  info.max_object_buffer_binding = 0;
  info.max_mesh_buffer_binding = 0;
  info.max_kernel_buffer_binding = 0;
  info.max_kernel_threadgroup_memory_binding = 0;
  info.max_object_threadgroup_memory_binding = 0;
  info.gpu_resource_id = 0;

  auto icb = device_->GetMTLDevice().newIndirectCommandBuffer(info, MaxCount, WMTResourceStorageModePrivate);

  auto [Ptr, Offset] = AllocateGPUHeap(sizeof(IndirectRenderCommandData), 16);

  auto data = reinterpret_cast<IndirectRenderCommandData *>(Ptr);

  data->cmd_buf = info.gpu_resource_id;
  data->max_count = MaxCount;

  {
    // populated outside
    data->max_count_buffer = 0;
    data->argument_buffer = 0;
    data->rootsig_qwords = 0;
    data->rootsig_qwords_stride = 0;
    data->static_samplers = 0;
    data->vertex_buffer = 0;
    data->vertex_argbuf_stride = 0;
    data->primitive_type = 0;
    data->index_buffer = 0;
    data->index_buffer_format = {};
  }

  {
    /**
     * TODO: move these out?
     */

    auto &cmd_use_icb = EncodeRenderCommand<wmtcmd_render_useresource>();
    cmd_use_icb.type = WMTRenderCommandUseResource;
    cmd_use_icb.stages = WMTRenderStageVertex;
    cmd_use_icb.usage = WMTResourceUsageRead | WMTResourceUsageWrite;
    cmd_use_icb.resource = icb;

    auto &cmd_setpso_res = EncodeRenderCommand<wmtcmd_render_setpso>();
    cmd_setpso_res.type = WMTRenderCommandSetPSO;
    cmd_setpso_res.pso = pCmdSig->render_resolver;

    auto &cmd_argbuf_res = EncodeRenderCommand<wmtcmd_render_setbuffer>();
    cmd_argbuf_res.type = WMTRenderCommandSetVertexBuffer;
    cmd_argbuf_res.buffer = gpu_heap_buffer_;
    cmd_argbuf_res.offset = Offset;
    cmd_argbuf_res.index = 30;

    auto &cmd_draw_res = EncodeRenderCommand<wmtcmd_render_draw>();
    cmd_draw_res.type = WMTRenderCommandDraw;
    cmd_draw_res.primitive_type = WMTPrimitiveTypePoint;
    cmd_draw_res.vertex_start = 0;
    cmd_draw_res.vertex_count = 1;
    cmd_draw_res.base_instance = 0;
    cmd_draw_res.instance_count = 1;

    auto &cmd_setpso = EncodeRenderCommand<wmtcmd_render_setpso>();
    cmd_setpso.type = WMTRenderCommandSetPSO;
    cmd_setpso.pso = pPSO->pso;
  }

  auto &cmd = EncodeRenderCommand<wmtcmd_render_executecommands>();
  cmd.type = WMTRenderCommandExecuteCommandsInBuffer;
  cmd.indirect_command_buffer = icb;
  cmd.location = 0;
  cmd.length = MaxCount;

  icb_.push_back(std::move(icb));

  return data;
}

template <>
WMT::Reference<WMT::ComputePipelineState>
SimpleCommandContext<MTLD3D12CommandAllocatorImpl>::getComputePipeline(std::string name) {
  auto lib = ctx.device_->GetLib().getLibrary();
  auto func = lib.newFunction(name.c_str());
  if (!func)
    return {};
  WMT::Reference<WMT::Error> err;
  auto pso = ctx.device_->GetMTLDevice().newComputePipelineState(func, err);
  if (err) {
    ERR("Failed to create compute PSO: ", err.description().getUTF8String());
  }
  return pso;
}

template <>
void
SimpleCommandContext<MTLD3D12CommandAllocatorImpl>::startComputePass() {
  ctx.InvalidateCurrentPass();
  auto compute = ctx.AllocatePass<ComputeEncoderData>();
  compute->type = EncoderType::Compute;
  compute->cmd_head.type = WMTComputeCommandNop;
  compute->cmd_head.next.set(0);
  compute->cmd_tail = (wmtcmd_base *)&compute->cmd_head;
}

template <>
void
SimpleCommandContext<MTLD3D12CommandAllocatorImpl>::endPass() {
  ctx.InvalidateCurrentPass();
}

template <>
void
SimpleCommandContext<MTLD3D12CommandAllocatorImpl>::setComputePSO(WMT::ComputePipelineState pso, WMTSize tgsize) {
  auto &setpso = ctx.EncodeComputeCommand<wmtcmd_compute_setpso>();
  setpso.type = WMTComputeCommandSetPSO;
  setpso.pso = pso;
  setpso.threadgroup_size = tgsize;
}

template <>
void
SimpleCommandContext<MTLD3D12CommandAllocatorImpl>::dispatch(WMTSize size) {
  auto &dispatch = ctx.EncodeComputeCommand<wmtcmd_compute_dispatch>();
  dispatch.type = WMTComputeCommandDispatchThreads;
  dispatch.size = size;
}

template <>
void
SimpleCommandContext<MTLD3D12CommandAllocatorImpl>::setComputeTexture(
    uint32_t index, const Rc<Texture> &texture, uint64_t viewId, int flags
) {
  auto &dst_ = texture->view(viewId);
  auto &settex = ctx.EncodeComputeCommand<wmtcmd_compute_settexture>();
  settex.type = WMTComputeCommandSetTexture;
  settex.texture = dst_.texture;
  settex.index = index;
}

template <>
void
SimpleCommandContext<MTLD3D12CommandAllocatorImpl>::setComputeTexelBuffer(
    uint32_t index, const Rc<Buffer> &buffer, uint64_t viewId, int flags
) {
  auto &dst_ = buffer->view_(viewId);
  auto &settexbuf = ctx.EncodeComputeCommand<wmtcmd_compute_settexture>();
  settexbuf.type = WMTComputeCommandSetTexture;
  settexbuf.texture = dst_.texture;
  settexbuf.index = index;
}

template <>
void
SimpleCommandContext<MTLD3D12CommandAllocatorImpl>::setComputeBuffer(
    uint32_t index, const Rc<Buffer> &buffer, uint32_t offset, uint32_t length, int flags
) {
  auto dst_ = buffer->current();
  auto &setbuf = ctx.EncodeComputeCommand<wmtcmd_compute_setbuffer>();
  setbuf.type = WMTComputeCommandSetBuffer;
  setbuf.buffer = dst_->buffer();
  setbuf.index = index;
  setbuf.offset = 0; // the `offset` and `length` parameter of this function are just for (potential) hazard tracking,
                     // which we don't need here
}

template <>
void *
SimpleCommandContext<MTLD3D12CommandAllocatorImpl>::setComputeBytes(uint32_t index, uint32_t length) {
  auto &setmeta = ctx.EncodeComputeCommand<wmtcmd_compute_setbytes>();
  setmeta.type = WMTComputeCommandSetBytes;
  void *temp = ctx.AllocateCPUHeap(length, 16);
  setmeta.bytes.set(temp);
  setmeta.length = length;
  setmeta.index = index;
  return temp;
}

}; // namespace dxmt