#include "dxil_container.hpp"
#include <cstdio>

namespace dxmt::dxil {

std::optional<DXILContainer> DXILContainer::parse(const void *data, size_t size) {
  if (!data || size < 16)
    return std::nullopt;

  auto *base = static_cast<const uint8_t *>(data);

  const uint32_t *vals = reinterpret_cast<const uint32_t *>(base);
  uint32_t program_version = vals[0];
  uint32_t prog_size = vals[1];

  const uint32_t *dxil_fields = vals + 2;
  uint32_t dxil_magic = dxil_fields[0];

  if (dxil_magic != DXIL_FOURCC)
    return std::nullopt;

  uint16_t dxil_minor = *reinterpret_cast<const uint16_t *>(base + 12);
  uint16_t dxil_major = *reinterpret_cast<const uint16_t *>(base + 14);
  (void)dxil_major;
  (void)dxil_minor;

  uint32_t bitcode_offset = *reinterpret_cast<const uint32_t *>(base + 16);
  uint32_t bitcode_size = *reinterpret_cast<const uint32_t *>(base + 20);

  FILE *_dbg = fopen("Z:\\tmp\\dxmt_dxil_trace.log", "a");
  if (_dbg) {
    fprintf(_dbg, "DXILContainer: ver=0x%08x prog_size=%u dxil_magic=0x%08x bc_off=%u bc_sz=%u blob_size=%zu\n",
      program_version, prog_size, dxil_magic, bitcode_offset, bitcode_size, size);
    fclose(_dbg);
  }

  uint32_t dxil_magic_offset = 8;
  uint32_t actual_bitcode_start = dxil_magic_offset + bitcode_offset;

  uint32_t kind_val = (program_version >> 16) & 0xFFFF;
  DxilShaderKind kind = static_cast<DxilShaderKind>(kind_val);

  DxilShaderModel sm;
  sm.major = (program_version >> 4) & 0xF;
  sm.minor = program_version & 0xF;

  if (actual_bitcode_start >= size)
    return std::nullopt;

  if (bitcode_size == 0 || actual_bitcode_start + bitcode_size > size)
    bitcode_size = size - actual_bitcode_start;

  const uint8_t *bitcode_ptr = base + actual_bitcode_start;

  DXILContainer result;
  result.m_shader.kind = kind;
  result.m_shader.shader_model = sm;
  result.m_shader.bitcode.data = bitcode_ptr;
  result.m_shader.bitcode.size = bitcode_size;

  switch (kind) {
  case DxilShaderKind::Compute: result.m_shader.entry_point = "cs_main"; break;
  case DxilShaderKind::Vertex: result.m_shader.entry_point = "vs_main"; break;
  case DxilShaderKind::Pixel: result.m_shader.entry_point = "ps_main"; break;
  case DxilShaderKind::Geometry: result.m_shader.entry_point = "gs_main"; break;
  case DxilShaderKind::Hull: result.m_shader.entry_point = "hs_main"; break;
  case DxilShaderKind::Domain: result.m_shader.entry_point = "ds_main"; break;
  default: result.m_shader.entry_point = "main"; break;
  }

  return result;
}

}
