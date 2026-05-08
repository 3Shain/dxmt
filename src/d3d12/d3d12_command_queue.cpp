#include "d3d12_command_queue.hpp"
#include "d3d12_command_list.hpp"
#include "d3d12_descriptor_heap.hpp"
#include "d3d12_device.hpp"
#include "d3d12_pipeline_state.hpp"
#include "d3d12_resource.hpp"
#include "d3d12_root_signature.hpp"
#include "log/log.hpp"
#include "util_string.hpp"
#include "Metal.hpp"

#define QTRACE(fmt, ...) do { FILE *_tf = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a"); if (_tf) { fprintf(_tf, fmt "\n", ##__VA_ARGS__); fclose(_tf); } } while(0)

namespace dxmt {

namespace {

struct ReplayState {
  WMT::CommandBuffer cmdbuf;
  WMT::RenderCommandEncoder render_enc;
  bool render_enc_open = false;

  MTLD3D12PipelineState *pso = nullptr;
  MTLD3D12RootSignature *graphics_root_sig = nullptr;
  D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
  D3D12_VERTEX_BUFFER_VIEW vbs[16] = {};
  D3D12_INDEX_BUFFER_VIEW ib = {};
  D3D12_VIEWPORT viewports[16] = {};
  uint32_t viewport_count = 0;
  D3D12_RECT scissor_rects[16] = {};
  uint32_t scissor_count = 0;
  float blend_factor[4] = {1, 1, 1, 1};
  uint32_t stencil_ref = 0;

  D3D12_CPU_DESCRIPTOR_HANDLE rt_handles[8] = {};
  D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = {};
  uint32_t rt_count = 0;
  bool has_dsv = false;

  ID3D12DescriptorHeap *desc_heaps[2] = {};
  uint32_t desc_heap_count = 0;

  D3D12_GPU_VIRTUAL_ADDRESS root_cbvs[16] = {};
  D3D12_GPU_DESCRIPTOR_HANDLE root_tables[16] = {};
  uint8_t root_constants_buf[16 * 64] = {};
  uint32_t root_constant_offsets[16] = {};
  uint32_t root_constant_sizes[16] = {};
  bool root_constant_set[16] = {};
  bool root_cbv_set[16] = {};
  bool root_table_set[16] = {};

  void CloseRenderEncoder() {
    if (render_enc_open) {
      render_enc.endEncoding();
      render_enc_open = false;
    }
  }

  WMTPrimitiveType GetMetalPrimitiveType() {
    switch (topology) {
    case D3D_PRIMITIVE_TOPOLOGY_POINTLIST: return WMTPrimitiveTypePoint;
    case D3D_PRIMITIVE_TOPOLOGY_LINELIST: return WMTPrimitiveTypeLine;
    case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP: return WMTPrimitiveTypeLineStrip;
    case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST: return WMTPrimitiveTypeTriangle;
    case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP: return WMTPrimitiveTypeTriangleStrip;
    default: return WMTPrimitiveTypeTriangle;
    }
  }

