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
#include "dxmt_format.hpp"

namespace dxmt {

enum class DirtyState {
  VertexBuffer,
  GraphicsRootArguments,
  GraphicsRootSignature,
  Viewport,
  ScissorRect,
};

enum class DrawCallStatus {
  Invalid,
  Ordinary,
};

inline bool
to_metal_primitive_type(D3D12_PRIMITIVE_TOPOLOGY topo, WMTPrimitiveType &primitive, uint32_t &control_point_num) {
  control_point_num = 0;
  switch (topo) {
  case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
    primitive = WMTPrimitiveTypePoint;
    break;
  case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
    primitive = WMTPrimitiveTypeLine;
    break;
  case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
    primitive = WMTPrimitiveTypeLineStrip;
    break;
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
    primitive = WMTPrimitiveTypeTriangle;
    break;
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
    primitive = WMTPrimitiveTypeTriangleStrip;
    break;
  case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
  case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
    // geometry
    primitive = WMTPrimitiveTypePoint;
    break;
  case D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST:
    primitive = WMTPrimitiveTypePoint;
    control_point_num = topo - 32;
    break;
  default:
    return false;
  }
  return true;
}

/* FIXME: it's not *public* */
unsigned getPlanarCount(WMTPixelFormat format);

// `Graphics`CommandList is a really confusing name
class MTLD3D12GraphicsCommandListImpl : public MTLD3D12DeviceChild<MTLD3D12GraphicsCommandList> {

  Com<MTLD3D12CommandAllocatorImpl, false> allocator_;

  /* state */

  Flags<DirtyState> dirty_state_;

  std::array<D3D12_VERTEX_BUFFER_VIEW, D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> vertex_buffers_;

  uint64_t index_buffer_address;
  WMT::Buffer index_buffer;
  WMTIndexType index_type;
  uint64_t index_offset;

  UINT num_rtvs;
  D3D12_CPU_DESCRIPTOR_HANDLE rtvs[8];
  D3D12_CPU_DESCRIPTOR_HANDLE dsv;

  D3D12_PRIMITIVE_TOPOLOGY topology_;

