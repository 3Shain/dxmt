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
#include <algorithm>
#include <cstring>

#define QTRACE(fmt, ...) do { FILE *_tf = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a"); if (_tf) { fprintf(_tf, fmt "\n", ##__VA_ARGS__); fclose(_tf); } } while(0)

static uint64_t g_enc_id = 0;
#define ENC_CREATE(type, handle) do { uint64_t _eid = __atomic_add_fetch(&g_enc_id, 1, __ATOMIC_SEQ_CST); QTRACE("[ENC+%llu] CREATE %s handle=%llu", (unsigned long long)_eid, type, (unsigned long long)(handle)); } while(0)
#define ENC_END(handle) do { QTRACE("[ENC] END handle=%llu", (unsigned long long)(handle)); } while(0)
#define ENC_COMMIT(cmdbuf_handle) do { QTRACE("[ENC] COMMIT cmdbuf=%llu", (unsigned long long)(cmdbuf_handle)); } while(0)

namespace dxmt {

namespace {

static uint64_t TextureMetadata(uint32_t array_length, float min_lod = 0.0f) {
  uint32_t min_lod_bits = 0;
  static_assert(sizeof(min_lod_bits) == sizeof(min_lod));
  memcpy(&min_lod_bits, &min_lod, sizeof(min_lod_bits));
  return ((uint64_t)array_length << 32) | min_lod_bits;
}

static uint64_t SamplerCubeGPUResourceID(const D3D12Descriptor *desc) {
  return desc->metal_sampler_cube_gpu_id ? desc->metal_sampler_cube_gpu_id
                                         : desc->metal_sampler_gpu_id;
}

static uint64_t SamplerLodBiasBits(const D3D12Descriptor *desc) {
  uint32_t bits = 0;
  float lod_bias = desc->sampler.MipLODBias;
  static_assert(sizeof(bits) == sizeof(lod_bias));
  memcpy(&bits, &lod_bias, sizeof(bits));
  return bits;
}

static bool ShaderVisibilityMatches(uint32_t param_visibility,
                                    D3D12_SHADER_VISIBILITY shader_visibility,
                                    bool exact_pass) {
  if (exact_pass)
    return param_visibility == shader_visibility;
  return param_visibility == D3D12_SHADER_VISIBILITY_ALL;
}

static uint32_t FormatByteSize(DXGI_FORMAT format) {
  switch (format) {
  case DXGI_FORMAT_R32G32B32A32_FLOAT:
  case DXGI_FORMAT_R32G32B32A32_UINT:
  case DXGI_FORMAT_R32G32B32A32_SINT:
    return 16;
  case DXGI_FORMAT_R32G32B32_FLOAT:
  case DXGI_FORMAT_R32G32B32_UINT:
  case DXGI_FORMAT_R32G32B32_SINT:
    return 12;
  case DXGI_FORMAT_R16G16B16A16_FLOAT:
  case DXGI_FORMAT_R16G16B16A16_UNORM:
  case DXGI_FORMAT_R16G16B16A16_UINT:
  case DXGI_FORMAT_R16G16B16A16_SNORM:
  case DXGI_FORMAT_R16G16B16A16_SINT:
  case DXGI_FORMAT_R32G32_FLOAT:
  case DXGI_FORMAT_R32G32_UINT:
  case DXGI_FORMAT_R32G32_SINT:
    return 8;
  case DXGI_FORMAT_R8G8B8A8_UNORM:
  case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
  case DXGI_FORMAT_R8G8B8A8_UINT:
  case DXGI_FORMAT_R8G8B8A8_SNORM:
  case DXGI_FORMAT_R8G8B8A8_SINT:
  case DXGI_FORMAT_B8G8R8A8_UNORM:
  case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
  case DXGI_FORMAT_R16G16_FLOAT:
  case DXGI_FORMAT_R16G16_UNORM:
  case DXGI_FORMAT_R16G16_UINT:
  case DXGI_FORMAT_R16G16_SNORM:
  case DXGI_FORMAT_R16G16_SINT:
  case DXGI_FORMAT_R32_FLOAT:
  case DXGI_FORMAT_R32_UINT:
  case DXGI_FORMAT_R32_SINT:
    return 4;
  case DXGI_FORMAT_R16_FLOAT:
  case DXGI_FORMAT_R16_UNORM:
  case DXGI_FORMAT_R16_UINT:
  case DXGI_FORMAT_R16_SNORM:
  case DXGI_FORMAT_R16_SINT:
  case DXGI_FORMAT_R8G8_UNORM:
  case DXGI_FORMAT_R8G8_UINT:
  case DXGI_FORMAT_R8G8_SNORM:
  case DXGI_FORMAT_R8G8_SINT:
    return 2;
  case DXGI_FORMAT_R8_UNORM:
  case DXGI_FORMAT_R8_UINT:
  case DXGI_FORMAT_R8_SNORM:
  case DXGI_FORMAT_R8_SINT:
    return 1;
  default:
    return 4;
  }
}

static uint64_t SRVBufferByteLength(const D3D12Descriptor *desc,
                                    const MTLD3D12Resource *res) {
  if (!desc)
    return res ? res->GetBufferByteLength() : 0;
  uint64_t stride = desc->srv.Buffer.StructureByteStride;
  if (!stride)
    stride = FormatByteSize(desc->srv.Format);
  uint64_t length = (uint64_t)desc->srv.Buffer.NumElements * stride;
  return length ? length : (res ? res->GetBufferByteLength() : 0);
}

static uint64_t UAVBufferByteLength(const D3D12Descriptor *desc,
                                    const MTLD3D12Resource *res) {
  if (!desc)
    return res ? res->GetBufferByteLength() : 0;
  uint64_t stride = desc->uav.Buffer.StructureByteStride;
  if (!stride)
    stride = FormatByteSize(desc->uav.Format);
  uint64_t length = (uint64_t)desc->uav.Buffer.NumElements * stride;
  return length ? length : (res ? res->GetBufferByteLength() : 0);
}

static uint64_t SRVBufferByteOffset(const D3D12Descriptor *desc) {
  if (!desc)
    return 0;
  uint64_t stride = desc->srv.Buffer.StructureByteStride;
  if (!stride)
    stride = FormatByteSize(desc->srv.Format);
  return (uint64_t)desc->srv.Buffer.FirstElement * stride;
}

static uint64_t UAVBufferByteOffset(const D3D12Descriptor *desc) {
  if (!desc)
    return 0;
  uint64_t stride = desc->uav.Buffer.StructureByteStride;
  if (!stride)
    stride = FormatByteSize(desc->uav.Format);
  return (uint64_t)desc->uav.Buffer.FirstElement * stride;
}

static uint32_t SRVTextureArrayLength(const D3D12Descriptor *desc,
                                      const MTLD3D12Resource *res) {
  if (!desc)
    return res ? res->GetTextureArrayLength() : 1;
  switch (desc->srv.ViewDimension) {
  case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
    return desc->srv.Texture1DArray.ArraySize ? desc->srv.Texture1DArray.ArraySize : 1;
  case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
    return desc->srv.Texture2DArray.ArraySize ? desc->srv.Texture2DArray.ArraySize : 1;
  case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
    return desc->srv.Texture2DMSArray.ArraySize ? desc->srv.Texture2DMSArray.ArraySize : 1;
  case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
    return desc->srv.TextureCubeArray.NumCubes ? desc->srv.TextureCubeArray.NumCubes * 6 : 6;
  case D3D12_SRV_DIMENSION_TEXTURECUBE:
    return 6;
  default:
    return res ? res->GetTextureArrayLength() : 1;
  }
}

static uint32_t UAVTextureArrayLength(const D3D12Descriptor *desc,
                                      const MTLD3D12Resource *res) {
  if (!desc)
    return res ? res->GetTextureArrayLength() : 1;
  switch (desc->uav.ViewDimension) {
  case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
    return desc->uav.Texture1DArray.ArraySize ? desc->uav.Texture1DArray.ArraySize : 1;
  case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
    return desc->uav.Texture2DArray.ArraySize ? desc->uav.Texture2DArray.ArraySize : 1;
  default:
    return res ? res->GetTextureArrayLength() : 1;
  }
}

static bool FormatHasStencil(DXGI_FORMAT format) {
  return format == DXGI_FORMAT_D24_UNORM_S8_UINT ||
         format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
}

static bool DSVHasStencil(const D3D12Descriptor *desc) {
  if (!desc || !desc->resource)
    return false;
  DXGI_FORMAT format = desc->dsv.Format;
  if (format == DXGI_FORMAT_UNKNOWN) {
    D3D12_RESOURCE_DESC resource_desc = {};
    static_cast<MTLD3D12Resource *>(desc->resource)->GetDesc(&resource_desc);
    format = resource_desc.Format;
  }
  return FormatHasStencil(format);
}

struct ReplayState {
  static constexpr uint32_t kVertexBufferSlotCount = 29;
  static constexpr uint32_t kVertexBufferTableSlot = 16;

  struct VertexBufferEntry {
    uint64_t buffer_handle;
    uint32_t stride;
    uint32_t length;
  };

  WMT::CommandBuffer cmdbuf;
  WMT::RenderCommandEncoder render_enc;
  bool render_enc_open = false;

  MTLD3D12PipelineState *pso = nullptr;
  MTLD3D12RootSignature *graphics_root_sig = nullptr;
  D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
  D3D12_VERTEX_BUFFER_VIEW vbs[kVertexBufferSlotCount] = {};
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

  static constexpr uint32_t kRootConstantBytes = 256;
  D3D12_GPU_VIRTUAL_ADDRESS root_cbvs[16] = {};
  D3D12_GPU_DESCRIPTOR_HANDLE root_tables[16] = {};
  uint8_t root_constants_buf[16 * kRootConstantBytes] = {};
  uint32_t root_constant_offsets[16] = {};
  uint32_t root_constant_sizes[16] = {};
  bool root_constant_set[16] = {};
  bool root_cbv_set[16] = {};
  bool root_table_set[16] = {};

  MTLD3D12RootSignature *compute_root_sig = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS comp_cbvs[16] = {};
  D3D12_GPU_DESCRIPTOR_HANDLE comp_tables[16] = {};
  uint8_t comp_constants_buf[16 * kRootConstantBytes] = {};
  uint32_t comp_constant_offsets[16] = {};
  uint32_t comp_constant_sizes[16] = {};
  bool comp_constant_set[16] = {};
  bool comp_cbv_set[16] = {};
  bool comp_table_set[16] = {};
  bool comp_uav_root[16] = {};

  static constexpr uint32_t kArgBufSlot = 30;
  static constexpr uint32_t kArgBufMaxQwords = 128;
  static constexpr uint32_t kConstantBufferTableSlot = 29;
  static constexpr uint32_t kConstantBufferMaxQwords = 32;
  uint64_t arg_buf_data[kArgBufMaxQwords] = {};
  uint64_t cbv_table_data[kConstantBufferMaxQwords] = {};
  uint64_t vs_arg_buf_data[kArgBufMaxQwords] = {};
  uint64_t vs_cbv_table_data[kConstantBufferMaxQwords] = {};
  uint64_t comp_arg_buf_data[kArgBufMaxQwords] = {};
  uint64_t comp_cbv_table_data[kConstantBufferMaxQwords] = {};
  WMT::Reference<WMT::Buffer> arg_buf;
  WMT::Reference<WMT::Buffer> cbv_table_buf;
  WMT::Reference<WMT::Buffer> vs_arg_buf;
  WMT::Reference<WMT::Buffer> vs_cbv_table_buf;
  WMT::Reference<WMT::Buffer> comp_arg_buf;
  WMT::Reference<WMT::Buffer> comp_cbv_table_buf;
  WMT::Reference<WMT::Buffer> root_constants_mtl_buf;
  VertexBufferEntry vertex_table_data[kVertexBufferSlotCount] = {};
  WMT::Reference<WMT::Buffer> vertex_table_buf;

