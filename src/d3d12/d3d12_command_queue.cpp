#include "d3d12_command_queue.hpp"
#include "d3d12_command_list.hpp"
#include "d3d12_descriptor_heap.hpp"
#include "d3d12_device.hpp"
#include "d3d12_pipeline_state.hpp"
#include "d3d12_resource.hpp"
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

  auto wmt_device = m_device->GetDXMTDevice().device();
  auto wmt_queue = WMT::CommandQueue{wmt_device.newCommandQueue(1).handle};

  for (UINT i = 0; i < command_list_count; i++) {
    auto *list = static_cast<MTLD3D12GraphicsCommandList *>(command_lists[i]);
    if (!list)
      continue;

    auto cmdbuf = wmt_queue.commandBuffer();
    if (!cmdbuf.handle) {
      Logger::err("ExecuteCommandLists: failed to create Metal command buffer");
      continue;
    }

    const auto &cmds = list->GetCommands();
    if (cmds.empty()) {
      cmdbuf.commit();
      continue;
    }

    Logger::info(str::format("  list[", i, "]: ", cmds.size(), " bytes of commands"));

    size_t offset = 0;
    while (offset < cmds.size()) {
      if (offset + sizeof(CmdHeader) > cmds.size())
        break;
      auto *header = reinterpret_cast<const CmdHeader *>(cmds.data() + offset);
      if (offset + header->size > cmds.size())
        break;

      switch (header->type) {
      case CmdType::DrawInstanced: {
        auto *cmd = reinterpret_cast<const CmdDrawInstanced *>(header);
        Logger::info(str::format("    DrawInstanced: v=", cmd->vertex_count,
                                  " i=", cmd->instance_count));
        break;
      }
      case CmdType::DrawIndexedInstanced: {
        auto *cmd = reinterpret_cast<const CmdDrawIndexedInstanced *>(header);
        Logger::info(str::format("    DrawIndexedInstanced: idx=", cmd->index_count,
                                  " inst=", cmd->instance_count));
        break;
      }
      case CmdType::Dispatch: {
        auto *cmd = reinterpret_cast<const CmdDispatch *>(header);
        Logger::info(str::format("    Dispatch: ", cmd->x, "x", cmd->y, "x", cmd->z));
        break;
      }
      case CmdType::CopyBufferRegion: {
        auto *cmd = reinterpret_cast<const CmdCopyBufferRegion *>(header);
        Logger::info(str::format("    CopyBuffer: ", cmd->byte_count, " bytes"));
        if (cmd->dst && cmd->src) {
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
        auto *pso = static_cast<MTLD3D12PipelineState *>(cmd->pso);
        if (pso)
          Logger::info(str::format("    SetPSO: compute=", pso->IsCompute()));
        break;
      }
      case CmdType::ResourceBarrier: {
        Logger::info("    ResourceBarrier");
        break;
      }
      case CmdType::OMSetRenderTargets: {
        auto *cmd = reinterpret_cast<const CmdOMSetRenderTargets *>(header);
        Logger::info(str::format("    SetRTs: ", cmd->rt_count, " rts, dsv=", cmd->has_dsv));
        break;
      }
      case CmdType::ClearRenderTargetView: {
        auto *cmd = reinterpret_cast<const CmdClearRTV *>(header);
        Logger::info(str::format("    ClearRTV: (", cmd->color[0], ",", cmd->color[1],
                                  ",", cmd->color[2], ",", cmd->color[3], ")"));
        break;
      }
      case CmdType::RSSetViewports: {
        auto *cmd = reinterpret_cast<const CmdRSSetViewports *>(header);
        Logger::info(str::format("    SetViewports: ", cmd->count));
        break;
      }
      case CmdType::IASetPrimitiveTopology: {
        auto *cmd = reinterpret_cast<const CmdIASetPrimitiveTopology *>(header);
        Logger::info(str::format("    SetTopology: ", cmd->topology));
        break;
      }
      case CmdType::SetGraphicsRootSignature: {
        Logger::info("    SetGraphicsRootSignature");
        break;
      }
      case CmdType::SetGraphicsRoot32BitConstants: {
        auto *cmd = reinterpret_cast<const CmdSetRoot32BitConstants *>(header);
        Logger::info(str::format("    SetRootConstants: slot=", cmd->root_param_index,
                                  " count=", cmd->count));
        break;
      }
      case CmdType::SetGraphicsRootConstantBufferView: {
        auto *cmd = reinterpret_cast<const CmdSetRootCBV *>(header);
        Logger::info(str::format("    SetRootCBV: slot=", cmd->root_param_index,
                                  " addr=", cmd->address));
        break;
      }
      case CmdType::SetGraphicsRootDescriptorTable: {
        auto *cmd = reinterpret_cast<const CmdSetRootDescriptorTable *>(header);
        Logger::info(str::format("    SetRootDescTable: slot=", cmd->root_param_index));
        break;
      }
      case CmdType::IASetVertexBuffers: {
        auto *cmd = reinterpret_cast<const CmdIASetVertexBuffers *>(header);
        Logger::info(str::format("    SetVBs: slot=", cmd->start_slot, " count=", cmd->count));
        break;
      }
      case CmdType::IASetIndexBuffer: {
        Logger::info("    SetIB");
        break;
      }
      case CmdType::SetDescriptorHeaps: {
        auto *cmd = reinterpret_cast<const CmdSetDescriptorHeaps *>(header);
        Logger::info(str::format("    SetDescHeaps: ", cmd->count));
        break;
      }
      default:
        Logger::info(str::format("    Unknown cmd: ", (uint32_t)header->type));
        break;
      }

      offset += header->size;
    }

    cmdbuf.commit();
    cmdbuf.waitUntilCompleted();

    auto status = cmdbuf.status();
    if (status != WMTCommandBufferStatusCompleted) {
      Logger::err(str::format("ExecuteCommandLists: cmdbuf status=", status));
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
