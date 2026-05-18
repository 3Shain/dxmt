#include "d3d12_pipeline_state.hpp"
#include "d3d12_device.hpp"
#include "d3d12_trace.hpp"
#include "log/log.hpp"
#include "util_string.hpp"
#include "Metal.hpp"

#define PTRACE(fmt, ...) do { FILE *_tf = fopen("Z:\\tmp\\dxmt_ps_args_debug.log", "a"); if (_tf) { fprintf(_tf, fmt "\n", ##__VA_ARGS__); fclose(_tf); } } while(0)
#include "airconv_public.h"
#include "dxmt_format.hpp"
#include "dxil/dxil_container.hpp"
#include "dxil/llvm_bitcode.hpp"
#include "dxil/dxil_to_msl.hpp"
#include "../../libs/DXBCParser/BlobContainer.h"
#include "../../libs/DXBCParser/DXBCUtils.h"
#include <cstdlib>
#include <cstring>
#include <map>
#include <unistd.h>
#include <vector>
#include <process.h>
#include <windows.h>

#define PSTRACE(fmt, ...) DXMTD3D12Trace("PSO", fmt, ##__VA_ARGS__)

namespace dxmt {

namespace {
constexpr uint32_t kMetalD3D12VertexBufferSlotCount = 29;

void EnsureShaderCacheDir() {
  CreateDirectoryA("Z:\\tmp\\dxmt_shader_cache", nullptr);
}

void DumpShaderBlob(const char *path, const void *bytecode, SIZE_T size) {
  if (!path || !bytecode || !size)
    return;
  EnsureShaderCacheDir();
  FILE *df = fopen(path, "wb");
  if (df) {
    fwrite(bytecode, 1, size, df);
    fclose(df);
  }
}

void DumpShaderText(const char *path, const char *text) {
  if (!path || !text)
    return;
  EnsureShaderCacheDir();
  FILE *df = fopen(path, "w");
  if (df) {
    fputs(text, df);
    fclose(df);
  }
}

const char *DxilShaderKindName(dxmt::dxil::DxilShaderKind kind) {
  switch (kind) {
  case dxmt::dxil::DxilShaderKind::Pixel: return "pixel";
  case dxmt::dxil::DxilShaderKind::Vertex: return "vertex";
  case dxmt::dxil::DxilShaderKind::Geometry: return "geometry";
  case dxmt::dxil::DxilShaderKind::Hull: return "hull";
  case dxmt::dxil::DxilShaderKind::Domain: return "domain";
  case dxmt::dxil::DxilShaderKind::Compute: return "compute";
  case dxmt::dxil::DxilShaderKind::Library: return "library";
  case dxmt::dxil::DxilShaderKind::Mesh: return "mesh";
  case dxmt::dxil::DxilShaderKind::Amplification: return "amplification";
  default: return "other";
  }
}

void DumpDXILModuleSummary(const char *path, const dxmt::dxil::LLVMModule &module,
                           const dxmt::dxil::DxilParsedShader &shader_info) {
  if (!path)
    return;
  EnsureShaderCacheDir();
  FILE *df = fopen(path, "w");
  if (!df)
    return;

  fprintf(df, "kind=%s(%u)\n", DxilShaderKindName(shader_info.kind),
          (uint32_t)shader_info.kind);
  fprintf(df, "shader_model=%u.%u\n", shader_info.shader_model.major,
          shader_info.shader_model.minor);
  fprintf(df, "entry=%s\n", shader_info.entry_point.c_str());
  fprintf(df, "bitcode_size=%u\n", shader_info.bitcode.size);
  fprintf(df, "source_filename=%s\n", module.source_filename.c_str());
  fprintf(df, "target_triple=%s\n", module.target_triple.c_str());
  fprintf(df, "types=%zu constants=%zu functions=%zu\n", module.types.size(),
          module.constants.size(), module.functions.size());

  size_t total_blocks = 0;
  size_t total_instructions = 0;
  std::map<int, size_t> opcode_counts;
  for (const auto &fn : module.functions) {
    total_blocks += fn.blocks.size();
    for (const auto &block : fn.blocks) {
      total_instructions += block.instructions.size();
      for (const auto &inst : block.instructions)
        opcode_counts[(int)inst.opcode]++;
    }
  }

  fprintf(df, "blocks=%zu instructions=%zu\n", total_blocks,
          total_instructions);
  fprintf(df, "\nfunctions:\n");
  for (const auto &fn : module.functions) {
    size_t inst_count = 0;
    for (const auto &block : fn.blocks)
      inst_count += block.instructions.size();
    fprintf(df,
            "  name=%s declaration=%d value=%u type=%u params=%u inst_start=%u blocks=%zu instructions=%zu\n",
            fn.name.c_str(), fn.is_declaration, fn.value_id, fn.type_id,
            fn.param_count, fn.instruction_start_value, fn.blocks.size(),
            inst_count);
  }

  fprintf(df, "\nopcodes:\n");
  for (const auto &entry : opcode_counts)
    fprintf(df, "  opcode=%d count=%zu\n", entry.first, entry.second);

  fclose(df);
}

void DumpDXILCompileReport(const char *path, const char *func_name, size_t hash,
                           SIZE_T bytecode_size, const char *dxbc_path,
                           const char *module_summary_path, const char *msl_path,
                           const dxmt::dxil::LLVMModule &module,
                           const dxmt::dxil::DxilParsedShader &shader_info,
                           const dxmt::dxil::MSLShader &msl_result) {
  if (!path)
    return;

  EnsureShaderCacheDir();
  FILE *df = fopen(path, "w");
  if (!df)
    return;

  fprintf(df, "hash=0x%016zx\n", hash);
  fprintf(df, "function=%s\n", func_name ? func_name : "<unknown>");
  fprintf(df, "kind=%s(%u)\n", DxilShaderKindName(shader_info.kind),
          (uint32_t)shader_info.kind);
  fprintf(df, "shader_model=%u.%u\n", shader_info.shader_model.major,
          shader_info.shader_model.minor);
  fprintf(df, "entry=%s\n", shader_info.entry_point.c_str());
  fprintf(df, "bytecode_size=%zu\n", bytecode_size);
  fprintf(df, "bitcode_size=%u\n", shader_info.bitcode.size);
  fprintf(df, "types=%zu constants=%zu functions=%zu\n", module.types.size(),
          module.constants.size(), module.functions.size());
  fprintf(df, "msl_size=%zu\n", msl_result.source.size());
  fprintf(df, "threadgroup_size=%u,%u,%u\n", msl_result.tg_size[0],
          msl_result.tg_size[1], msl_result.tg_size[2]);
  fprintf(df, "unsupported_intrinsics=%u\n",
          msl_result.unsupported_intrinsics);
  fprintf(df, "unsupported_opcodes=%u\n", msl_result.unsupported_opcodes);
  fprintf(df, "dxbc=%s\n", dxbc_path ? dxbc_path : "");
  fprintf(df, "module=%s\n", module_summary_path ? module_summary_path : "");
  fprintf(df, "msl=%s\n", msl_path ? msl_path : "");
  fprintf(df, "\ndiagnostics:\n");
  for (const auto &diagnostic : msl_result.diagnostics)
    fprintf(df, "  %s\n", diagnostic.c_str());

  fclose(df);

  FILE *index = fopen("Z:\\tmp\\dxmt_shader_cache\\dxil_report_index.tsv", "a");
  if (index) {
    fprintf(index, "0x%016zx\t%s\t%s\t%u.%u\t%u\t%u\t%s\n", hash,
            DxilShaderKindName(shader_info.kind), func_name ? func_name : "",
            shader_info.shader_model.major, shader_info.shader_model.minor,
            msl_result.unsupported_intrinsics, msl_result.unsupported_opcodes,
            path);
    fclose(index);
  }
}

constexpr WMTColorWriteMask kColorWriteMaskMap[16] = {
    (WMTColorWriteMask)0,
    WMTColorWriteMaskRed,
    WMTColorWriteMaskGreen,
    (WMTColorWriteMask)(WMTColorWriteMaskRed | WMTColorWriteMaskGreen),
    WMTColorWriteMaskBlue,
    (WMTColorWriteMask)(WMTColorWriteMaskRed | WMTColorWriteMaskBlue),
    (WMTColorWriteMask)(WMTColorWriteMaskGreen | WMTColorWriteMaskBlue),
    (WMTColorWriteMask)(WMTColorWriteMaskRed | WMTColorWriteMaskGreen |
                        WMTColorWriteMaskBlue),
    WMTColorWriteMaskAlpha,
    (WMTColorWriteMask)(WMTColorWriteMaskRed | WMTColorWriteMaskAlpha),
    (WMTColorWriteMask)(WMTColorWriteMaskGreen | WMTColorWriteMaskAlpha),
    (WMTColorWriteMask)(WMTColorWriteMaskRed | WMTColorWriteMaskGreen |
                        WMTColorWriteMaskAlpha),
    (WMTColorWriteMask)(WMTColorWriteMaskBlue | WMTColorWriteMaskAlpha),
    (WMTColorWriteMask)(WMTColorWriteMaskRed | WMTColorWriteMaskBlue |
                        WMTColorWriteMaskAlpha),
    (WMTColorWriteMask)(WMTColorWriteMaskGreen | WMTColorWriteMaskBlue |
                        WMTColorWriteMaskAlpha),
    (WMTColorWriteMask)(WMTColorWriteMaskRed | WMTColorWriteMaskGreen |
                        WMTColorWriteMaskBlue | WMTColorWriteMaskAlpha),
};

constexpr WMTCompareFunction kCompareFunctionMap[] = {
    WMTCompareFunctionNever,
    WMTCompareFunctionNever,
    WMTCompareFunctionLess,
    WMTCompareFunctionEqual,
    WMTCompareFunctionLessEqual,
    WMTCompareFunctionGreater,
    WMTCompareFunctionNotEqual,
    WMTCompareFunctionGreaterEqual,
    WMTCompareFunctionAlways,
};

constexpr WMTStencilOperation kStencilOperationMap[] = {
    WMTStencilOperationZero,
    WMTStencilOperationKeep,
    WMTStencilOperationZero,
    WMTStencilOperationReplace,
    WMTStencilOperationIncrementClamp,
    WMTStencilOperationDecrementClamp,
    WMTStencilOperationInvert,
    WMTStencilOperationIncrementWrap,
    WMTStencilOperationDecrementWrap,
};

uint32_t AlignD3D12InputOffset(uint32_t offset, uint32_t size) {
  uint32_t alignment = size < 4 ? size : 4;
  if (alignment <= 1)
    return offset;
  return (offset + alignment - 1) & ~(alignment - 1);
}
} // namespace

std::mutex MTLD3D12PipelineState::s_shader_mutex;
std::unordered_map<size_t, WMT::Reference<WMT::Function>> MTLD3D12PipelineState::s_shader_cache;

MTLD3D12PipelineState::MTLD3D12PipelineState(MTLD3D12Device *device,
                                             bool is_compute)
    : m_device(device), m_is_compute(is_compute) {
  m_device->AddRef();
}

MTLD3D12PipelineState::~MTLD3D12PipelineState() {
  if (m_root_sig)
    m_root_sig->Release();
  m_render_pso = nullptr;
  m_compute_pso = nullptr;
  m_device->Release();
}

void MTLD3D12PipelineState::ClearCompileFailure() {
  m_compile_failure_stage.clear();
  m_compile_failure_detail.clear();
}

bool MTLD3D12PipelineState::RecordCompileFailure(const char *stage, const std::string &detail) {
  m_compile_failure_stage = stage ? stage : "unknown";
  m_compile_failure_detail = detail;
  PSTRACE("PSO COMPILE FAILURE: this=%p compute=%d stage=%s detail=%s",
          (void *)this, m_is_compute, m_compile_failure_stage.c_str(),
          m_compile_failure_detail.c_str());
  return false;
}

WMTPixelFormat MTLD3D12PipelineState::DXGIToMTLPixelFormat(DXGI_FORMAT format) {
  switch (format) {
  case DXGI_FORMAT_R8G8B8A8_UNORM: return WMTPixelFormatRGBA8Unorm;
  case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return WMTPixelFormatRGBA8Unorm_sRGB;
  case DXGI_FORMAT_B8G8R8A8_UNORM: return WMTPixelFormatBGRA8Unorm;
  case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return WMTPixelFormatBGRA8Unorm_sRGB;
  case DXGI_FORMAT_R16G16B16A16_FLOAT: return WMTPixelFormatRGBA16Float;
  case DXGI_FORMAT_R32G32B32A32_FLOAT: return WMTPixelFormatRGBA32Float;
  case DXGI_FORMAT_R10G10B10A2_UNORM: return WMTPixelFormatRGB10A2Unorm;
  case DXGI_FORMAT_R11G11B10_FLOAT: return WMTPixelFormatRG11B10Float;
  case DXGI_FORMAT_R8_UNORM: return WMTPixelFormatR8Unorm;
  case DXGI_FORMAT_R16_FLOAT: return WMTPixelFormatR16Float;
  case DXGI_FORMAT_R32_FLOAT: return WMTPixelFormatR32Float;
  case DXGI_FORMAT_D32_FLOAT: return WMTPixelFormatDepth32Float;
  case DXGI_FORMAT_D24_UNORM_S8_UINT: return WMTPixelFormatDepth24Unorm_Stencil8;
  case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return WMTPixelFormatDepth32Float_Stencil8;
  case DXGI_FORMAT_D16_UNORM: return WMTPixelFormatDepth16Unorm;
  case DXGI_FORMAT_R16G16_FLOAT: return WMTPixelFormatRG16Float;
  case DXGI_FORMAT_R16G16_UNORM: return WMTPixelFormatRG16Unorm;
  case DXGI_FORMAT_R8G8_UNORM: return WMTPixelFormatRG8Unorm;
  case DXGI_FORMAT_BC1_UNORM: return WMTPixelFormatBC1_RGBA;
  case DXGI_FORMAT_BC1_UNORM_SRGB: return WMTPixelFormatBC1_RGBA_sRGB;
  case DXGI_FORMAT_BC2_UNORM: return WMTPixelFormatBC2_RGBA;
  case DXGI_FORMAT_BC2_UNORM_SRGB: return WMTPixelFormatBC2_RGBA_sRGB;
  case DXGI_FORMAT_BC3_UNORM: return WMTPixelFormatBC3_RGBA;
  case DXGI_FORMAT_BC3_UNORM_SRGB: return WMTPixelFormatBC3_RGBA_sRGB;
  case DXGI_FORMAT_BC4_UNORM: return WMTPixelFormatBC4_RUnorm;
  case DXGI_FORMAT_BC4_SNORM: return WMTPixelFormatBC4_RSnorm;
  case DXGI_FORMAT_BC5_UNORM: return WMTPixelFormatBC5_RGUnorm;
  case DXGI_FORMAT_BC5_SNORM: return WMTPixelFormatBC5_RGSnorm;
  case DXGI_FORMAT_BC6H_UF16: return WMTPixelFormatBC6H_RGBUfloat;
  case DXGI_FORMAT_BC6H_SF16: return WMTPixelFormatBC6H_RGBFloat;
  case DXGI_FORMAT_BC7_UNORM: return WMTPixelFormatBC7_RGBAUnorm;
  case DXGI_FORMAT_BC7_UNORM_SRGB: return WMTPixelFormatBC7_RGBAUnorm_sRGB;
  default: return WMTPixelFormatInvalid;
  }
}

bool MTLD3D12PipelineState::CompileShader(const void *bytecode, SIZE_T size,
                                          ShaderType type,
                                          const char *func_name,
                                          WMT::Reference<WMT::Function> &out_func,
                                          sm50_shader_t *out_shader_handle,
                                          MTL_SHADER_REFLECTION *out_reflection) {
  size_t hash = 0;
  hash = hash * 131 + (size_t)type;
  if (bytecode && size > 0) {
    const uint8_t *p = (const uint8_t *)bytecode;
    for (SIZE_T i = 0; i < size; i++)
      hash = hash * 131 + p[i];
  }
  if (type == ShaderType::Vertex) {
    hash = hash * 131 + m_input_layout.NumElements;
    for (UINT i = 0; i < m_input_layout.NumElements; i++) {
      const auto &el = m_input_layout.pInputElementDescs[i];
      hash = hash * 131 + el.SemanticIndex;
      hash = hash * 131 + el.Format;
      hash = hash * 131 + el.InputSlot;
      hash = hash * 131 + el.AlignedByteOffset;
      hash = hash * 131 + el.InputSlotClass;
      hash = hash * 131 + el.InstanceDataStepRate;
      if (el.SemanticName) {
        for (const char *s = el.SemanticName; *s; s++)
          hash = hash * 131 + (unsigned char)*s;
      }
    }
  }
  {
    std::lock_guard<std::mutex> lock(s_shader_mutex);
    PSTRACE("CompileShader: %s hash=0x%zx size=%zu cache_entries=%zu", func_name, hash, size, s_shader_cache.size());
    auto it = s_shader_cache.find(hash);
    if (it != s_shader_cache.end() && !out_shader_handle && !out_reflection) {
      out_func = it->second;
      PSTRACE("CompileShader: %s CACHE HIT hash=0x%zx", func_name, hash);
      return true;
    }
  }

  if (bytecode && size >= 4) {
    auto *magic = (const uint32_t *)bytecode;
    PSTRACE("CompileShader: %s size=%zu magic=0x%08x (DXBC=0x43425844 DXIL=0x4C495844)", func_name, size, *magic);
    if (*magic == 0x43425844 && size >= 32) {
      auto *chunks = (const uint32_t *)bytecode;
      uint32_t container_size = chunks[6];
      uint32_t num_chunks = chunks[7];
      PSTRACE("  DXBC: container_size=%u num_chunks=%u", container_size, num_chunks);
      for (uint32_t i = 0; i < num_chunks && i < 16; i++) {
        uint32_t offset = chunks[8 + i];
        if (offset + 8 <= size) {
          char tag[5] = {};
          memcpy(tag, (const char *)bytecode + offset, 4);
          uint32_t chunk_size = 0;
          memcpy(&chunk_size, (const char *)bytecode + offset + 4,
                 sizeof(chunk_size));
          PSTRACE("  chunk[%u]: tag='%s' offset=%u size=%u", i, tag, offset, chunk_size);
        }
      }
    }
  }
  sm50_error_t sm50_err = nullptr;
  sm50_shader_t shader = nullptr;
  MTL_SHADER_REFLECTION reflection = {};

  if (SM50Initialize(bytecode, size, &shader, &reflection, &sm50_err)) {
    char err_buf[256] = {};
    SM50GetErrorMessage(sm50_err, err_buf, sizeof(err_buf));
    SM50FreeError(sm50_err);

    bool has_dxil = false;
    using namespace microsoft;
    CDXBCParser dxbcParser;
    if (SUCCEEDED(dxbcParser.ReadDXBC(bytecode, size))) {
      for (UINT32 i = 0; i < dxbcParser.GetBlobCount(); i++) {
        if (dxbcParser.GetBlobFourCC(i) == dxmt::dxil::DXIL_FOURCC) {
          has_dxil = true;
          const void *blob = dxbcParser.GetBlob(i);
          UINT32 blob_size = dxbcParser.GetBlobSize(i);
          PSTRACE("DXIL blob found index=%u size=%u", i, blob_size);

          auto wmt_device = m_device->GetDXMTDevice().device();

          char cache_path[256];
          snprintf(cache_path, sizeof(cache_path), "/tmp/dxmt_shader_cache/%016zx", hash);
          char dxbc_path[256], metallib_path[256], reflection_path[256],
              module_summary_path[256], dxil_report_path[256],
              metallib_error_path[256];
          snprintf(dxbc_path, sizeof(dxbc_path), "%s.dxbc", cache_path);
          snprintf(metallib_path, sizeof(metallib_path), "%s.metallib", cache_path);
          snprintf(reflection_path, sizeof(reflection_path), "%s.json", cache_path);
          snprintf(module_summary_path, sizeof(module_summary_path),
                   "%s.module.txt", cache_path);
          snprintf(dxil_report_path, sizeof(dxil_report_path),
                   "%s.dxil_report.txt", cache_path);
          snprintf(metallib_error_path, sizeof(metallib_error_path),
                   "%s.metallib.err.txt", cache_path);
          EnsureShaderCacheDir();

          FILE *mf = fopen(metallib_path, "rb");
          if (!mf) {
            PSTRACE("  metallib not cached, attempting DXIL->MSL compilation");

            auto container = dxmt::dxil::DXILContainer::parse(blob, blob_size);
            if (!container) {
              PSTRACE("  DXILContainer::parse FAILED for %s", func_name);
              DumpShaderBlob(dxbc_path, bytecode, size);
              return RecordCompileFailure("shader/dxil_container_parse",
                                          str::format(func_name, " DXIL container parse failed; dumped ", dxbc_path));
            }

            auto &shader_info = container->shader();
            PSTRACE("  DXIL container parsed: kind=%u sm=%u.%u bc_size=%u",
                    (uint32_t)shader_info.kind, shader_info.shader_model.major,
                    shader_info.shader_model.minor, shader_info.bitcode.size);

            auto module = dxmt::dxil::BitcodeReader::parse(
                shader_info.bitcode.data, shader_info.bitcode.size);
            if (!module) {
              PSTRACE("  BitcodeReader::parse FAILED");
              DumpShaderBlob(dxbc_path, bytecode, size);
              return RecordCompileFailure("shader/bitcode_parse",
                                          str::format(func_name, " DXIL bitcode parse failed; dumped ", dxbc_path));
            }

            PSTRACE("  Bitcode parsed: types=%zu functions=%zu constants=%zu",
                    module->types.size(), module->functions.size(), module->constants.size());
            DumpDXILModuleSummary(module_summary_path, *module, shader_info);
            PSTRACE("  DXIL module summary written to %s", module_summary_path);

            auto msl_result = dxmt::dxil::DXILToMSL::convert(*module, shader_info);
            if (!msl_result) {
              PSTRACE("  DXILToMSL::convert FAILED");
              DumpShaderBlob(dxbc_path, bytecode, size);
              return RecordCompileFailure("shader/dxil_to_msl",
                                          str::format(func_name,
                                                      " DXIL to MSL conversion failed; module ",
                                                      module_summary_path,
                                                      "; dxbc ", dxbc_path));
            }

            PSTRACE("  MSL generated: %zu bytes, entry=%s unsupported_intrinsics=%u unsupported_opcodes=%u",
                    msl_result->source.size(), msl_result->entry_point.c_str(),
                    msl_result->unsupported_intrinsics, msl_result->unsupported_opcodes);

            char msl_path[256];
            char msl_error_path[256];
            snprintf(msl_path, sizeof(msl_path), "%s.msl", cache_path);
            snprintf(msl_error_path, sizeof(msl_error_path),
                     "%s.msl.err.txt", cache_path);
            DumpShaderText(msl_path, msl_result->source.c_str());
            PSTRACE("  MSL source written to %s", msl_path);
            DumpDXILCompileReport(dxil_report_path, func_name, hash, size,
                                  dxbc_path, module_summary_path, msl_path,
                                  *module, shader_info, *msl_result);
            PSTRACE("  DXIL compile report written to %s", dxil_report_path);

            WMT::Reference<WMT::Error> compile_err;
            auto library = wmt_device.newLibraryWithSource(
                msl_result->source.c_str(), msl_result->source.size(), compile_err);

            if (compile_err.handle) {
              char *err_desc = (char *)NSObject_description(compile_err.handle);
              DumpShaderText(msl_error_path, err_desc ? err_desc : "unknown");
              PSTRACE("  newLibraryWithSource FAILED: %s", err_desc ? err_desc : "unknown");
              Logger::err(str::format("DXIL MSL compilation failed for ", func_name, ": ",
                                       err_desc ? err_desc : "unknown error"));
              DumpShaderBlob(dxbc_path, bytecode, size);
              return RecordCompileFailure("shader/metal_library_source",
                                          str::format(func_name, " MSL compile failed: ",
                                                      err_desc ? err_desc : "unknown",
                                                      "; msl ", msl_path,
                                                      "; error ", msl_error_path,
                                                      "; dxbc ", dxbc_path));
            }

            PSTRACE("  Metal library compiled OK from source lib_handle=%llu", (unsigned long long)library.handle);

            const char *dump_msl = std::getenv("DXMT_DUMP_MSL");
            if (dump_msl && dump_msl[0] && strcmp(dump_msl, "0") != 0) {
              char dump_path[256];
              snprintf(dump_path, sizeof(dump_path), "/tmp/dxmt_msl_%s_%016zx.metal", func_name, hash);
              FILE *df = fopen(dump_path, "w");
              if (df) { fwrite(msl_result->source.c_str(), 1, msl_result->source.size(), df); fclose(df); }
            }

            const char *entry_name = msl_result->entry_point.c_str();
            if (strcmp(entry_name, "cs_main") != 0 &&
                strcmp(entry_name, "vs_main") != 0 &&
                strcmp(entry_name, "ps_main") != 0) {
              switch (shader_info.kind) {
              case dxmt::dxil::DxilShaderKind::Compute: entry_name = "cs_main"; break;
              case dxmt::dxil::DxilShaderKind::Vertex: entry_name = "vs_main"; break;
              case dxmt::dxil::DxilShaderKind::Pixel: entry_name = "ps_main"; break;
              default: break;
              }
            }

            out_func = library.newFunction(entry_name);
            PSTRACE("  newFunction(%s) on lib=%llu -> func_handle=%llu", entry_name, (unsigned long long)library.handle, (unsigned long long)out_func.handle);
            if (!out_func.handle) {
              PSTRACE("  newFunction(%s) returned null, trying alternatives", entry_name);
              out_func = library.newFunction("main");
              if (!out_func.handle)
                out_func = library.newFunction("cs_main");
              if (!out_func.handle)
                out_func = library.newFunction("vs_main");
              if (!out_func.handle)
                out_func = library.newFunction("ps_main");
            }

            if (out_func.handle) {
              PSTRACE("  DXIL shader compiled OK! entry=%s", entry_name);
              s_shader_cache[hash] = out_func;

              if (type == ShaderType::Vertex)
                m_vs_uses_stage_in = true;

              if (shader_info.kind == dxmt::dxil::DxilShaderKind::Compute) {
                m_threadgroup_size.width = msl_result->tg_size[0];
                m_threadgroup_size.height = msl_result->tg_size[1];
                m_threadgroup_size.depth = msl_result->tg_size[2];
              }
              return true;
            } else {
              PSTRACE("  newFunction returned null for all entry points");
              Logger::err(str::format("DXIL: failed to get function from compiled library for ", func_name));
              return RecordCompileFailure("shader/metal_function_lookup",
                                          str::format(func_name, " function lookup failed after MSL compile; msl ",
                                                      msl_path));
            }
          }

          PSTRACE("  loading cached metallib from %s", metallib_path);
          fseek(mf, 0, SEEK_END);
          long lib_size = ftell(mf);
          fseek(mf, 0, SEEK_SET);
          PSTRACE("  metallib size=%ld", lib_size);
          if (lib_size > 0) {
            std::vector<uint8_t> lib_data(lib_size);
            fread(lib_data.data(), 1, lib_size, mf);
            fclose(mf);
            auto dispatch_data = WMT::MakeDispatchData(lib_data.data(), lib_size);
            WMT::Reference<WMT::Error> err;
            auto library = wmt_device.newLibrary(dispatch_data, err);
            if (!err.handle) {
              char actual_entry[256] = {};
              char rbuf[4096] = {};
              FILE *rf = fopen(reflection_path, "r");
              if (rf) {
                fread(rbuf, 1, sizeof(rbuf)-1, rf);
                fclose(rf);
                char *ep = strstr(rbuf, "\"EntryPoint\"");
                if (ep) {
                  char *q1 = strchr(ep + 13, '"');
                  char *q2 = q1 ? strchr(q1+1, '"') : nullptr;
                  if (q1 && q2) {
                    size_t len = q2 - q1 - 1;
                    if (len < sizeof(actual_entry)) {
                      memcpy(actual_entry, q1+1, len);
                      actual_entry[len] = 0;
                    }
                  }
                }
              }
              const char *fn_name = actual_entry[0] ? actual_entry : func_name;
              PSTRACE("  trying newFunction(%s)", fn_name);
              out_func = library.newFunction(fn_name);
              if (!out_func.handle && actual_entry[0]) {
                out_func = library.newFunction(func_name);
              }
              if (!out_func.handle)
                out_func = library.newFunction("main");
              if (!out_func.handle)
                out_func = library.newFunction("cs_main");
              if (!out_func.handle)
                out_func = library.newFunction("vs_main");
              if (!out_func.handle)
                out_func = library.newFunction("ps_main");
              if (out_func.handle) {
                PSTRACE("  DXIL loaded from cache OK! entry=%s", fn_name);
                s_shader_cache[hash] = out_func;
                if (type == ShaderType::Vertex)
                  m_vs_uses_stage_in = true;
                char *tg = strstr(rbuf, "\"tg_size\"");
                if (tg) {
                  int tw=1,th=1,td=1;
                  if (sscanf(tg, "\"tg_size\": [%d, %d, %d]", &tw, &th, &td) == 3 ||
                      sscanf(tg, "\"tg_size\":[%d,%d,%d]", &tw, &th, &td) == 3) {
                    m_threadgroup_size.width = tw;
                    m_threadgroup_size.height = th;
                    m_threadgroup_size.depth = td;
                    PSTRACE("  threadgroup_size from reflection: %dx%dx%d", tw, th, td);
                  }
                }
                return true;
              } else {
                PSTRACE("  WMT newFunction returned null");
                DumpShaderBlob(dxbc_path, bytecode, size);
                return RecordCompileFailure(
                    "shader/dxil_cached_function_lookup",
                    str::format(func_name,
                                " cached metallib function lookup failed; metallib ",
                                metallib_path, "; reflection ", reflection_path,
                                "; dxbc ", dxbc_path));
              }
            } else {
              char *err_desc = (char *)NSObject_description(err.handle);
              DumpShaderText(metallib_error_path,
                             err_desc ? err_desc : "unknown");
              DumpShaderBlob(dxbc_path, bytecode, size);
              PSTRACE("  WMT newLibrary FAILED: %s",
                      err_desc ? err_desc : "unknown");
              return RecordCompileFailure(
                  "shader/dxil_cached_metallib_load",
                  str::format(func_name,
                              " cached metallib load failed: ",
                              err_desc ? err_desc : "unknown",
                              "; metallib ", metallib_path, "; error ",
                              metallib_error_path, "; dxbc ", dxbc_path));
            }
          } else {
            fclose(mf);
            DumpShaderBlob(dxbc_path, bytecode, size);
            return RecordCompileFailure(
                "shader/dxil_cached_metallib_empty",
                str::format(func_name, " cached metallib empty; metallib ",
                            metallib_path, "; dxbc ", dxbc_path));
          }
          break;
        }
      }
    }
    if (!has_dxil) {
      char dxbc_path[256];
      snprintf(dxbc_path, sizeof(dxbc_path),
               "/tmp/dxmt_shader_cache/%016zx.sm50_failed.dxbc", hash);
      DumpShaderBlob(dxbc_path, bytecode, size);
      PSTRACE("SM50Init FAILED for %s: %s (no DXIL chunk, dumped %s)",
              func_name, err_buf, dxbc_path);
    }
    return RecordCompileFailure(has_dxil ? "shader/dxil_metallib_cache" : "shader/sm50_init",
                                str::format(func_name, " SM50Initialize failed: ", err_buf));
  }

  SM50_SHADER_COMMON_DATA common = {};
  common.next = nullptr;
  common.type = SM50_SHADER_COMMON;
  common.metal_version = SM50_SHADER_METAL_310;
  common.flags = {};

  if (type == ShaderType::Compute) {
    uint32_t tgx = reflection.ThreadgroupSize[0] ? reflection.ThreadgroupSize[0] : 1;
    uint32_t tgy = reflection.ThreadgroupSize[1] ? reflection.ThreadgroupSize[1] : 1;
    uint32_t tgz = reflection.ThreadgroupSize[2] ? reflection.ThreadgroupSize[2] : 1;
    m_threadgroup_size.width = tgx;
    m_threadgroup_size.height = tgy;
    m_threadgroup_size.depth = tgz;
    PSTRACE("CompileShader: %s SM50 threadgroup_size=%ux%ux%u",
            func_name, tgx, tgy, tgz);
  }

  std::vector<SM50_IA_INPUT_ELEMENT> ia_elements;
  SM50_SHADER_IA_INPUT_LAYOUT_DATA ia_layout = {};
  SM50_SHADER_COMPILATION_ARGUMENT_DATA *compile_args =
      (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&common;
  if (type == ShaderType::Vertex) {
    uint32_t slot_mask = 0;
    BuildIAInputLayout(bytecode, size, ia_elements, slot_mask);
    m_ia_slot_mask = slot_mask;
    ia_layout.next = &common;
    ia_layout.type = SM50_SHADER_IA_INPUT_LAYOUT;
    ia_layout.index_buffer_format = SM50_INDEX_BUFFER_FORMAT_NONE;
    ia_layout.slot_mask = slot_mask;
    ia_layout.num_elements = (uint32_t)ia_elements.size();
    ia_layout.elements = ia_elements.data();
    compile_args = (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&ia_layout;
    PSTRACE("CompileShader: %s IA args elements=%u slot_mask=0x%x",
            func_name, ia_layout.num_elements, ia_layout.slot_mask);
  }

  sm50_bitcode_t compile_result = nullptr;
  if (SM50Compile(shader, compile_args,
                  func_name, &compile_result, &sm50_err)) {
    char err_buf[256] = {};
    SM50GetErrorMessage(sm50_err, err_buf, sizeof(err_buf));
    char dxbc_path[256];
    snprintf(dxbc_path, sizeof(dxbc_path),
             "/tmp/dxmt_shader_cache/%016zx.sm50_compile_failed.dxbc", hash);
    DumpShaderBlob(dxbc_path, bytecode, size);
    PSTRACE("SM50Compile failed for %s: %s (dumped %s)",
            func_name, err_buf, dxbc_path);
    Logger::err(str::format("SM50Compile failed for ", func_name, ": ", err_buf));
    SM50FreeError(sm50_err);
    SM50Destroy(shader);
    return RecordCompileFailure("shader/sm50_compile",
                                str::format(func_name, " SM50Compile failed: ",
                                            err_buf, "; dumped ", dxbc_path));
  }

  SM50_COMPILED_BITCODE bitcode = {};
  SM50GetCompiledBitcode(compile_result, &bitcode);

  {
    char dump_path[256];
    snprintf(dump_path, sizeof(dump_path), "/tmp/dxmt_sm50_%s.metallib", func_name);
    FILE *df = fopen(dump_path, "wb");
    if (df) { fwrite(bitcode.Data, 1, bitcode.Size, df); fclose(df); }
    Logger::info(str::format("  SM50 dumped ", func_name, " to ", dump_path, " (", bitcode.Size, " bytes)"));
  }

  auto wmt_device = m_device->GetDXMTDevice().device();
  WMT::Reference<WMT::Error> err;
  auto lib_data = WMT::MakeDispatchData(bitcode.Data, bitcode.Size);
  auto library = wmt_device.newLibrary(lib_data, err);

  if (err.handle) {
    char *err_desc = (char *)NSObject_description(err.handle);
    char dxbc_path[256];
    snprintf(dxbc_path, sizeof(dxbc_path),
             "/tmp/dxmt_shader_cache/%016zx.sm50_metal_library_failed.dxbc", hash);
    DumpShaderBlob(dxbc_path, bytecode, size);
    PSTRACE("Failed to create Metal library for %s: %s (dumped %s)",
            func_name, err_desc ? err_desc : "unknown", dxbc_path);
    Logger::err(str::format("Failed to create Metal library for ", func_name));
    SM50DestroyBitcode(compile_result);
    SM50Destroy(shader);
    return RecordCompileFailure("shader/sm50_metal_library",
                                str::format(func_name, " SM50 Metal library creation failed: ",
                                            err_desc ? err_desc : "unknown",
                                            "; dumped ", dxbc_path));
  }

  out_func = library.newFunction(func_name);
  SM50DestroyBitcode(compile_result);

  if (out_reflection) {
    *out_reflection = reflection;
  }

  if (out_shader_handle) {
    *out_shader_handle = shader;
  } else {
    SM50Destroy(shader);
  }

  if (!out_func.handle) {
    char dxbc_path[256];
    snprintf(dxbc_path, sizeof(dxbc_path),
             "/tmp/dxmt_shader_cache/%016zx.sm50_function_lookup_failed.dxbc", hash);
    DumpShaderBlob(dxbc_path, bytecode, size);
    PSTRACE("Failed to get function %s from Metal library (dumped %s)",
            func_name, dxbc_path);
    Logger::err(str::format("Failed to get function ", func_name));
    return RecordCompileFailure("shader/sm50_function_lookup",
                                str::format(func_name,
                                            " SM50 function lookup failed; dumped ",
                                            dxbc_path));
  }

  PSTRACE("CompileShader: %s SM50 OK function=%llu", func_name,
          (unsigned long long)out_func.handle);
  Logger::info(str::format("  Compiled ", func_name, " OK"));
  {
    std::lock_guard<std::mutex> lock(s_shader_mutex);
    s_shader_cache[hash] = out_func;
  }
  return true;
}

void MTLD3D12PipelineState::BuildIAInputLayout(
    const void *bytecode, SIZE_T size,
    std::vector<SM50_IA_INPUT_ELEMENT> &elements,
    uint32_t &slot_mask) const {
  slot_mask = 0;
  elements.clear();

  if (!bytecode || !size || !m_input_layout.NumElements ||
      !m_input_layout.pInputElementDescs)
    return;

  using namespace microsoft;
  CSignatureParser parser;
  HRESULT hr = DXBCGetInputSignature(bytecode, &parser);
  if (FAILED(hr)) {
    PSTRACE("BuildIAInputLayout: DXBCGetInputSignature failed hr=0x%lx", hr);
    return;
  }

  const D3D11_SIGNATURE_PARAMETER *params = nullptr;
  uint32_t param_count = parser.GetParameters(&params);
  uint32_t append_offset[WMT_MAX_VERTEX_BUFFER_LAYOUTS] = {};

  for (UINT i = 0; i < m_input_layout.NumElements; i++) {
    const auto &desc = m_input_layout.pInputElementDescs[i];
    if (desc.InputSlot >= kMetalD3D12VertexBufferSlotCount) {
      PSTRACE("BuildIAInputLayout skip[%u]: slot %u outside cap %u",
              i, desc.InputSlot, kMetalD3D12VertexBufferSlotCount);
      continue;
    }

    MTL_DXGI_FORMAT_DESC metal_format = {};
    if (FAILED(MTLQueryDXGIFormat(m_device->GetMTLDevice(), desc.Format, metal_format)) ||
        !metal_format.AttributeFormat || !metal_format.BytesPerTexel) {
      PSTRACE("BuildIAInputLayout skip[%u]: unsupported fmt=%u",
              i, (unsigned)desc.Format);
      continue;
    }

    auto *sig = std::find_if(
        params, params + param_count,
        [&](const D3D11_SIGNATURE_PARAMETER &input_sig) {
          return input_sig.SystemValue == D3D10_SB_NAME_UNDEFINED &&
                 desc.SemanticIndex == input_sig.SemanticIndex &&
                 desc.SemanticName && input_sig.SemanticName &&
                 strcasecmp(desc.SemanticName, input_sig.SemanticName) == 0;
        });
    if (sig == params + param_count) {
      PSTRACE("BuildIAInputLayout skip[%u]: semantic %s%u not consumed by VS",
              i, desc.SemanticName ? desc.SemanticName : "?", desc.SemanticIndex);
      continue;
    }

    uint32_t aligned_offset =
        desc.AlignedByteOffset == D3D12_APPEND_ALIGNED_ELEMENT
            ? AlignD3D12InputOffset(append_offset[desc.InputSlot],
                                    metal_format.BytesPerTexel)
            : desc.AlignedByteOffset;
    append_offset[desc.InputSlot] = aligned_offset + metal_format.BytesPerTexel;

    SM50_IA_INPUT_ELEMENT element = {};
    element.reg = sig->Register;
    element.slot = desc.InputSlot;
    element.aligned_byte_offset = aligned_offset;
    element.format = metal_format.AttributeFormat;
    element.step_function =
        desc.InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
    element.step_rate =
        desc.InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
            ? desc.InstanceDataStepRate
            : 1;
    elements.push_back(element);
    slot_mask |= 1u << desc.InputSlot;

    PSTRACE("BuildIAInputLayout element[%zu]: semantic=%s%u reg=%u slot=%u offset=%u fmt=%u step=%u/%u",
            elements.size() - 1, desc.SemanticName ? desc.SemanticName : "?",
            desc.SemanticIndex, element.reg, element.slot,
            element.aligned_byte_offset, element.format,
            element.step_function, element.step_rate);
  }
}

bool MTLD3D12PipelineState::Compile() {
  PTRACE("Compile() called compiled=%d is_compute=%d", m_compiled, m_is_compute);
  if (m_compiled)
    return true;
  ClearCompileFailure();

  auto wmt_device = m_device->GetDXMTDevice().device();
  WMT::Reference<WMT::Error> err;

  if (m_is_compute) {
    if (m_cs.empty()) {
      Logger::err("Compute PSO has no CS bytecode");
      return RecordCompileFailure("pso/compute_no_cs", "Compute PSO has no CS bytecode");
    }

    WMT::Reference<WMT::Function> cs_func;
    if (!CompileShader(m_cs.data(), m_cs.size(), ShaderType::Compute,
                       "cs_main", cs_func, &m_cs_shader, &m_cs_reflection))
      return false;

    WMTComputePipelineInfo info = {};
    WMT::InitializeComputePipelineInfo(info);
    info.compute_function = cs_func.handle;

    m_compute_pso = wmt_device.newComputePipelineState(info, err);
    if (!m_compute_pso.handle) {
      char *err_desc = err.handle ? (char *)NSObject_description(err.handle) : nullptr;
      Logger::err(str::format("Failed to create compute PSO: ",
                              err_desc ? err_desc : "unknown"));
      if (m_cs_shader) {
        SM50Destroy(m_cs_shader);
        m_cs_shader = nullptr;
      }
      return RecordCompileFailure("pso/metal_compute_pso",
                                  str::format("Metal compute PSO creation failed: ",
                                              err_desc ? err_desc : "unknown"));
    }

    PTRACE("CS_ARGS_DEBUG: shader=%llu NumCB=%u NumArgs=%u CBufBindIdx=%u ArgBufBindIdx=%u ArgTableQwords=%u",
      (unsigned long long)(uintptr_t)m_cs_shader,
      m_cs_reflection.NumConstantBuffers, m_cs_reflection.NumArguments,
      m_cs_reflection.ConstanttBufferTableBindIndex,
      m_cs_reflection.ArgumentBufferBindIndex,
      m_cs_reflection.ArgumentTableQwords);
    if (m_cs_shader && (m_cs_reflection.NumArguments > 0 ||
                        m_cs_reflection.NumConstantBuffers > 0)) {
      if (m_cs_reflection.NumConstantBuffers > 0)
        m_cs_cb_args.resize(m_cs_reflection.NumConstantBuffers);
      if (m_cs_reflection.NumArguments > 0)
        m_cs_args.resize(m_cs_reflection.NumArguments);
      SM50GetArgumentsInfo(m_cs_shader,
                           m_cs_cb_args.empty() ? nullptr : m_cs_cb_args.data(),
                           m_cs_args.empty() ? nullptr : m_cs_args.data());
      for (size_t i = 0; i < m_cs_cb_args.size(); i++) {
        PTRACE("CS_ARGS_DEBUG: cb[%zu] type=%d slot=%u flags=0x%x offset=%u",
          i, (int)m_cs_cb_args[i].Type, m_cs_cb_args[i].SM50BindingSlot,
          m_cs_cb_args[i].Flags, m_cs_cb_args[i].StructurePtrOffset);
      }
      for (size_t i = 0; i < m_cs_args.size(); i++) {
        PTRACE("CS_ARGS_DEBUG: arg[%zu] type=%d slot=%u flags=0x%x offset=%u",
          i, (int)m_cs_args[i].Type, m_cs_args[i].SM50BindingSlot,
          m_cs_args[i].Flags, m_cs_args[i].StructurePtrOffset);
      }
    }
    if (m_cs_shader) {
      SM50Destroy(m_cs_shader);
      m_cs_shader = nullptr;
    }

    m_compiled = true;
    Logger::info("Compute PSO compiled successfully");
    return true;
  }

  WMT::Reference<WMT::Function> vs_func, ps_func;

  if (!m_hs.empty() || !m_ds.empty()) {
    return RecordCompileFailure(
        "pso/unsupported_tessellation",
        str::format("Graphics PSO uses HS bytes=", m_hs.size(),
                    " DS bytes=", m_ds.size(),
                    " but D3D12 tessellation is not implemented"));
  }

  if (!m_gs.empty()) {
    return RecordCompileFailure(
        "pso/unsupported_geometry_shader",
        str::format("Graphics PSO uses GS bytes=", m_gs.size(),
                    " but D3D12 geometry shaders are not implemented"));
  }

  if (m_has_stream_output) {
    return RecordCompileFailure(
        "pso/unsupported_stream_output",
        "Graphics PSO uses stream output, which is not implemented");
  }

  if (!m_vs.empty()) {
    if (!CompileShader(m_vs.data(), m_vs.size(), ShaderType::Vertex,
                       "vs_main", vs_func, &m_vs_shader, &m_vs_reflection))
      return false;
  }

  if (!m_ps.empty()) {
    if (!CompileShader(m_ps.data(), m_ps.size(), ShaderType::Pixel,
                       "ps_main", ps_func, &m_ps_shader, &m_ps_reflection))
      return false;
  }

  WMTRenderPipelineInfo info;
  WMT::InitializeRenderPipelineInfo(info);

  if (vs_func.handle)
    info.vertex_function = vs_func.handle;
  if (ps_func.handle)
    info.fragment_function = ps_func.handle;

  info.rasterization_enabled = (m_rasterizer_desc.FillMode != D3D12_FILL_MODE_WIREFRAME);
  info.raster_sample_count = m_sample_count ? m_sample_count : 1;

  for (UINT i = 0; i < m_num_render_targets && i < 8; i++) {
    auto fmt = DXGIToMTLPixelFormat(m_rtv_formats[i]);
    if (fmt != WMTPixelFormatInvalid)
      info.colors[i].pixel_format = fmt;
  }

  auto depth_fmt = DXGIToMTLPixelFormat(m_dsv_format);
  if (depth_fmt != WMTPixelFormatInvalid) {
    info.depth_pixel_format = depth_fmt;
    if (m_dsv_format == DXGI_FORMAT_D24_UNORM_S8_UINT ||
        m_dsv_format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
      info.stencil_pixel_format = depth_fmt;
  }

  if (m_blend_desc.RenderTarget[0].BlendEnable) {
    for (UINT i = 0; i < m_num_render_targets && i < 8; i++) {
      auto &rt = m_blend_desc.RenderTarget[i];
      info.colors[i].blending_enabled = rt.BlendEnable ? true : false;
      info.colors[i].write_mask = kColorWriteMaskMap[rt.RenderTargetWriteMask & 0xf];

      auto map_blend = [](D3D12_BLEND b) -> WMTBlendFactor {
        switch (b) {
        case D3D12_BLEND_ZERO: return WMTBlendFactorZero;
        case D3D12_BLEND_ONE: return WMTBlendFactorOne;
        case D3D12_BLEND_SRC_COLOR: return WMTBlendFactorSourceColor;
        case D3D12_BLEND_INV_SRC_COLOR: return WMTBlendFactorOneMinusSourceColor;
        case D3D12_BLEND_SRC_ALPHA: return WMTBlendFactorSourceAlpha;
        case D3D12_BLEND_INV_SRC_ALPHA: return WMTBlendFactorOneMinusSourceAlpha;
        case D3D12_BLEND_DEST_ALPHA: return WMTBlendFactorDestinationAlpha;
        case D3D12_BLEND_INV_DEST_ALPHA: return WMTBlendFactorOneMinusDestinationAlpha;
        case D3D12_BLEND_DEST_COLOR: return WMTBlendFactorDestinationColor;
        case D3D12_BLEND_INV_DEST_COLOR: return WMTBlendFactorOneMinusDestinationColor;
        case D3D12_BLEND_SRC_ALPHA_SAT: return WMTBlendFactorSourceAlphaSaturated;
        case D3D12_BLEND_BLEND_FACTOR: return WMTBlendFactorBlendColor;
        case D3D12_BLEND_INV_BLEND_FACTOR: return WMTBlendFactorOneMinusBlendColor;
        default: return WMTBlendFactorOne;
        }
      };

      auto map_op = [](D3D12_BLEND_OP op) -> WMTBlendOperation {
        switch (op) {
        case D3D12_BLEND_OP_ADD: return WMTBlendOperationAdd;
        case D3D12_BLEND_OP_SUBTRACT: return WMTBlendOperationSubtract;
        case D3D12_BLEND_OP_REV_SUBTRACT: return WMTBlendOperationReverseSubtract;
        case D3D12_BLEND_OP_MIN: return WMTBlendOperationMin;
        case D3D12_BLEND_OP_MAX: return WMTBlendOperationMax;
        default: return WMTBlendOperationAdd;
        }
      };

      info.colors[i].src_rgb_blend_factor = map_blend(rt.SrcBlend);
      info.colors[i].dst_rgb_blend_factor = map_blend(rt.DestBlend);
      info.colors[i].rgb_blend_operation = map_op(rt.BlendOp);
      info.colors[i].src_alpha_blend_factor = map_blend(rt.SrcBlendAlpha);
      info.colors[i].dst_alpha_blend_factor = map_blend(rt.DestBlendAlpha);
      info.colors[i].alpha_blend_operation = map_op(rt.BlendOpAlpha);
    }
  }

  switch (m_topology) {
  case D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT: info.input_primitive_topology = WMTPrimitiveTopologyClassPoint; break;
  case D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE: info.input_primitive_topology = WMTPrimitiveTopologyClassLine; break;
  case D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE: info.input_primitive_topology = WMTPrimitiveTopologyClassTriangle; break;
  default: info.input_primitive_topology = WMTPrimitiveTopologyClassUnspecified; break;
  }

  info.immutable_vertex_buffers = (1 << 16) | (1 << 29) | (1 << 30);
  info.immutable_fragment_buffers = (1 << 29) | (1 << 30);

  WMTVertexDescriptor vtx_desc = {};
  if (m_input_layout.NumElements > 0 && m_input_layout.pInputElementDescs) {
    uint32_t append_offset[WMT_MAX_VERTEX_BUFFER_LAYOUTS] = {};
    uint32_t slot_stride[WMT_MAX_VERTEX_BUFFER_LAYOUTS] = {};
    uint32_t max_slot = 0;
    bool slot_per_vertex[WMT_MAX_VERTEX_BUFFER_LAYOUTS] = {};
    uint32_t attribute_count = 0;
    uint32_t next_attribute = 0;
    const microsoft::D3D11_SIGNATURE_PARAMETER *input_sig_params = nullptr;
    uint32_t input_sig_count = 0;
    microsoft::CSignatureParser input_sig_parser;
    bool has_input_signature =
        !m_vs.empty() &&
        SUCCEEDED(DXBCGetInputSignature(m_vs.data(), &input_sig_parser));
    if (has_input_signature) {
      input_sig_count = input_sig_parser.GetParameters(&input_sig_params);
      PSTRACE("D3D12 PSO input-layout: shader input signature params=%u",
              input_sig_count);
    } else {
      PSTRACE("D3D12 PSO input-layout: shader input signature unavailable; using layout order");
    }

    PSTRACE("D3D12 PSO input-layout: elements=%u metal_attr_cap=%u metal_slot_cap=%u",
            m_input_layout.NumElements, WMT_MAX_VERTEX_ATTRIBUTES,
            kMetalD3D12VertexBufferSlotCount);

    for (UINT i = 0; i < m_input_layout.NumElements; i++) {
      auto &el = m_input_layout.pInputElementDescs[i];

      MTL_DXGI_FORMAT_DESC metal_format = {};
      if (FAILED(MTLQueryDXGIFormat(m_device->GetMTLDevice(), el.Format, metal_format)) ||
          !metal_format.AttributeFormat || !metal_format.BytesPerTexel) {
        PSTRACE("D3D12 PSO input-layout skip[%u]: unsupported fmt=%u semantic=%s%u",
                i, (unsigned)el.Format, el.SemanticName ? el.SemanticName : "?",
                el.SemanticIndex);
        continue;
      }

      if (el.InputSlot >= kMetalD3D12VertexBufferSlotCount) {
        PSTRACE("D3D12 PSO input-layout skip[%u]: input slot %u is outside Metal-backed slot cap %u",
                i, el.InputSlot, kMetalD3D12VertexBufferSlotCount);
        continue;
      }

      if (attribute_count >= WMT_MAX_VERTEX_ATTRIBUTES) {
        PSTRACE("D3D12 PSO input-layout skip[%u]: attribute cap %u reached",
                i, WMT_MAX_VERTEX_ATTRIBUTES);
        continue;
      }

      uint32_t attr_index = next_attribute;
      if (has_input_signature && input_sig_params) {
        auto *sig = std::find_if(
            input_sig_params, input_sig_params + input_sig_count,
            [&](const microsoft::D3D11_SIGNATURE_PARAMETER &input_sig) {
              return input_sig.SystemValue == microsoft::D3D10_SB_NAME_UNDEFINED &&
                     el.SemanticName && input_sig.SemanticName &&
                     el.SemanticIndex == input_sig.SemanticIndex &&
                     strcasecmp(el.SemanticName, input_sig.SemanticName) == 0;
            });
        if (sig != input_sig_params + input_sig_count) {
          attr_index = sig->Register;
        } else {
          PSTRACE("D3D12 PSO input-layout desc[%u]: semantic %s%u not found in input signature; using attr order %u",
                  i, el.SemanticName ? el.SemanticName : "?",
                  el.SemanticIndex, attr_index);
        }
      }

      if (attr_index >= WMT_MAX_VERTEX_ATTRIBUTES) {
        PSTRACE("D3D12 PSO input-layout skip[%u]: mapped attribute %u outside cap %u",
                i, attr_index, WMT_MAX_VERTEX_ATTRIBUTES);
        continue;
      }
      next_attribute = std::max(next_attribute, attr_index + 1);

      uint32_t aligned_offset =
          el.AlignedByteOffset == D3D12_APPEND_ALIGNED_ELEMENT
              ? AlignD3D12InputOffset(append_offset[el.InputSlot],
                                      metal_format.BytesPerTexel)
              : el.AlignedByteOffset;
      uint32_t end = aligned_offset + metal_format.BytesPerTexel;
      append_offset[el.InputSlot] = end;
      if (end > slot_stride[el.InputSlot])
        slot_stride[el.InputSlot] = end;
      if (el.InputSlot >= max_slot)
        max_slot = el.InputSlot + 1;
      slot_per_vertex[el.InputSlot] = (el.InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA);

      auto &attr = vtx_desc.attributes[attr_index];
      attr.format = metal_format.AttributeFormat;
      attr.offset = aligned_offset;
      attr.buffer_index = el.InputSlot;
      attribute_count = std::max(attribute_count, attr_index + 1);

      PSTRACE("D3D12 PSO input-layout attr[%u]<-desc[%u]: semantic=%s%u fmt=%u mtl_fmt=%u slot=%u offset=%u stride_end=%u class=%u step=%u",
              attr_index, i, el.SemanticName ? el.SemanticName : "?",
              el.SemanticIndex, (unsigned)el.Format,
              (unsigned)metal_format.AttributeFormat, el.InputSlot,
              aligned_offset, end, (unsigned)el.InputSlotClass,
              el.InstanceDataStepRate);
    }
    vtx_desc.attribute_count = attribute_count;
    vtx_desc.layout_count = max_slot;
    for (uint32_t s = 0; s < max_slot; s++) {
      vtx_desc.layouts[s].stride = slot_stride[s];
      vtx_desc.layouts[s].step_function = slot_per_vertex[s]
          ? WMTVertexStepFunctionPerVertex : WMTVertexStepFunctionPerInstance;
      vtx_desc.layouts[s].step_rate = 1;
      PSTRACE("D3D12 PSO input-layout slot[%u]: stride=%u step=%u",
              s, slot_stride[s], (unsigned)vtx_desc.layouts[s].step_function);
    }
    if (m_vs_uses_stage_in && attribute_count > 0) {
      info.vertex_descriptor = &vtx_desc;
      PSTRACE("D3D12 PSO input-layout attached as Metal vertex descriptor for DXIL stage_in");
    } else {
      PSTRACE("D3D12 PSO input-layout compiled for SM50 vertex pulling; Metal vertex descriptor disabled");
    }
  }

  PSTRACE("D3D12 PSO state this=%p rts=%u dsv_fmt=%u depth=%u stencil=%u blend0=%u write_mask0=0x%x cull=%u fill=%u front_ccw=%u depth_clip=%u",
          (void *)this, m_num_render_targets, (unsigned)m_dsv_format,
          (unsigned)m_depth_stencil_desc.DepthEnable,
          (unsigned)m_depth_stencil_desc.StencilEnable,
          (unsigned)m_blend_desc.RenderTarget[0].BlendEnable,
          (unsigned)m_blend_desc.RenderTarget[0].RenderTargetWriteMask,
          (unsigned)m_rasterizer_desc.CullMode,
          (unsigned)m_rasterizer_desc.FillMode,
          (unsigned)m_rasterizer_desc.FrontCounterClockwise,
          (unsigned)m_rasterizer_desc.DepthClipEnable);

  m_render_pso = wmt_device.newRenderPipelineState(info, err);
  if (!m_render_pso.handle) {
    char *err_desc = err.handle ? (char *)NSObject_description(err.handle) : nullptr;
    Logger::err(str::format("Failed to create render PSO: ", err_desc ? err_desc : "unknown"));
    return RecordCompileFailure("pso/metal_render_pso",
                                str::format("Metal render PSO creation failed: ",
                                            err_desc ? err_desc : "unknown"));
  }

  if (m_depth_stencil_desc.DepthEnable || m_depth_stencil_desc.StencilEnable) {
    struct WMTDepthStencilInfo ds_info = {};
    ds_info.depth_compare_function = WMTCompareFunctionAlways;
    ds_info.depth_write_enabled = false;
    ds_info.front_stencil.enabled = false;
    ds_info.back_stencil.enabled = false;
    if (m_depth_stencil_desc.DepthFunc >= D3D12_COMPARISON_FUNC_LESS &&
        m_depth_stencil_desc.DepthFunc <= D3D12_COMPARISON_FUNC_ALWAYS) {
      ds_info.depth_compare_function =
          kCompareFunctionMap[m_depth_stencil_desc.DepthFunc];
    }
    ds_info.depth_write_enabled =
        m_depth_stencil_desc.DepthEnable &&
        m_depth_stencil_desc.DepthWriteMask == D3D12_DEPTH_WRITE_MASK_ALL;
    if (m_depth_stencil_desc.StencilEnable) {
      ds_info.front_stencil.enabled = true;
      ds_info.front_stencil.depth_stencil_pass_op =
          kStencilOperationMap[m_depth_stencil_desc.FrontFace.StencilPassOp];
      ds_info.front_stencil.stencil_fail_op =
          kStencilOperationMap[m_depth_stencil_desc.FrontFace.StencilFailOp];
      ds_info.front_stencil.depth_fail_op =
          kStencilOperationMap[m_depth_stencil_desc.FrontFace.StencilDepthFailOp];
      ds_info.front_stencil.stencil_compare_function =
          kCompareFunctionMap[m_depth_stencil_desc.FrontFace.StencilFunc];
      ds_info.front_stencil.write_mask = m_depth_stencil_desc.StencilWriteMask;
      ds_info.front_stencil.read_mask = m_depth_stencil_desc.StencilReadMask;

      ds_info.back_stencil.enabled = true;
      ds_info.back_stencil.depth_stencil_pass_op =
          kStencilOperationMap[m_depth_stencil_desc.BackFace.StencilPassOp];
      ds_info.back_stencil.stencil_fail_op =
          kStencilOperationMap[m_depth_stencil_desc.BackFace.StencilFailOp];
      ds_info.back_stencil.depth_fail_op =
          kStencilOperationMap[m_depth_stencil_desc.BackFace.StencilDepthFailOp];
      ds_info.back_stencil.stencil_compare_function =
          kCompareFunctionMap[m_depth_stencil_desc.BackFace.StencilFunc];
      ds_info.back_stencil.write_mask = m_depth_stencil_desc.StencilWriteMask;
      ds_info.back_stencil.read_mask = m_depth_stencil_desc.StencilReadMask;
    }
    m_depth_stencil_state = wmt_device.newDepthStencilState(ds_info);
  }

  {
    PTRACE("VS_ARGS_DEBUG: shader=%llu NumCB=%u NumArgs=%u CBufBindIdx=%u ArgBufBindIdx=%u ArgTableQwords=%u",
      (unsigned long long)(uintptr_t)m_vs_shader,
      m_vs_reflection.NumConstantBuffers, m_vs_reflection.NumArguments,
      m_vs_reflection.ConstanttBufferTableBindIndex,
      m_vs_reflection.ArgumentBufferBindIndex,
      m_vs_reflection.ArgumentTableQwords);
    if (m_vs_shader && (m_vs_reflection.NumArguments > 0 ||
                        m_vs_reflection.NumConstantBuffers > 0)) {
      if (m_vs_reflection.NumConstantBuffers > 0)
        m_vs_cb_args.resize(m_vs_reflection.NumConstantBuffers);
      if (m_vs_reflection.NumArguments > 0)
        m_vs_args.resize(m_vs_reflection.NumArguments);
      SM50GetArgumentsInfo(m_vs_shader,
                           m_vs_cb_args.empty() ? nullptr : m_vs_cb_args.data(),
                           m_vs_args.empty() ? nullptr : m_vs_args.data());
      for (size_t i = 0; i < m_vs_cb_args.size(); i++) {
        PTRACE("VS_ARGS_DEBUG: cb[%zu] type=%d slot=%u flags=0x%x offset=%u",
          i, (int)m_vs_cb_args[i].Type, m_vs_cb_args[i].SM50BindingSlot,
          m_vs_cb_args[i].Flags, m_vs_cb_args[i].StructurePtrOffset);
      }
      for (size_t i = 0; i < m_vs_args.size(); i++) {
        PTRACE("VS_ARGS_DEBUG: arg[%zu] type=%d slot=%u flags=0x%x offset=%u",
          i, (int)m_vs_args[i].Type, m_vs_args[i].SM50BindingSlot,
          m_vs_args[i].Flags, m_vs_args[i].StructurePtrOffset);
      }
      SM50Destroy(m_vs_shader);
      m_vs_shader = nullptr;
    }
  }

  {
    PTRACE("PS_ARGS_DEBUG: shader=%llu NumCB=%u NumArgs=%u CBufBindIdx=%u ArgBufBindIdx=%u ArgTableQwords=%u",
      (unsigned long long)(uintptr_t)m_ps_shader,
      m_ps_reflection.NumConstantBuffers, m_ps_reflection.NumArguments,
      m_ps_reflection.ConstanttBufferTableBindIndex,
      m_ps_reflection.ArgumentBufferBindIndex,
      m_ps_reflection.ArgumentTableQwords);
    if (m_ps_shader && (m_ps_reflection.NumArguments > 0 ||
                        m_ps_reflection.NumConstantBuffers > 0)) {
      if (m_ps_reflection.NumConstantBuffers > 0)
        m_ps_cb_args.resize(m_ps_reflection.NumConstantBuffers);
      if (m_ps_reflection.NumArguments > 0)
        m_ps_args.resize(m_ps_reflection.NumArguments);
      SM50GetArgumentsInfo(m_ps_shader,
                           m_ps_cb_args.empty() ? nullptr : m_ps_cb_args.data(),
                           m_ps_args.empty() ? nullptr : m_ps_args.data());
      for (size_t i = 0; i < m_ps_cb_args.size(); i++) {
        PTRACE("PS_ARGS_DEBUG: cb[%zu] type=%d slot=%u flags=0x%x offset=%u",
          i, (int)m_ps_cb_args[i].Type, m_ps_cb_args[i].SM50BindingSlot,
          m_ps_cb_args[i].Flags, m_ps_cb_args[i].StructurePtrOffset);
      }
      for (size_t i = 0; i < m_ps_args.size(); i++) {
        PTRACE("PS_ARGS_DEBUG: arg[%zu] type=%d slot=%u flags=0x%x offset=%u",
          i, (int)m_ps_args[i].Type, m_ps_args[i].SM50BindingSlot,
          m_ps_args[i].Flags, m_ps_args[i].StructurePtrOffset);
      }
      SM50Destroy(m_ps_shader);
      m_ps_shader = nullptr;
    }
  }

  m_compiled = true;
  Logger::info(str::format("Graphics PSO compiled: RTs=", m_num_render_targets,
                            " DSV=", (int)m_dsv_format,
                            " samples=", m_sample_count));
  return true;
}

void MTLD3D12PipelineState::SetGraphicsDesc(
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc) {
  if (desc.pRootSignature) {
    m_root_sig = desc.pRootSignature;
    m_root_sig->AddRef();
  }

  if (desc.VS.pShaderBytecode && desc.VS.BytecodeLength) {
    m_vs.resize(desc.VS.BytecodeLength);
    memcpy(m_vs.data(), desc.VS.pShaderBytecode, desc.VS.BytecodeLength);
  }
  if (desc.PS.pShaderBytecode && desc.PS.BytecodeLength) {
    m_ps.resize(desc.PS.BytecodeLength);
    memcpy(m_ps.data(), desc.PS.pShaderBytecode, desc.PS.BytecodeLength);
  }
  if (desc.GS.pShaderBytecode && desc.GS.BytecodeLength) {
    m_gs.resize(desc.GS.BytecodeLength);
    memcpy(m_gs.data(), desc.GS.pShaderBytecode, desc.GS.BytecodeLength);
  }
  if (desc.HS.pShaderBytecode && desc.HS.BytecodeLength) {
    m_hs.resize(desc.HS.BytecodeLength);
    memcpy(m_hs.data(), desc.HS.pShaderBytecode, desc.HS.BytecodeLength);
  }
  if (desc.DS.pShaderBytecode && desc.DS.BytecodeLength) {
    m_ds.resize(desc.DS.BytecodeLength);
    memcpy(m_ds.data(), desc.DS.pShaderBytecode, desc.DS.BytecodeLength);
  }

  m_blend_desc = desc.BlendState;
  m_rasterizer_desc = desc.RasterizerState;
  m_depth_stencil_desc = desc.DepthStencilState;
  m_has_stream_output = desc.StreamOutput.NumEntries > 0 ||
                        desc.StreamOutput.NumStrides > 0 ||
                        desc.StreamOutput.pSODeclaration ||
                        desc.StreamOutput.pBufferStrides;
  m_vs_uses_stage_in = false;
  m_input_elements.clear();
  m_input_semantic_names.clear();
  m_input_layout = {};
  if (desc.InputLayout.NumElements > 0 && desc.InputLayout.pInputElementDescs) {
    m_input_semantic_names.reserve(desc.InputLayout.NumElements);
    m_input_elements.reserve(desc.InputLayout.NumElements);
    for (UINT i = 0; i < desc.InputLayout.NumElements; i++) {
      auto element = desc.InputLayout.pInputElementDescs[i];
      m_input_semantic_names.emplace_back(element.SemanticName ? element.SemanticName : "");
      element.SemanticName = m_input_semantic_names.back().c_str();
      m_input_elements.push_back(element);
    }
    m_input_layout.NumElements = (UINT)m_input_elements.size();
    m_input_layout.pInputElementDescs = m_input_elements.data();
  }
  m_strip_cut_value = desc.IBStripCutValue;
  m_topology = desc.PrimitiveTopologyType;
  m_num_render_targets = desc.NumRenderTargets;
  memcpy(m_rtv_formats, desc.RTVFormats, sizeof(m_rtv_formats));
  m_dsv_format = desc.DSVFormat;
  m_sample_mask = desc.SampleMask;
  m_sample_count = desc.SampleDesc.Count ? desc.SampleDesc.Count : 1;
}

void MTLD3D12PipelineState::SetComputeDesc(
    const D3D12_COMPUTE_PIPELINE_STATE_DESC &desc) {
  if (desc.pRootSignature) {
    m_root_sig = desc.pRootSignature;
    m_root_sig->AddRef();
  }
  if (desc.CS.pShaderBytecode && desc.CS.BytecodeLength) {
    m_cs.resize(desc.CS.BytecodeLength);
    memcpy(m_cs.data(), desc.CS.pShaderBytecode, desc.CS.BytecodeLength);
  }
}

HRESULT STDMETHODCALLTYPE
MTLD3D12PipelineState::QueryInterface(REFIID riid, void **ppvObject) {
  if (!ppvObject)
    return E_POINTER;
  *ppvObject = nullptr;

  if (riid == IID_IUnknown || riid == IID_ID3D12Object ||
      riid == IID_ID3D12DeviceChild || riid == IID_ID3D12Pageable ||
      riid == IID_ID3D12PipelineState) {
    *ppvObject = ref(this);
    return S_OK;
  }
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE
MTLD3D12PipelineState::AddRef() { return ++m_refCount; }

ULONG STDMETHODCALLTYPE MTLD3D12PipelineState::Release() {
  uint32_t rc = --m_refCount;
  if (!rc)
    delete this;
  return rc;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12PipelineState::GetPrivateData(REFGUID guid, UINT *data_size,
                                      void *data) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12PipelineState::SetPrivateData(REFGUID guid, UINT data_size,
                                      const void *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12PipelineState::SetPrivateDataInterface(REFGUID guid,
                                               const IUnknown *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D12PipelineState::SetName(LPCWSTR name) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12PipelineState::GetDevice(REFIID riid, void **device) {
  return m_device->QueryInterface(riid, device);
}

HRESULT STDMETHODCALLTYPE
MTLD3D12PipelineState::GetCachedBlob(ID3DBlob **blob) {
  return E_NOTIMPL;
}

} // namespace dxmt