  void BuildArgumentBuffer(MTLD3D12Device *device) {
    if (!pso || pso->GetPSArguments().empty()) {
      QTRACE("BuildArgumentBuffer: no PSO or no args");
      return;
    }
    auto &args = pso->GetPSArguments();
    uint32_t qword_count = pso->GetPSReflection().ArgumentTableQwords;
    QTRACE("BuildArgumentBuffer: %u args, %u qwords, NumArguments=%u", (unsigned)args.size(), qword_count, (unsigned)pso->GetPSReflection().NumArguments);
    if (qword_count == 0 || qword_count > kArgBufMaxQwords) {
      QTRACE("BuildArgumentBuffer: invalid qword_count=%u", qword_count);
      return;
    }
    memset(arg_buf_data, 0, qword_count * 8);

    auto *root_sig = pso->GetRootSignature();
    auto *dxmt_sig = root_sig ? static_cast<MTLD3D12RootSignature *>(root_sig) : nullptr;

    for (auto &arg : args) {
      uint32_t root_idx = ~0u;
      uint32_t descriptor_offset = 0;
      if (dxmt_sig) {
        D3D12_DESCRIPTOR_RANGE_TYPE range_type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        bool table_arg = true;
        if (arg.Type == SM50BindingType::SRV) {
          range_type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        } else if (arg.Type == SM50BindingType::Sampler) {
          range_type = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        } else if (arg.Type == SM50BindingType::UAV) {
          range_type = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        } else {
          table_arg = false;
        }

        if (table_arg) {
          dxmt_sig->FindDescriptorTableRangeForVisibility(
              range_type, arg.SM50BindingSlot, arg.SM50RegisterSpace,
              D3D12_SHADER_VISIBILITY_PIXEL, &root_idx, &descriptor_offset);
        }
      }
      if (root_idx == ~0u || !root_table_set[root_idx] || desc_heap_count == 0) {
        QTRACE("BuildArgBuf: arg type=%d slot=%u root_idx=%u desc_off=%u table_set=%d heaps=%u skip",
          (int)arg.Type, arg.SM50BindingSlot, root_idx, descriptor_offset, root_idx != ~0u ? root_table_set[root_idx] : 0, desc_heap_count);
        continue;
      }

      for (uint32_t h = 0; h < desc_heap_count; h++) {
        auto *heap = static_cast<MTLD3D12DescriptorHeap *>(desc_heaps[h]);
        if (!heap) continue;
        auto *desc = heap->GetDescriptorFromGPUHandle(root_tables[root_idx], descriptor_offset);
        if (!desc) continue;

        if (arg.Type == SM50BindingType::SRV) {
          QTRACE("BuildArgBuf: SRV root=%u desc_off=%u desc=%p res=%p flags=0x%x offset=%u",
            root_idx, descriptor_offset, (void*)desc, desc->resource ? (void*)desc->resource : nullptr, arg.Flags, arg.StructurePtrOffset);
          if ((arg.Flags & MTL_SM50_SHADER_ARGUMENT_BUFFER) && desc->resource) {
            auto *res = static_cast<MTLD3D12Resource *>(desc->resource);
            if (res->GetMTLBuffer().handle) {
              arg_buf_data[arg.StructurePtrOffset] =
                  res->GetGPUVirtualAddress() + SRVBufferByteOffset(desc);
              arg_buf_data[arg.StructurePtrOffset + 1] =
                  SRVBufferByteLength(desc, res);
              if (render_enc_open) {
                render_enc.useResource(res->GetMTLBuffer(), WMTResourceUsageRead, (WMTRenderStages)(WMTRenderStageVertex | WMTRenderStageFragment));
              }
            }
          } else if (desc->resource) {
            auto *res = static_cast<MTLD3D12Resource *>(desc->resource);
            if (res->GetMTLTexture().handle) {
              uint64_t gpu_id = res->GetTextureGPUResourceID();
              QTRACE("BuildArgBuf: SRV tex_handle=%llu gpu_id=0x%llx", (unsigned long long)res->GetMTLTexture().handle, (unsigned long long)gpu_id);
              arg_buf_data[arg.StructurePtrOffset] = gpu_id;
              arg_buf_data[arg.StructurePtrOffset + 1] =
                  TextureMetadata(SRVTextureArrayLength(desc, res), 0.0f);
              if (render_enc_open) {
                render_enc.useResource(res->GetMTLTexture(), (WMTResourceUsage)(WMTResourceUsageSample | WMTResourceUsageRead), (WMTRenderStages)(WMTRenderStageVertex | WMTRenderStageFragment));
                QTRACE("BuildArgBuf: useResource texture handle=%llu", (unsigned long long)res->GetMTLTexture().handle);
              }
            } else if (res->GetMTLBuffer().handle) {
              arg_buf_data[arg.StructurePtrOffset] =
                  res->GetGPUVirtualAddress() + SRVBufferByteOffset(desc);
              arg_buf_data[arg.StructurePtrOffset + 1] =
                  SRVBufferByteLength(desc, res);
              if (render_enc_open) {
                render_enc.useResource(res->GetMTLBuffer(), WMTResourceUsageRead, (WMTRenderStages)(WMTRenderStageVertex | WMTRenderStageFragment));
              }
            }
          }
        } else if (arg.Type == SM50BindingType::Sampler) {
          QTRACE("BuildArgBuf: Sampler root=%u desc_off=%u desc_type=%u gpu_id=0x%llx offset=%u",
            root_idx, descriptor_offset, desc->type, (unsigned long long)desc->metal_sampler_gpu_id, arg.StructurePtrOffset);
          if (desc->type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER && desc->metal_sampler_gpu_id) {
            arg_buf_data[arg.StructurePtrOffset] = desc->metal_sampler_gpu_id;
            arg_buf_data[arg.StructurePtrOffset + 1] =
                SamplerCubeGPUResourceID(desc);
            arg_buf_data[arg.StructurePtrOffset + 2] = SamplerLodBiasBits(desc);
          }
        } else if (arg.Type == SM50BindingType::UAV) {
          QTRACE("BuildArgBuf: UAV root=%u desc_off=%u desc=%p res=%p flags=0x%x offset=%u",
            root_idx, descriptor_offset, (void*)desc, desc->resource ? (void*)desc->resource : nullptr, arg.Flags, arg.StructurePtrOffset);
          if (desc->resource) {
            auto *res = static_cast<MTLD3D12Resource *>(desc->resource);
            if ((arg.Flags & MTL_SM50_SHADER_ARGUMENT_BUFFER) && res->GetMTLBuffer().handle) {
              arg_buf_data[arg.StructurePtrOffset] =
                  res->GetGPUVirtualAddress() + UAVBufferByteOffset(desc);
              arg_buf_data[arg.StructurePtrOffset + 1] =
                  UAVBufferByteLength(desc, res);
              if (render_enc_open) {
                render_enc.useResource(res->GetMTLBuffer(),
                  (WMTResourceUsage)(WMTResourceUsageRead | WMTResourceUsageWrite),
                  (WMTRenderStages)(WMTRenderStageVertex | WMTRenderStageFragment));
              }
            } else if (res->GetMTLTexture().handle) {
              arg_buf_data[arg.StructurePtrOffset] = res->GetTextureGPUResourceID();
              arg_buf_data[arg.StructurePtrOffset + 1] =
                  TextureMetadata(UAVTextureArrayLength(desc, res), 0.0f);
              if (render_enc_open) {
                render_enc.useResource(res->GetMTLTexture(),
                  (WMTResourceUsage)(WMTResourceUsageRead | WMTResourceUsageWrite),
                  (WMTRenderStages)(WMTRenderStageVertex | WMTRenderStageFragment));
              }
            }
          }
        }
      }
    }

    if (!arg_buf.handle) {
      WMTBufferInfo buf_info = {};
      buf_info.length = kArgBufMaxQwords * 8;
      buf_info.options = WMTResourceStorageModeShared;
      arg_buf = device->GetDXMTDevice().device().newBuffer(buf_info);
    }
    if (arg_buf.handle) {
      arg_buf.updateContents(0, arg_buf_data, qword_count * 8);
      QTRACE("BuildArgumentBuffer: wrote %u qwords to argbuf", qword_count);
      for (uint32_t i = 0; i < qword_count && i < 8; i++) {
        QTRACE("  arg_buf[%u] = 0x%llx", i, (unsigned long long)arg_buf_data[i]);
      }
      if (render_enc_open) {
        render_enc.useResource(arg_buf, WMTResourceUsageRead, (WMTRenderStages)(WMTRenderStageVertex | WMTRenderStageFragment));
        QTRACE("BuildArgumentBuffer: useResource argbuf handle=%llu", (unsigned long long)arg_buf.handle);
      }
    }
  }

  void BuildConstantBufferTable(MTLD3D12Device *device) {
    if (!pso || pso->GetPSConstantBuffers().empty()) {
      return;
    }

    memset(cbv_table_data, 0, sizeof(cbv_table_data));

    auto *root_sig = pso->GetRootSignature();
    auto *dxmt_sig = root_sig ? static_cast<MTLD3D12RootSignature *>(root_sig) : nullptr;
    auto &cb_args = pso->GetPSConstantBuffers();
    uint32_t qword_count = 0;

    for (const auto &arg : cb_args) {
      if (arg.Type != SM50BindingType::ConstantBuffer ||
          arg.StructurePtrOffset >= kConstantBufferMaxQwords)
        continue;

      qword_count = std::max(qword_count, arg.StructurePtrOffset + 1);
      uint64_t gpu_address = 0;

      uint32_t root_idx = ~0u;
      if (dxmt_sig) {
        auto &params = dxmt_sig->GetParameters();
        for (uint32_t pass = 0; pass < 2 && root_idx == ~0u; pass++) {
          for (uint32_t p = 0; p < params.size() && p < 16; p++) {
            if (params[p].type == D3D12_ROOT_PARAMETER_TYPE_CBV &&
                params[p].register_index == arg.SM50BindingSlot &&
                params[p].register_space == arg.SM50RegisterSpace &&
                ShaderVisibilityMatches(params[p].shader_visibility,
                                        D3D12_SHADER_VISIBILITY_PIXEL,
                                        pass == 0)) {
              root_idx = p;
              break;
            }
          }
        }
      }

      if (root_idx != ~0u && root_cbv_set[root_idx]) {
        gpu_address = root_cbvs[root_idx];
      } else if (dxmt_sig) {
        uint32_t table_root_idx = ~0u;
        uint32_t descriptor_offset = 0;
        if (dxmt_sig->FindDescriptorTableRangeForVisibility(
                D3D12_DESCRIPTOR_RANGE_TYPE_CBV, arg.SM50BindingSlot,
                arg.SM50RegisterSpace, D3D12_SHADER_VISIBILITY_PIXEL, &table_root_idx,
                &descriptor_offset) &&
            table_root_idx < 16 && root_table_set[table_root_idx]) {
          for (uint32_t h = 0; h < desc_heap_count; h++) {
            auto *heap = static_cast<MTLD3D12DescriptorHeap *>(desc_heaps[h]);
            if (!heap) continue;
            auto *desc = heap->GetDescriptorFromGPUHandle(
                root_tables[table_root_idx], descriptor_offset);
            if (desc && desc->cbv.BufferLocation) {
              gpu_address = desc->cbv.BufferLocation;
              break;
            }
          }
        }
      }

      cbv_table_data[arg.StructurePtrOffset] = gpu_address;
      QTRACE("BuildConstantBufferTable: cb slot=%u offset=%u gpu=0x%llx",
             arg.SM50BindingSlot, arg.StructurePtrOffset,
             (unsigned long long)gpu_address);

      if (gpu_address && render_enc_open) {
        auto *res = device->LookupResourceByGPUAddress(gpu_address);
        if (res && res->GetMTLBuffer().handle) {
          render_enc.useResource(res->GetMTLBuffer(), WMTResourceUsageRead,
                                 WMTRenderStageFragment);
        }
      }
    }

    if (qword_count == 0)
      return;

    if (!cbv_table_buf.handle) {
      WMTBufferInfo buf_info = {};
      buf_info.length = kConstantBufferMaxQwords * 8;
      buf_info.options = WMTResourceStorageModeShared;
      cbv_table_buf = device->GetDXMTDevice().device().newBuffer(buf_info);
    }
    if (cbv_table_buf.handle) {
      cbv_table_buf.updateContents(0, cbv_table_data, qword_count * 8);
      if (render_enc_open) {
        render_enc.setFragmentBuffer(cbv_table_buf, 0, kConstantBufferTableSlot);
        render_enc.useResource(cbv_table_buf, WMTResourceUsageRead,
                               WMTRenderStageFragment);
        QTRACE("BuildConstantBufferTable: bound slot=%u qwords=%u",
               kConstantBufferTableSlot, qword_count);
      }
    }
  }

