#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "air_signature.hpp"
#include "dxbc_instructions.hpp"
#include "shader_common.hpp"

namespace dxmt::dxbc {

inline air::Interpolation
to_air_interpolation(microsoft::D3D10_SB_INTERPOLATION_MODE mode) {
  using namespace microsoft;
  switch (mode) {
  case D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE:
    return air::Interpolation::sample_no_perspective;
  case D3D10_SB_INTERPOLATION_LINEAR_SAMPLE:
    return air::Interpolation::sample_perspective;
  case D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID:
    return air::Interpolation::centroid_no_perspective;
  case D3D10_SB_INTERPOLATION_LINEAR_CENTROID:
    return air::Interpolation::centroid_perspective;
  case D3D10_SB_INTERPOLATION_CONSTANT:
    return air::Interpolation::flat;
  case D3D10_SB_INTERPOLATION_LINEAR:
    return air::Interpolation::center_perspective;
  case D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE:
    return air::Interpolation::center_no_perspective;
  default:
    assert(0 && "Unexpected D3D10_SB_INTERPOLATION_MODE");
  }
}

inline dxmt::shader::common::ResourceType
to_shader_resource_type(microsoft::D3D10_SB_RESOURCE_DIMENSION dim) {
  switch (dim) {
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_UNKNOWN:
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_BUFFER:
  case microsoft::D3D11_SB_RESOURCE_DIMENSION_RAW_BUFFER:
  case microsoft::D3D11_SB_RESOURCE_DIMENSION_STRUCTURED_BUFFER:
    return shader::common::ResourceType::TextureBuffer;
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_TEXTURE1D:
    return shader::common::ResourceType::Texture1D;
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_TEXTURE2D:
    return shader::common::ResourceType::Texture2D;
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_TEXTURE2DMS:
    return shader::common::ResourceType::Texture2DMultisampled;
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_TEXTURE3D:
    return shader::common::ResourceType::Texture3D;
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_TEXTURECUBE:
    return shader::common::ResourceType::TextureCube;
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_TEXTURE1DARRAY:
    return shader::common::ResourceType::Texture1DArray;
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_TEXTURE2DARRAY:
    return shader::common::ResourceType::Texture2DArray;
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_TEXTURE2DMSARRAY:
    return shader::common::ResourceType::Texture2DMultisampledArray;
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_TEXTURECUBEARRAY:
    return shader::common::ResourceType::TextureCubeArray;
  }
  assert(0 && "invalid D3D10_SB_RESOURCE_DIMENSION");
};

inline dxmt::shader::common::ScalerDataType
to_shader_scaler_type(microsoft::D3D10_SB_RESOURCE_RETURN_TYPE type) {
  switch (type) {

  case microsoft::D3D10_SB_RETURN_TYPE_UNORM:
  case microsoft::D3D10_SB_RETURN_TYPE_SNORM:
  case microsoft::D3D10_SB_RETURN_TYPE_FLOAT:
    return shader::common::ScalerDataType::Float;
  case microsoft::D3D10_SB_RETURN_TYPE_SINT:
    return shader::common::ScalerDataType::Int;
  case microsoft::D3D10_SB_RETURN_TYPE_UINT:
    return shader::common::ScalerDataType::Uint;
  case microsoft::D3D10_SB_RETURN_TYPE_MIXED:
    return shader::common::ScalerDataType::Uint; // kinda weird but ok
  case microsoft::D3D11_SB_RETURN_TYPE_DOUBLE:
  case microsoft::D3D11_SB_RETURN_TYPE_CONTINUED:
  case microsoft::D3D11_SB_RETURN_TYPE_UNUSED:
    break;
  }
  assert(0 && "invalid D3D10_SB_RESOURCE_RETURN_TYPE");
}

#pragma region shader reflection

struct ResourceRange {
  uint32_t range_id;
  uint32_t lower_bound;
  uint32_t size;
  uint32_t space;
};

struct ShaderResourceViewInfo {
  ResourceRange range;
  shader::common::ScalerDataType scaler_type;
  shader::common::ResourceType resource_type;
  bool read = false;
  bool sampled = false;
  bool compared = false; // therefore we use depth texture!

  uint32_t strucure_stride = 0;
  uint32_t arg_index;
  uint32_t arg_size_index;
};
struct UnorderedAccessViewInfo {
  ResourceRange range;
  shader::common::ScalerDataType scaler_type;
  shader::common::ResourceType resource_type;
  bool read = false;
  bool written = false;
  bool global_coherent = false;
  bool rasterizer_order = false;
  bool with_counter = false;

  uint32_t strucure_stride = 0;
  uint32_t arg_index;
  uint32_t arg_size_index;
  uint32_t arg_counter_index;
};
struct ConstantBufferInfo {
  ResourceRange range;
  uint32_t size_in_vec4;
  uint32_t arg_index;
};
struct SamplerInfo {
  ResourceRange range;
  uint32_t arg_index;
};

struct ThreadgroupBufferInfo {
  uint32_t size_in_uint;
  uint32_t size;
  uint32_t stride;
  bool structured;
};

#pragma endregion

class ShaderInfo {
public:
  std::vector<std::array<uint32_t, 4>> immConstantBufferData;
  std::map<uint32_t, ShaderResourceViewInfo> srvMap;
  std::map<uint32_t, UnorderedAccessViewInfo> uavMap;
  std::map<uint32_t, ConstantBufferInfo> cbufferMap;
  std::map<uint32_t, SamplerInfo> samplerMap;
  std::map<uint32_t, ThreadgroupBufferInfo> tgsmMap;
  uint32_t tempRegisterCount = 0;
  std::unordered_map<
    uint32_t, std::pair<uint32_t /* count */, uint32_t /* mask */>>
    indexableTempRegisterCounts;
  air::ArgumentBufferBuilder binding_table_cbuffer;
  air::ArgumentBufferBuilder binding_table;
  bool skipOptimization = false;
  bool refactoringAllowed = true;
  bool use_cmp_exch = false;
};

Instruction readInstruction(
  const microsoft::D3D10ShaderBinary::CInstruction &Inst,
  ShaderInfo &shader_info
);

} // namespace dxmt::dxbc