  void EnsureRenderEncoder() {
    if (render_enc_open)
      return;

    if (rt_count == 0) {
      QTRACE("EnsureRenderEncoder: no render targets set, skipping");
      return;
    }

    WMTRenderPassInfo rp = {};
    for (uint32_t i = 0; i < 8; i++) {
      rp.colors[i].texture = NULL_OBJECT_HANDLE;
      rp.colors[i].load_action = WMTLoadActionLoad;
      rp.colors[i].store_action = WMTStoreActionStore;
      rp.colors[i].level = 0;
      rp.colors[i].slice = 0;
    }
    rp.depth.texture = NULL_OBJECT_HANDLE;
    rp.depth.load_action = WMTLoadActionLoad;
    rp.depth.store_action = WMTStoreActionStore;
    rp.stencil.texture = NULL_OBJECT_HANDLE;
    rp.stencil.load_action = WMTLoadActionLoad;
    rp.stencil.store_action = WMTStoreActionStore;

    bool has_valid_rt = false;
    for (uint32_t i = 0; i < rt_count && i < 8; i++) {
      auto *desc = reinterpret_cast<const D3D12Descriptor *>(rt_handles[i].ptr);
      if (desc && desc->resource) {
        auto *res = static_cast<MTLD3D12Resource *>(desc->resource);
        if (res->GetMTLTexture().handle) {
          rp.colors[i].texture = res->GetMTLTexture().handle;
        } else if (res->GetMTLBuffer().handle) {
          WMTTextureInfo tex_info = {};
          tex_info.width = 1;
          tex_info.height = 1;
          tex_info.depth = 1;
          tex_info.array_length = 1;
          tex_info.mipmap_level_count = 1;
          tex_info.sample_count = 1;
          tex_info.type = WMTTextureType2D;
          tex_info.usage = WMTTextureUsageRenderTarget;
          tex_info.pixel_format = WMTPixelFormatBGRA8Unorm;
          tex_info.options = WMTResourceStorageModeShared;
          auto tex = res->GetMTLBuffer().newTexture(tex_info, 0, 0);
          rp.colors[i].texture = tex.handle;
          has_valid_rt = true;
        }
      }
    }

    if (has_dsv) {
      auto *desc = reinterpret_cast<const D3D12Descriptor *>(dsv_handle.ptr);
      if (desc && desc->resource) {
        auto *res = static_cast<MTLD3D12Resource *>(desc->resource);
        if (res->GetMTLTexture().handle) {
          rp.depth.texture = res->GetMTLTexture().handle;
          rp.stencil.texture = res->GetMTLTexture().handle;
          has_valid_rt = true;
        }
      }
    }

    if (!has_valid_rt) {
      QTRACE("EnsureRenderEncoder: no valid RT texture found, skipping");
      return;
    }

    QTRACE("EnsureRenderEncoder: creating render encoder rt_count=%u", rt_count);
    render_enc = cmdbuf.renderCommandEncoder(rp);
    if (!render_enc.handle) {
      QTRACE("EnsureRenderEncoder: FAILED to create render encoder!");
      return;
    }
    render_enc_open = true;

    if (pso && pso->IsCompiled() && pso->GetRenderPSO().handle) {
      render_enc.setRenderPipelineState(pso->GetRenderPSO());
    }

    if (viewport_count > 0) {
      for (uint32_t i = 0; i < viewport_count; i++) {
        WMTViewport vp = {(double)viewports[i].TopLeftX,
                          (double)viewports[i].TopLeftY,
                          (double)viewports[i].Width,
                          (double)viewports[i].Height,
                          viewports[i].MinDepth,
                          viewports[i].MaxDepth};
        render_enc.setViewport(vp);
      }
    }
  }