  void BuildVertexConstantBufferTable(MTLD3D12Device *device) {
    if (!pso || pso->GetVSConstantBuffers().empty()) {
      return;
    }

    memset(vs_cbv_table_data, 0, sizeof(vs_cbv_table_data));

    auto *root_sig = pso->GetRootSignature();
    auto *dxmt_sig = root_sig ? static_cast<MTLD3D12RootSignature *>(root_sig) : nullptr;
    auto &cb_args = pso->GetVSConstantBuffers();
    uint32_t qword_count = 0;

    for (const auto &arg : cb_args) {
      if (arg.Type != SM50BindingType::ConstantBuffer ||
          arg.StructurePtrOffset >= kConstantBufferMaxQwords)
        continue;

      qword_count = std::max(qword_count, arg.StructurePtrOffset + 1);
      uint64_t gpu_address = 0;

      uint32_t root_idx = ~0u;
      if (dxmt_sig) {
        auto &params = dxmt_sig->GetParameters();
        for (uint32_t pass = 0; pass < 2 && root_idx == ~0u; pass++) {
          for (uint32_t p = 0; p < params.size() && p < 16; p++) {
            if (params[p].type == D3D12_ROOT_PARAMETER_TYPE_CBV &&
                params[p].register_index == arg.SM50BindingSlot &&
                params[p].register_space == arg.SM50RegisterSpace &&
                ShaderVisibilityMatches(params[p].shader_visibility,
                                        D3D12_SHADER_VISIBILITY_VERTEX,
                                        pass == 0)) {
              root_idx = p;
              break;
            }
          }
        }
      }

      if (root_idx != ~0u && root_cbv_set[root_idx]) {
        gpu_address = root_cbvs[root_idx];
      } else if (dxmt_sig) {
        uint32_t table_root_idx = ~0u;
        uint32_t descriptor_offset = 0;
        if (dxmt_sig->FindDescriptorTableRangeForVisibility(
                D3D12_DESCRIPTOR_RANGE_TYPE_CBV, arg.SM50BindingSlot,
                arg.SM50RegisterSpace, D3D12_SHADER_VISIBILITY_VERTEX, &table_root_idx,
                &descriptor_offset) &&
            table_root_idx < 16 && root_table_set[table_root_idx]) {
          for (uint32_t h = 0; h < desc_heap_count; h++) {
            auto *heap = static_cast<MTLD3D12DescriptorHeap *>(desc_heaps[h]);
            if (!heap) continue;
            auto *desc = heap->GetDescriptorFromGPUHandle(
                root_tables[table_root_idx], descriptor_offset);
            if (desc && desc->cbv.BufferLocation) {
              gpu_address = desc->cbv.BufferLocation;
              break;
            }
          }
        }
      }

      vs_cbv_table_data[arg.StructurePtrOffset] = gpu_address;
      QTRACE("BuildVertexConstantBufferTable: cb slot=%u offset=%u gpu=0x%llx",
             arg.SM50BindingSlot, arg.StructurePtrOffset,
             (unsigned long long)gpu_address);

      if (gpu_address && render_enc_open) {
        auto *res = device->LookupResourceByGPUAddress(gpu_address);
        if (res && res->GetMTLBuffer().handle) {
          render_enc.useResource(res->GetMTLBuffer(), WMTResourceUsageRead,
                                 WMTRenderStageVertex);
        }
      }
    }

    if (qword_count == 0)
      return;

    if (!vs_cbv_table_buf.handle) {
      WMTBufferInfo buf_info = {};
      buf_info.length = kConstantBufferMaxQwords * 8;
      buf_info.options = WMTResourceStorageModeShared;
      vs_cbv_table_buf = device->GetDXMTDevice().device().newBuffer(buf_info);
    }
    if (vs_cbv_table_buf.handle) {
      vs_cbv_table_buf.updateContents(0, vs_cbv_table_data, qword_count * 8);
      if (render_enc_open) {
        render_enc.setVertexBuffer(vs_cbv_table_buf, 0, kConstantBufferTableSlot);
        render_enc.useResource(vs_cbv_table_buf, WMTResourceUsageRead,
                               WMTRenderStageVertex);
        QTRACE("BuildVertexConstantBufferTable: bound slot=%u qwords=%u",
               kConstantBufferTableSlot, qword_count);
      }
    }
  }

  void BuildVertexArgumentBuffer(MTLD3D12Device *device) {
    if (!pso || pso->GetVSArguments().empty()) {
      return;
    }

    auto &args = pso->GetVSArguments();
    uint32_t qword_count = pso->GetVSReflection().ArgumentTableQwords;
    QTRACE("BuildVertexArgumentBuffer: %u args, %u qwords, NumArguments=%u",
           (unsigned)args.size(), qword_count,
           (unsigned)pso->GetVSReflection().NumArguments);
    if (qword_count == 0 || qword_count > kArgBufMaxQwords) {
      QTRACE("BuildVertexArgumentBuffer: invalid qword_count=%u", qword_count);
      return;
    }
    memset(vs_arg_buf_data, 0, qword_count * 8);

    auto *root_sig = pso->GetRootSignature();
    auto *dxmt_sig = root_sig ? static_cast<MTLD3D12RootSignature *>(root_sig) : nullptr;

    for (auto &arg : args) {
      uint32_t root_idx = ~0u;
      uint32_t descriptor_offset = 0;
      if (dxmt_sig) {
        D3D12_DESCRIPTOR_RANGE_TYPE range_type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        bool table_arg = true;
        if (arg.Type == SM50BindingType::SRV) {
          range_type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        } else if (arg.Type == SM50BindingType::Sampler) {
          range_type = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        } else if (arg.Type == SM50BindingType::UAV) {
          range_type = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        } else {
          table_arg = false;
        }
        if (table_arg) {
          dxmt_sig->FindDescriptorTableRangeForVisibility(
              range_type, arg.SM50BindingSlot, arg.SM50RegisterSpace,
              D3D12_SHADER_VISIBILITY_VERTEX, &root_idx, &descriptor_offset);
        }
      }
      if (root_idx == ~0u || !root_table_set[root_idx] || desc_heap_count == 0) {
        QTRACE("BuildVertexArgBuf: arg type=%d slot=%u root_idx=%u desc_off=%u table_set=%d heaps=%u skip",
          (int)arg.Type, arg.SM50BindingSlot, root_idx, descriptor_offset,
          root_idx != ~0u ? root_table_set[root_idx] : 0, desc_heap_count);
        continue;
      }

      for (uint32_t h = 0; h < desc_heap_count; h++) {
        auto *heap = static_cast<MTLD3D12DescriptorHeap *>(desc_heaps[h]);
        if (!heap) continue;
        auto *desc = heap->GetDescriptorFromGPUHandle(root_tables[root_idx], descriptor_offset);
        if (!desc) continue;

        if (arg.Type == SM50BindingType::SRV) {
          QTRACE("BuildVertexArgBuf: SRV root=%u desc_off=%u desc=%p res=%p flags=0x%x offset=%u",
            root_idx, descriptor_offset, (void*)desc,
            desc->resource ? (void*)desc->resource : nullptr,
            arg.Flags, arg.StructurePtrOffset);
          if (desc->resource) {
            auto *res = static_cast<MTLD3D12Resource *>(desc->resource);
            if ((arg.Flags & MTL_SM50_SHADER_ARGUMENT_BUFFER) && res->GetMTLBuffer().handle) {
              vs_arg_buf_data[arg.StructurePtrOffset] =
                  res->GetGPUVirtualAddress() + SRVBufferByteOffset(desc);
              vs_arg_buf_data[arg.StructurePtrOffset + 1] =
                  SRVBufferByteLength(desc, res);
              if (render_enc_open)
                render_enc.useResource(res->GetMTLBuffer(), WMTResourceUsageRead,
                                       WMTRenderStageVertex);
            } else if (res->GetMTLTexture().handle) {
              vs_arg_buf_data[arg.StructurePtrOffset] = res->GetTextureGPUResourceID();
              vs_arg_buf_data[arg.StructurePtrOffset + 1] =
                  TextureMetadata(SRVTextureArrayLength(desc, res), 0.0f);
              if (render_enc_open)
                render_enc.useResource(res->GetMTLTexture(),
                  (WMTResourceUsage)(WMTResourceUsageSample | WMTResourceUsageRead),
                  WMTRenderStageVertex);
            } else if (res->GetMTLBuffer().handle) {
              vs_arg_buf_data[arg.StructurePtrOffset] =
                  res->GetGPUVirtualAddress() + SRVBufferByteOffset(desc);
              vs_arg_buf_data[arg.StructurePtrOffset + 1] =
                  SRVBufferByteLength(desc, res);
              if (render_enc_open)
                render_enc.useResource(res->GetMTLBuffer(), WMTResourceUsageRead,
                                       WMTRenderStageVertex);
            }
          }
        } else if (arg.Type == SM50BindingType::Sampler) {
          QTRACE("BuildVertexArgBuf: Sampler root=%u desc_off=%u desc_type=%u gpu_id=0x%llx offset=%u",
            root_idx, descriptor_offset, desc->type,
            (unsigned long long)desc->metal_sampler_gpu_id,
            arg.StructurePtrOffset);
          if (desc->type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER && desc->metal_sampler_gpu_id) {
            vs_arg_buf_data[arg.StructurePtrOffset] = desc->metal_sampler_gpu_id;
            vs_arg_buf_data[arg.StructurePtrOffset + 1] =
                SamplerCubeGPUResourceID(desc);
            vs_arg_buf_data[arg.StructurePtrOffset + 2] =
                SamplerLodBiasBits(desc);
          }
        } else if (arg.Type == SM50BindingType::UAV && desc->resource) {
          auto *res = static_cast<MTLD3D12Resource *>(desc->resource);
          QTRACE("BuildVertexArgBuf: UAV root=%u desc_off=%u desc=%p res=%p flags=0x%x offset=%u",
            root_idx, descriptor_offset, (void*)desc, (void*)res,
            arg.Flags, arg.StructurePtrOffset);
          if ((arg.Flags & MTL_SM50_SHADER_ARGUMENT_BUFFER) && res->GetMTLBuffer().handle) {
            vs_arg_buf_data[arg.StructurePtrOffset] =
                res->GetGPUVirtualAddress() + UAVBufferByteOffset(desc);
            vs_arg_buf_data[arg.StructurePtrOffset + 1] =
                UAVBufferByteLength(desc, res);
            if (render_enc_open)
              render_enc.useResource(res->GetMTLBuffer(),
                (WMTResourceUsage)(WMTResourceUsageRead | WMTResourceUsageWrite),
                WMTRenderStageVertex);
          } else if (res->GetMTLTexture().handle) {
            vs_arg_buf_data[arg.StructurePtrOffset] = res->GetTextureGPUResourceID();
            vs_arg_buf_data[arg.StructurePtrOffset + 1] =
                TextureMetadata(UAVTextureArrayLength(desc, res), 0.0f);
            if (render_enc_open)
              render_enc.useResource(res->GetMTLTexture(),
                (WMTResourceUsage)(WMTResourceUsageRead | WMTResourceUsageWrite),
                WMTRenderStageVertex);
          }
        }
      }
    }

    if (!vs_arg_buf.handle) {
      WMTBufferInfo buf_info = {};
      buf_info.length = kArgBufMaxQwords * 8;
      buf_info.options = WMTResourceStorageModeShared;
      vs_arg_buf = device->GetDXMTDevice().device().newBuffer(buf_info);
    }
    if (vs_arg_buf.handle) {
      vs_arg_buf.updateContents(0, vs_arg_buf_data, qword_count * 8);
      if (render_enc_open) {
        render_enc.setVertexBuffer(vs_arg_buf, 0, kArgBufSlot);
        render_enc.useResource(vs_arg_buf, WMTResourceUsageRead,
                               WMTRenderStageVertex);
        QTRACE("BuildVertexArgumentBuffer: bound slot=%u qwords=%u handle=%llu",
               kArgBufSlot, qword_count,
               (unsigned long long)vs_arg_buf.handle);
      }
    }
  }

