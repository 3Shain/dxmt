#include "d3d12_root_signature.hpp"
#include "d3d12_device.hpp"
#include "log/log.hpp"
#include "util_string.hpp"
#include <cstring>

namespace dxmt {

#pragma pack(push, 1)
struct RSHeader {
  uint32_t num_parameters;
  uint32_t num_static_samplers;
  uint32_t flags;
};

struct RSParameter {
  uint8_t type;
  uint8_t visibility;
  union {
    struct {
      uint32_t register_space;
      uint32_t register_index;
      uint32_t num_32bit_values;
    } constants;
    struct {
      uint32_t register_space;
      uint32_t register_index;
    } descriptor;
    struct {
      uint32_t num_ranges;
    } table;
  };
};

struct RSDescriptorRange {
  uint8_t range_type;
  uint32_t num_descriptors;
  uint32_t base_register;
  uint32_t register_space;
  uint32_t offset_in_table;
};

struct RSStaticSampler {
  uint32_t filter;
  uint32_t address_u;
  uint32_t address_v;
  uint32_t address_w;
  float mip_lod_bias;
  uint32_t max_anisotropy;
  uint32_t comparison_func;
  uint32_t border_color;
  float min_lod;
  float max_lod;
  uint32_t register_space;
  uint32_t register_index;
  uint32_t shader_register_space;
  uint8_t shader_visibility;
};

struct DXContainerHeader {
  uint8_t magic[4];
  uint8_t digest[16];
  uint16_t major;
  uint16_t minor;
  uint32_t file_size;
  uint32_t part_count;
};

struct DXContainerPartHeader {
  uint8_t name[4];
  uint32_t size;
};

struct DXRootSignatureHeader {
  uint32_t version;
  uint32_t num_parameters;
  uint32_t parameters_offset;
  uint32_t num_static_samplers;
  uint32_t static_sampler_offset;
  uint32_t flags;
};

struct DXRootParameterHeader {
  uint32_t parameter_type;
  uint32_t shader_visibility;
  uint32_t parameter_offset;
};

struct DXRootConstants {
  uint32_t shader_register;
  uint32_t register_space;
  uint32_t num_32bit_values;
};

struct DXRootDescriptor10 {
  uint32_t shader_register;
  uint32_t register_space;
};

struct DXDescriptorTable {
  uint32_t num_ranges;
  uint32_t ranges_offset;
};

struct DXDescriptorRange10 {
  uint32_t range_type;
  uint32_t num_descriptors;
  uint32_t base_shader_register;
  uint32_t register_space;
  uint32_t offset_in_table;
};

struct DXDescriptorRange11 {
  uint32_t range_type;
  uint32_t num_descriptors;
  uint32_t base_shader_register;
  uint32_t register_space;
  uint32_t flags;
  uint32_t offset_in_table;
};

struct DXStaticSampler {
  uint32_t filter;
  uint32_t address_u;
  uint32_t address_v;
  uint32_t address_w;
  float mip_lod_bias;
  uint32_t max_anisotropy;
  uint32_t comparison_func;
  uint32_t border_color;
  float min_lod;
  float max_lod;
  uint32_t shader_register;
  uint32_t register_space;
  uint32_t shader_visibility;
};
#pragma pack(pop)

#define RSTRACE(fmt, ...) do { FILE *_tf = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a"); if (_tf) { fprintf(_tf, fmt "\n", ##__VA_ARGS__); fclose(_tf); } } while(0)

static bool range_contains(size_t size, uint32_t offset, size_t bytes) {
  return offset <= size && bytes <= size - offset;
}

static WMTSamplerAddressMode
map_sampler_address_mode(D3D12_TEXTURE_ADDRESS_MODE mode) {
  switch (mode) {
  case D3D12_TEXTURE_ADDRESS_MODE_WRAP:
    return WMTSamplerAddressModeRepeat;
  case D3D12_TEXTURE_ADDRESS_MODE_MIRROR:
    return WMTSamplerAddressModeMirrorRepeat;
  case D3D12_TEXTURE_ADDRESS_MODE_CLAMP:
    return WMTSamplerAddressModeClampToEdge;
  case D3D12_TEXTURE_ADDRESS_MODE_BORDER:
    return WMTSamplerAddressModeClampToBorderColor;
  case D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE:
    return WMTSamplerAddressModeMirrorClampToEdge;
  default:
    return WMTSamplerAddressModeClampToEdge;
  }
}

static WMTSamplerBorderColor
map_sampler_border_color(D3D12_STATIC_BORDER_COLOR color) {
  switch (color) {
  case D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK:
    return WMTSamplerBorderColorOpaqueBlack;
  case D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE:
    return WMTSamplerBorderColorOpaqueWhite;
  default:
    return WMTSamplerBorderColorTransparentBlack;
  }
}

static WMTSamplerInfo sampler_info_from_static_desc(
    const D3D12_STATIC_SAMPLER_DESC &desc) {
  WMTSamplerInfo info = {};
  switch (desc.Filter) {
  case D3D12_FILTER_MIN_MAG_MIP_POINT:
    info.min_filter = WMTSamplerMinMagFilterNearest;
    info.mag_filter = WMTSamplerMinMagFilterNearest;
    info.mip_filter = WMTSamplerMipFilterNearest;
    break;
  case D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR:
    info.min_filter = WMTSamplerMinMagFilterNearest;
    info.mag_filter = WMTSamplerMinMagFilterNearest;
    info.mip_filter = WMTSamplerMipFilterLinear;
    break;
  case D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT:
    info.min_filter = WMTSamplerMinMagFilterNearest;
    info.mag_filter = WMTSamplerMinMagFilterLinear;
    info.mip_filter = WMTSamplerMipFilterNearest;
    break;
  case D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR:
    info.min_filter = WMTSamplerMinMagFilterNearest;
    info.mag_filter = WMTSamplerMinMagFilterLinear;
    info.mip_filter = WMTSamplerMipFilterLinear;
    break;
  case D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT:
    info.min_filter = WMTSamplerMinMagFilterLinear;
    info.mag_filter = WMTSamplerMinMagFilterNearest;
    info.mip_filter = WMTSamplerMipFilterNearest;
    break;
  case D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
    info.min_filter = WMTSamplerMinMagFilterLinear;
    info.mag_filter = WMTSamplerMinMagFilterNearest;
    info.mip_filter = WMTSamplerMipFilterLinear;
    break;
  case D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT:
    info.min_filter = WMTSamplerMinMagFilterLinear;
    info.mag_filter = WMTSamplerMinMagFilterLinear;
    info.mip_filter = WMTSamplerMipFilterNearest;
    break;
  case D3D12_FILTER_MIN_MAG_MIP_LINEAR:
  case D3D12_FILTER_ANISOTROPIC:
  case D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR:
  case D3D12_FILTER_COMPARISON_ANISOTROPIC:
    info.min_filter = WMTSamplerMinMagFilterLinear;
    info.mag_filter = WMTSamplerMinMagFilterLinear;
    info.mip_filter = WMTSamplerMipFilterLinear;
    break;
  default:
    info.min_filter = WMTSamplerMinMagFilterLinear;
    info.mag_filter = WMTSamplerMinMagFilterLinear;
    info.mip_filter = WMTSamplerMipFilterLinear;
    break;
  }

  if (desc.Filter == D3D12_FILTER_ANISOTROPIC ||
      desc.Filter == D3D12_FILTER_COMPARISON_ANISOTROPIC)
    info.max_anisotroy = desc.MaxAnisotropy;

  info.s_address_mode = map_sampler_address_mode(desc.AddressU);
  info.t_address_mode = map_sampler_address_mode(desc.AddressV);
  info.r_address_mode = map_sampler_address_mode(desc.AddressW);
  info.border_color = map_sampler_border_color(desc.BorderColor);
  info.lod_min_clamp = desc.MinLOD;
  info.lod_max_clamp = desc.MaxLOD;
  info.normalized_coords = true;
  info.support_argument_buffers = true;
  if (desc.Filter >= D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT &&
      desc.Filter <= D3D12_FILTER_COMPARISON_ANISOTROPIC &&
      desc.ComparisonFunc >= D3D12_COMPARISON_FUNC_LESS &&
      desc.ComparisonFunc <= D3D12_COMPARISON_FUNC_ALWAYS) {
    info.compare_function =
        static_cast<WMTCompareFunction>(desc.ComparisonFunc - 1);
  }
  return info;
}

static uint64_t sampler_lod_bias_bits(float lod_bias) {
  uint32_t bits = 0;
  static_assert(sizeof(bits) == sizeof(lod_bias));
  memcpy(&bits, &lod_bias, sizeof(bits));
  return bits;
}

RootStaticSampler make_static_sampler(WMT::Device device,
                                      const D3D12_STATIC_SAMPLER_DESC &desc) {
  RootStaticSampler sampler = {};
  sampler.shader_register = desc.ShaderRegister;
  sampler.register_space = desc.RegisterSpace;
  sampler.shader_visibility = desc.ShaderVisibility;
  sampler.lod_bias_bits = sampler_lod_bias_bits(desc.MipLODBias);

  WMTSamplerInfo info = sampler_info_from_static_desc(desc);
  sampler.sampler = device.newSamplerState(info);
  sampler.sampler_gpu_id = info.gpu_resource_id;

  WMTSamplerInfo cube_info = info;
  if (cube_info.min_filter == WMTSamplerMinMagFilterLinear &&
      cube_info.mag_filter == WMTSamplerMinMagFilterLinear) {
    cube_info.s_address_mode = WMTSamplerAddressModeClampToBorderColor;
    cube_info.t_address_mode = WMTSamplerAddressModeClampToBorderColor;
    cube_info.r_address_mode = WMTSamplerAddressModeClampToBorderColor;
  } else {
    cube_info.s_address_mode = WMTSamplerAddressModeClampToEdge;
    cube_info.t_address_mode = WMTSamplerAddressModeClampToEdge;
    cube_info.r_address_mode = WMTSamplerAddressModeClampToEdge;
  }
  sampler.sampler_cube = device.newSamplerState(cube_info);
  sampler.sampler_cube_gpu_id = cube_info.gpu_resource_id;
  return sampler;
}

MTLD3D12RootSignature::MTLD3D12RootSignature(MTLD3D12Device *device,
                                             const void *blob, SIZE_T blob_size)
    : m_device(device) {
  m_device->AddRef();
  Parse(blob, blob_size);
  Logger::info(str::format("D3D12RootSignature: ", m_parameters.size(),
                            " params, ", m_num_static_samplers,
                            " static samplers, flags=", m_flags));
}

MTLD3D12RootSignature::~MTLD3D12RootSignature() { m_device->Release(); }

void MTLD3D12RootSignature::Parse(const void *blob, SIZE_T blob_size) {
  if (!blob || blob_size < sizeof(RSHeader))
    return;

  auto parse_rts0 = [&](const uint8_t *data, size_t size) -> bool {
    if (size < sizeof(DXRootSignatureHeader))
      return false;

    auto header = reinterpret_cast<const DXRootSignatureHeader *>(data);
    if ((header->version != D3D_ROOT_SIGNATURE_VERSION_1_0 &&
         header->version != D3D_ROOT_SIGNATURE_VERSION_1_1) ||
        !range_contains(size, header->parameters_offset,
                        header->num_parameters *
                            sizeof(DXRootParameterHeader)) ||
        header->num_parameters > 64 ||
        header->num_static_samplers > 64)
      return false;

    if (header->num_static_samplers > 0 &&
        !range_contains(size, header->static_sampler_offset,
                        header->num_static_samplers * 52u))
      return false;

    m_parameters.clear();
    m_static_samplers.clear();
    m_num_static_samplers = header->num_static_samplers;
    m_flags = static_cast<D3D12_ROOT_SIGNATURE_FLAGS>(header->flags);

    auto param_headers = reinterpret_cast<const DXRootParameterHeader *>(
        data + header->parameters_offset);

    for (uint32_t i = 0; i < header->num_parameters; i++) {
      const auto &src = param_headers[i];
      if (!range_contains(size, src.parameter_offset, sizeof(uint32_t)))
        return false;

      RootParameter rp = {};
      rp.type = static_cast<D3D12_ROOT_PARAMETER_TYPE>(src.parameter_type);
      rp.shader_visibility = src.shader_visibility;

      switch (rp.type) {
      case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS: {
        if (!range_contains(size, src.parameter_offset,
                            sizeof(DXRootConstants)))
          return false;
        auto constants = reinterpret_cast<const DXRootConstants *>(
            data + src.parameter_offset);
        rp.register_space = constants->register_space;
        rp.register_index = constants->shader_register;
        break;
      }
      case D3D12_ROOT_PARAMETER_TYPE_CBV:
      case D3D12_ROOT_PARAMETER_TYPE_SRV:
      case D3D12_ROOT_PARAMETER_TYPE_UAV: {
        size_t descriptor_size = header->version == D3D_ROOT_SIGNATURE_VERSION_1_0
                                     ? sizeof(DXRootDescriptor10)
                                     : sizeof(DXRootDescriptor10) + sizeof(uint32_t);
        if (!range_contains(size, src.parameter_offset, descriptor_size))
          return false;
        auto descriptor = reinterpret_cast<const DXRootDescriptor10 *>(
            data + src.parameter_offset);
        rp.register_space = descriptor->register_space;
        rp.register_index = descriptor->shader_register;
        break;
      }
      case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: {
        if (!range_contains(size, src.parameter_offset,
                            sizeof(DXDescriptorTable)))
          return false;
        auto table = reinterpret_cast<const DXDescriptorTable *>(
            data + src.parameter_offset);
        if (table->num_ranges > 256)
          return false;

        size_t range_size = header->version == D3D_ROOT_SIGNATURE_VERSION_1_0
                                ? sizeof(DXDescriptorRange10)
                                : sizeof(DXDescriptorRange11);
        const uint8_t *ranges_base = data + src.parameter_offset + sizeof(*table);
        uint32_t ranges_offset = table->ranges_offset;
        if (range_contains(size, ranges_offset, table->num_ranges * range_size))
          ranges_base = data + ranges_offset;
        else if (!range_contains(size,
                                 (uint32_t)(src.parameter_offset + sizeof(*table)),
                                 table->num_ranges * range_size))
          return false;

        rp.descriptor_table_entries = table->num_ranges;
        uint32_t append_offset = 0;
        for (uint32_t r = 0; r < table->num_ranges; r++) {
          RootDescriptorRange range = {};
          if (header->version == D3D_ROOT_SIGNATURE_VERSION_1_0) {
            auto src_range = reinterpret_cast<const DXDescriptorRange10 *>(
                ranges_base + r * range_size);
            range.range_type =
                static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(src_range->range_type);
            range.num_descriptors = src_range->num_descriptors;
            range.base_register = src_range->base_shader_register;
            range.register_space = src_range->register_space;
            range.offset_in_table = src_range->offset_in_table;
          } else {
            auto src_range = reinterpret_cast<const DXDescriptorRange11 *>(
                ranges_base + r * range_size);
            range.range_type =
                static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(src_range->range_type);
            range.num_descriptors = src_range->num_descriptors;
            range.base_register = src_range->base_shader_register;
            range.register_space = src_range->register_space;
            range.offset_in_table = src_range->offset_in_table;
          }

          range.offset_in_table =
              range.offset_in_table == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
                  ? append_offset
                  : range.offset_in_table;
          rp.ranges.push_back(range);

          if (r == 0) {
            rp.range_type = range.range_type;
            rp.num_descriptors = range.num_descriptors;
            rp.register_space = range.register_space;
            rp.register_index = range.base_register;
          }

          if (range.num_descriptors != UINT32_MAX)
            append_offset = range.offset_in_table + range.num_descriptors;
          else
            append_offset = range.offset_in_table;
        }
        break;
      }
      default:
        RSTRACE("RootSignature RTS0 unknown parameter type=%u", src.parameter_type);
        return false;
      }

      m_parameters.push_back(rp);
    }

    auto static_samplers = reinterpret_cast<const DXStaticSampler *>(
        data + header->static_sampler_offset);
    for (uint32_t i = 0; i < header->num_static_samplers; i++) {
      D3D12_STATIC_SAMPLER_DESC desc = {};
      desc.Filter = static_cast<D3D12_FILTER>(static_samplers[i].filter);
      desc.AddressU =
          static_cast<D3D12_TEXTURE_ADDRESS_MODE>(static_samplers[i].address_u);
      desc.AddressV =
          static_cast<D3D12_TEXTURE_ADDRESS_MODE>(static_samplers[i].address_v);
      desc.AddressW =
          static_cast<D3D12_TEXTURE_ADDRESS_MODE>(static_samplers[i].address_w);
      desc.MipLODBias = static_samplers[i].mip_lod_bias;
      desc.MaxAnisotropy = static_samplers[i].max_anisotropy;
      desc.ComparisonFunc =
          static_cast<D3D12_COMPARISON_FUNC>(static_samplers[i].comparison_func);
      desc.BorderColor = static_cast<D3D12_STATIC_BORDER_COLOR>(
          static_samplers[i].border_color);
      desc.MinLOD = static_samplers[i].min_lod;
      desc.MaxLOD = static_samplers[i].max_lod;
      desc.ShaderRegister = static_samplers[i].shader_register;
      desc.RegisterSpace = static_samplers[i].register_space;
      desc.ShaderVisibility =
          static_cast<D3D12_SHADER_VISIBILITY>(static_samplers[i].shader_visibility);
      m_static_samplers.push_back(
          make_static_sampler(m_device->GetMTLDevice(), desc));
    }

    RSTRACE("RootSignature parsed RTS0 version=%u params=%u samplers=%u flags=0x%x",
            header->version, header->num_parameters,
            header->num_static_samplers, header->flags);
    return true;
  };

  auto bytes = static_cast<const uint8_t *>(blob);
  if (blob_size >= sizeof(DXContainerHeader) &&
      memcmp(bytes, "DXBC", 4) == 0) {
    auto container = reinterpret_cast<const DXContainerHeader *>(bytes);
    if (container->part_count <= 256 &&
        container->file_size <= blob_size &&
        range_contains(blob_size, sizeof(DXContainerHeader),
                       container->part_count * sizeof(uint32_t))) {
      auto part_offsets = reinterpret_cast<const uint32_t *>(
          bytes + sizeof(DXContainerHeader));
      for (uint32_t i = 0; i < container->part_count; i++) {
        uint32_t offset = part_offsets[i];
        if (!range_contains(blob_size, offset, sizeof(DXContainerPartHeader)))
          continue;
        auto part = reinterpret_cast<const DXContainerPartHeader *>(
            bytes + offset);
        if (memcmp(part->name, "RTS0", 4) != 0)
          continue;
        if (!range_contains(blob_size, offset + sizeof(*part), part->size))
          continue;
        if (parse_rts0(bytes + offset + sizeof(*part), part->size))
          return;
      }
    }
  }

  if (parse_rts0(bytes, blob_size))
    return;

  auto header = static_cast<const RSHeader *>(blob);
  size_t min_size = sizeof(RSHeader) +
                    header->num_parameters * sizeof(RSParameter);
  if (header->num_parameters > 64 || header->num_static_samplers > 64 ||
      min_size > blob_size) {
    RSTRACE("RootSignature parse failed: unknown blob size=%zu magic=0x%08x",
            blob_size, blob_size >= 4 ? *(const uint32_t *)blob : 0);
    return;
  }

  m_parameters.clear();
  m_static_samplers.clear();
  m_num_static_samplers = header->num_static_samplers;
  m_flags = static_cast<D3D12_ROOT_SIGNATURE_FLAGS>(header->flags);

  auto params = reinterpret_cast<const uint8_t *>(blob) + sizeof(RSHeader);
  auto end = reinterpret_cast<const uint8_t *>(blob) + blob_size;
  for (uint32_t i = 0; i < header->num_parameters; i++) {
    if (params + sizeof(RSParameter) > end)
      return;
    auto p = reinterpret_cast<const RSParameter *>(params);
    RootParameter rp = {};
    rp.type = static_cast<D3D12_ROOT_PARAMETER_TYPE>(p->type);
    rp.shader_visibility = p->visibility;

    if (p->type == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS) {
      rp.register_space = p->constants.register_space;
      rp.register_index = p->constants.register_index;
    } else if (p->type == D3D12_ROOT_PARAMETER_TYPE_CBV ||
               p->type == D3D12_ROOT_PARAMETER_TYPE_SRV ||
               p->type == D3D12_ROOT_PARAMETER_TYPE_UAV) {
      rp.register_space = p->descriptor.register_space;
      rp.register_index = p->descriptor.register_index;
    } else if (p->type == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
      if (params + sizeof(RSParameter) +
              p->table.num_ranges * sizeof(RSDescriptorRange) >
          end)
        return;
      auto ranges = reinterpret_cast<const RSDescriptorRange *>(
          params + sizeof(RSParameter));
      rp.descriptor_table_entries = p->table.num_ranges;
      if (p->table.num_ranges > 0) {
        rp.range_type =
            static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(ranges[0].range_type);
        rp.num_descriptors = ranges[0].num_descriptors;
        rp.register_space = ranges[0].register_space;
        rp.register_index = ranges[0].base_register;
      }
      uint32_t append_offset = 0;
      for (uint32_t r = 0; r < p->table.num_ranges; r++) {
        RootDescriptorRange range = {};
        range.range_type =
            static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(ranges[r].range_type);
        range.num_descriptors = ranges[r].num_descriptors;
        range.base_register = ranges[r].base_register;
        range.register_space = ranges[r].register_space;
        range.offset_in_table =
            ranges[r].offset_in_table == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
                ? append_offset
                : ranges[r].offset_in_table;
        rp.ranges.push_back(range);

        if (range.num_descriptors != UINT32_MAX)
          append_offset = range.offset_in_table + range.num_descriptors;
        else
          append_offset = range.offset_in_table;
      }
      params += p->table.num_ranges * sizeof(RSDescriptorRange);
    }
    m_parameters.push_back(rp);
    params += sizeof(RSParameter);
  }

  for (uint32_t i = 0; i < header->num_static_samplers; i++) {
    if (params + sizeof(RSStaticSampler) > end)
      return;
    auto sampler = reinterpret_cast<const RSStaticSampler *>(params);
    D3D12_STATIC_SAMPLER_DESC desc = {};
    desc.Filter = static_cast<D3D12_FILTER>(sampler->filter);
    desc.AddressU =
        static_cast<D3D12_TEXTURE_ADDRESS_MODE>(sampler->address_u);
    desc.AddressV =
        static_cast<D3D12_TEXTURE_ADDRESS_MODE>(sampler->address_v);
    desc.AddressW =
        static_cast<D3D12_TEXTURE_ADDRESS_MODE>(sampler->address_w);
    desc.MipLODBias = sampler->mip_lod_bias;
    desc.MaxAnisotropy = sampler->max_anisotropy;
    desc.ComparisonFunc =
        static_cast<D3D12_COMPARISON_FUNC>(sampler->comparison_func);
    desc.BorderColor =
        static_cast<D3D12_STATIC_BORDER_COLOR>(sampler->border_color);
    desc.MinLOD = sampler->min_lod;
    desc.MaxLOD = sampler->max_lod;
    desc.ShaderRegister = sampler->register_index;
    desc.RegisterSpace = sampler->register_space;
    desc.ShaderVisibility =
        static_cast<D3D12_SHADER_VISIBILITY>(sampler->shader_visibility);
    m_static_samplers.push_back(
        make_static_sampler(m_device->GetMTLDevice(), desc));
    params += sizeof(RSStaticSampler);
  }
}

bool MTLD3D12RootSignature::FindDescriptorTableRange(
    D3D12_DESCRIPTOR_RANGE_TYPE range_type, uint32_t shader_register,
    uint32_t *root_parameter_index, uint32_t *descriptor_offset) const {
  return FindDescriptorTableRange(range_type, shader_register, 0,
                                  root_parameter_index, descriptor_offset);
}

bool MTLD3D12RootSignature::FindDescriptorTableRange(
    D3D12_DESCRIPTOR_RANGE_TYPE range_type, uint32_t shader_register,
    uint32_t register_space, uint32_t *root_parameter_index,
    uint32_t *descriptor_offset) const {
  return FindDescriptorTableRangeForVisibility(
      range_type, shader_register, register_space, D3D12_SHADER_VISIBILITY_ALL,
      root_parameter_index, descriptor_offset);
}

bool MTLD3D12RootSignature::FindDescriptorTableRangeForVisibility(
    D3D12_DESCRIPTOR_RANGE_TYPE range_type, uint32_t shader_register,
    D3D12_SHADER_VISIBILITY shader_visibility, uint32_t *root_parameter_index,
    uint32_t *descriptor_offset) const {
  return FindDescriptorTableRangeForVisibility(
      range_type, shader_register, 0, shader_visibility, root_parameter_index,
      descriptor_offset);
}

bool MTLD3D12RootSignature::FindDescriptorTableRangeForVisibility(
    D3D12_DESCRIPTOR_RANGE_TYPE range_type, uint32_t shader_register,
    uint32_t register_space, D3D12_SHADER_VISIBILITY shader_visibility,
    uint32_t *root_parameter_index, uint32_t *descriptor_offset) const {
  for (uint32_t visibility_pass = 0; visibility_pass < 2; visibility_pass++) {
    for (uint32_t p = 0; p < m_parameters.size(); p++) {
      const auto &param = m_parameters[p];
      if (param.type != D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        continue;
      if (visibility_pass == 0 &&
          param.shader_visibility != shader_visibility)
        continue;
      if (visibility_pass == 1 &&
          param.shader_visibility != D3D12_SHADER_VISIBILITY_ALL)
        continue;

      for (const auto &range : param.ranges) {
        if (range.range_type != range_type)
          continue;
        if (range.register_space != register_space)
          continue;
        if (shader_register < range.base_register)
          continue;
        uint32_t relative = shader_register - range.base_register;
        if (range.num_descriptors != UINT32_MAX &&
            relative >= range.num_descriptors)
          continue;

        if (root_parameter_index)
          *root_parameter_index = p;
        if (descriptor_offset)
          *descriptor_offset = range.offset_in_table + relative;
        return true;
      }
    }
  }

  return false;
}

const RootStaticSampler *MTLD3D12RootSignature::FindStaticSampler(
    uint32_t shader_register, uint32_t register_space,
    D3D12_SHADER_VISIBILITY shader_visibility) const {
  for (uint32_t pass = 0; pass < 2; pass++) {
    for (const auto &sampler : m_static_samplers) {
      if (sampler.shader_register != shader_register ||
          sampler.register_space != register_space)
        continue;
      if (pass == 0 && sampler.shader_visibility != shader_visibility)
        continue;
      if (pass == 1 &&
          sampler.shader_visibility != D3D12_SHADER_VISIBILITY_ALL)
        continue;
      return &sampler;
    }
  }
  return nullptr;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12RootSignature::QueryInterface(REFIID riid, void **ppvObject) {
  if (!ppvObject)
    return E_POINTER;
  *ppvObject = nullptr;

  if (riid == IID_IUnknown || riid == IID_ID3D12Object ||
      riid == IID_ID3D12DeviceChild || riid == IID_ID3D12RootSignature) {
    *ppvObject = ref(this);
    return S_OK;
  }
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE
MTLD3D12RootSignature::AddRef() { return ++m_refCount; }

ULONG STDMETHODCALLTYPE MTLD3D12RootSignature::Release() {
  uint32_t rc = --m_refCount;
  if (!rc)
    delete this;
  return rc;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12RootSignature::GetPrivateData(REFGUID guid, UINT *data_size,
                                      void *data) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12RootSignature::SetPrivateData(REFGUID guid, UINT data_size,
                                      const void *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12RootSignature::SetPrivateDataInterface(REFGUID guid,
                                               const IUnknown *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12RootSignature::SetName(LPCWSTR name) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12RootSignature::GetDevice(REFIID riid, void **device) {
  return m_device->QueryInterface(riid, device);
}

} // namespace dxmt