  void ApplyRootBindings() {
    if (!render_enc_open || !pso)
      return;

    for (uint32_t i = 0; i < 16; i++) {
      if (root_cbv_set[i] && root_cbvs[i]) {
        // TODO: resolve GPU address to actual Metal buffer via device lookup
      }

      if (root_constant_set[i] && root_constant_sizes[i] > 0) {
        render_enc.setFragmentBytes(root_constants_buf + root_constant_offsets[i],
                                     root_constant_sizes[i], i);
      }

      if (root_table_set[i] && root_cbv_set[i]) {
        // descriptor table + CBV at this slot
      }
    }
  }
};

WMTIndexType DXGIToWMTIndexFormat(DXGI_FORMAT fmt) {
  switch (fmt) {
  case DXGI_FORMAT_R16_UINT: return WMTIndexTypeUInt16;
  case DXGI_FORMAT_R32_UINT: return WMTIndexTypeUInt32;
  default: return WMTIndexTypeUInt16;
  }
}

} // anonymous namespace

static bool rt_handles_match(D3D12_CPU_DESCRIPTOR_HANDLE a,
                             D3D12_CPU_DESCRIPTOR_HANDLE b) {
  return a.ptr == b.ptr;
}

MTLD3D12CommandQueue::MTLD3D12CommandQueue(MTLD3D12Device *device,
                                           CommandQueue &queue,
                                           D3D12_COMMAND_QUEUE_DESC desc)
    : m_device(device), m_queue(queue), m_desc(desc) {
  m_device->AddRef();
  auto wmt_dev = m_device->GetDXMTDevice().device();
  m_wmt_queue = wmt_dev.newCommandQueue(1);
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

  if (riid == __uuidof(IMTLDXGIDevice)) {
    return m_device->QueryInterface(riid, ppvObject);
  }
  QTRACE("CmdQueue::QI unknown IID %s -> E_NOINTERFACE", str::format(riid).c_str());
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
  QTRACE("CmdQueue::GetPrivateData E_NOTIMPL");
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
  QTRACE("ExecuteCommandLists count=%u", command_list_count);

  for (UINT li = 0; li < command_list_count; li++) {
    QTRACE("ECL: processing list %u", li);
    auto *list = static_cast<MTLD3D12GraphicsCommandList *>(command_lists[li]);
    if (!list) {
      QTRACE("ECL: list %u is null, skipping", li);
      continue;
    }

    QTRACE("ECL: creating cmdbuf from m_wmt_queue");
    auto cmdbuf = m_wmt_queue.commandBuffer();
    QTRACE("ECL: cmdbuf handle=%llu", (unsigned long long)cmdbuf.handle);
    if (!cmdbuf.handle) {
      Logger::err("ExecuteCommandLists: failed to create Metal command buffer");
      continue;
    }

    const auto &cmds = list->GetCommands();
    QTRACE("ExecuteCommandLists: cmds.size=%zu empty=%d", cmds.size(), cmds.empty());
    if (cmds.empty()) {
      QTRACE("ExecuteCommandLists: empty cmdlist, committing");
      cmdbuf.commit();
      QTRACE("ExecuteCommandLists: empty cmdlist committed ok");
      continue;
    }

    ReplayState st;
    st.cmdbuf = cmdbuf;

    QTRACE("ExecuteCommandLists: cmd_size=%zu", cmds.size());
    size_t offset = 0;
    size_t cmd_count = 0;
    uint32_t type_counts[30] = {};
    while (offset < cmds.size()) {
      if (offset + sizeof(CmdHeader) > cmds.size())
        break;
      auto *header = reinterpret_cast<const CmdHeader *>(cmds.data() + offset);
      if (header->size < sizeof(CmdHeader) || offset + header->size > cmds.size())
        break;

      if ((uint32_t)header->type < 30)
        type_counts[(uint32_t)header->type]++;
      cmd_count++;

      switch (header->type) {
      case CmdType::DrawInstanced: {
        auto *cmd = reinterpret_cast<const CmdDrawInstanced *>(header);
        st.EnsureRenderEncoder();
        st.ApplyRootBindings();
        QTRACE("DrawInstanced v=%u i=%u enc_open=%d", cmd->vertex_count, cmd->instance_count, st.render_enc_open);

        if (cmd->instance_count > 0 && cmd->vertex_count > 0 && st.render_enc_open) {
          struct wmtcmd_render_draw draw = {};
          draw.type = WMTRenderCommandDraw;
          draw.next.set(nullptr);
          draw.primitive_type = st.GetMetalPrimitiveType();
          draw.vertex_start = cmd->start_vertex;
          draw.vertex_count = cmd->vertex_count;
          draw.base_instance = cmd->start_instance;
          draw.instance_count = cmd->instance_count;
          st.render_enc.encodeCommands(
              reinterpret_cast<const wmtcmd_render_nop *>(&draw));
        }
        break;
      }
      case CmdType::DrawIndexedInstanced: {
        auto *cmd = reinterpret_cast<const CmdDrawIndexedInstanced *>(header);
        st.EnsureRenderEncoder();
        st.ApplyRootBindings();

        if (cmd->instance_count > 0 && cmd->index_count > 0 && st.ib.BufferLocation) {
          auto *ib_res = m_device->LookupResourceByGPUAddress(st.ib.BufferLocation);
          if (!ib_res && st.ib.BufferLocation) {
            ib_res = reinterpret_cast<MTLD3D12Resource *>(st.ib.BufferLocation);
          }
          struct wmtcmd_render_draw_indexed draw = {};
          draw.type = WMTRenderCommandDrawIndexed;
          draw.next.set(nullptr);
          draw.primitive_type = st.GetMetalPrimitiveType();
          draw.index_type = DXGIToWMTIndexFormat(st.ib.Format);
          draw.index_count = cmd->index_count;
          draw.index_buffer = ib_res ? ib_res->GetMTLBuffer().handle : NULL_OBJECT_HANDLE;
          draw.index_buffer_offset = st.ib.SizeInBytes ? 0 : 0;
          draw.instance_count = cmd->instance_count;
          draw.base_vertex = cmd->base_vertex;
          draw.base_instance = cmd->start_instance;
          st.render_enc.encodeCommands(
              reinterpret_cast<const wmtcmd_render_nop *>(&draw));
        }
        break;
      }
      case CmdType::Dispatch: {
        auto *cmd = reinterpret_cast<const CmdDispatch *>(header);
        if (st.pso && st.pso->IsCompiled() && st.pso->IsCompute() &&
            st.pso->GetComputePSO().handle) {
          auto comp = cmdbuf.computeCommandEncoder(false);
          struct wmtcmd_compute_setpso setpso = {};
          setpso.type = WMTComputeCommandSetPSO;
          setpso.next.set(nullptr);
          setpso.pso = st.pso->GetComputePSO();
          comp.encodeCommands(
              reinterpret_cast<const wmtcmd_compute_nop *>(&setpso));
          comp.endEncoding();
        }
        break;
      }
      case CmdType::CopyBufferRegion: {
        auto *cmd = reinterpret_cast<const CmdCopyBufferRegion *>(header);
        if (cmd->dst && cmd->src) {
          st.CloseRenderEncoder();
          auto *dst_res = static_cast<MTLD3D12Resource *>(cmd->dst);
          auto *src_res = static_cast<MTLD3D12Resource *>(cmd->src);
          if (dst_res->GetMTLBuffer().handle && src_res->GetMTLBuffer().handle) {
            auto blit = cmdbuf.blitCommandEncoder();
            struct wmtcmd_blit_copy_from_buffer_to_buffer copy = {};
            copy.type = WMTBlitCommandCopyFromBufferToBuffer;
            copy.next.set(nullptr);
            copy.src = src_res->GetMTLBuffer().handle;
            copy.src_offset = cmd->src_offset;
            copy.dst = dst_res->GetMTLBuffer().handle;
            copy.dst_offset = cmd->dst_offset;
            copy.copy_length = cmd->byte_count;
            blit.encodeCommands(reinterpret_cast<const wmtcmd_blit_nop *>(&copy));
            blit.endEncoding();
          }
        }
        break;
      }
      case CmdType::SetPipelineState: {
        auto *cmd = reinterpret_cast<const CmdSetPipelineState *>(header);
        st.pso = static_cast<MTLD3D12PipelineState *>(cmd->pso);
        if (st.render_enc_open && st.pso && st.pso->IsCompiled() &&
            st.pso->GetRenderPSO().handle) {
          st.render_enc.setRenderPipelineState(st.pso->GetRenderPSO());
        }
        break;
      }
      case CmdType::ResourceBarrier: {
        break;
      }
      case CmdType::OMSetRenderTargets: {
        auto *cmd = reinterpret_cast<const CmdOMSetRenderTargets *>(header);
        st.CloseRenderEncoder();
        st.rt_count = cmd->rt_count;
        for (uint32_t i = 0; i < cmd->rt_count && i < 8; i++)
          st.rt_handles[i] = cmd->rts[i];
        st.has_dsv = cmd->has_dsv;
        if (cmd->has_dsv)
          st.dsv_handle = cmd->dsv;
        break;
      }
      case CmdType::ClearRenderTargetView: {
        auto *cmd = reinterpret_cast<const CmdClearRTV *>(header);
        st.CloseRenderEncoder();

        WMTRenderPassInfo rp = {};
        for (uint32_t i = 0; i < 8; i++) {
          rp.colors[i].texture = NULL_OBJECT_HANDLE;
          rp.colors[i].load_action = WMTLoadActionDontCare;
          rp.colors[i].store_action = WMTStoreActionDontCare;
        }
        rp.depth.texture = NULL_OBJECT_HANDLE;
        rp.depth.load_action = WMTLoadActionDontCare;
        rp.depth.store_action = WMTStoreActionDontCare;
        rp.stencil.texture = NULL_OBJECT_HANDLE;
        rp.stencil.load_action = WMTLoadActionDontCare;
        rp.stencil.store_action = WMTStoreActionDontCare;

        for (uint32_t i = 0; i < st.rt_count && i < 8; i++) {
          auto *desc = reinterpret_cast<const D3D12Descriptor *>(st.rt_handles[i].ptr);
          if (desc && desc->resource) {
            auto *res = static_cast<MTLD3D12Resource *>(desc->resource);
            if (res->GetMTLTexture().handle) {
              rp.colors[i].texture = res->GetMTLTexture().handle;
              if (rt_handles_match(st.rt_handles[i], cmd->rtv)) {
                rp.colors[i].load_action = WMTLoadActionClear;
                rp.colors[i].store_action = WMTStoreActionStore;
                rp.colors[i].clear_color = {cmd->color[0], cmd->color[1],
                                            cmd->color[2], cmd->color[3]};
              } else {
                rp.colors[i].load_action = WMTLoadActionLoad;
                rp.colors[i].store_action = WMTStoreActionStore;
              }
            }
          }
        }

        if (st.has_dsv) {
          auto *desc = reinterpret_cast<const D3D12Descriptor *>(st.dsv_handle.ptr);
          if (desc && desc->resource) {
            auto *res = static_cast<MTLD3D12Resource *>(desc->resource);
            if (res->GetMTLTexture().handle) {
              rp.depth.texture = res->GetMTLTexture().handle;
              rp.depth.load_action = WMTLoadActionLoad;
              rp.depth.store_action = WMTStoreActionStore;
            }
          }
        }

        auto enc = cmdbuf.renderCommandEncoder(rp);
        enc.endEncoding();
        break;
      }
      case CmdType::ClearDepthStencilView: {
        auto *cmd = reinterpret_cast<const CmdClearDSV *>(header);
        break;
      }
      case CmdType::RSSetViewports: {
        auto *cmd = reinterpret_cast<const CmdRSSetViewports *>(header);
        auto *vps = reinterpret_cast<const D3D12_VIEWPORT *>(
            reinterpret_cast<const uint8_t *>(cmd) +
            sizeof(CmdRSSetViewports) - sizeof(D3D12_VIEWPORT));
        st.viewport_count = cmd->count > 16 ? 16 : cmd->count;
        for (uint32_t i = 0; i < st.viewport_count; i++)
          st.viewports[i] = vps[i];
        if (st.render_enc_open) {
          for (uint32_t i = 0; i < st.viewport_count; i++) {
            WMTViewport vp = {(double)vps[i].TopLeftX, (double)vps[i].TopLeftY,
                              (double)vps[i].Width, (double)vps[i].Height,
                              vps[i].MinDepth, vps[i].MaxDepth};
            st.render_enc.setViewport(vp);
          }
        }
        break;
      }
      case CmdType::RSSetScissorRects: {
        auto *cmd = reinterpret_cast<const CmdRSSetScissorRects *>(header);
        st.scissor_count = cmd->count > 16 ? 16 : cmd->count;
        break;
      }
      case CmdType::IASetPrimitiveTopology: {
        auto *cmd = reinterpret_cast<const CmdIASetPrimitiveTopology *>(header);
        st.topology = cmd->topology;
        break;
      }
      case CmdType::SetGraphicsRootSignature: {
        auto *cmd = reinterpret_cast<const CmdSetRootSignature *>(header);
        st.graphics_root_sig = static_cast<MTLD3D12RootSignature *>(cmd->root_sig);
        break;
      }
      case CmdType::SetGraphicsRoot32BitConstants: {
        auto *cmd = reinterpret_cast<const CmdSetRoot32BitConstants *>(header);
        if (cmd->root_param_index < 16) {
          uint32_t sz = cmd->count * 4;
          uint32_t off = cmd->dst_offset * 4;
          if (off + sz <= sizeof(st.root_constants_buf)) {
            memcpy(st.root_constants_buf + off, cmd->data, sz);
            st.root_constant_offsets[cmd->root_param_index] = off;
            st.root_constant_sizes[cmd->root_param_index] = sz;
            st.root_constant_set[cmd->root_param_index] = true;
          }
        }
        break;
      }
      case CmdType::SetGraphicsRootConstantBufferView: {
        auto *cmd = reinterpret_cast<const CmdSetRootCBV *>(header);
        if (cmd->root_param_index < 16) {
          st.root_cbvs[cmd->root_param_index] = cmd->address;
          st.root_cbv_set[cmd->root_param_index] = true;
        }
        break;
      }
      case CmdType::SetGraphicsRootDescriptorTable: {
        auto *cmd = reinterpret_cast<const CmdSetRootDescriptorTable *>(header);
        if (cmd->root_param_index < 16) {
          st.root_tables[cmd->root_param_index] = cmd->base_descriptor;
          st.root_table_set[cmd->root_param_index] = true;
        }
        break;
      }
      case CmdType::IASetVertexBuffers: {
        auto *cmd = reinterpret_cast<const CmdIASetVertexBuffers *>(header);
        auto *views = reinterpret_cast<const D3D12_VERTEX_BUFFER_VIEW *>(
            reinterpret_cast<const uint8_t *>(cmd) +
            sizeof(CmdIASetVertexBuffers) - sizeof(D3D12_VERTEX_BUFFER_VIEW));
        if (st.render_enc_open) {
          for (uint32_t i = 0; i < cmd->count; i++) {
            if (views[i].BufferLocation) {
              auto *res = m_device->LookupResourceByGPUAddress(views[i].BufferLocation);
              if (res && res->GetMTLBuffer().handle) {
                st.render_enc.setVertexBuffer(res->GetMTLBuffer(),
                                              views[i].SizeInBytes ? 0 : 0,
                                              cmd->start_slot + i);
              }
            }
          }
        }
        for (uint32_t i = 0; i < cmd->count && (cmd->start_slot + i) < 16; i++)
          st.vbs[cmd->start_slot + i] = views[i];
        break;
      }
      case CmdType::IASetIndexBuffer: {
        auto *cmd = reinterpret_cast<const CmdIASetIndexBuffer *>(header);
        st.ib = cmd->view;
        break;
      }
      case CmdType::OMSetBlendFactor: {
        auto *cmd = reinterpret_cast<const CmdOMBlendFactor *>(header);
        memcpy(st.blend_factor, cmd->factor, 16);
        break;
      }
      case CmdType::OMSetStencilRef: {
        auto *cmd = reinterpret_cast<const CmdOMStencilRef *>(header);
        st.stencil_ref = cmd->stencil_ref;
        break;
      }
      case CmdType::SetDescriptorHeaps: {
        auto *cmd = reinterpret_cast<const CmdSetDescriptorHeaps *>(header);
        st.desc_heap_count = cmd->count > 2 ? 2 : cmd->count;
        auto *heaps = reinterpret_cast<ID3D12DescriptorHeap *const *>(
            reinterpret_cast<const uint8_t *>(cmd) +
            sizeof(CmdSetDescriptorHeaps) - sizeof(ID3D12DescriptorHeap *));
        for (uint32_t i = 0; i < st.desc_heap_count; i++)
          st.desc_heaps[i] = heaps[i];
        break;
      }
      default:
        break;
      }

      offset += header->size;
    }

    QTRACE("ECL: replayed %zu cmds, types:", cmd_count);
    for (int i = 0; i < 30; i++)
      if (type_counts[i])
        QTRACE("  type[%d]=%u", i, type_counts[i]);

    st.CloseRenderEncoder();
    QTRACE("ExecuteCommandLists: committing cmdbuf");
    cmdbuf.commit();
    cmdbuf.waitUntilCompleted();

    auto status = cmdbuf.status();
    QTRACE("ExecuteCommandLists: cmdbuf status=%d", (int)status);
    if (status != WMTCommandBufferStatusCompleted) {
      auto err = cmdbuf.error();
      Logger::err(str::format("ExecuteCommandLists: cmdbuf status=", status, " error_handle=", err.handle));
    }
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
  QTRACE("Signal value=%llu", (unsigned long long)value);
  if (!fence)
    return E_POINTER;
  return fence->Signal(value);
}

HRESULT STDMETHODCALLTYPE
MTLD3D12CommandQueue::Wait(ID3D12Fence *fence, UINT64 value) {
  if (!fence)
    return E_POINTER;
  return fence->SetEventOnCompletion(value, nullptr);
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