  uint32_t BuildComputeConstantBufferTable(MTLD3D12Device *device) {
    if (!pso || pso->GetCSConstantBuffers().empty())
      return 0;

    memset(comp_cbv_table_data, 0, sizeof(comp_cbv_table_data));
    auto *dxmt_sig = compute_root_sig
        ? compute_root_sig
        : static_cast<MTLD3D12RootSignature *>(pso->GetRootSignature());
    auto &cb_args = pso->GetCSConstantBuffers();
    uint32_t qword_count = 0;

    for (const auto &arg : cb_args) {
      if (arg.Type != SM50BindingType::ConstantBuffer ||
          arg.StructurePtrOffset >= kConstantBufferMaxQwords)
        continue;

      qword_count = std::max(qword_count, arg.StructurePtrOffset + 1);
      uint64_t gpu_address = 0;

      uint32_t root_idx = ~0u;
      if (dxmt_sig) {
        auto &params = dxmt_sig->GetParameters();
        for (uint32_t p = 0; p < params.size() && p < 16; p++) {
          if (params[p].type == D3D12_ROOT_PARAMETER_TYPE_CBV &&
              params[p].register_index == arg.SM50BindingSlot &&
              params[p].register_space == arg.SM50RegisterSpace) {
            root_idx = p;
            break;
          }
        }
      }

      if (root_idx != ~0u && comp_cbv_set[root_idx]) {
        gpu_address = comp_cbvs[root_idx];
      } else if (root_idx != ~0u && root_cbv_set[root_idx]) {
        gpu_address = root_cbvs[root_idx];
      } else if (dxmt_sig) {
        uint32_t table_root_idx = ~0u;
        uint32_t descriptor_offset = 0;
        if (dxmt_sig->FindDescriptorTableRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                                               arg.SM50BindingSlot,
                                               arg.SM50RegisterSpace,
                                               &table_root_idx,
                                               &descriptor_offset) &&
            table_root_idx < 16) {
          bool table_set = comp_table_set[table_root_idx] || root_table_set[table_root_idx];
          D3D12_GPU_DESCRIPTOR_HANDLE table_handle =
              comp_table_set[table_root_idx] ? comp_tables[table_root_idx]
                                             : root_tables[table_root_idx];
          if (table_set) {
            for (uint32_t h = 0; h < desc_heap_count; h++) {
              auto *heap = static_cast<MTLD3D12DescriptorHeap *>(desc_heaps[h]);
              if (!heap) continue;
              auto *desc = heap->GetDescriptorFromGPUHandle(table_handle,
                                                            descriptor_offset);
              if (desc && desc->cbv.BufferLocation) {
                gpu_address = desc->cbv.BufferLocation;
                break;
              }
            }
          }
        }
      }

      comp_cbv_table_data[arg.StructurePtrOffset] = gpu_address;
      QTRACE("BuildComputeCBVTable: cb slot=%u offset=%u gpu=0x%llx",
             arg.SM50BindingSlot, arg.StructurePtrOffset,
             (unsigned long long)gpu_address);
    }

    if (qword_count == 0)
      return 0;

    if (!comp_cbv_table_buf.handle) {
      WMTBufferInfo buf_info = {};
      buf_info.length = kConstantBufferMaxQwords * 8;
      buf_info.options = WMTResourceStorageModeShared;
      comp_cbv_table_buf = device->GetDXMTDevice().device().newBuffer(buf_info);
    }
    if (comp_cbv_table_buf.handle) {
      comp_cbv_table_buf.updateContents(0, comp_cbv_table_data, qword_count * 8);
      QTRACE("BuildComputeCBVTable: wrote qwords=%u", qword_count);
      return qword_count;
    }
    return 0;
  }

  uint32_t BuildComputeArgumentBuffer(MTLD3D12Device *device) {
    if (!pso || pso->GetCSArguments().empty())
      return 0;

    uint32_t qword_count = pso->GetCSReflection().ArgumentTableQwords;
    if (qword_count == 0 || qword_count > kArgBufMaxQwords) {
      QTRACE("BuildComputeArgBuf: invalid qword_count=%u", qword_count);
      return 0;
    }
    memset(comp_arg_buf_data, 0, qword_count * 8);

    auto *dxmt_sig = compute_root_sig
        ? compute_root_sig
        : static_cast<MTLD3D12RootSignature *>(pso->GetRootSignature());

    for (const auto &arg : pso->GetCSArguments()) {
      D3D12_DESCRIPTOR_RANGE_TYPE range_type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
      bool table_arg = true;
      if (arg.Type == SM50BindingType::SRV) {
        range_type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
      } else if (arg.Type == SM50BindingType::Sampler) {
        range_type = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
      } else if (arg.Type == SM50BindingType::UAV) {
        range_type = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
      } else {
        table_arg = false;
      }

      uint32_t root_idx = ~0u;
      uint32_t descriptor_offset = 0;
      if (table_arg && dxmt_sig) {
        dxmt_sig->FindDescriptorTableRange(range_type, arg.SM50BindingSlot,
                                           arg.SM50RegisterSpace,
                                           &root_idx, &descriptor_offset);
      }
      if (root_idx == ~0u || root_idx >= 16 ||
          !(comp_table_set[root_idx] || root_table_set[root_idx]) ||
          desc_heap_count == 0) {
        QTRACE("BuildComputeArgBuf: arg type=%d slot=%u root_idx=%u desc_off=%u skip",
               (int)arg.Type, arg.SM50BindingSlot, root_idx, descriptor_offset);
        continue;
      }

      D3D12_GPU_DESCRIPTOR_HANDLE table_handle =
          comp_table_set[root_idx] ? comp_tables[root_idx] : root_tables[root_idx];
      for (uint32_t h = 0; h < desc_heap_count; h++) {
        auto *heap = static_cast<MTLD3D12DescriptorHeap *>(desc_heaps[h]);
        if (!heap) continue;
        auto *desc = heap->GetDescriptorFromGPUHandle(table_handle, descriptor_offset);
        if (!desc) continue;

        if (arg.Type == SM50BindingType::Sampler) {
          QTRACE("BuildComputeArgBuf: Sampler root=%u desc_off=%u desc_type=%u gpu_id=0x%llx offset=%u",
                 root_idx, descriptor_offset, desc->type,
                 (unsigned long long)desc->metal_sampler_gpu_id,
                 arg.StructurePtrOffset);
          if (desc->type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER &&
              desc->metal_sampler_gpu_id) {
            comp_arg_buf_data[arg.StructurePtrOffset] = desc->metal_sampler_gpu_id;
            comp_arg_buf_data[arg.StructurePtrOffset + 1] =
                SamplerCubeGPUResourceID(desc);
            comp_arg_buf_data[arg.StructurePtrOffset + 2] =
                SamplerLodBiasBits(desc);
          }
          continue;
        }

        if (!desc->resource)
          continue;
        auto *res = static_cast<MTLD3D12Resource *>(desc->resource);
        QTRACE("BuildComputeArgBuf: res arg type=%d root=%u desc_off=%u res=%p flags=0x%x offset=%u",
               (int)arg.Type, root_idx, descriptor_offset, (void*)res,
               arg.Flags, arg.StructurePtrOffset);
        if ((arg.Flags & MTL_SM50_SHADER_ARGUMENT_BUFFER) &&
            res->GetMTLBuffer().handle) {
          if (arg.Type == SM50BindingType::UAV) {
            comp_arg_buf_data[arg.StructurePtrOffset] =
                res->GetGPUVirtualAddress() + UAVBufferByteOffset(desc);
            comp_arg_buf_data[arg.StructurePtrOffset + 1] =
                UAVBufferByteLength(desc, res);
          } else {
            comp_arg_buf_data[arg.StructurePtrOffset] =
                res->GetGPUVirtualAddress() + SRVBufferByteOffset(desc);
            comp_arg_buf_data[arg.StructurePtrOffset + 1] =
                SRVBufferByteLength(desc, res);
          }
        } else if (res->GetMTLTexture().handle) {
          comp_arg_buf_data[arg.StructurePtrOffset] = res->GetTextureGPUResourceID();
          comp_arg_buf_data[arg.StructurePtrOffset + 1] =
              arg.Type == SM50BindingType::UAV
                  ? TextureMetadata(UAVTextureArrayLength(desc, res), 0.0f)
                  : TextureMetadata(SRVTextureArrayLength(desc, res), 0.0f);
        }
      }
    }

    if (!comp_arg_buf.handle) {
      WMTBufferInfo buf_info = {};
      buf_info.length = kArgBufMaxQwords * 8;
      buf_info.options = WMTResourceStorageModeShared;
      comp_arg_buf = device->GetDXMTDevice().device().newBuffer(buf_info);
    }
    if (comp_arg_buf.handle) {
      comp_arg_buf.updateContents(0, comp_arg_buf_data, qword_count * 8);
      QTRACE("BuildComputeArgBuf: wrote qwords=%u", qword_count);
      return qword_count;
    }
    return 0;
  }

  void CloseRenderEncoder() {
    if (render_enc_open) {
      ENC_END(render_enc.handle);
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
        auto tex = res->GetMTLTexture();
        QTRACE("EnsureRenderEncoder: rt[%u] desc=%p res=%p tex=%llu",
               i, (void*)desc, (void*)res,
               (unsigned long long)tex.handle);
        if (tex.handle) {
          rp.colors[i].texture = tex.handle;
          has_valid_rt = true;
        }
      }
    }

    if (has_dsv) {
      auto *desc = reinterpret_cast<const D3D12Descriptor *>(dsv_handle.ptr);
      if (desc && desc->resource) {
        auto *res = static_cast<MTLD3D12Resource *>(desc->resource);
        QTRACE("EnsureRenderEncoder: dsv desc=%p res=%p tex=%llu",
               (void*)desc, (void*)res,
               (unsigned long long)res->GetMTLTexture().handle);
        if (res->GetMTLTexture().handle) {
          rp.depth.texture = res->GetMTLTexture().handle;
          if (DSVHasStencil(desc))
            rp.stencil.texture = res->GetMTLTexture().handle;
          has_valid_rt = true;
        }
      }
    }

    if (!has_valid_rt) {
      QTRACE("EnsureRenderEncoder: no valid RT texture found, skipping");
      return;
    }

    QTRACE("EnsureRenderEncoder: creating render encoder rt_count=%u pso=%p compiled=%d stage=%s detail=%s",
           rt_count, (void*)pso, pso ? pso->IsCompiled() : 0,
           pso ? pso->GetCompileFailureStage() : "no_pso",
           pso ? pso->GetCompileFailureDetail() : "");
    render_enc = cmdbuf.renderCommandEncoder(rp);
    ENC_CREATE("render_ensure", render_enc.handle);
    if (!render_enc.handle) {
      QTRACE("EnsureRenderEncoder: FAILED to create render encoder!");
      return;
    }
    render_enc_open = true;

    if (pso && pso->IsCompiled() && pso->GetRenderPSO().handle) {
      render_enc.setRenderPipelineState(pso->GetRenderPSO());
      if (pso->IsDepthStencilEnabled() && pso->GetDepthStencilState().handle) {
        render_enc.setDepthStencilState(pso->GetDepthStencilState());
      }
      ApplyFixedFunctionState();
    } else {
      QTRACE("EnsureRenderEncoder: RENDER_PSO_NOT_BOUND pso=%p compiled=%d render_handle=%llu stage=%s detail=%s",
             (void*)pso, pso ? pso->IsCompiled() : 0,
             (unsigned long long)(pso ? pso->GetRenderPSO().handle : 0),
             pso ? pso->GetCompileFailureStage() : "no_pso",
             pso ? pso->GetCompileFailureDetail() : "");
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

    if (scissor_count > 0) {
      const auto &rect = scissor_rects[0];
      LONG left = std::max<LONG>(0, rect.left);
      LONG top = std::max<LONG>(0, rect.top);
      LONG right = std::max<LONG>(left, rect.right);
      LONG bottom = std::max<LONG>(top, rect.bottom);
      render_enc.setScissorRect({(uint64_t)left, (uint64_t)top,
                                 (uint64_t)(right - left),
                                 (uint64_t)(bottom - top)});
    }
  }

  void ApplyFixedFunctionState() {
    if (!render_enc_open || !pso)
      return;

    const auto &rast = pso->GetRasterizerDesc();
    WMTTriangleFillMode fill_mode =
        rast.FillMode == D3D12_FILL_MODE_WIREFRAME ? WMTTriangleFillModeLines
                                                   : WMTTriangleFillModeFill;
    WMTCullMode cull_mode = WMTCullModeNone;
    if (rast.CullMode == D3D12_CULL_MODE_BACK)
      cull_mode = WMTCullModeBack;
    else if (rast.CullMode == D3D12_CULL_MODE_FRONT)
      cull_mode = WMTCullModeFront;
    WMTDepthClipMode depth_clip =
        rast.DepthClipEnable ? WMTDepthClipModeClip : WMTDepthClipModeClamp;
    WMTWinding winding = rast.FrontCounterClockwise
                             ? WMTWindingCounterClockwise
                             : WMTWindingClockwise;
    render_enc.setRasterizerState(fill_mode, cull_mode, depth_clip, winding,
                                  (float)rast.DepthBias,
                                  rast.SlopeScaledDepthBias,
                                  rast.DepthBiasClamp);
    render_enc.setBlendFactorAndStencilRef(blend_factor, stencil_ref);
    QTRACE("ApplyFixedFunctionState: fill=%u cull=%u depth_clip=%u winding=%u blend=(%.3f,%.3f,%.3f,%.3f) stencil=%u",
           (unsigned)fill_mode, (unsigned)cull_mode, (unsigned)depth_clip,
           (unsigned)winding, blend_factor[0], blend_factor[1],
           blend_factor[2], blend_factor[3], stencil_ref);
  }