  UINT num_viewports;
  D3D12_VIEWPORT
  viewports[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {{}};

  UINT num_scissors;
  D3D12_RECT
  scissors[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {{}};

  Com<MTLD3D12GraphicsPipelineState, false> pso_graphics_;
  Com<MTLD3D12RootSignature, false> rootsig_graphics_;
  uint64_t rootarg_graphics_staging_[64];

public:
  MTLD3D12GraphicsCommandListImpl(MTLD3D12Device *pDevice) : MTLD3D12DeviceChild<MTLD3D12GraphicsCommandList>(pDevice) {}

  ~MTLD3D12GraphicsCommandListImpl() {}

  HRESULT
  Initialize(ID3D12CommandAllocator *pAllocator, ID3D12PipelineState *pInitialPipelineState) {
    auto allocator = static_cast<MTLD3D12CommandAllocatorImpl *>(pAllocator);

    if (allocator_ != allocator)
      allocator_ = allocator;

    pso_graphics_ = nullptr;
    if (auto pso = static_cast<MTLD3D12PipelineState *>(pInitialPipelineState)) {
      if (!pso->IsComputePipelineState)
        pso_graphics_ = static_cast<MTLD3D12GraphicsPipelineState *>(pInitialPipelineState);
    }

    num_rtvs = {};
    memset(rtvs, 0, sizeof(rtvs));
    dsv = {};

    topology_ = {};

    num_viewports = {};
    memset(viewports, 0, sizeof(viewports));

    num_scissors = {};
    memset(scissors, 0, sizeof(scissors));

    rootsig_graphics_ = nullptr;
    memset(rootarg_graphics_staging_, 0, sizeof(rootarg_graphics_staging_));

    memset(vertex_buffers_.data(), 0, sizeof(vertex_buffers_));

    index_buffer_address = 0;
    index_buffer = {};
    index_type = {};
    index_offset = 0;

    dirty_state_.clrAll();

    encoder_count = std::numeric_limits<size_t>::max();
    return allocator_->StartRecord(&entry);
  }

  HRESULT
  STDMETHODCALLTYPE
  QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D12Object) || riid == __uuidof(ID3D12DeviceChild) ||
        riid == __uuidof(ID3D12CommandList) || riid == __uuidof(ID3D12GraphicsCommandList)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D12GraphicsCommandList), riid)) {
      WARN("D3D12GraphicsCommandList: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }

  D3D12_COMMAND_LIST_TYPE STDMETHODCALLTYPE
  GetType() {
    return D3D12_COMMAND_LIST_TYPE_DIRECT;
  }

  HRESULT STDMETHODCALLTYPE
  Close() {
    if (encoder_count < std::numeric_limits<size_t>::max())
      return E_FAIL;
    return allocator_->EndRecord(&encoder_count);
  };

  HRESULT STDMETHODCALLTYPE
  Reset(ID3D12CommandAllocator *pAllocator, ID3D12PipelineState *pInitialState) {
    if (encoder_count == std::numeric_limits<size_t>::max())
      return E_FAIL;
    return Initialize(pAllocator, pInitialState);
  };

  void STDMETHODCALLTYPE ClearState(ID3D12PipelineState *pPipelineState) { IMPLEMENT_ME };

  std::tuple<uint64_t, uint64_t>
  PopulateVertexBufferTable(uint32_t Count) {
    auto slot_mask = pso_graphics_ ? pso_graphics_->slot_mask : 0;
    if (!slot_mask)
      return {0, 0};
    uint32_t max_slot = 32 - __builtin_clz(slot_mask);
    struct VERTEX_BUFFER_ENTRY {
      uint64_t buffer_handle;
      uint32_t stride;
      uint32_t length;
    };
    auto stride = align(sizeof(VERTEX_BUFFER_ENTRY) * max_slot, 16);

    auto [mapped, offset] = allocator_->AllocateGPUHeap(stride * Count, 16);

    for (unsigned i = 0; i < Count; i++) {
      VERTEX_BUFFER_ENTRY *entries = (VERTEX_BUFFER_ENTRY *)(reinterpret_cast<char *>(mapped) + i * stride);
      for (unsigned slot = 0, index = 0; slot < max_slot; slot++) {
        if (!(slot_mask & (1 << slot)))
          continue;
        auto &state = vertex_buffers_[slot];
        entries[index].buffer_handle = state.BufferLocation;
        entries[index].stride = state.StrideInBytes;
        entries[index++].length = state.SizeInBytes;
      };
    }

    return {offset, stride};
  }

  void
  EncodeVertexBuffers() {
    auto [Offset, Stride] = PopulateVertexBufferTable(1);
    if (!Stride)
      return;

    auto &cmd = allocator_->EncodeRenderCommand<wmtcmd_render_setbuffer>();
    cmd.type = WMTRenderCommandSetVertexBuffer;
    cmd.buffer = allocator_->gpu_heap_buffer_;
    cmd.offset = Offset;
    cmd.index = SM50_BINDING_INDEX_VERTEX_BUFFER;
  }

  DrawCallStatus
  PreDraw() {
    if (!allocator_->encoder_current || allocator_->encoder_current->type != EncoderType::Render) {

      allocator_->InvalidateCurrentPass();
      auto render = allocator_->AllocatePass<RenderEncoderData>();
      render->type = EncoderType::Render;
      render->cmd_head.type = WMTRenderCommandNop;
      render->cmd_head.next.set(0);
      render->cmd_tail = (wmtcmd_base *)&render->cmd_head;
      render->dsv_planar_flags = 0;
      render->dsv_readonly_flags = 0;
      render->render_target_count = num_rtvs;

      unsigned render_target_width = 16384, render_target_height = 16384, render_target_array_length = 0;

      unsigned effective_rtvs = 0;
      for (unsigned i = 0; i < num_rtvs; i++) {
        if (!rtvs[i].ptr)
          continue;
        effective_rtvs++;
        auto [Heap, Index] = GetRenderTargetHeap(device_, rtvs[i]);
        auto AttachmentDesc = Heap->GetRenderTarget(Index);
        if (!AttachmentDesc.Texture)
          continue;
        auto &rt = render->colors[i];
        rt.attachment = AttachmentDesc.Texture->view(AttachmentDesc.View);
        rt.depth_plane = AttachmentDesc.DepthPlane;
        rt.load_action = WMTLoadActionLoad;
        rt.store_action = WMTStoreActionStore;
        render_target_width = std::min(render_target_width, AttachmentDesc.Width);
        render_target_height = std::min(render_target_height, AttachmentDesc.Height);
        render_target_array_length = std::max(render_target_array_length, AttachmentDesc.RenderTargetArrayLength);
      }
      while (dsv.ptr) {
        effective_rtvs++;
        auto [Heap, Index] = GetRenderTargetHeap(device_, dsv);
        auto AttachmentDesc = Heap->GetRenderTarget(Index);
        if (!AttachmentDesc.Texture)
          continue;
        auto dsv_planar_flags = DepthStencilPlanarFlags(AttachmentDesc.Texture->pixelFormat(AttachmentDesc.View));
        if (dsv_planar_flags & 1) {
          auto &rt = render->depth;
          rt.attachment = AttachmentDesc.Texture->view(AttachmentDesc.View);
          rt.depth_plane = 0;
          rt.load_action = WMTLoadActionLoad;
          rt.store_action = WMTStoreActionStore;
        }
        if (dsv_planar_flags & 2) {
          auto &rt = render->stencil;
          rt.attachment = AttachmentDesc.Texture->view(AttachmentDesc.View);
          rt.depth_plane = AttachmentDesc.DepthPlane;
          rt.load_action = WMTLoadActionLoad;
          rt.store_action = WMTStoreActionStore;
        }
        render->dsv_planar_flags = dsv_planar_flags;
        render_target_width = std::min(render_target_width, AttachmentDesc.Width);
        render_target_height = std::min(render_target_height, AttachmentDesc.Height);
        render_target_array_length = std::max(render_target_array_length, AttachmentDesc.RenderTargetArrayLength);
        break;
      }
      render->render_target_width = render_target_width;
      render->render_target_height = render_target_height;
      render->render_target_array_length = render_target_array_length;
      if (effective_rtvs == 0) {
        IMPLEMENT_ME
        // render->default_raster_sample_count = std::max(1u, forced_sample_count);
      }

      if (pso_graphics_) {
        UpdateGraphicsPSO(pso_graphics_.ptr());
      }
      dirty_state_.set(DirtyState::VertexBuffer, DirtyState::GraphicsRootArguments, DirtyState::GraphicsRootSignature);
      dirty_state_.set(DirtyState::Viewport, DirtyState::ScissorRect);
    }
    if (dirty_state_.test(DirtyState::VertexBuffer)) {
      EncodeVertexBuffers();
      dirty_state_.clr(DirtyState::VertexBuffer);
    }

    if (dirty_state_.test(DirtyState::GraphicsRootArguments)) {
      if (rootsig_graphics_) {
        auto [Ptr, Offset] = allocator_->AllocateGPUHeap(sizeof(uint64_t) * rootsig_graphics_->UploadQwords, 64);
        memcpy(Ptr, rootarg_graphics_staging_, rootsig_graphics_->UploadQwords * sizeof(uint64_t));
        auto &cmd_vsargbuf = allocator_->EncodeRenderCommand<wmtcmd_render_setbuffer>();
        cmd_vsargbuf.type = WMTRenderCommandSetVertexBuffer;
        cmd_vsargbuf.buffer = allocator_->gpu_heap_buffer_;
        cmd_vsargbuf.offset = Offset;
        cmd_vsargbuf.index = SM50_BINDING_INDEX_ROOT_ARGUMENTS;
        auto &cmd_fsargbuf = allocator_->EncodeRenderCommand<wmtcmd_render_setbuffer>();
        cmd_fsargbuf.type = WMTRenderCommandSetFragmentBuffer;
        cmd_fsargbuf.buffer = allocator_->gpu_heap_buffer_;
        cmd_fsargbuf.offset = Offset;
        cmd_fsargbuf.index = SM50_BINDING_INDEX_ROOT_ARGUMENTS;
      }
      dirty_state_.clr(DirtyState::GraphicsRootArguments);
    }

    if (dirty_state_.test(DirtyState::GraphicsRootSignature)) {
      if (rootsig_graphics_) {
        auto static_sampler_encode_size = sizeof(uint64_t) * rootsig_graphics_->NumStaticSamplers * 4;
        auto [Ptr, Offset] = allocator_->AllocateGPUHeap(static_sampler_encode_size, 64);
        memcpy(Ptr, rootsig_graphics_->EncodedStaticSamplers, static_sampler_encode_size);
        auto &cmd_vsargbuf = allocator_->EncodeRenderCommand<wmtcmd_render_setbuffer>();
        cmd_vsargbuf.type = WMTRenderCommandSetVertexBuffer;
        cmd_vsargbuf.buffer = allocator_->gpu_heap_buffer_;
        cmd_vsargbuf.offset = Offset;
        cmd_vsargbuf.index = SM50_BINDING_INDEX_STATIC_SAMPLERS;
        auto &cmd_fsargbuf = allocator_->EncodeRenderCommand<wmtcmd_render_setbuffer>();
        cmd_fsargbuf.type = WMTRenderCommandSetFragmentBuffer;
        cmd_fsargbuf.buffer = allocator_->gpu_heap_buffer_;
        cmd_fsargbuf.offset = Offset;
        cmd_fsargbuf.index = SM50_BINDING_INDEX_STATIC_SAMPLERS;
      }
      dirty_state_.clr(DirtyState::GraphicsRootSignature);
    }

    if (dirty_state_.test(DirtyState::Viewport)) {
      auto metal_viewport = allocator_->AllocateCommandData<WMTViewport>(num_viewports);
      for (auto i = 0u; i < num_viewports; i++) {
        auto &viewport = viewports[i];
        metal_viewport[i] = {viewport.TopLeftX, viewport.TopLeftY, viewport.Width,
                             viewport.Height,   viewport.MinDepth, viewport.MaxDepth};
      }
      auto &cmd = allocator_->EncodeRenderCommand<wmtcmd_render_setviewports>();
      cmd.type = WMTRenderCommandSetViewports;
      cmd.viewports.set(metal_viewport);
      cmd.viewport_count = num_viewports;
      dirty_state_.clr(DirtyState::Viewport);
    }

    if (dirty_state_.test(DirtyState::ScissorRect)) {
      auto metal_scissors = allocator_->AllocateCommandData<WMTScissorRect>(num_viewports /* yes */);
      for (auto i = 0u; i < num_viewports; i++) {
        if (i < num_scissors) {
          auto &d3d_rect = scissors[i];
          LONG left = std::clamp(d3d_rect.left, (LONG)0, (LONG)16384);
          LONG top = std::clamp(d3d_rect.top, (LONG)0, (LONG)16384);
          LONG right = std::clamp(d3d_rect.right, left, (LONG)16384);
          LONG bottom = std::clamp(d3d_rect.bottom, top, (LONG)16384);
          metal_scissors[i] = {uint32_t(left), uint32_t(top), uint32_t(right - left), uint32_t(bottom - top)};
        } else {
          metal_scissors[i] = {0, 0, 16384, 16384};
        }
      }
      auto &cmd = allocator_->EncodeRenderCommand<wmtcmd_render_setscissorrects>();
      cmd.type = WMTRenderCommandSetScissorRects;
      cmd.scissor_rects.set(metal_scissors);
      cmd.rect_count = num_viewports;
      dirty_state_.clr(DirtyState::ScissorRect);
    }
    return DrawCallStatus::Ordinary;
  }

  void STDMETHODCALLTYPE
  DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation) {
    WMTPrimitiveType primitive_type;
    uint32_t cp_count;
    if (!to_metal_primitive_type(topology_, primitive_type, cp_count))
      return;
    DrawCallStatus status = PreDraw();
    if (status == DrawCallStatus::Invalid)
      return;

    auto &cmd_draw = allocator_->EncodeRenderCommand<wmtcmd_render_draw>();
    cmd_draw.type = WMTRenderCommandDraw;
    cmd_draw.primitive_type = primitive_type;
    cmd_draw.base_instance = StartInstanceLocation;
    cmd_draw.instance_count = InstanceCount;
    cmd_draw.vertex_start = StartVertexLocation;
    cmd_draw.vertex_count = VertexCountPerInstance;
  };

  void STDMETHODCALLTYPE
  DrawIndexedInstanced(
      UINT IndexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, INT BaseVertexLocation,
      UINT StartInstanceLocation
  ) {
    WMTPrimitiveType primitive_type;
    uint32_t cp_count;
    if (!to_metal_primitive_type(topology_, primitive_type, cp_count))
      return;
    DrawCallStatus status = PreDraw();
    if (status == DrawCallStatus::Invalid)
      return;
    auto &cmd_draw = allocator_->EncodeRenderCommand<wmtcmd_render_draw_indexed>();
    cmd_draw.type = WMTRenderCommandDrawIndexed;
    cmd_draw.primitive_type = primitive_type;
    cmd_draw.index_type = index_type;
    cmd_draw.index_count = IndexCountPerInstance;
    cmd_draw.index_buffer = index_buffer;
    cmd_draw.index_buffer_offset = index_offset + StartVertexLocation * (index_type == WMTIndexTypeUInt32 ? 4 : 2);
    cmd_draw.instance_count = InstanceCount;
    cmd_draw.base_vertex = BaseVertexLocation;
    cmd_draw.base_instance = StartInstanceLocation;
  };

  void STDMETHODCALLTYPE Dispatch(UINT X, UINT Y, UINT Z) { IMPLEMENT_ME };

  bool
  PreBlit() {
    if (!allocator_->encoder_current || allocator_->encoder_current->type != EncoderType::Blit) {
      allocator_->InvalidateCurrentPass();
      auto render = allocator_->AllocatePass<BlitEncoderData>();
      render->type = EncoderType::Blit;
      render->cmd_head.type = WMTBlitCommandNop;
      render->cmd_head.next.set(0);
      render->cmd_tail = (wmtcmd_base *)&render->cmd_head;
    }
    return true;
  }

  void STDMETHODCALLTYPE
  CopyBufferRegion(
      ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT64 ByteCount
  ) {
    if (!pDstBuffer || !pSrcBuffer)
      return;
    if (!PreBlit())
      return;

    auto &cmd_cp = allocator_->EncodeBlitCommand<wmtcmd_blit_copy_from_buffer_to_buffer>();
    cmd_cp.type = WMTBlitCommandCopyFromBufferToBuffer;
    cmd_cp.src = static_cast<MTLD3D12Resource *>(pSrcBuffer)->buffer->current()->buffer();
    cmd_cp.dst = static_cast<MTLD3D12Resource *>(pDstBuffer)->buffer->current()->buffer();
    cmd_cp.src_offset = SrcOffset;
    cmd_cp.dst_offset = DstOffset;
    cmd_cp.copy_length = ByteCount;
  };

  void STDMETHODCALLTYPE
  CopyTextureRegion(
      const D3D12_TEXTURE_COPY_LOCATION *pDst, UINT DstX, UINT DstY, UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION *pSrc,
      const D3D12_BOX *pSrcBox
  ) {
    if (!pDst || !pSrc)
      return;
    if (!PreBlit())
      return;

    if (pDst->Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX) {
      auto &dst = static_cast<MTLD3D12Resource *>(pDst->pResource)->texture;
      if (!dst)
        return;

      auto dst_planar_count = getPlanarCount(dst->pixelFormat());

      if (pSrc->Type == D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT) {
        auto &src = static_cast<MTLD3D12Resource *>(pSrc->pResource)->buffer;
        if (!src)
          return;
        if (pSrcBox)
          IMPLEMENT_ME

        auto &cmd_cp = allocator_->EncodeBlitCommand<wmtcmd_blit_copy_from_buffer_to_texture_withblitoption>();
        cmd_cp.type = WMTBlitCommandCopyFromBufferToTexture;
        cmd_cp.src = src->current()->buffer();
        cmd_cp.src_offset = pSrc->PlacedFootprint.Offset;
        cmd_cp.bytes_per_row = pSrc->PlacedFootprint.Footprint.RowPitch;
        cmd_cp.bytes_per_image = 0;
        cmd_cp.size = {
            pSrc->PlacedFootprint.Footprint.Width, pSrc->PlacedFootprint.Footprint.Height,
            pSrc->PlacedFootprint.Footprint.Depth
        };
        cmd_cp.dst = dst->current()->texture();
        cmd_cp.level = pDst->SubresourceIndex % dst->miplevelCount();
        cmd_cp.slice = pDst->SubresourceIndex / dst->miplevelCount();
        cmd_cp.options = dst_planar_count ? (pSrc->SubresourceIndex ? WMTBlitOptionStencilFromDepthStencil
                                                                    : WMTBlitOptionDepthFromDepthStencil)
                                          : WMTBlitOptionNone;
        cmd_cp.origin = {DstX, DstY, DstZ};
      } else {
        auto &src = static_cast<MTLD3D12Resource *>(pSrc->pResource)->texture;
        if (!src)
          return;
        auto src_planar_count = getPlanarCount(src->pixelFormat());
        if (!pSrcBox)
          IMPLEMENT_ME

        // copy between depth-stencil texture is tricky
        if (dst_planar_count > 1 || src_planar_count > 1)
          IMPLEMENT_ME

        auto &cmd_cp = allocator_->EncodeBlitCommand<wmtcmd_blit_copy_from_texture_to_texture>();
        cmd_cp.type = WMTBlitCommandCopyFromTextureToTexture;
        cmd_cp.src = src->current()->texture();
        cmd_cp.src_level = pSrc->SubresourceIndex % src->miplevelCount();
        cmd_cp.src_slice = pSrc->SubresourceIndex / src->miplevelCount();
        cmd_cp.src_origin = {pSrcBox->left, pSrcBox->top, pSrcBox->front};
        cmd_cp.src_size = {
            pSrcBox->right - pSrcBox->left, pSrcBox->bottom - pSrcBox->top, pSrcBox->back - pSrcBox->front
        };
        cmd_cp.dst = dst->current()->texture();
        cmd_cp.dst_level = pDst->SubresourceIndex % dst->miplevelCount();
        cmd_cp.dst_slice = pDst->SubresourceIndex / dst->miplevelCount();
        cmd_cp.dst_origin = {DstX, DstY, DstZ};
      }
    } else if (pDst->Type == D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT) {
      auto &dst = static_cast<MTLD3D12Resource *>(pDst->pResource)->buffer;
      if (!dst)
        return;
      if (pSrcBox)
        IMPLEMENT_ME
      if (pSrc->Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX) {
        auto &src = static_cast<MTLD3D12Resource *>(pSrc->pResource)->texture;
        if (!src)
          return;

        auto &cmd_cp = allocator_->EncodeBlitCommand<wmtcmd_blit_copy_from_texture_to_buffer>();
        cmd_cp.type = WMTBlitCommandCopyFromTextureToBuffer;
        cmd_cp.src = src->current()->texture();
        cmd_cp.level = pSrc->SubresourceIndex % src->miplevelCount();
        cmd_cp.slice = pSrc->SubresourceIndex / src->miplevelCount();
        cmd_cp.origin = {0, 0, 0};
        cmd_cp.size = {
            pDst->PlacedFootprint.Footprint.Width, pDst->PlacedFootprint.Footprint.Height,
            pDst->PlacedFootprint.Footprint.Depth
        };
        cmd_cp.dst = dst->current()->buffer();
        cmd_cp.offset = pDst->PlacedFootprint.Offset;
        cmd_cp.bytes_per_row = pDst->PlacedFootprint.Footprint.RowPitch;
        cmd_cp.bytes_per_image = 0;
      } else {
        // so it is buffer to buffer copy?
        IMPLEMENT_ME
      }
    }
  };

  void STDMETHODCALLTYPE CopyResource(ID3D12Resource *pDstResource, ID3D12Resource *pSrcResource) { IMPLEMENT_ME };

  void STDMETHODCALLTYPE CopyTiles(
      ID3D12Resource *tiled_resource, const D3D12_TILED_RESOURCE_COORDINATE *tile_region_start_coordinate,
      const D3D12_TILE_REGION_SIZE *tile_region_size, ID3D12Resource *buffer, UINT64 buffer_offset,
      D3D12_TILE_COPY_FLAGS flags
  ) {
    IMPLEMENT_ME
  };

  void STDMETHODCALLTYPE ResolveSubresource(
      ID3D12Resource *pDstResource, UINT DstSubresource, ID3D12Resource *pSrcResource, UINT SrcSubresource,
      DXGI_FORMAT Format
  ) {
    IMPLEMENT_ME
  };

  void STDMETHODCALLTYPE
  IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY Topology) {
    topology_ = Topology;
  };

  void STDMETHODCALLTYPE
  RSSetViewports(UINT NumViewports, const D3D12_VIEWPORT *pViewports) {
    if (NumViewports > D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)
      return;
    num_viewports = NumViewports;
    for (auto i = 0u; i < NumViewports; i++) {
      viewports[i] = pViewports[i];
    }
    dirty_state_.set(DirtyState::Viewport);
  };

  void STDMETHODCALLTYPE
  RSSetScissorRects(UINT NumRects, const D3D12_RECT *rects) {
    if (NumRects > D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)
      return;
    num_scissors = NumRects;
    for (auto i = 0u; i < NumRects; i++) {
      scissors[i] = rects[i];
    }
    dirty_state_.set(DirtyState::ScissorRect);
  };

  void STDMETHODCALLTYPE OMSetBlendFactor(const FLOAT BlendFactors[4]) { IMPLEMENT_ME };

  void STDMETHODCALLTYPE OMSetStencilRef(UINT StencilRef) { IMPLEMENT_ME };

  void
  UpdateGraphicsPSO(MTLD3D12GraphicsPipelineState *pso_graphics) {
    auto &cmd_setpso = allocator_->EncodeRenderCommand<wmtcmd_render_setpso>();
    cmd_setpso.type = WMTRenderCommandSetPSO;
    cmd_setpso.pso = pso_graphics->pso;

    auto &cmd_setdsso = allocator_->EncodeRenderCommand<wmtcmd_render_setdsso>();
    cmd_setdsso.type = WMTRenderCommandSetDSSO;
    cmd_setdsso.dsso = pso_graphics->dsso;
    cmd_setdsso.stencil_ref = 0; /* FIXME */

    auto &cmd_setrs = allocator_->EncodeRenderCommand<wmtcmd_render_setrasterizerstate>();
    cmd_setrs.type = WMTRenderCommandSetRasterizerState;
    cmd_setrs.cull_mode = pso_graphics->cull_mode;
    cmd_setrs.depth_clip_mode = pso_graphics->depth_clip_mode;
    cmd_setrs.fill_mode = pso_graphics->fill_mode;
    cmd_setrs.depth_bias = pso_graphics->depth_bias;
    cmd_setrs.depth_bias_clamp = pso_graphics->depth_bias_clamp;
    cmd_setrs.scole_scale = pso_graphics->scole_scale;
    cmd_setrs.winding = pso_graphics->winding;
  }

  void STDMETHODCALLTYPE
  SetPipelineState(ID3D12PipelineState *pPSO) {
    if (!pPSO) {
      pso_graphics_ = nullptr;
      return;
    }
    auto graphics_pso = static_cast<MTLD3D12GraphicsPipelineState *>(pPSO);
    if (pso_graphics_.ptr() == graphics_pso)
      return;
    pso_graphics_ = graphics_pso;
    if (!allocator_->encoder_current || allocator_->encoder_current->type != EncoderType::Render)
      return;

    UpdateGraphicsPSO(graphics_pso);
    dirty_state_.set(DirtyState::VertexBuffer);
  };

  void STDMETHODCALLTYPE ResourceBarrier(UINT Count, const D3D12_RESOURCE_BARRIER *barriers) {
    // TODO: in the initial implementation, we force synchronize everything and ignore barriers (which can be used as
    // optimization hints later)
  };

  void STDMETHODCALLTYPE ExecuteBundle(ID3D12GraphicsCommandList *CommandList) { IMPLEMENT_ME };

  void STDMETHODCALLTYPE SetDescriptorHeaps(UINT HeapCount, ID3D12DescriptorHeap *const *Heaps) {
    // no need to do anything here because because we encode the full descriptor table address in root argument
  };

  void STDMETHODCALLTYPE SetComputeRootSignature(ID3D12RootSignature *pRootSignature) { IMPLEMENT_ME };

  void STDMETHODCALLTYPE
  SetGraphicsRootSignature(ID3D12RootSignature *pRootSignature) {
    if (rootsig_graphics_.ptr() == pRootSignature)
      return;
    if (pRootSignature) {
      rootsig_graphics_ = static_cast<MTLD3D12RootSignature *>(pRootSignature);
      assert(rootsig_graphics_->UploadQwords < std::size(rootarg_graphics_staging_));
    } else {
      rootsig_graphics_ = nullptr;
    }
    dirty_state_.set(DirtyState::GraphicsRootArguments, DirtyState::GraphicsRootSignature);
  };

  void STDMETHODCALLTYPE SetComputeRootDescriptorTable(UINT Index, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) {
    IMPLEMENT_ME
  };

  void STDMETHODCALLTYPE
  SetGraphicsRootDescriptorTable(UINT Index, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) {
    if (!rootsig_graphics_)
      return;
    if (Index > rootsig_graphics_->ParameterSlots)
      return;
    rootarg_graphics_staging_[rootsig_graphics_->SlotQwordOffsets[Index]] = BaseDescriptor.ptr;
    dirty_state_.set(DirtyState::GraphicsRootArguments);
  };

  void STDMETHODCALLTYPE SetComputeRoot32BitConstant(UINT Index, UINT Data, UINT DstOffset) { IMPLEMENT_ME };

  void STDMETHODCALLTYPE
  SetGraphicsRoot32BitConstant(UINT Index, UINT Data, UINT DstOffset) {
    if (!rootsig_graphics_)
      return;
    if (Index > rootsig_graphics_->ParameterSlots)
      return;
    auto dst = reinterpret_cast<uint32_t *>(rootarg_graphics_staging_ + rootsig_graphics_->SlotQwordOffsets[Index]);
    dst[DstOffset] = Data;
    dirty_state_.set(DirtyState::GraphicsRootArguments);
  };

  void STDMETHODCALLTYPE
  SetComputeRoot32BitConstants(UINT Index, UINT ConstantCount, const void *pData, UINT DstOffset) {
    IMPLEMENT_ME
  };

  void STDMETHODCALLTYPE
  SetGraphicsRoot32BitConstants(UINT Index, UINT ConstantCount, const void *pData, UINT DstOffset) {
    if (!rootsig_graphics_)
      return;
    if (Index > rootsig_graphics_->ParameterSlots)
      return;
    auto src = reinterpret_cast<const uint32_t *>(pData);
    auto dst = reinterpret_cast<uint32_t *>(rootarg_graphics_staging_ + rootsig_graphics_->SlotQwordOffsets[Index]);
    for (unsigned i = 0; i < ConstantCount; i++) {
      dst[i + DstOffset] = src[i];
    }
    dirty_state_.set(DirtyState::GraphicsRootArguments);
  };

  void STDMETHODCALLTYPE SetComputeRootConstantBufferView(UINT Index, D3D12_GPU_VIRTUAL_ADDRESS VA) { IMPLEMENT_ME };

  void STDMETHODCALLTYPE
  SetGraphicsRootConstantBufferView(UINT Index, D3D12_GPU_VIRTUAL_ADDRESS VA) {
    if (!rootsig_graphics_)
      return;
    if (Index > rootsig_graphics_->ParameterSlots)
      return;
    rootarg_graphics_staging_[rootsig_graphics_->SlotQwordOffsets[Index]] = VA;
    dirty_state_.set(DirtyState::GraphicsRootArguments);
  };

  void STDMETHODCALLTYPE SetComputeRootShaderResourceView(UINT Index, D3D12_GPU_VIRTUAL_ADDRESS VA) { IMPLEMENT_ME };

  void STDMETHODCALLTYPE
  SetGraphicsRootShaderResourceView(UINT Index, D3D12_GPU_VIRTUAL_ADDRESS VA) {
    if (!rootsig_graphics_)
      return;
    if (Index > rootsig_graphics_->ParameterSlots)
      return;
    rootarg_graphics_staging_[rootsig_graphics_->SlotQwordOffsets[Index]] = VA;
    dirty_state_.set(DirtyState::GraphicsRootArguments);
  };

  void STDMETHODCALLTYPE SetComputeRootUnorderedAccessView(UINT Index, D3D12_GPU_VIRTUAL_ADDRESS VA) { IMPLEMENT_ME };

  void STDMETHODCALLTYPE
  SetGraphicsRootUnorderedAccessView(UINT Index, D3D12_GPU_VIRTUAL_ADDRESS VA) {
    if (!rootsig_graphics_)
      return;
    if (Index > rootsig_graphics_->ParameterSlots)
      return;
    rootarg_graphics_staging_[rootsig_graphics_->SlotQwordOffsets[Index]] = VA;
    dirty_state_.set(DirtyState::GraphicsRootArguments);
  };

  void STDMETHODCALLTYPE
  IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW *pView) {
    if (pView) {
      index_buffer_address = pView->BufferLocation;
      index_buffer = device_->LookupBufferByVA(pView->BufferLocation, &index_offset)->buffer();
      index_type = pView->Format == DXGI_FORMAT_R32_UINT ? WMTIndexTypeUInt32 : WMTIndexTypeUInt16;
    } else {
      index_buffer_address = 0;
      index_buffer = {};
      index_type = {};
      index_offset = {};
    }
  };

  void STDMETHODCALLTYPE
  IASetVertexBuffers(UINT StartSlot, UINT Count, const D3D12_VERTEX_BUFFER_VIEW *Views) {
    for (unsigned Slot = StartSlot; Slot < StartSlot + Count; Slot++) {
      vertex_buffers_[Slot] = Views ? Views[Slot - StartSlot] : D3D12_VERTEX_BUFFER_VIEW{};
    }
    dirty_state_.set(DirtyState::VertexBuffer);
  };

  void STDMETHODCALLTYPE SOSetTargets(UINT StartSlot, UINT Count, const D3D12_STREAM_OUTPUT_BUFFER_VIEW *Views) {
    IMPLEMENT_ME
  };

  void STDMETHODCALLTYPE
  OMSetRenderTargets(
      UINT NumRTV, const D3D12_CPU_DESCRIPTOR_HANDLE *RTVs, WINBOOL SingleDescriptor,
      const D3D12_CPU_DESCRIPTOR_HANDLE *DSV
  ) {
    allocator_->InvalidateCurrentPass();

    num_rtvs = NumRTV;
    for (unsigned i = 0; i < NumRTV; i++) {
      auto RTV = SingleDescriptor ? D3D12_CPU_DESCRIPTOR_HANDLE{RTVs[0].ptr + i * 32 /* kRTVDSVHeapIncrementalSize */}
                                  : RTVs[i];
      rtvs[i] = RTV;
    }
    dsv = DSV ? *DSV : D3D12_CPU_DESCRIPTOR_HANDLE();
  };

  void STDMETHODCALLTYPE
  ClearDepthStencilView(
      D3D12_CPU_DESCRIPTOR_HANDLE DSV, D3D12_CLEAR_FLAGS Flags, FLOAT Depth, UINT8 Stencil, UINT RectCount,
      const D3D12_RECT *Rects
  ) {
    if (Rects || RectCount > 1) {
      ERR("ClearDepthStencilView: unhandled parameter Rects=", Rects, " RectCount=", RectCount);
      return;
    }
    if ((Flags & 3) == 0)
      return;
    auto [Heap, Index] = GetRenderTargetHeap(device_, DSV);
    auto AttachmentDesc = Heap->GetRenderTarget(Index);
    if (!AttachmentDesc.Texture)
      return;
    allocator_->InvalidateCurrentPass();
    auto encoder_info = allocator_->AllocatePass<ClearEncoderData>();
    encoder_info->type = EncoderType::Clear;
    encoder_info->clear_dsv = Flags & 3;
    encoder_info->depth_stencil = {Depth, Stencil};
    encoder_info->attachment = AttachmentDesc.Texture->view(AttachmentDesc.View);
    encoder_info->array_length = AttachmentDesc.RenderTargetArrayLength;
    encoder_info->width = AttachmentDesc.Width;
    encoder_info->height = AttachmentDesc.Height;

    allocator_->InvalidateCurrentPass();
  };

  void STDMETHODCALLTYPE
  ClearRenderTargetView(
      D3D12_CPU_DESCRIPTOR_HANDLE RTV, const FLOAT Color[4], UINT RectCount, const D3D12_RECT *Rects
  ) {
    if (Rects || RectCount > 1) {
      ERR("ClearRenderTargetView: unhandled parameter Rects=", Rects, " RectCount=", RectCount);
      return;
    }
    auto [Heap, Index] = GetRenderTargetHeap(device_, RTV);
    auto AttachmentDesc = Heap->GetRenderTarget(Index);
    if (!AttachmentDesc.Texture)
      return;
    allocator_->InvalidateCurrentPass();
    auto encoder_info = allocator_->AllocatePass<ClearEncoderData>();
    encoder_info->type = EncoderType::Clear;
    encoder_info->clear_dsv = 0;
    encoder_info->color = {Color[0], Color[1], Color[2], Color[3]};
    SanitizeRTVClearColor(AttachmentDesc.Texture->pixelFormat(AttachmentDesc.View), encoder_info->color);
    encoder_info->attachment = AttachmentDesc.Texture->view(AttachmentDesc.View);
    encoder_info->array_length = AttachmentDesc.RenderTargetArrayLength;
    encoder_info->width = AttachmentDesc.Width;
    encoder_info->height = AttachmentDesc.Height;

    allocator_->InvalidateCurrentPass();
  };

  void STDMETHODCALLTYPE ClearUnorderedAccessViewUint(
      D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle, D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle, ID3D12Resource *pResource,
      const UINT Values[4], UINT RectCount, const D3D12_RECT *pRects
  ) {
    IMPLEMENT_ME
  };

  void STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(
      D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle, D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle, ID3D12Resource *pResource,
      const float Values[4], UINT RectCount, const D3D12_RECT *pRects
  ) {
    IMPLEMENT_ME
  };

  void STDMETHODCALLTYPE DiscardResource(ID3D12Resource *pResource, const D3D12_DISCARD_REGION *pRegion) {
    // do nothing for now
  };

  void STDMETHODCALLTYPE
  BeginQuery(ID3D12QueryHeap *pHeap, D3D12_QUERY_TYPE Type, UINT Index) {
    DEBUG("BeginQuery");
  };

  void STDMETHODCALLTYPE
  EndQuery(ID3D12QueryHeap *pHeap, D3D12_QUERY_TYPE Type, UINT Index) {
    DEBUG("EndQuery");
  };

  void STDMETHODCALLTYPE
  ResolveQueryData(
      ID3D12QueryHeap *pHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT QueryCount, ID3D12Resource *pDstBuffer,
      UINT64 AlignedDstBufferOffset
  ) {
    DEBUG("ResolveQueryData");
  };

  void STDMETHODCALLTYPE SetPredication(ID3D12Resource *pBuffer, UINT64 AlignedBufferOffset, D3D12_PREDICATION_OP Op) {
    IMPLEMENT_ME
  };

  void STDMETHODCALLTYPE SetMarker(UINT Metadata, const void *data, UINT size) { IMPLEMENT_ME };

  void STDMETHODCALLTYPE BeginEvent(UINT Metadata, const void *data, UINT size) { IMPLEMENT_ME };

  void STDMETHODCALLTYPE EndEvent() { IMPLEMENT_ME };

  void STDMETHODCALLTYPE ExecuteIndirect(
      ID3D12CommandSignature *pCommandSignature, UINT MaxCommandCount, ID3D12Resource *pArgBuffer,
      UINT64 ArgBufferOffset, ID3D12Resource *pCountBuffer, UINT64 CountBufferOffset
  ) {
    IMPLEMENT_ME
  };
};

HRESULT STDMETHODCALLTYPE
MTLD3D12CommandAllocatorImpl::CreateCommandList(
    UINT NodeMask, D3D12_COMMAND_LIST_TYPE Type, ID3D12PipelineState *pInitialPipelineState, REFIID riid,
    void **ppCommandList
) {
  if (Type != type_)
    return E_INVALIDARG;

  auto cmd_list = Com(new MTLD3D12GraphicsCommandListImpl(device_));
  HRESULT hr = cmd_list->Initialize(this, pInitialPipelineState);
  if (FAILED(hr))
    return hr;
  return cmd_list->QueryInterface(riid, ppCommandList);
}

}; // namespace dxmt