  void ApplyRootBindings(MTLD3D12Device *device) {
    if (!render_enc_open || !pso)
      return;

    uint32_t tex_slot = 0, samp_slot = 0;

    bool has_root_constants = false;
    for (uint32_t i = 0; i < 16; i++)
      has_root_constants |= root_constant_set[i] && root_constant_sizes[i] > 0;

    if (has_root_constants) {
      if (!root_constants_mtl_buf.handle) {
        WMTBufferInfo buf_info = {};
        buf_info.length = sizeof(root_constants_buf);
        buf_info.options = WMTResourceStorageModeShared;
        root_constants_mtl_buf = device->GetDXMTDevice().device().newBuffer(buf_info);
      }
      if (root_constants_mtl_buf.handle) {
        root_constants_mtl_buf.updateContents(0, root_constants_buf, sizeof(root_constants_buf));
        render_enc.useResource(root_constants_mtl_buf, WMTResourceUsageRead,
                               (WMTRenderStages)(WMTRenderStageVertex | WMTRenderStageFragment));
      }
    }

    for (uint32_t i = 0; i < 16; i++) {
      if (root_constant_set[i] && root_constant_sizes[i] > 0 && root_constants_mtl_buf.handle) {
        render_enc.setVertexBuffer(root_constants_mtl_buf, root_constant_offsets[i], i);
        render_enc.setFragmentBuffer(root_constants_mtl_buf, root_constant_offsets[i], i);
        QTRACE("ApplyRootBindings: constants idx=%u off=%u size=%u via buffer",
               i, root_constant_offsets[i], root_constant_sizes[i]);
      }

      if (root_cbv_set[i] && root_cbvs[i]) {
        auto *res = device->LookupResourceByGPUAddress(root_cbvs[i]);
        if (res && res->GetMTLBuffer().handle) {
          uint64_t offset = root_cbvs[i] - res->GetGPUVirtualAddress();
          render_enc.setVertexBuffer(res->GetMTLBuffer(), offset, i);
          render_enc.setFragmentBuffer(res->GetMTLBuffer(), offset, i);
        }
      }

      if (root_table_set[i] && desc_heap_count > 0) {
        for (uint32_t h = 0; h < desc_heap_count; h++) {
          auto *heap = static_cast<MTLD3D12DescriptorHeap *>(desc_heaps[h]);
          if (!heap) continue;
          auto *desc = heap->GetDescriptorFromGPUHandle(root_tables[i]);
          if (!desc) continue;
          if (desc->type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER && desc->metal_sampler.handle) {
            render_enc.setFragmentSamplerState(desc->metal_sampler, samp_slot++);
            continue;
          }
          if (!desc->resource) continue;
          auto *res = static_cast<MTLD3D12Resource *>(desc->resource);
          if (res->GetMTLBuffer().handle) {
            uint64_t off = 0;
            if (desc->cbv.BufferLocation) {
              auto *cbv_res = device->LookupResourceByGPUAddress(desc->cbv.BufferLocation);
              if (cbv_res) off = desc->cbv.BufferLocation - cbv_res->GetGPUVirtualAddress();
            }
            render_enc.setVertexBuffer(res->GetMTLBuffer(), off, i);
            render_enc.setFragmentBuffer(res->GetMTLBuffer(), off, i);
          } else if (res->GetMTLTexture().handle) {
            render_enc.setFragmentTexture(res->GetMTLTexture(), tex_slot++);
          }
        }
      }
    }
  }

  void ApplyVertexBuffers(MTLD3D12Device *device) {
    if (!render_enc_open)
      return;

    uint32_t slot_mask = pso ? pso->GetIAInputSlotMask() : 0;
    if (slot_mask) {
      memset(vertex_table_data, 0, sizeof(vertex_table_data));
      uint32_t table_index = 0;
      for (uint32_t slot = 0; slot < kVertexBufferSlotCount; slot++) {
        if (!(slot_mask & (1u << slot)))
          continue;

        auto &view = vbs[slot];
        auto *res = view.BufferLocation
                        ? device->LookupResourceByGPUAddress(view.BufferLocation)
                        : nullptr;
        if (res && res->GetMTLBuffer().handle) {
          vertex_table_data[table_index].buffer_handle = view.BufferLocation;
          vertex_table_data[table_index].stride = view.StrideInBytes;
          vertex_table_data[table_index].length = view.SizeInBytes;
          render_enc.useResource(res->GetMTLBuffer(), WMTResourceUsageRead,
                                 WMTRenderStageVertex);
          QTRACE("ApplyVertexBuffers: table[%u]<-slot=%u gpu=0x%llx size=%u stride=%u",
                 table_index, slot, (unsigned long long)view.BufferLocation,
                 view.SizeInBytes, view.StrideInBytes);
        } else {
          QTRACE("ApplyVertexBuffers: table[%u]<-slot=%u unresolved gpu=0x%llx",
                 table_index, slot, (unsigned long long)view.BufferLocation);
        }
        table_index++;
      }

      if (!vertex_table_buf.handle) {
        WMTBufferInfo buf_info = {};
        buf_info.length = sizeof(vertex_table_data);
        buf_info.options = WMTResourceStorageModeShared;
        vertex_table_buf = device->GetDXMTDevice().device().newBuffer(buf_info);
      }
      if (vertex_table_buf.handle) {
        vertex_table_buf.updateContents(0, vertex_table_data, sizeof(vertex_table_data));
        render_enc.setVertexBuffer(vertex_table_buf, 0, kVertexBufferTableSlot);
        render_enc.useResource(vertex_table_buf, WMTResourceUsageRead,
                               WMTRenderStageVertex);
        QTRACE("ApplyVertexBuffers: bound IA vertex table slot=%u mask=0x%x entries=%u",
               kVertexBufferTableSlot, slot_mask, table_index);
      }
      return;
    }

    for (uint32_t i = 0; i < kVertexBufferSlotCount; i++) {
      if (vbs[i].BufferLocation) {
        auto *res = device->LookupResourceByGPUAddress(vbs[i].BufferLocation);
        if (res && res->GetMTLBuffer().handle) {
          uint64_t offset = vbs[i].BufferLocation - res->GetGPUVirtualAddress();
          QTRACE("ApplyVertexBuffers: slot=%u gpu=0x%llx offset=%llu size=%u stride=%u",
                 i, (unsigned long long)vbs[i].BufferLocation,
                 (unsigned long long)offset, vbs[i].SizeInBytes,
                 vbs[i].StrideInBytes);
          render_enc.setVertexBuffer(res->GetMTLBuffer(), offset, i);
        } else {
          QTRACE("ApplyVertexBuffers: slot=%u gpu=0x%llx unresolved",
                 i, (unsigned long long)vbs[i].BufferLocation);
        }
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
  m_barrier_event = wmt_dev.newEvent();
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

     const auto cmds = list->GetCommands();
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
      if (header->size < sizeof(CmdHeader) || header->size > 65536 || offset + header->size > cmds.size()) {
        QTRACE("ECL: corrupt cmd at offset=%zu type=%d size=%zu cmds_size=%zu — skipping rest",
               offset, (int)header->type, header->size, cmds.size());
        break;
      }

      if ((uint32_t)header->type < 30)
        type_counts[(uint32_t)header->type]++;
      cmd_count++;

      if (cmd_count <= 5 || (cmd_count % 50) == 0)
        QTRACE("ECL cmd[%zu] type=%d size=%u offset=%zu", cmd_count, (int)header->type, (unsigned)header->size, offset);

      switch (header->type) {
      case CmdType::DrawInstanced: {
        auto *cmd = reinterpret_cast<const CmdDrawInstanced *>(header);
        st.EnsureRenderEncoder();
        st.ApplyRootBindings(m_device);
        st.BuildVertexConstantBufferTable(m_device);
        st.BuildVertexArgumentBuffer(m_device);
        st.BuildConstantBufferTable(m_device);
        st.BuildArgumentBuffer(m_device);
        if (st.render_enc_open && st.arg_buf.handle) {
          st.render_enc.setFragmentBuffer(st.arg_buf, 0, st.kArgBufSlot);
        }
        st.ApplyVertexBuffers(m_device);
        QTRACE("DrawInstanced v=%u i=%u enc_open=%d pso=%p compiled=%d stage=%s detail=%s",
               cmd->vertex_count, cmd->instance_count, st.render_enc_open,
               (void*)st.pso, st.pso ? st.pso->IsCompiled() : 0,
               st.pso ? st.pso->GetCompileFailureStage() : "no_pso",
               st.pso ? st.pso->GetCompileFailureDetail() : "");

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
        } else {
          QTRACE("DrawInstanced SKIPPED v=%u i=%u enc_open=%d pso=%p compiled=%d stage=%s detail=%s",
                 cmd->vertex_count, cmd->instance_count, st.render_enc_open,
                 (void*)st.pso, st.pso ? st.pso->IsCompiled() : 0,
                 st.pso ? st.pso->GetCompileFailureStage() : "no_pso",
                 st.pso ? st.pso->GetCompileFailureDetail() : "");
        }
        break;
      }
      case CmdType::DrawIndexedInstanced: {
        auto *cmd = reinterpret_cast<const CmdDrawIndexedInstanced *>(header);
        st.EnsureRenderEncoder();
        st.ApplyRootBindings(m_device);
        st.BuildVertexConstantBufferTable(m_device);
        st.BuildVertexArgumentBuffer(m_device);
        st.BuildConstantBufferTable(m_device);
        st.BuildArgumentBuffer(m_device);
        if (st.render_enc_open && st.arg_buf.handle) {
          st.render_enc.setFragmentBuffer(st.arg_buf, 0, st.kArgBufSlot);
        }
        st.ApplyVertexBuffers(m_device);

        if (cmd->instance_count > 0 && cmd->index_count > 0 && st.ib.BufferLocation) {
          auto *ib_res = m_device->LookupResourceByGPUAddress(st.ib.BufferLocation);
          if (!ib_res && st.ib.BufferLocation) {
            ib_res = reinterpret_cast<MTLD3D12Resource *>(st.ib.BufferLocation);
          }
          uint64_t index_buffer_offset = 0;
          if (ib_res) {
            index_buffer_offset = st.ib.BufferLocation - ib_res->GetGPUVirtualAddress();
            if (st.render_enc_open && ib_res->GetMTLBuffer().handle) {
              st.render_enc.useResource(ib_res->GetMTLBuffer(), WMTResourceUsageRead,
                                        WMTRenderStageVertex);
            }
          }
          QTRACE("DrawIndexedInstanced idx=%u inst=%u base_vertex=%d base_instance=%u ib_gpu=0x%llx ib_res=%p ib_off=%llu enc_open=%d pso=%p compiled=%d stage=%s detail=%s",
                 cmd->index_count, cmd->instance_count, cmd->base_vertex,
                 cmd->start_instance, (unsigned long long)st.ib.BufferLocation,
                 (void *)ib_res, (unsigned long long)index_buffer_offset,
                 st.render_enc_open, (void*)st.pso,
                 st.pso ? st.pso->IsCompiled() : 0,
                 st.pso ? st.pso->GetCompileFailureStage() : "no_pso",
                 st.pso ? st.pso->GetCompileFailureDetail() : "");
          struct wmtcmd_render_draw_indexed draw = {};
          draw.type = WMTRenderCommandDrawIndexed;
          draw.next.set(nullptr);
          draw.primitive_type = st.GetMetalPrimitiveType();
          draw.index_type = DXGIToWMTIndexFormat(st.ib.Format);
          draw.index_count = cmd->index_count;
          draw.index_buffer = ib_res ? ib_res->GetMTLBuffer().handle : NULL_OBJECT_HANDLE;
          draw.index_buffer_offset = index_buffer_offset;
          draw.instance_count = cmd->instance_count;
          draw.base_vertex = cmd->base_vertex;
          draw.base_instance = cmd->start_instance;
          st.render_enc.encodeCommands(
              reinterpret_cast<const wmtcmd_render_nop *>(&draw));
        } else {
          QTRACE("DrawIndexedInstanced SKIPPED idx=%u inst=%u ib_gpu=0x%llx enc_open=%d pso=%p compiled=%d stage=%s detail=%s",
                 cmd->index_count, cmd->instance_count,
                 (unsigned long long)st.ib.BufferLocation, st.render_enc_open,
                 (void*)st.pso, st.pso ? st.pso->IsCompiled() : 0,
                 st.pso ? st.pso->GetCompileFailureStage() : "no_pso",
                 st.pso ? st.pso->GetCompileFailureDetail() : "");
        }
        break;
      }
      case CmdType::Dispatch: {
        auto *cmd = reinterpret_cast<const CmdDispatch *>(header);
        QTRACE("Dispatch x=%u y=%u z=%u pso=%p compiled=%d compute=%d heaps=%u stage=%s detail=%s",
               cmd->x, cmd->y, cmd->z, (void*)st.pso,
               st.pso ? st.pso->IsCompiled() : 0,
               st.pso ? st.pso->IsCompute() : 0,
               st.desc_heap_count,
               st.pso ? st.pso->GetCompileFailureStage() : "no_pso",
               st.pso ? st.pso->GetCompileFailureDetail() : "");
        if (st.pso && st.pso->IsCompiled() && st.pso->IsCompute() &&
            st.pso->GetComputePSO().handle) {
          st.CloseRenderEncoder();
          auto comp = cmdbuf.computeCommandEncoder(false);
          ENC_CREATE("compute_dispatch", comp.handle);

          uint8_t cmd_buf[4096];
          uint8_t *cmd_ptr = cmd_buf;
          wmtcmd_compute_nop *chain_head = nullptr;
          wmtcmd_base *chain_tail = nullptr;

          auto append_cmd = [&](void *data, size_t sz) -> wmtcmd_base * {
            auto *c = (wmtcmd_base *)cmd_ptr;
            memcpy(cmd_ptr, data, sz);
            cmd_ptr += sz;
            c->next.set(nullptr);
            if (chain_tail)
              chain_tail->next.set(c);
            else
              chain_head = (wmtcmd_compute_nop *)c;
            chain_tail = c;
            return c;
          };

          struct wmtcmd_compute_setpso setpso = {};
          setpso.type = WMTComputeCommandSetPSO;
          setpso.pso = st.pso->GetComputePSO();
          setpso.threadgroup_size = st.pso->GetThreadgroupSize();
          append_cmd(&setpso, sizeof(setpso));

          uint32_t comp_cb_qwords = st.BuildComputeConstantBufferTable(m_device);
          if (comp_cb_qwords > 0 && st.comp_cbv_table_buf.handle) {
            struct wmtcmd_compute_setbuffer sbuf = {};
            sbuf.type = WMTComputeCommandSetBuffer;
            sbuf.buffer = st.comp_cbv_table_buf.handle;
            sbuf.offset = 0;
            sbuf.index = st.kConstantBufferTableSlot;
            append_cmd(&sbuf, sizeof(sbuf));
            QTRACE("Dispatch: bound compute CBV table slot=%u qwords=%u handle=%llu",
                   st.kConstantBufferTableSlot, comp_cb_qwords,
                   (unsigned long long)st.comp_cbv_table_buf.handle);
          }

          uint32_t comp_arg_qwords = st.BuildComputeArgumentBuffer(m_device);
          if (comp_arg_qwords > 0 && st.comp_arg_buf.handle) {
            struct wmtcmd_compute_setbuffer sbuf = {};
            sbuf.type = WMTComputeCommandSetBuffer;
            sbuf.buffer = st.comp_arg_buf.handle;
            sbuf.offset = 0;
            sbuf.index = st.kArgBufSlot;
            append_cmd(&sbuf, sizeof(sbuf));
            QTRACE("Dispatch: bound compute arg table slot=%u qwords=%u handle=%llu",
                   st.kArgBufSlot, comp_arg_qwords,
                   (unsigned long long)st.comp_arg_buf.handle);
          }

          bool is_uav_slot[16] = {};
          if (st.compute_root_sig) {
            auto &params = st.compute_root_sig->GetParameters();
            QTRACE("ECL UAV scan: root_sig=%p num_params=%u", (void*)st.compute_root_sig, (uint32_t)params.size());
            for (uint32_t p = 0; p < params.size() && p < 16; p++) {
              QTRACE("  param[%u] type=%u range_type=%u vis=%u", p, params[p].type, params[p].range_type, params[p].shader_visibility);
              if (params[p].type == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE &&
                  params[p].range_type == D3D12_DESCRIPTOR_RANGE_TYPE_UAV) {
                is_uav_slot[p] = true;
              } else if (params[p].type == D3D12_ROOT_PARAMETER_TYPE_UAV) {
                is_uav_slot[p] = true;
              }
            }
          } else {
            QTRACE("ECL UAV scan: no compute_root_sig set!");
          }

          for (uint32_t i = 0; i < 16; i++) {
            bool const_set = st.comp_constant_set[i] || st.root_constant_set[i];
            uint32_t const_size = st.comp_constant_set[i] ? st.comp_constant_sizes[i] : st.root_constant_sizes[i];
            uint32_t const_off = st.comp_constant_set[i] ? st.comp_constant_offsets[i] : st.root_constant_offsets[i];
            uint8_t *const_buf = st.comp_constant_set[i] ? st.comp_constants_buf : st.root_constants_buf;

            bool cbv_set = st.comp_cbv_set[i] || st.root_cbv_set[i];
            D3D12_GPU_VIRTUAL_ADDRESS cbv_addr = st.comp_cbv_set[i] ? st.comp_cbvs[i] : st.root_cbvs[i];

            bool tbl_set = st.comp_table_set[i] || st.root_table_set[i];
            D3D12_GPU_DESCRIPTOR_HANDLE tbl_handle = st.comp_table_set[i] ? st.comp_tables[i] : st.root_tables[i];

            if (const_set && const_size > 0) {
              struct wmtcmd_compute_setbytes sb = {};
              sb.type = WMTComputeCommandSetBytes;
              sb.length = const_size;
              sb.index = i;
              sb.bytes.ptr = (void *)(const_buf + const_off);
              append_cmd(&sb, sizeof(sb));
            }
            if (cbv_set && cbv_addr) {
              auto *res = m_device->LookupResourceByGPUAddress(cbv_addr);
              if (res && res->GetMTLBuffer().handle) {
                struct wmtcmd_compute_setbuffer sbuf = {};
                sbuf.type = WMTComputeCommandSetBuffer;
                sbuf.buffer = res->GetMTLBuffer().handle;
                sbuf.offset = cbv_addr - res->GetGPUVirtualAddress();
                sbuf.index = i;
                append_cmd(&sbuf, sizeof(sbuf));
                if (st.comp_uav_root[i]) {
                  struct wmtcmd_compute_useresource use = {};
                  use.type = WMTComputeCommandUseResource;
                  use.resource = res->GetMTLBuffer().handle;
                  use.usage = (WMTResourceUsage)(WMTResourceUsageRead | WMTResourceUsageWrite);
                  append_cmd(&use, sizeof(use));
                  QTRACE("  UAV UseResource root buf slot=%u handle=%llu", i, (unsigned long long)res->GetMTLBuffer().handle);
                }
              }
            }
            if (tbl_set && st.desc_heap_count > 0) {
              for (uint32_t h = 0; h < st.desc_heap_count; h++) {
                auto *heap = static_cast<MTLD3D12DescriptorHeap *>(st.desc_heaps[h]);
                if (heap) {
                  auto *desc = heap->GetDescriptorFromGPUHandle(tbl_handle);
                  QTRACE("  tbl[%u] heap=%u handle=0x%llx desc=%p res=%p", i, h,
                         (unsigned long long)tbl_handle.ptr, (void*)desc,
                         desc ? (void*)desc->resource : nullptr);
                  if (desc && desc->resource) {
                    auto *res = static_cast<MTLD3D12Resource *>(desc->resource);
                    if (res->GetMTLBuffer().handle) {
                      struct wmtcmd_compute_setbuffer sbuf = {};
                      sbuf.type = WMTComputeCommandSetBuffer;
                      sbuf.buffer = res->GetMTLBuffer().handle;
                      sbuf.offset = 0;
                      sbuf.index = i;
                      append_cmd(&sbuf, sizeof(sbuf));
                      if (is_uav_slot[i]) {
                        struct wmtcmd_compute_useresource use = {};
                        use.type = WMTComputeCommandUseResource;
                        use.resource = res->GetMTLBuffer().handle;
                        use.usage = (WMTResourceUsage)(WMTResourceUsageRead | WMTResourceUsageWrite);
                        append_cmd(&use, sizeof(use));
                      }
                    } else if (res->GetMTLTexture().handle) {
                      struct wmtcmd_compute_settexture stex = {};
                      stex.type = WMTComputeCommandSetTexture;
                      stex.texture = res->GetMTLTexture().handle;
                      stex.index = i;
                      append_cmd(&stex, sizeof(stex));
                      if (is_uav_slot[i]) {
                        QTRACE("  UAV UseResource tex slot=%u handle=%llu", i, (unsigned long long)res->GetMTLTexture().handle);
                        struct wmtcmd_compute_useresource use = {};
                        use.type = WMTComputeCommandUseResource;
                        use.resource = res->GetMTLTexture().handle;
                        use.usage = (WMTResourceUsage)(WMTResourceUsageRead | WMTResourceUsageWrite);
                        append_cmd(&use, sizeof(use));
                      }
                    }
                  }
                }
              }
            }
          }

          int num_consts = 0, num_cbvs = 0, num_tables = 0;
          for (uint32_t i = 0; i < 16; i++) {
            if ((st.comp_constant_set[i] || st.root_constant_set[i]) &&
                (st.comp_constant_sizes[i] > 0 || st.root_constant_sizes[i] > 0))
              num_consts++;
            if ((st.comp_cbv_set[i] && st.comp_cbvs[i]) ||
                (st.root_cbv_set[i] && st.root_cbvs[i]))
              num_cbvs++;
            if (st.comp_table_set[i] || st.root_table_set[i])
              num_tables++;
          }
          QTRACE("  bindings: consts=%d cbvs=%d tables=%d tg=%llux%llux%llu",
                 num_consts, num_cbvs, num_tables,
                 st.pso->GetThreadgroupSize().width,
                 st.pso->GetThreadgroupSize().height,
                 st.pso->GetThreadgroupSize().depth);

          struct wmtcmd_compute_dispatch disp = {};
          disp.type = WMTComputeCommandDispatch;
          disp.size = {(uint64_t)cmd->x, (uint64_t)cmd->y, (uint64_t)cmd->z};
          append_cmd(&disp, sizeof(disp));

          if (chain_head)
            comp.encodeCommands(chain_head);
          ENC_END(comp.handle);
          comp.endEncoding();
        } else {
          QTRACE("Dispatch SKIPPED x=%u y=%u z=%u pso=%p compiled=%d compute=%d stage=%s detail=%s",
                 cmd->x, cmd->y, cmd->z, (void*)st.pso,
                 st.pso ? st.pso->IsCompiled() : 0,
                 st.pso ? st.pso->IsCompute() : 0,
                 st.pso ? st.pso->GetCompileFailureStage() : "no_pso",
                 st.pso ? st.pso->GetCompileFailureDetail() : "");
        }
        break;
      }
      case CmdType::CopyBufferRegion: {
        auto *cmd = reinterpret_cast<const CmdCopyBufferRegion *>(header);
        QTRACE("CopyBufferRegion dst=%p +%llu src=%p +%llu bytes=%llu", (void*)cmd->dst, (unsigned long long)cmd->dst_offset, (void*)cmd->src, (unsigned long long)cmd->src_offset, (unsigned long long)cmd->byte_count);
        if (cmd->dst && cmd->src) {
          st.CloseRenderEncoder();
          auto *dst_res = static_cast<MTLD3D12Resource *>(cmd->dst);
          auto *src_res = static_cast<MTLD3D12Resource *>(cmd->src);
          if (dst_res->GetMTLBuffer().handle && src_res->GetMTLBuffer().handle) {
            auto blit = cmdbuf.blitCommandEncoder();
            ENC_CREATE("blit_copybuf", blit.handle);
            struct wmtcmd_blit_copy_from_buffer_to_buffer copy = {};
            copy.type = WMTBlitCommandCopyFromBufferToBuffer;
            copy.next.set(nullptr);
            copy.src = src_res->GetMTLBuffer().handle;
            copy.src_offset = cmd->src_offset;
            copy.dst = dst_res->GetMTLBuffer().handle;
            copy.dst_offset = cmd->dst_offset;
            copy.copy_length = cmd->byte_count;
            blit.encodeCommands(reinterpret_cast<const wmtcmd_blit_nop *>(&copy));
            ENC_END(blit.handle);
            blit.endEncoding();
          }
        }
        break;
      }
      case CmdType::CopyTextureRegion: {
        auto *cmd = reinterpret_cast<const CmdCopyTextureRegion *>(header);
        auto *dst_res = static_cast<MTLD3D12Resource *>(cmd->dst_resource);
        auto *src_res = static_cast<MTLD3D12Resource *>(cmd->src_resource);
        QTRACE("CopyTextureRegion dst=%p(%p) src=%p(%p) dst_type=%u src_type=%u",
          (void*)dst_res, dst_res ? (void*)dst_res->GetMTLTexture().handle : nullptr,
          (void*)src_res, src_res ? (void*)src_res->GetMTLTexture().handle : nullptr,
          cmd->dst_type, cmd->src_type);
        if (!dst_res || !src_res) break;

        QTRACE("CopyTextureRegion dst=%p src=%p dst_type=%u src_type=%u",
          (void*)dst_res, (void*)src_res, cmd->dst_type, cmd->src_type);

        st.CloseRenderEncoder();
        auto blit = cmdbuf.blitCommandEncoder();
        ENC_CREATE("blit_copytex", blit.handle);
        if (!blit.handle) {
          QTRACE("CopyTextureRegion: FAILED to create blit encoder");
          break;
        }

        bool src_is_buffer = (cmd->src_type == D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT);
        bool dst_is_buffer = (cmd->dst_type == D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT);

        auto src_tex = src_res->GetMTLTexture();
        auto dst_tex = dst_res->GetMTLTexture();
        auto src_buf = src_res->GetMTLBuffer();
        auto dst_buf = dst_res->GetMTLBuffer();

        if (!src_is_buffer && !src_tex.handle) src_is_buffer = (src_buf.handle != 0);
        if (!dst_is_buffer && !dst_tex.handle) dst_is_buffer = (dst_buf.handle != 0);

        QTRACE("CopyTextureRegion src_tex=%llu src_buf=%llu dst_tex=%llu dst_buf=%llu src_buf_flag=%d dst_buf_flag=%d",
          (unsigned long long)src_tex.handle, (unsigned long long)src_buf.handle,
          (unsigned long long)dst_tex.handle, (unsigned long long)dst_buf.handle,
          src_is_buffer, dst_is_buffer);

        UINT copy_w, copy_h, copy_d;
        if (cmd->has_src_box) {
          copy_w = cmd->src_box.right - cmd->src_box.left;
          copy_h = cmd->src_box.bottom - cmd->src_box.top;
          copy_d = cmd->src_box.back - cmd->src_box.front;
        } else {
          D3D12_RESOURCE_DESC tex_desc;
          if (!dst_is_buffer && dst_tex.handle) {
            dst_res->GetDesc(&tex_desc);
            copy_w = tex_desc.Width;
            copy_h = tex_desc.Height;
            copy_d = 1;
          } else if (!src_is_buffer && src_tex.handle) {
            src_res->GetDesc(&tex_desc);
            copy_w = tex_desc.Width;
            copy_h = tex_desc.Height;
            copy_d = 1;
          } else {
            copy_w = 1;
            copy_h = 1;
            copy_d = 1;
          }
          if (copy_w == 0) copy_w = 1;
          if (copy_h == 0) copy_h = 1;
        }

        if (src_is_buffer && !dst_is_buffer && dst_tex.handle) {
          UINT row_pitch = cmd->src_footprint_row_pitch;
          if (row_pitch == 0) row_pitch = copy_w * 4;
          struct wmtcmd_blit_copy_from_buffer_to_texture copy = {};
          copy.type = WMTBlitCommandCopyFromBufferToTexture;
          copy.next.set(nullptr);
          copy.src = src_buf.handle;
          copy.src_offset = cmd->src_offset;
          copy.bytes_per_row = row_pitch;
          copy.bytes_per_image = row_pitch * copy_h;
          copy.size = {copy_w, copy_h, copy_d};
          copy.dst = dst_tex.handle;
          copy.slice = 0;
          copy.level = 0;
          copy.origin = {cmd->dst_x, cmd->dst_y, cmd->dst_z};
          blit.encodeCommands(reinterpret_cast<const wmtcmd_blit_nop *>(&copy));
        } else if (!src_is_buffer && dst_is_buffer && src_tex.handle) {
          struct wmtcmd_blit_copy_from_texture_to_buffer copy = {};
          copy.type = WMTBlitCommandCopyFromTextureToBuffer;
          copy.next.set(nullptr);
          copy.src = src_tex.handle;
          copy.slice = 0;
          copy.level = 0;
          UINT src_x = cmd->has_src_box ? cmd->src_box.left : 0;
          UINT src_y = cmd->has_src_box ? cmd->src_box.top : 0;
          UINT src_z = cmd->has_src_box ? cmd->src_box.front : 0;
          copy.origin = {src_x, src_y, src_z};
          copy.size = {copy_w, copy_h, copy_d};
          copy.dst = dst_buf.handle;
          copy.offset = cmd->dst_offset;
          copy.bytes_per_row = cmd->dst_footprint_row_pitch;
          copy.bytes_per_image = cmd->dst_footprint_row_pitch * copy_h;
          blit.encodeCommands(reinterpret_cast<const wmtcmd_blit_nop *>(&copy));
        } else if (!src_is_buffer && !dst_is_buffer && src_tex.handle && dst_tex.handle) {
          struct wmtcmd_blit_copy_from_texture_to_texture copy = {};
          copy.type = WMTBlitCommandCopyFromTextureToTexture;
          copy.next.set(nullptr);
          copy.src = src_tex.handle;
          copy.src_slice = 0;
          copy.src_level = 0;
          UINT src_x = cmd->has_src_box ? cmd->src_box.left : 0;
          UINT src_y = cmd->has_src_box ? cmd->src_box.top : 0;
          UINT src_z = cmd->has_src_box ? cmd->src_box.front : 0;
          copy.src_origin = {src_x, src_y, src_z};
          copy.src_size = {copy_w, copy_h, copy_d};
          copy.dst = dst_tex.handle;
          copy.dst_slice = 0;
          copy.dst_level = 0;
          copy.dst_origin = {cmd->dst_x, cmd->dst_y, cmd->dst_z};
          blit.encodeCommands(reinterpret_cast<const wmtcmd_blit_nop *>(&copy));
        } else {
          QTRACE("CopyTextureRegion: unhandled buffer-to-buffer or null resources");
        }

        QTRACE("CopyTextureRegion: blit.endEncoding src_buf=%d dst_buf=%d w=%u h=%u d=%u",
          src_is_buffer, dst_is_buffer, copy_w, copy_h, copy_d);
        ENC_END(blit.handle);
        blit.endEncoding();
        break;
      }
      case CmdType::CopyResource: {
        auto *cmd = reinterpret_cast<const CmdCopyResource *>(header);
        auto *dst_res = static_cast<MTLD3D12Resource *>(cmd->dst);
        auto *src_res = static_cast<MTLD3D12Resource *>(cmd->src);
        if (!dst_res || !src_res) break;
        st.CloseRenderEncoder();

        if (dst_res->GetMTLBuffer().handle && src_res->GetMTLBuffer().handle) {
          auto blit = cmdbuf.blitCommandEncoder();
          ENC_CREATE("blit_copyres_buf", blit.handle);
          struct wmtcmd_blit_copy_from_buffer_to_buffer copy = {};
          copy.type = WMTBlitCommandCopyFromBufferToBuffer;
          copy.next.set(nullptr);
          copy.src = src_res->GetMTLBuffer().handle;
          copy.src_offset = 0;
          copy.dst = dst_res->GetMTLBuffer().handle;
          copy.dst_offset = 0;
          D3D12_RESOURCE_DESC src_desc;
          src_res->GetDesc(&src_desc);
          copy.copy_length = src_desc.Width;
          blit.encodeCommands(reinterpret_cast<const wmtcmd_blit_nop *>(&copy));
          ENC_END(blit.handle);
          blit.endEncoding();
        } else if (dst_res->GetMTLTexture().handle && src_res->GetMTLTexture().handle) {
          auto blit = cmdbuf.blitCommandEncoder();
          ENC_CREATE("blit_copyres_tex", blit.handle);
          D3D12_RESOURCE_DESC src_desc;
          src_res->GetDesc(&src_desc);
          struct wmtcmd_blit_copy_from_texture_to_texture copy = {};
          copy.type = WMTBlitCommandCopyFromTextureToTexture;
          copy.next.set(nullptr);
          copy.src = src_res->GetMTLTexture().handle;
          copy.src_slice = 0;
          copy.src_level = 0;
          copy.src_origin = {0, 0, 0};
          copy.src_size = {src_desc.Width, src_desc.Height, 1};
          copy.dst = dst_res->GetMTLTexture().handle;
          copy.dst_slice = 0;
          copy.dst_level = 0;
          copy.dst_origin = {0, 0, 0};
          blit.encodeCommands(reinterpret_cast<const wmtcmd_blit_nop *>(&copy));
          ENC_END(blit.handle);
          blit.endEncoding();
        }
        break;
      }
      case CmdType::SetPipelineState: {
        auto *cmd = reinterpret_cast<const CmdSetPipelineState *>(header);
        st.pso = static_cast<MTLD3D12PipelineState *>(cmd->pso);
        QTRACE("SetPipelineState pso=%p compiled=%d compute=%d stage=%s detail=%s",
               (void*)st.pso, st.pso ? st.pso->IsCompiled() : 0,
               st.pso ? st.pso->IsCompute() : 0,
               st.pso ? st.pso->GetCompileFailureStage() : "no_pso",
               st.pso ? st.pso->GetCompileFailureDetail() : "");
        if (st.render_enc_open && st.pso && st.pso->IsCompiled() &&
            st.pso->GetRenderPSO().handle) {
          st.render_enc.setRenderPipelineState(st.pso->GetRenderPSO());
          if (st.pso->IsDepthEnabled() && st.pso->GetDepthStencilState().handle) {
            st.render_enc.setDepthStencilState(st.pso->GetDepthStencilState());
          }
        }
        break;
      }
      case CmdType::ResourceBarrier: {
        auto *cmd = reinterpret_cast<const CmdResourceBarrier *>(header);
        QTRACE("ResourceBarrier count=%u", cmd->count);
        for (uint32_t i = 0; i < cmd->count; i++) {
          const auto &barrier = cmd->barriers[i];
          if (barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) {
            QTRACE("  barrier[%u] transition res=%p sub=%u before=0x%x after=0x%x flags=0x%x",
                   i, (void*)barrier.Transition.pResource,
                   barrier.Transition.Subresource,
                   barrier.Transition.StateBefore,
                   barrier.Transition.StateAfter,
                   barrier.Flags);
          } else if (barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_UAV) {
            QTRACE("  barrier[%u] uav res=%p flags=0x%x",
                   i, (void*)barrier.UAV.pResource, barrier.Flags);
          } else if (barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_ALIASING) {
            QTRACE("  barrier[%u] alias before=%p after=%p flags=0x%x",
                   i, (void*)barrier.Aliasing.pResourceBefore,
                   (void*)barrier.Aliasing.pResourceAfter,
                   barrier.Flags);
          } else {
            QTRACE("  barrier[%u] type=%u flags=0x%x",
                   i, barrier.Type, barrier.Flags);
          }
        }

        st.CloseRenderEncoder();
        if (m_barrier_event.handle) {
          uint64_t seq = ++m_barrier_seq;
          QTRACE("ResourceBarrier queue-order seq=%llu event=%llu",
                 (unsigned long long)seq,
                 (unsigned long long)m_barrier_event.handle);
          cmdbuf.encodeSignalEvent(m_barrier_event, seq);
          cmdbuf.encodeWaitForEvent(m_barrier_event, seq);
        } else {
          QTRACE("ResourceBarrier queue-order skipped: no event");
        }
        break;
      }
      case CmdType::OMSetRenderTargets: {
        auto *cmd = reinterpret_cast<const CmdOMSetRenderTargets *>(header);
        st.CloseRenderEncoder();
        st.rt_count = cmd->rt_count;
        QTRACE("OMSetRenderTargets count=%u single=%u has_dsv=%u",
               cmd->rt_count, cmd->single_handle ? 1 : 0,
               cmd->has_dsv ? 1 : 0);
        for (uint32_t i = 0; i < cmd->rt_count && i < 8; i++) {
          st.rt_handles[i] = cmd->rts[i];
          auto *desc = reinterpret_cast<const D3D12Descriptor *>(st.rt_handles[i].ptr);
          auto *res = desc ? static_cast<MTLD3D12Resource *>(desc->resource) : nullptr;
          QTRACE("OMSetRenderTargets rt[%u] handle=0x%llx desc=%p res=%p tex=%llu",
                 i, (unsigned long long)st.rt_handles[i].ptr,
                 (void*)desc, (void*)res,
                 res ? (unsigned long long)res->GetMTLTexture().handle : 0ull);
        }
        st.has_dsv = cmd->has_dsv;
        if (cmd->has_dsv) {
          st.dsv_handle = cmd->dsv;
          auto *desc = reinterpret_cast<const D3D12Descriptor *>(st.dsv_handle.ptr);
          auto *res = desc ? static_cast<MTLD3D12Resource *>(desc->resource) : nullptr;
          QTRACE("OMSetRenderTargets dsv handle=0x%llx desc=%p res=%p tex=%llu",
                 (unsigned long long)st.dsv_handle.ptr,
                 (void*)desc, (void*)res,
                 res ? (unsigned long long)res->GetMTLTexture().handle : 0ull);
        }
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

        {
          auto *desc = reinterpret_cast<const D3D12Descriptor *>(cmd->rtv.ptr);
          if (desc && desc->resource) {
            auto *res = static_cast<MTLD3D12Resource *>(desc->resource);
            QTRACE("ClearRenderTargetView handle=0x%llx desc=%p res=%p tex=%llu color=%f,%f,%f,%f",
                   (unsigned long long)cmd->rtv.ptr, (void*)desc,
                   (void*)res,
                   (unsigned long long)res->GetMTLTexture().handle,
                   cmd->color[0], cmd->color[1], cmd->color[2], cmd->color[3]);
            if (res->GetMTLTexture().handle) {
              rp.colors[0].texture = res->GetMTLTexture().handle;
              rp.colors[0].load_action = WMTLoadActionClear;
              rp.colors[0].store_action = WMTStoreActionStore;
              rp.colors[0].clear_color = {cmd->color[0], cmd->color[1],
                                          cmd->color[2], cmd->color[3]};
            }
          }
        }

        for (uint32_t i = 0; i < st.rt_count && i < 8; i++) {
          if (rt_handles_match(st.rt_handles[i], cmd->rtv))
            continue;
          auto *desc = reinterpret_cast<const D3D12Descriptor *>(st.rt_handles[i].ptr);
          if (desc && desc->resource) {
            auto *res = static_cast<MTLD3D12Resource *>(desc->resource);
            if (res->GetMTLTexture().handle && !rp.colors[i].texture) {
              rp.colors[i].texture = res->GetMTLTexture().handle;
              rp.colors[i].load_action = WMTLoadActionLoad;
              rp.colors[i].store_action = WMTStoreActionStore;
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
              if (DSVHasStencil(desc)) {
                rp.stencil.texture = res->GetMTLTexture().handle;
                rp.stencil.load_action = WMTLoadActionLoad;
                rp.stencil.store_action = WMTStoreActionStore;
              }
            }
          }
        }

        auto enc = cmdbuf.renderCommandEncoder(rp);
        ENC_CREATE("render_clearrtv", enc.handle);
        ENC_END(enc.handle);
        enc.endEncoding();
        break;
      }
      case CmdType::ClearDepthStencilView: {
        auto *cmd = reinterpret_cast<const CmdClearDSV *>(header);
        st.CloseRenderEncoder();

        WMTRenderPassInfo rp = {};
        for (uint32_t i = 0; i < 8; i++) {
          rp.colors[i].texture = NULL_OBJECT_HANDLE;
          rp.colors[i].load_action = WMTLoadActionDontCare;
          rp.colors[i].store_action = WMTStoreActionDontCare;
        }

        for (uint32_t i = 0; i < st.rt_count && i < 8; i++) {
          auto *desc = reinterpret_cast<const D3D12Descriptor *>(st.rt_handles[i].ptr);
          if (desc && desc->resource) {
            auto *res = static_cast<MTLD3D12Resource *>(desc->resource);
            if (res->GetMTLTexture().handle) {
              rp.colors[i].texture = res->GetMTLTexture().handle;
              rp.colors[i].load_action = WMTLoadActionLoad;
              rp.colors[i].store_action = WMTStoreActionStore;
            }
          }
        }

        rp.depth.texture = NULL_OBJECT_HANDLE;
        rp.depth.load_action = WMTLoadActionDontCare;
        rp.depth.store_action = WMTStoreActionDontCare;
        rp.stencil.texture = NULL_OBJECT_HANDLE;
        rp.stencil.load_action = WMTLoadActionDontCare;
        rp.stencil.store_action = WMTStoreActionDontCare;

        {
          auto *desc = reinterpret_cast<const D3D12Descriptor *>(cmd->dsv.ptr);
          if (desc && desc->resource) {
            auto *res = static_cast<MTLD3D12Resource *>(desc->resource);
            if (res->GetMTLTexture().handle) {
              rp.depth.texture = res->GetMTLTexture().handle;
              rp.depth.load_action = (cmd->flags & D3D12_CLEAR_FLAG_DEPTH)
                                         ? WMTLoadActionClear
                                         : WMTLoadActionLoad;
              rp.depth.store_action = WMTStoreActionStore;
              if (cmd->flags & D3D12_CLEAR_FLAG_DEPTH)
                rp.depth.clear_depth = cmd->depth;
              if (DSVHasStencil(desc)) {
                rp.stencil.texture = res->GetMTLTexture().handle;
                rp.stencil.load_action = (cmd->flags & D3D12_CLEAR_FLAG_STENCIL)
                                             ? WMTLoadActionClear
                                             : WMTLoadActionLoad;
                rp.stencil.store_action = WMTStoreActionStore;
                if (cmd->flags & D3D12_CLEAR_FLAG_STENCIL)
                  rp.stencil.clear_stencil = cmd->stencil;
              }
              QTRACE("ClearDepthStencilView handle=0x%llx flags=0x%x stencil_attached=%d depth=%f stencil=%u",
                     (unsigned long long)cmd->dsv.ptr, cmd->flags,
                     DSVHasStencil(desc), cmd->depth, cmd->stencil);
            }
          }
        }

        auto enc = cmdbuf.renderCommandEncoder(rp);
        ENC_CREATE("render_cleardsv", enc.handle);
        ENC_END(enc.handle);
        enc.endEncoding();
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
        auto *rects = reinterpret_cast<const D3D12_RECT *>(
            reinterpret_cast<const uint8_t *>(cmd) +
            sizeof(CmdRSSetScissorRects) - sizeof(D3D12_RECT));
        st.scissor_count = cmd->count > 16 ? 16 : cmd->count;
        for (uint32_t i = 0; i < st.scissor_count; i++)
          st.scissor_rects[i] = rects[i];
        if (st.render_enc_open && st.scissor_count > 0) {
          const auto &rect = st.scissor_rects[0];
          LONG left = std::max<LONG>(0, rect.left);
          LONG top = std::max<LONG>(0, rect.top);
          LONG right = std::max<LONG>(left, rect.right);
          LONG bottom = std::max<LONG>(top, rect.bottom);
          st.render_enc.setScissorRect({(uint64_t)left, (uint64_t)top,
                                        (uint64_t)(right - left),
                                        (uint64_t)(bottom - top)});
        }
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
        QTRACE("SetGraphicsRoot32BitConstants idx=%u count=%u", cmd->root_param_index, cmd->count);
        if (cmd->root_param_index < 16) {
          uint32_t sz = cmd->count * 4;
          uint32_t local_off = cmd->dst_offset * 4;
          uint32_t off = cmd->root_param_index * st.kRootConstantBytes + local_off;
          if (local_off + sz <= st.kRootConstantBytes &&
              off + sz <= sizeof(st.root_constants_buf)) {
            memcpy(st.root_constants_buf + off, cmd->data, sz);
            st.root_constant_offsets[cmd->root_param_index] =
                cmd->root_param_index * st.kRootConstantBytes;
            st.root_constant_sizes[cmd->root_param_index] =
                std::max(st.root_constant_sizes[cmd->root_param_index],
                         local_off + sz);
            st.root_constant_set[cmd->root_param_index] = true;
          } else {
            QTRACE("SetGraphicsRoot32BitConstants idx=%u overflow local_off=%u size=%u",
                   cmd->root_param_index, local_off, sz);
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
      case CmdType::SetGraphicsRootShaderResourceView: {
        auto *cmd = reinterpret_cast<const CmdSetRootCBV *>(header);
        if (cmd->root_param_index < 16) {
          st.root_cbvs[cmd->root_param_index] = cmd->address;
          st.root_cbv_set[cmd->root_param_index] = true;
        }
        break;
      }
      case CmdType::SetGraphicsRootUnorderedAccessView: {
        auto *cmd = reinterpret_cast<const CmdSetRootCBV *>(header);
        if (cmd->root_param_index < 16) {
          st.root_cbvs[cmd->root_param_index] = cmd->address;
          st.root_cbv_set[cmd->root_param_index] = true;
        }
        break;
      }
      case CmdType::SetGraphicsRootDescriptorTable: {
        auto *cmd = reinterpret_cast<const CmdSetRootDescriptorTable *>(header);
        QTRACE("SetGraphicsRootDescriptorTable idx=%u handle=0x%llx", cmd->root_param_index, (unsigned long long)cmd->base_descriptor.ptr);
        if (cmd->root_param_index < 16) {
          st.root_tables[cmd->root_param_index] = cmd->base_descriptor;
          st.root_table_set[cmd->root_param_index] = true;
        }
        break;
      }
      case CmdType::SetComputeRootSignature: {
        auto *cmd = reinterpret_cast<const CmdSetRootSignature *>(header);
        st.compute_root_sig = static_cast<MTLD3D12RootSignature *>(cmd->root_sig);
        break;
      }
      case CmdType::SetComputeRoot32BitConstants: {
        auto *cmd = reinterpret_cast<const CmdSetRoot32BitConstants *>(header);
        if (cmd->root_param_index < 16) {
          uint32_t sz = cmd->count * 4;
          uint32_t local_off = cmd->dst_offset * 4;
          uint32_t off = cmd->root_param_index * st.kRootConstantBytes + local_off;
          if (local_off + sz <= st.kRootConstantBytes &&
              off + sz <= sizeof(st.comp_constants_buf)) {
            memcpy(st.comp_constants_buf + off, cmd->data, sz);
            st.comp_constant_offsets[cmd->root_param_index] =
                cmd->root_param_index * st.kRootConstantBytes;
            st.comp_constant_sizes[cmd->root_param_index] =
                std::max(st.comp_constant_sizes[cmd->root_param_index],
                         local_off + sz);
            st.comp_constant_set[cmd->root_param_index] = true;
          } else {
            QTRACE("SetComputeRoot32BitConstants idx=%u overflow local_off=%u size=%u",
                   cmd->root_param_index, local_off, sz);
          }
        }
        break;
      }
      case CmdType::SetComputeRootConstantBufferView: {
        auto *cmd = reinterpret_cast<const CmdSetRootCBV *>(header);
        if (cmd->root_param_index < 16) {
          st.comp_cbvs[cmd->root_param_index] = cmd->address;
          st.comp_cbv_set[cmd->root_param_index] = true;
          st.comp_uav_root[cmd->root_param_index] = false;
        }
        break;
      }
      case CmdType::SetComputeRootShaderResourceView: {
        auto *cmd = reinterpret_cast<const CmdSetRootCBV *>(header);
        if (cmd->root_param_index < 16) {
          st.comp_cbvs[cmd->root_param_index] = cmd->address;
          st.comp_cbv_set[cmd->root_param_index] = true;
          st.comp_uav_root[cmd->root_param_index] = false;
        }
        break;
      }
      case CmdType::SetComputeRootUnorderedAccessView: {
        auto *cmd = reinterpret_cast<const CmdSetRootCBV *>(header);
        if (cmd->root_param_index < 16) {
          st.comp_cbvs[cmd->root_param_index] = cmd->address;
          st.comp_cbv_set[cmd->root_param_index] = true;
          st.comp_uav_root[cmd->root_param_index] = true;
        }
        break;
      }
      case CmdType::SetComputeRootDescriptorTable: {
        auto *cmd = reinterpret_cast<const CmdSetRootDescriptorTable *>(header);
        if (cmd->root_param_index < 16) {
          st.comp_tables[cmd->root_param_index] = cmd->base_descriptor;
          st.comp_table_set[cmd->root_param_index] = true;
        }
        break;
      }
      case CmdType::IASetVertexBuffers: {
        auto *cmd = reinterpret_cast<const CmdIASetVertexBuffers *>(header);
        auto *views = reinterpret_cast<const D3D12_VERTEX_BUFFER_VIEW *>(
            reinterpret_cast<const uint8_t *>(cmd) +
            sizeof(CmdIASetVertexBuffers) - sizeof(D3D12_VERTEX_BUFFER_VIEW));
        for (uint32_t i = 0; i < cmd->count; i++) {
          uint32_t slot = cmd->start_slot + i;
          if (slot >= ReplayState::kVertexBufferSlotCount) {
            QTRACE("IASetVertexBuffers: skip slot=%u outside Metal-backed slot cap %u",
                   slot, ReplayState::kVertexBufferSlotCount);
            continue;
          }
          st.vbs[cmd->start_slot + i] = views[i];
          QTRACE("IASetVertexBuffers: slot=%u gpu=0x%llx size=%u stride=%u",
                 slot, (unsigned long long)views[i].BufferLocation,
                 views[i].SizeInBytes, views[i].StrideInBytes);
        }
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
    ENC_COMMIT(cmdbuf.handle);
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
  QTRACE("CmdQueue::Signal value=%llu fence_iface=%p", (unsigned long long)value, (void *)fence);
  if (!fence)
    return E_POINTER;
  {
    FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
    if (f) { fprintf(f, "CmdQueue::Signal value=%llu fence=%p\n", (unsigned long long)value, (void *)fence); fclose(f); }
  }
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
